// winmm.dll proxy: the game imports WINMM, so Windows loads this DLL from the
// game folder first. Every export jumps to the real system winmm.dll.
// The export list is generated from the system DLL by tools/gen_winmm_proxy.py.

#include <windows.h>

#include "winmm_proxy.h"

namespace {

#define WINMM_EXPORT(index, name) +1
constexpr int kExportCount = 0
#include "winmm_stubs.inc"
    ;
#undef WINMM_EXPORT

const char* const kExportNames[kExportCount] = {
#define WINMM_EXPORT(index, name) #name,
#include "winmm_stubs.inc"
#undef WINMM_EXPORT
};

HMODULE g_realWinmm = nullptr;

}  // namespace

// Read by the naked stubs, so it needs C linkage and a stable name.
extern "C" FARPROC g_winmmTargets[kExportCount] = {};

#define WINMM_EXPORT(index, name)                                   \
    extern "C" __declspec(naked) void Proxy_##name() {              \
        __asm { jmp dword ptr [g_winmmTargets + (index) * 4] }      \
    }
#include "winmm_stubs.inc"
#undef WINMM_EXPORT

bool LoadRealWinmm() {
    // A 32-bit process sees System32 redirected to SysWOW64.
    wchar_t path[MAX_PATH];
    UINT len = GetSystemDirectoryW(path, MAX_PATH);
    if (len == 0 || len > MAX_PATH - 12) return false;
    lstrcatW(path, L"\\winmm.dll");
    g_realWinmm = LoadLibraryW(path);
    if (!g_realWinmm) return false;
    for (int i = 0; i < kExportCount; ++i) {
        g_winmmTargets[i] = GetProcAddress(g_realWinmm, kExportNames[i]);
    }
    return true;
}
