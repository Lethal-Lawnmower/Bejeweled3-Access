#include "text_capture.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include "game_memory.h"
#include "log.h"
#include "sexy.h"
#include "text.h"

// All addresses below are from static analysis of the Steam Bejeweled3.exe
// (SHA-256 d5516244...88c3); docs/re/text-drawing.md has the evidence for each.
// They are kept here rather than in sexy.h because only this module uses
// them; every hook re-checks the live bytes before patching.

namespace {

// --- Font vtables (RTTI, tools/rtti_vtables.py) ---------------------------
constexpr uintptr_t kVtImageFont = 0x00875854;
constexpr uintptr_t kVtSysFont = 0x00876BA4;
// Font::DrawString(Graphics*, int x, int y, const SexyString&, const Color&,
// const Rect& clip): slot 11 (+0x2C), __thiscall, callee pops 0x18.
constexpr int kSlotDrawString = 11;
// ImageFont::DrawStringEx(Graphics*, int x, int y, const SexyString&,
// const Color&, const Rect* clip, RectList* drawnAreas, int* width): slot 14
// (+0x38), __thiscall, callee pops 0x20. ImageFont::DrawString forwards here.
constexpr int kSlotDrawStringEx = 14;
constexpr uintptr_t kImageFontDrawString = 0x0058DDF0;
constexpr uintptr_t kImageFontDrawStringEx = 0x0058CFD0;
constexpr uintptr_t kSysFontDrawString = 0x005A1430;

// --- WidgetManager::DrawScreen (FUN_005856b0) -----------------------------
// Custom (LTCG) convention: WidgetManager* in EDI, no stack arguments, plain
// RET, returns AL != 0 when at least one widget was drawn.
constexpr uintptr_t kDrawScreen = 0x005856B0;
// Its first instructions: push ebp; mov ebp,esp; and esp,-8 (6 bytes). The
// detour overwrites them with a 5-byte jmp and a nop and replays them.
constexpr unsigned char kDrawScreenPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8};
// WidgetManager fields used here.
constexpr int kWmCurGraphics = 0x68;  // mCurG: root Graphics* while DrawScreen runs

// --- Graphics (ctor FUN_00431ff0, Translate FUN_00434330) -----------------
constexpr int kGfxDestImage = 0x04;  // Image*
constexpr int kGfxTransX = 0x08;     // float
constexpr int kGfxTransY = 0x0C;     // float
constexpr int kGfxScaleX = 0x10;     // float
constexpr int kGfxScaleY = 0x14;     // float
constexpr int kGfxHeaderSize = 0x18;
// Color {int r, g, b, a} (Color::ToInt FUN_004362d0).
constexpr int kColorSize = 0x10;

constexpr size_t kMaxTextsPerFrame = 1024;

using DrawStringFn = void(__fastcall*)(void* self, void* edx, uintptr_t g, int x, int y,
                                       uintptr_t str, uintptr_t color, uintptr_t clip);
using DrawStringExFn = void(__fastcall*)(void* self, void* edx, uintptr_t g, int x, int y,
                                         uintptr_t str, uintptr_t color, uintptr_t clip,
                                         uintptr_t drawnAreas, int* width);

DrawStringFn g_origImageFontDrawString = nullptr;
DrawStringFn g_origSysFontDrawString = nullptr;
DrawStringExFn g_origImageFontDrawStringEx = nullptr;

bool g_installTried = false;
bool g_fontHooksActive = false;
bool g_frameHookActive = false;

// Recording state. Touched only by the draw thread (g_drawThread).
DWORD g_drawThread = 0;
int g_drawScreenDepth = 0;  // >0 while inside DrawScreen
int g_fontDepth = 0;        // nested font calls (DrawString -> DrawStringEx)
uintptr_t g_currentWm = 0;
int g_remapX = 0, g_remapY = 0;  // WidgetManager mouse remap logical rect origin
uintptr_t g_screenImage = 0;     // dest image of the root Graphics (0 = not read yet)
std::vector<DrawnText> g_building;

// Published frame, shared with readers under g_lock.
SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<DrawnText> g_lastFrame;
uint32_t g_frameNumber = 0;
bool g_loggedFirstFrame = false;

// Return addresses of active DrawScreen calls (the detour replaces the
// return address with ReturnThunk). DrawScreen is not expected to nest; the
// stack only exists so that a nested call can never lose a return address.
constexpr int kMaxReturnDepth = 8;
uintptr_t g_returnStack[kMaxReturnDepth];
int g_returnDepth = 0;
uintptr_t g_drawScreenResume = kDrawScreen + 6;  // first original instruction after the prologue

// One guarded copy per object instead of several ReadU32 calls: the font
// hook runs for every string every frame.
bool CopyGuarded(void* dst, uintptr_t src, size_t len) {
    if (src < 0x10000) return false;
    __try {
        memcpy(dst, reinterpret_cast<const void*>(src), len);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteCode(uintptr_t addr, const void* bytes, size_t len) {
    DWORD old;
    void* p = reinterpret_cast<void*>(addr);
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(p, bytes, len);
    DWORD ignored;
    VirtualProtect(p, len, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return true;
}

// Replaces one vtable slot if it still holds the expected function.
bool PatchSlot(uintptr_t vtable, int slot, uintptr_t expected, void* hook, uintptr_t* original) {
    uintptr_t addr = vtable + 4 * slot;
    uint32_t current = 0;
    if (!ReadU32(addr, &current) || current != expected) {
        Log("text: vtable %08X slot %d holds %08X, expected %08X; not hooked", vtable, slot,
            current, expected);
        return false;
    }
    uint32_t value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook));
    if (!WriteCode(addr, &value, sizeof value)) {
        Log("text: VirtualProtect failed for vtable %08X slot %d (error %lu)", vtable, slot,
            GetLastError());
        return false;
    }
    *original = expected;
    return true;
}

// Records one string. Runs inside the game's draw; must stay cheap and must
// never throw into game code.
void Record(uintptr_t font, uintptr_t g, int x, int y, uintptr_t str, uintptr_t color) {
    if (g_drawScreenDepth <= 0 || g_fontDepth > 0) return;
    if (g_building.size() >= kMaxTextsPerFrame) return;

    unsigned char gfx[kGfxHeaderSize];
    if (!CopyGuarded(gfx, g, sizeof gfx)) return;
    uintptr_t dest;
    float transX, transY, scaleX, scaleY;
    memcpy(&dest, gfx + kGfxDestImage, 4);
    memcpy(&transX, gfx + kGfxTransX, 4);
    memcpy(&transY, gfx + kGfxTransY, 4);
    memcpy(&scaleX, gfx + kGfxScaleX, 4);
    memcpy(&scaleY, gfx + kGfxScaleY, 4);

    if (g_screenImage == 0) {
        // mCurG is set a little after DrawScreen starts, so read it lazily.
        uint32_t root = 0, rootDest = 0;
        if (ReadU32(g_currentWm + kWmCurGraphics, &root) && root &&
            ReadU32(root + kGfxDestImage, &rootDest))
            g_screenImage = rootDest;
    }

    DrawnText t;
    if (!ReadWString(str, &t.text) || t.text.empty()) return;
    t.font = font;
    t.scaleX = scaleX;
    t.scaleY = scaleY;
    t.onScreen = g_screenImage != 0 && dest == g_screenImage;
    // Graphics::DrawImage adds the truncated translation to the int position
    // (FUN_00433080), and DrawScreen translates the root Graphics by minus
    // the remap rectangle origin, so undo that to get WidgetManager space.
    t.x = static_cast<int>(transX) + x;
    t.y = static_cast<int>(transY) + y;
    if (t.onScreen) {
        t.x += g_remapX;
        t.y += g_remapY;
    }
    int32_t c[4];
    if (color && CopyGuarded(c, color, kColorSize))
        t.color = (uint32_t(c[3] & 0xFF) << 24) | (uint32_t(c[0] & 0xFF) << 16) |
                  (uint32_t(c[1] & 0xFF) << 8) | uint32_t(c[2] & 0xFF);
    g_building.push_back(std::move(t));
}

bool OnDrawThread() { return g_drawThread != 0 && GetCurrentThreadId() == g_drawThread; }

// The hooks use __fastcall with a dummy EDX argument to receive __thiscall
// calls: ECX = this, stack arguments popped by the callee.
void __fastcall HookImageFontDrawString(void* self, void* edx, uintptr_t g, int x, int y,
                                        uintptr_t str, uintptr_t color, uintptr_t clip) {
    if (!OnDrawThread()) return g_origImageFontDrawString(self, edx, g, x, y, str, color, clip);
    Record(reinterpret_cast<uintptr_t>(self), g, x, y, str, color);
    ++g_fontDepth;
    g_origImageFontDrawString(self, edx, g, x, y, str, color, clip);
    --g_fontDepth;
}

void __fastcall HookSysFontDrawString(void* self, void* edx, uintptr_t g, int x, int y,
                                      uintptr_t str, uintptr_t color, uintptr_t clip) {
    if (!OnDrawThread()) return g_origSysFontDrawString(self, edx, g, x, y, str, color, clip);
    Record(reinterpret_cast<uintptr_t>(self), g, x, y, str, color);
    ++g_fontDepth;
    g_origSysFontDrawString(self, edx, g, x, y, str, color, clip);
    --g_fontDepth;
}

void __fastcall HookImageFontDrawStringEx(void* self, void* edx, uintptr_t g, int x, int y,
                                          uintptr_t str, uintptr_t color, uintptr_t clip,
                                          uintptr_t drawnAreas, int* width) {
    if (!OnDrawThread())
        return g_origImageFontDrawStringEx(self, edx, g, x, y, str, color, clip, drawnAreas,
                                           width);
    // Reached directly by callers holding an ImageFont*, or nested from
    // DrawString (then g_fontDepth > 0 and Record skips it).
    Record(reinterpret_cast<uintptr_t>(self), g, x, y, str, color);
    ++g_fontDepth;
    g_origImageFontDrawStringEx(self, edx, g, x, y, str, color, clip, drawnAreas, width);
    --g_fontDepth;
}

// Called from the DrawScreen detour with the WidgetManager (EDI) and the
// caller's return address. Returns true if the return address should be
// redirected to ReturnThunk.
bool __cdecl OnDrawScreenEnter(uintptr_t wm, uintptr_t returnAddress) {
    DWORD thread = GetCurrentThreadId();
    if (g_drawThread != thread) {
        // Compare with the install thread in the "text: hooks installed" line:
        // they should match (the doc explains why it matters).
        Log("text: DrawScreen runs on thread %lu", thread);
        g_drawThread = thread;
        g_drawScreenDepth = 0;
    }
    if (g_returnDepth >= kMaxReturnDepth) return false;  // never lose a return address
    g_returnStack[g_returnDepth++] = returnAddress;
    if (g_drawScreenDepth++ == 0) {
        g_currentWm = wm;
        g_screenImage = 0;
        g_fontDepth = 0;  // a C++ exception may have skipped a decrement last frame
        int32_t rx = 0, ry = 0;
        ReadI32(wm + sexy::kWmLogicalRect, &rx);
        ReadI32(wm + sexy::kWmLogicalRect + 4, &ry);
        g_remapX = rx;
        g_remapY = ry;
        g_building.clear();
    }
    return true;
}

// Called from ReturnThunk with DrawScreen's EAX. Returns the original return
// address to continue at.
uintptr_t __cdecl OnDrawScreenExit(uint32_t eax) {
    uintptr_t ret = g_returnStack[--g_returnDepth];
    if (--g_drawScreenDepth == 0) {
        bool drew = (eax & 0xFF) != 0;
        // A DrawScreen that drew no widget leaves the previous image on
        // screen, so the previous frame's text stays the current one.
        if (drew) {
            AcquireSRWLockExclusive(&g_lock);
            g_lastFrame.swap(g_building);
            ++g_frameNumber;
            uint32_t frame = g_frameNumber;
            size_t count = g_lastFrame.size();
            ReleaseSRWLockExclusive(&g_lock);
            if (!g_loggedFirstFrame) {
                g_loggedFirstFrame = true;
                Log("text: first frame published (frame %u, %u strings, remap %d,%d)", frame,
                    static_cast<unsigned>(count), g_remapX, g_remapY);
            }
        }
        g_building.clear();
    }
    if (g_drawScreenDepth < 0) g_drawScreenDepth = 0;
    return ret;
}

// DrawScreen returns here instead of to its caller. Preserves every register
// and the flags, so the caller sees exactly what DrawScreen returned.
__declspec(naked) void ReturnThunk() {
    __asm {
        push 0                      // slot for the real return address
        pushad
        pushfd
        push eax                    // DrawScreen's result (AL)
        call OnDrawScreenExit
        add esp, 4
        mov [esp + 36], eax         // 36 = pushfd (4) + pushad (32)
        popfd
        popad
        ret
    }
}

// Jumped to from DrawScreen's first byte. Records the frame start, then
// replays the overwritten prologue and continues in the original function.
__declspec(naked) void DrawScreenDetour() {
    __asm {
        pushad
        pushfd
        push dword ptr [esp + 36]   // caller's return address
        push edi                    // WidgetManager*
        call OnDrawScreenEnter
        add esp, 8
        test al, al
        jz keep_return
        lea eax, ReturnThunk
        mov [esp + 36], eax
    keep_return:
        popfd
        popad
        push ebp
        mov ebp, esp
        and esp, 0xFFFFFFF8
        jmp dword ptr [g_drawScreenResume]
    }
}

bool InstallFrameHook() {
    unsigned char current[6];
    if (!CopyGuarded(current, kDrawScreen, sizeof current) ||
        memcmp(current, kDrawScreenPrologue, sizeof current) != 0) {
        Log("text: DrawScreen %08X prologue differs (%02X %02X %02X %02X %02X %02X); not hooked",
            kDrawScreen, current[0], current[1], current[2], current[3], current[4], current[5]);
        return false;
    }
    unsigned char patch[6];
    patch[0] = 0xE9;  // jmp rel32
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&DrawScreenDetour) -
                                       (kDrawScreen + 5));
    memcpy(patch + 1, &rel, 4);
    patch[5] = 0x90;  // nop: keeps the replaced 6 bytes whole for disassemblers
    if (!WriteCode(kDrawScreen, patch, sizeof patch)) {
        Log("text: VirtualProtect failed for DrawScreen (error %lu)", GetLastError());
        return false;
    }
    return true;
}

bool OriginBefore(const DrawnText& a, const DrawnText& b) {
    // Strings on one line can have slightly different baselines (fonts,
    // colour segments), so treat baselines within 4 px as the same row.
    if (a.y < b.y - 4) return true;
    if (b.y < a.y - 4) return false;
    return a.x < b.x;
}

}  // namespace

void TextCaptureInstall() {
    if (g_installTried) return;
    g_installTried = true;
    g_building.reserve(kMaxTextsPerFrame);

    // The frame hook goes first: without a frame boundary nothing is
    // recorded, so the font hooks alone would be useless.
    g_frameHookActive = InstallFrameHook();
    if (!g_frameHookActive) {
        Log("text: BLOCKER: frame boundary hook failed; text capture disabled");
        return;
    }
    uintptr_t orig = 0;
    // The DrawStringEx slot is patched first so that a DrawString hooked a
    // moment later already finds its forwarding target hooked.
    bool ex = PatchSlot(kVtImageFont, kSlotDrawStringEx, kImageFontDrawStringEx,
                        reinterpret_cast<void*>(&HookImageFontDrawStringEx), &orig);
    if (ex) g_origImageFontDrawStringEx = reinterpret_cast<DrawStringExFn>(orig);
    bool img = PatchSlot(kVtImageFont, kSlotDrawString, kImageFontDrawString,
                         reinterpret_cast<void*>(&HookImageFontDrawString), &orig);
    if (img) g_origImageFontDrawString = reinterpret_cast<DrawStringFn>(orig);
    bool sys = PatchSlot(kVtSysFont, kSlotDrawString, kSysFontDrawString,
                         reinterpret_cast<void*>(&HookSysFontDrawString), &orig);
    if (sys) g_origSysFontDrawString = reinterpret_cast<DrawStringFn>(orig);
    g_fontHooksActive = ex || img || sys;
    Log("text: hooks installed: DrawScreen=1 ImageFont.DrawStringEx=%d ImageFont.DrawString=%d "
        "SysFont.DrawString=%d (install thread %lu)",
        ex, img, sys, GetCurrentThreadId());
}

bool TextCaptureActive() { return g_frameHookActive && g_fontHooksActive; }

std::vector<DrawnText> TextCaptureLastFrame() {
    AcquireSRWLockShared(&g_lock);
    std::vector<DrawnText> copy = g_lastFrame;
    ReleaseSRWLockShared(&g_lock);
    return copy;
}

uint32_t TextCaptureFrameNumber() {
    AcquireSRWLockShared(&g_lock);
    uint32_t n = g_frameNumber;
    ReleaseSRWLockShared(&g_lock);
    return n;
}

std::vector<DrawnText> TextCaptureInRect(int x, int y, int width, int height) {
    std::vector<DrawnText> out;
    AcquireSRWLockShared(&g_lock);
    for (const DrawnText& t : g_lastFrame) {
        if (t.onScreen && t.x >= x && t.x < x + width && t.y >= y && t.y < y + height)
            out.push_back(t);
    }
    ReleaseSRWLockShared(&g_lock);
    std::stable_sort(out.begin(), out.end(), OriginBefore);
    return out;
}

void TextCaptureDumpLastFrame() {
    std::vector<DrawnText> frame = TextCaptureLastFrame();
    Log("dump: text frame %u (%u strings, hooks active=%d)", TextCaptureFrameNumber(),
        static_cast<unsigned>(frame.size()), TextCaptureActive());
    for (const DrawnText& t : frame) {
        Log("dump:   text %s pos=%d,%d font=%08X color=%08X scale=%.2f,%.2f \"%s\"",
            t.onScreen ? "screen" : "offscreen", t.x, t.y, t.font, t.color, t.scaleX, t.scaleY,
            WideToUtf8(t.text).c_str());
    }
}
