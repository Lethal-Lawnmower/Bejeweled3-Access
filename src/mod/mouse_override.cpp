#include "mouse_override.h"

#include <cstring>

#include "log.h"

namespace {

using GetCursorPosFn = BOOL(WINAPI*)(LPPOINT);
using WindowFromPointFn = HWND(WINAPI*)(POINT);

GetCursorPosFn g_realGetCursorPos = nullptr;
WindowFromPointFn g_realWindowFromPoint = nullptr;

// Read and written on the window thread only (the game calls these from
// its main loop, the mod sets them from its window procedure).
bool g_active = false;
HWND g_window = nullptr;
POINT g_screenPos = {};

BOOL WINAPI HookGetCursorPos(LPPOINT out) {
    if (g_active && out) {
        *out = g_screenPos;
        return TRUE;
    }
    return g_realGetCursorPos(out);
}

HWND WINAPI HookWindowFromPoint(POINT pt) {
    if (g_active && pt.x == g_screenPos.x && pt.y == g_screenPos.y) return g_window;
    return g_realWindowFromPoint(pt);
}

// Replaces one USER32 import of the main executable. Returns the original.
void* PatchImport(const char* function, void* hook) {
    auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; desc->Name; ++desc) {
        if (_stricmp(reinterpret_cast<char*>(base + desc->Name), "USER32.dll") != 0) continue;
        auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
        auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (strcmp(reinterpret_cast<char*>(byName->Name), function) != 0) continue;
            void* original = reinterpret_cast<void*>(slots->u1.Function);
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &old))
                return nullptr;
            slots->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&slots->u1.Function, sizeof(void*), old, &old);
            return original;
        }
    }
    return nullptr;
}

}  // namespace

void MouseOverrideInstall() {
    static bool tried = false;
    if (tried) return;
    tried = true;
    g_realGetCursorPos = reinterpret_cast<GetCursorPosFn>(
        PatchImport("GetCursorPos", reinterpret_cast<void*>(&HookGetCursorPos)));
    g_realWindowFromPoint = reinterpret_cast<WindowFromPointFn>(
        PatchImport("WindowFromPoint", reinterpret_cast<void*>(&HookWindowFromPoint)));
    Log("mouse: import hooks GetCursorPos=%d WindowFromPoint=%d", g_realGetCursorPos != nullptr,
        g_realWindowFromPoint != nullptr);
    // A failed patch leaves the import untouched; restore the pointers the
    // hooks call so they never jump to null.
    if (!g_realGetCursorPos) g_realGetCursorPos = &GetCursorPos;
    if (!g_realWindowFromPoint) g_realWindowFromPoint = &WindowFromPoint;
}

void MouseOverrideSet(HWND hwnd, POINT clientPos) {
    POINT screen = clientPos;
    if (!ClientToScreen(hwnd, &screen)) return;
    g_window = hwnd;
    g_screenPos = screen;
    g_active = true;
}

void MouseOverrideClear() { g_active = false; }
