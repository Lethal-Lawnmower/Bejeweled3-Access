#include "board_access.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "game_actions.h"
#include "game_memory.h"
#include "game_strings.h"
#include "hud.h"
#include "log.h"
#include "mode_info.h"
#include "settings.h"
#include "speech.h"
#include "text.h"

// Fields from the board research (static, with the live check in
// docs/board-research-summary.md marked there).
namespace {

constexpr int kGrid = 0xF8;               // Piece*[64], index col + 8*row (live)
constexpr int kCursorCol = 0x2C44;        // -1 = cursor not shown (live)
constexpr int kCursorRow = 0x2C48;
constexpr int kStillTicks = 0xE3C;        // counts while the board is still (live)
constexpr int kRefillCountdown = 0xE40;   // non-zero while a refill lands (live)
constexpr int kMovesBegin = 0xC48;        // vector of move records (live: empty when settled)
constexpr int kMovesEnd = 0xC4C;
constexpr int kMoveRecordSize = 0xD8;
constexpr int kMoveCascadeStat = 0x40 + 4 * 0x1B;  // per-move cascade passes (static)
constexpr int kScore = 0xD24;             // (live)
constexpr int kStats = 0xC84;             // board stats int[0x26] (static)
constexpr int kStatGemsCleared = 4;
constexpr int kStatSwapsTried = 15;
constexpr int kStatLegalMoves = 20;
constexpr int kVfTooltip = 0x394;         // Board::GetTooltipText slot (static)

// Piece record (no vtable).
constexpr int kPieceId = 0x04;
constexpr int kPieceFall = 0x1C;          // float, 0 when not falling
constexpr int kPieceColour = 0x220;       // 0..6, -1 none (live)
constexpr int kPieceFlags = 0x228;        // special flags (live)
constexpr int kPieceExplodeCountdown = 0x230;
constexpr int kPieceSelected = 0x23C;     // byte
constexpr int kPieceCounter = 0x244;      // seconds / bomb count (static)
constexpr int kColourNameBase = 455;      // string id 455 + colour (static)

constexpr uint32_t kFlagTime = 0x20000, kFlagMoveBomb = 0x20, kFlagTimeBomb = 0x40;

// The game's hint starts a block of animation curves on the hinted gem
// (FUN_00748480; runtime 2026-10-30: only that gem's 0x284..0x49C changed on
// a still board when the hint button was pressed).
constexpr int kHintBlockBegin = 0x280, kHintBlockEnd = 0x4A0;
constexpr int kBoardHintButton = 0x286C;  // hint button, id 0 (docs/re/hud.md)
// ShowHint 0x0075AEB0 always zeroes the idle counter (MOV [ESI+0x26A4],0),
// so a hint is only accepted right after that counter drops.
constexpr int kIdleCounter = 0x26A4;

struct Cell {
    bool present = false;
    uint32_t hintHash = 0;  // hash of the hint animation block
    int id = 0, colour = -1;
    uint32_t flags = 0;
    int counter = 0;
    bool selected = false, still = true;
};

struct Snapshot {
    bool ok = false;
    Cell cells[64];
    int cursorCol = -1, cursorRow = -1;
    int stillTicks = 0, refill = 0, score = 0, moves = 0, maxCascade = 0, idle = 0;
    int stats[0x26] = {};
};

// Tracking between ticks.
uintptr_t g_board = 0;
Snapshot g_prev;
bool g_inMove = false;
Snapshot g_moveStart;  // state when the current move began
int g_moveMaxCascade = 0;
std::map<int, std::wstring> g_startNames;  // special gem id -> game name at move start
bool g_hintArmed = true;   // one announcement per hint animation start
int g_hintWindow = 0;      // ticks left to find the hinted gem after a hint
const ModeHooks* g_hooks = nullptr;  // this board's mode extras (mode_info.h)
int g_lastSpecial = -1;    // cell X last named; repeated X continues after it
int g_lastSpecialFrom = -1;  // cursor cell at that time
int g_hintRequested = 0;   // ticks left in which a hint counts as asked for (H)

// A swap the player just asked for (W/A/S/D, or an arrow with a gem
// selected): what moved where, resolved from the game's own move counters.
struct PendingSwap {
    bool active = false;
    std::wstring what;   // e.g. "Yellow swapped down with White"
    int triedBefore = 0, legalBefore = 0;
    int ticksLeft = 0;
};
PendingSwap g_swap;
int g_quietTicks = 0;  // cursor/selection speech held back around a swap
bool g_swapReported = false;  // the illegal swap was already explained

bool ReadCell(uintptr_t piece, Cell* out) {
    int32_t id, colour, counter, countdown;
    uint32_t flags;
    uint8_t sel;
    float fall;
    if (!ReadI32(piece + kPieceId, &id) || !ReadI32(piece + kPieceColour, &colour) ||
        !ReadU32(piece + kPieceFlags, &flags) || !ReadI32(piece + kPieceCounter, &counter) ||
        !ReadU8(piece + kPieceSelected, &sel) || !ReadU32(piece + kPieceFall, (uint32_t*)&fall) ||
        !ReadI32(piece + kPieceExplodeCountdown, &countdown))
        return false;
    out->present = true;
    out->id = id;
    out->colour = colour;
    out->flags = flags;
    out->counter = counter;
    out->selected = sel != 0;
    out->still = fall == 0.0f && countdown <= 0;
    uint32_t block[(kHintBlockEnd - kHintBlockBegin) / 4];
    uint32_t hash = 2166136261u;
    for (int k = 0; k < (int)(sizeof block / 4); ++k) {
        if (!ReadU32(piece + kHintBlockBegin + 4 * k, &block[k])) return false;
        hash = (hash ^ block[k]) * 16777619u;
    }
    out->hintHash = hash;
    return true;
}

bool TakeSnapshot(uintptr_t board, Snapshot* s) {
    *s = Snapshot();
    for (int i = 0; i < 64; ++i) {
        uint32_t piece;
        if (!ReadU32(board + kGrid + 4 * i, &piece)) return false;
        if (piece && !ReadCell(piece, &s->cells[i])) return false;
    }
    uint32_t mb, me;
    if (!ReadI32(board + kCursorCol, &s->cursorCol) || !ReadI32(board + kCursorRow, &s->cursorRow) ||
        !ReadI32(board + kStillTicks, &s->stillTicks) || !ReadI32(board + kRefillCountdown, &s->refill) ||
        !ReadI32(board + kScore, &s->score) || !ReadI32(board + kIdleCounter, &s->idle) ||
        !ReadU32(board + kMovesBegin, &mb) ||
        !ReadU32(board + kMovesEnd, &me))
        return false;
    for (int k = 0; k < 0x26; ++k) ReadI32(board + kStats + 4 * k, &s->stats[k]);
    if (me >= mb && (me - mb) % kMoveRecordSize == 0 && (me - mb) / kMoveRecordSize < 64) {
        s->moves = (int)((me - mb) / kMoveRecordSize);
        for (int m = 0; m < s->moves; ++m) {
            int32_t passes = 0;
            if (ReadI32(mb + m * kMoveRecordSize + kMoveCascadeStat, &passes) && passes > s->maxCascade)
                s->maxCascade = passes;
        }
    }
    s->ok = true;
    return true;
}

// ---- names, from the game ---------------------------------------------------

std::wstring ColourName(int colour) {
    std::wstring name;
    if (colour >= 0 && colour <= 6 && GameString(kColourNameBase + colour, &name))
        return CleanGameText(name);
    return colour < 0 ? L"" : L"colour " + std::to_wstring(colour);
}

// The game's own tooltip for a piece: thiscall bool(Board*, Piece*,
// wstring* title, wstring* body), RET 0xC (docs/re/board-descriptions.md).
struct GameWString {
    uint32_t proxy;
    uint32_t buffer[4];
    uint32_t size;
    uint32_t capacity;
};
using TooltipFn = bool(__fastcall*)(uintptr_t board, uintptr_t edx, uintptr_t piece,
                                    GameWString* title, GameWString* body);
using GameFreeFn = void(__cdecl*)(void*);
constexpr uintptr_t kGameFree = 0x0064DBCF;  // the game's operator delete (hover_text.cpp)

bool GuardedTooltip(uintptr_t fn, uintptr_t board, uintptr_t piece, GameWString* t,
                    GameWString* b, bool* result) {
    __try {
        *result = reinterpret_cast<TooltipFn>(fn)(board, 0, piece, t, b);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void GuardedFree(uint32_t block) {
    __try {
        reinterpret_cast<GameFreeFn>(kGameFree)(reinterpret_cast<void*>((uintptr_t)block));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::map<uint64_t, std::wstring> g_specialNames;  // cache: board id + flags -> title

// Title the game gives a special gem ("" for a plain gem or on failure).
std::wstring SpecialName(uintptr_t board, int cellIndex, uint32_t flags) {
    if (!flags) return L"";
    uint64_t key = ((uint64_t)flags << 32) | (uint32_t)board;
    auto it = g_specialNames.find(key);
    if (it != g_specialNames.end()) return it->second;
    uint32_t piece, vt, fn;
    std::wstring title;
    if (ReadU32(board + kGrid + 4 * cellIndex, &piece) && piece && ReadU32(board, &vt) &&
        ReadU32(vt + kVfTooltip, &fn) && fn >= 0x00401000 && fn < 0x007BF000) {
        GameWString t = {}, b = {};
        t.capacity = b.capacity = 7;
        bool has = false;
        if (GuardedTooltip(fn, board, piece, &t, &b, &has)) {
            if (has) ReadWString(reinterpret_cast<uintptr_t>(&t), &title);
            if (t.capacity >= 8 && t.buffer[0]) GuardedFree(t.buffer[0]);
            if (b.capacity >= 8 && b.buffer[0]) GuardedFree(b.buffer[0]);
        } else {
            Log("board: tooltip call faulted for flags %#x", flags);
        }
    }
    title = CleanGameText(title);
    g_specialNames[key] = title;
    return title;
}

// ---- sound cues, with the game's own sounds --------------------------------

// PlaySample 0x0076AAF0: EAX = gApp, EDX = sound id, stack (int pan,
// double volume, double pitch), RET 0x14 (docs/re/board-events.md). The
// game calls it as (0, 1.0, 0.0). Sound ids live in globals filled at load.
constexpr uintptr_t kPlaySample = 0x0076AAF0;
constexpr uintptr_t kSoundButtonMouseover = 0x008D88A0;  // SOUND_BUTTON_MOUSEOVER id

__declspec(noinline) void __stdcall CallPlaySample(uint32_t app, uint32_t soundId) {
    double volume = 1.0, pitch = 0.0;
    __asm {
        push dword ptr [pitch + 4]
        push dword ptr [pitch]
        push dword ptr [volume + 4]
        push dword ptr [volume]
        push 0
        mov eax, app
        mov edx, soundId
        mov ecx, kPlaySample
        call ecx
    }
}

bool GuardedPlay(uint32_t app, uint32_t soundId) {
    __try {
        CallPlaySample(app, soundId);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void PlayCue(uintptr_t soundGlobal) {
    if (!CurrentSettings().soundCues) return;
    uint32_t app, id;
    if (!ReadU32(0x008E1730, &app) || !app || !ReadU32(soundGlobal, &id) || id == 0) return;
    if (!GuardedPlay(app, id)) Log("board: PlaySample faulted for sound %u", id);
}

std::wstring Position(int col, int row) {
    const Settings& s = CurrentSettings();
    if (s.coords == CoordStyle::Chess)
        return std::wstring(1, (wchar_t)(L'A' + col)) + std::to_wstring(row + 1);
    if (s.coords == CoordStyle::RowColumn)
        return L"row " + std::to_wstring(row + 1) + L", column " + std::to_wstring(col + 1);
    return L"column " + std::to_wstring(col + 1) + L", row " + std::to_wstring(row + 1);
}

std::wstring Gem(uintptr_t board, const Snapshot& s, int col, int row, bool withSelection = true) {
    const Cell& c = s.cells[col + 8 * row];
    if (!c.present) return L"empty";
    std::wstring colour = ColourName(c.colour);
    std::wstring special = SpecialName(board, col + 8 * row, c.flags);
    std::wstring out = colour;
    if (!special.empty()) out += (out.empty() ? L"" : L" ") + special;
    else if (c.flags) out += L" special";
    if (c.flags & (kFlagTime | kFlagMoveBomb | kFlagTimeBomb)) out += L" " + std::to_wstring(c.counter);
    if (withSelection && c.selected) out += L", selected";
    return out.empty() ? L"gem" : out;
}

// The four gems around a cell, as a sighted player sees them at a glance.
std::wstring NeighboursText(uintptr_t board, const Snapshot& s, int col, int row) {
    std::wstring out;
    struct { int dc, dr; const wchar_t* name; } dirs[] = {
        {0, -1, L"up"}, {0, 1, L"down"}, {-1, 0, L"left"}, {1, 0, L"right"}};
    for (const auto& d : dirs) {
        int c2 = col + d.dc, r2 = row + d.dr;
        std::wstring what = (c2 < 0 || c2 > 7 || r2 < 0 || r2 > 7) ? L"edge" : Gem(board, s, c2, r2);
        out += (out.empty() ? L"" : L". ") + std::wstring(d.name) + L" " + what;
    }
    return out;
}

// The mode's words for a cell (butterfly, ice, dirt, ...), with a leading
// comma, or nothing.
std::wstring CellExtra(uintptr_t board, int col, int row) {
    if (!g_hooks || !g_hooks->cellText) return L"";
    uint32_t piece = 0;
    if (!ReadU32(board + kGrid + 4 * (col + 8 * row), &piece)) return L"";
    std::wstring extra = g_hooks->cellText(board, piece, col, row);
    return extra.empty() ? L"" : L", " + extra;
}

void SpeakCursor(uintptr_t board, const Snapshot& s) {
    const Settings& set = CurrentSettings();
    std::wstring text;
    if (set.cursor == CursorSpeech::PositionOnly)
        text = Position(s.cursorCol, s.cursorRow);
    else if (set.cursor == CursorSpeech::WithPosition)
        text = Gem(board, s, s.cursorCol, s.cursorRow) + L", " + Position(s.cursorCol, s.cursorRow);
    else
        text = Gem(board, s, s.cursorCol, s.cursorRow);
    if (set.cursor != CursorSpeech::PositionOnly) text += CellExtra(board, s.cursorCol, s.cursorRow);
    Speak(text, true);
}

// ---- after-move summary -----------------------------------------------------

bool Settled(const Snapshot& now, const Snapshot& before) {
    return now.moves == 0 && now.refill == 0 && now.stillTicks > before.stillTicks;
}

void SpeakSummary(const Snapshot& start, const Snapshot& end) {
    int tried = end.stats[kStatSwapsTried] - start.stats[kStatSwapsTried];
    int legal = end.stats[kStatLegalMoves] - start.stats[kStatLegalMoves];
    const Settings& set = CurrentSettings();
    if (tried > 0 && legal <= 0) {
        if (!g_swapReported) Speak(L"no match, swapped back", true);
        g_swapReported = false;
        return;
    }
    g_swapReported = false;
    if (set.afterMove == AfterMove::Off) return;
    if (set.afterMove == AfterMove::SettledOnly) {
        Speak(L"settled", false);
        return;
    }
    std::wstring out;
    auto add = [&](const std::wstring& s) { out += (out.empty() ? L"" : L", ") + s; };
    int points = end.score - start.score;
    if (points) add(L"plus " + std::to_wstring(points));
    int cleared = end.stats[kStatGemsCleared] - start.stats[kStatGemsCleared];
    if (cleared > 0) add(std::to_wstring(cleared) + L" gems");
    if (g_moveMaxCascade > 1) {
        int n = g_moveMaxCascade - 1;
        add(std::to_wstring(n) + (n == 1 ? L" cascade" : L" cascades"));
    }
    // Special gems made and set off, named by the game's own tooltip titles:
    // new special ids (or new flags on a gem) after the move were made; ids
    // that were special before and are gone were set off.
    std::map<int, uint32_t> before, after;
    std::map<int, int> afterCell;
    for (const Cell& c : start.cells) if (c.present && c.flags) before[c.id] = c.flags;
    for (int i = 0; i < 64; ++i) {
        const Cell& c = end.cells[i];
        if (c.present && c.flags) { after[c.id] = c.flags; afterCell[c.id] = i; }
    }
    std::map<std::wstring, int> made, setOff;
    for (const auto& a : after) {
        auto b = before.find(a.first);
        if (b == before.end() || b->second != a.second)
            ++made[SpecialName(g_board, afterCell[a.first], a.second)];
    }
    for (const auto& b : before)
        if (!after.count(b.first)) ++setOff[g_startNames.count(b.first) ? g_startNames[b.first] : L""];
    // "FLAME GEM made twice" rather than adding an s to the game's own names.
    auto times = [](int n) {
        return n == 1 ? std::wstring() : n == 2 ? std::wstring(L" twice") : L" " + std::to_wstring(n) + L" times";
    };
    for (const auto& m : made)
        add((m.first.empty() ? std::wstring(L"special") : m.first) + L" made" + times(m.second));
    for (const auto& o : setOff)
        add((o.first.empty() ? std::wstring(L"special") : o.first) + L" set off" + times(o.second));
    if (!out.empty()) Speak(out, false);
}

}  // namespace

void BoardReset() {
    g_board = 0;
    g_prev = Snapshot();
    g_inMove = false;
    g_specialNames.clear();
    g_lastSpecial = g_lastSpecialFrom = -1;
    if (g_hooks && g_hooks->reset) g_hooks->reset();
    g_hooks = nullptr;
}

void BoardTick(uintptr_t board) {
    if (board != g_board) {
        BoardReset();
        g_board = board;
        g_hooks = &ModeHooksFor(board);
        if (g_hooks->reset) g_hooks->reset();
    }
    // Mode events (card added, butterfly at the top, ...) queue after other speech.
    if (g_hooks->tick) {
        std::vector<std::wstring> events;
        g_hooks->tick(board, &events);
        for (const std::wstring& e : events) Speak(e, false);
    }
    Snapshot now;
    if (!TakeSnapshot(board, &now)) return;
    if (!g_prev.ok) {
        g_prev = now;
        return;
    }

    // Resolve a swap the player asked for, from the game's move counters.
    if (g_swap.active) {
        int tried = now.stats[kStatSwapsTried] - g_swap.triedBefore;
        int legal = now.stats[kStatLegalMoves] - g_swap.legalBefore;
        if (tried > 0) {
            g_swap.active = false;
            g_quietTicks = 10;
            if (legal > 0) {
                Speak(g_swap.what, true);  // the move summary follows when settled
            } else {
                Speak(g_swap.what + L", no match, swapped back", true);
                g_swapReported = true;
            }
        } else if (--g_swap.ticksLeft <= 0) {
            g_swap.active = false;  // the game did not take the swap
        }
    }
    bool quiet = g_swap.active || g_quietTicks > 0;
    if (g_quietTicks > 0) --g_quietTicks;

    // The game's cursor moved, or the gem under it changed after a swap.
    if (!quiet && now.cursorCol >= 0 && now.cursorRow >= 0 &&
        (now.cursorCol != g_prev.cursorCol || now.cursorRow != g_prev.cursorRow))
        SpeakCursor(board, now);

    // The game showed a hint (button or automatic): its idle counter drops to
    // 0, and within a few ticks exactly one gem's hint animation block changes.
    if (g_hintRequested > 0) --g_hintRequested;
    if (now.idle < g_prev.idle && !g_inMove) g_hintWindow = 6;
    if (g_hintWindow > 0 && !g_inMove) {
        --g_hintWindow;
        int changed = -1, count = 0;
        for (int i = 0; i < 64; ++i) {
            const Cell& a = g_prev.cells[i];
            const Cell& b = now.cells[i];
            if (a.present && b.present && a.id == b.id && a.hintHash != b.hintHash) {
                changed = i;
                ++count;
            }
        }
        bool wanted = g_hintRequested > 0 || CurrentSettings().autoHints;
        if (count == 1 && g_hintArmed && wanted) {
            g_hintArmed = false;
            g_hintWindow = 0;
            g_hintRequested = 0;
            // The game's hint marks one gem; say it and what surrounds it, as
            // a sighted player sees it. The direction is left to the player.
            // User (2026-10-30): just the hinted gem; Q reads its neighbours.
            std::wstring text = L"hint, " + Gem(board, now, changed % 8, changed / 8) + L", " +
                                Position(changed % 8, changed / 8);
            if (CurrentSettings().hintNeighbours)
                text += L". " + NeighboursText(board, now, changed % 8, changed / 8);
            Speak(text, false);
        }
        if (count == 0) g_hintArmed = true;  // ready for the next hint
    } else if (g_hintWindow == 0) {
        g_hintArmed = true;
    }

    // Selection changes (Space).
    if (!quiet && CurrentSettings().announceSelection) {
        for (int i = 0; i < 64; ++i) {
            const Cell& a = g_prev.cells[i];
            const Cell& b = now.cells[i];
            if (b.present && a.present && a.id == b.id && a.selected != b.selected)
                Speak(b.selected ? L"selected" : L"deselected", true);
        }
    }

    // A move starts when the board stops being still; it ends when settled.
    bool moving = now.moves > 0 || now.refill > 0 || now.stillTicks == g_prev.stillTicks;
    bool swapCounted = now.stats[kStatSwapsTried] != g_prev.stats[kStatSwapsTried];
    if (!g_inMove && (moving || swapCounted)) {
        g_inMove = true;
        g_moveStart = g_prev;
        g_moveMaxCascade = 0;
        // Name the specials now: once set off they are gone from the board.
        g_startNames.clear();
        for (int i = 0; i < 64; ++i)
            if (g_prev.cells[i].present && g_prev.cells[i].flags)
                g_startNames[g_prev.cells[i].id] = SpecialName(board, i, g_prev.cells[i].flags);
    }
    if (g_inMove) {
        if (now.maxCascade > g_moveMaxCascade) g_moveMaxCascade = now.maxCascade;
        if (Settled(now, g_prev)) {
            g_inMove = false;
            PlayCue(kSoundButtonMouseover);  // the board is ready again
            SpeakSummary(g_moveStart, now);
        }
    }
    g_prev = now;
}

std::wstring BoardModeStatus(uintptr_t board) {
    const ModeHooks& hooks = ModeHooksFor(board);
    return hooks.briefStatus ? hooks.briefStatus(board) : L"";
}

std::wstring BoardKeyHelp() {
    const Settings& set = CurrentSettings();
    std::wstring out = L"Board keys. Arrows move the cursor. Space";
    if (set.enterSelects) out += L" or Enter";
    out += L" selects a gem, then an arrow swaps it. W, A, S, D swap the gem under the cursor "
           L"up, left, down or right. E where am I. Q the four gems around. H hint. "
           L"T time left. F6 score and progress. M mode details, such as the poker hand.";
    if (set.nextSpecialKey) out += L" X next special gem.";
    if (set.readRowColumnKeys) out += L" R row, C column.";
    out += L" Tab, then Enter, the board's buttons. Escape game menu.";
    return out;
}

bool BoardKeyDown(uintptr_t board, WPARAM key) {
    Snapshot s;
    if (!TakeSnapshot(board, &s)) return false;
    const Settings& set = CurrentSettings();
    bool cursor = s.cursorCol >= 0 && s.cursorRow >= 0;
    int col = s.cursorCol, row = s.cursorRow;

    // Remember what a swap key is about to move, to say what happened.
    int selected = -1;
    for (int i = 0; i < 64; ++i)
        if (s.cells[i].present && s.cells[i].selected) selected = i;
    int fromCol = -1, fromRow = -1, dc = 0, dr = 0;
    if ((key == 'W' || key == 'A' || key == 'S' || key == 'D') && cursor) {
        fromCol = col;
        fromRow = row;
    } else if ((key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT) && selected >= 0) {
        fromCol = selected % 8;
        fromRow = selected / 8;
    }
    if (fromCol >= 0) {
        dc = (key == 'A' || key == VK_LEFT) ? -1 : (key == 'D' || key == VK_RIGHT) ? 1 : 0;
        dr = (key == 'W' || key == VK_UP) ? -1 : (key == 'S' || key == VK_DOWN) ? 1 : 0;
        int toCol = fromCol + dc, toRow = fromRow + dr;
        if (toCol >= 0 && toCol < 8 && toRow >= 0 && toRow < 8) {
            const wchar_t* dir = dr < 0 ? L"up" : dr > 0 ? L"down" : dc < 0 ? L"left" : L"right";
            g_swap.active = true;
            g_swap.what = Gem(board, s, fromCol, fromRow, false) + L" swapped " + dir + L" with " +
                          Gem(board, s, toCol, toRow, false);
            g_swap.triedBefore = s.stats[kStatSwapsTried];
            g_swap.legalBefore = s.stats[kStatLegalMoves];
            g_swap.ticksLeft = 40;  // 2 s for the game to take it
        }
    }

    // Swap keys belong to the game; only warn when the gems have not landed.
    if (key == 'W' || key == 'A' || key == 'S' || key == 'D') {
        if (set.stillMovingWarning && cursor) {
            int c2 = col + dc, r2 = row + dr;  // dc/dr set above for this key
            bool busy = !s.cells[col + 8 * row].still ||
                        (c2 >= 0 && c2 < 8 && r2 >= 0 && r2 < 8 && !s.cells[c2 + 8 * r2].still);
            if (busy) Speak(L"still moving", true);
        }
        return false;
    }

    switch (key) {
        case 'H': {  // the game's own hint button
            uint32_t button = 0;
            if (!ReadU32(board + kBoardHintButton, &button) || !button ||
                !game_actions::PressButton(button))
                Speak(L"hint not available right now", true);
            else
                g_hintRequested = 10;  // the game shows it within a few ticks
            return true;
        }
        case 'E':  // where am I
            if (!cursor) {
                Speak(L"no cursor yet, press an arrow key", true);
            } else {
                Speak(Gem(board, s, col, row) + CellExtra(board, col, row) + L", " +
                          Position(col, row), true);
            }
            return true;
        case 'M': {  // the mode's own objects: poker hand, butterflies, ice, ...
            std::wstring text = g_hooks && g_hooks->details ? g_hooks->details(board) : L"";
            Speak(text.empty() ? L"no mode details" : text, true);
            return true;
        }
        case 'Q':  // neighbours
            if (!cursor) {
                Speak(L"no cursor yet, press an arrow key", true);
                return true;
            }
            Speak(NeighboursText(board, s, col, row), true);
            return true;
        case 'T': {  // time left, as the game's timer shows it
            HudState hud;
            bool read = ReadHud(&hud) && hud.timeLimitSeconds.known;
            if (read && hud.timeLimitSeconds.value == 0)
                Speak(L"no time limit", true);
            else if (!read || !hud.ticksLeft.known)
                Speak(L"time not available", true);
            else
                Speak(HudTimeText(hud.ticksLeft.value) + L" left", true);
            return true;
        }
        case 'R':
        case 'C': {
            if (!set.readRowColumnKeys) return false;
            if (!cursor) {
                Speak(L"no cursor yet, press an arrow key", true);
                return true;
            }
            std::wstring out;
            for (int i = 0; i < 8; ++i) {
                std::wstring g = key == 'R' ? Gem(board, s, i, row) : Gem(board, s, col, i);
                out += (out.empty() ? L"" : L", ") + g;
            }
            Speak(out, true);
            return true;
        }
        case 'X': {
            if (!set.nextSpecialKey) return false;
            // From the cursor in reading order; pressing X again without
            // moving the cursor goes on to the next one.
            int from = cursor ? col + 8 * row : -1;
            int start = from >= 0 ? from + 1 : 0;
            if (g_lastSpecial >= 0 && g_lastSpecialFrom == from) start = g_lastSpecial + 1;
            for (int k = 0; k < 64; ++k) {
                int i = (start + k) % 64;
                if (s.cells[i].present && s.cells[i].flags) {
                    g_lastSpecial = i;
                    g_lastSpecialFrom = from;
                    Speak(Gem(board, s, i % 8, i / 8) + L", " + Position(i % 8, i / 8), true);
                    return true;
                }
            }
            Speak(L"no special gems", true);
            return true;
        }
    }
    return false;
}
