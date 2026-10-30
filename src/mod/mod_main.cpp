// Entry point: DllMain of the winmm.dll proxy. DllMain only loads the real
// winmm and starts a thread; everything else runs outside the loader lock.

#include <windows.h>

#include "../proxy/winmm_proxy.h"
#include "log.h"
#include "navigator.h"
#include "settings.h"
#include "speech.h"
#include "mouse_override.h"
#include "text.h"
#include "text_capture.h"
#include "version.h"

namespace {

constexpr UINT_PTR kTimerId = 0xB3A11;
constexpr UINT kTickMs = 50;

HWND g_hwnd = nullptr;
WNDPROC g_originalProc = nullptr;
bool g_ansiWindow = false;
bool g_timerStarted = false;

void StartWindowFinder();

LRESULT CALLBACK ModWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!g_timerStarted) {
        // SetTimer needs the window's own thread, so start it from here.
        g_timerStarted = SetTimer(hwnd, kTimerId, kTickMs, nullptr) != 0;
        // Patching the draw path must happen on the drawing thread, outside a
        // draw; a message handler is such a point. Acts only on the first call.
        TextCaptureInstall();
        MouseOverrideInstall();
    }
    switch (msg) {
        case WM_TIMER:
            if (wp == kTimerId) {
                NavigatorTick(hwnd);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (NavigatorKeyDown(hwnd, wp)) return 0;
            break;
        case WM_CHAR:
            if (NavigatorChar(wp)) return 0;
            break;
        case WM_MOUSEMOVE:
            NavigatorMouseMove(lp);
            break;
        case WM_MOUSELEAVE:
            if (NavigatorWithholdMouseLeave()) return 0;
            break;
        case WM_NCDESTROY: {
            // The game recreates its window when switching display modes.
            Log("main: window %p destroyed; waiting for a new one", hwnd);
            WNDPROC original = g_originalProc;
            g_hwnd = nullptr;
            g_timerStarted = false;
            StartWindowFinder();
            return g_ansiWindow ? CallWindowProcA(original, hwnd, msg, wp, lp)
                                : CallWindowProcW(original, hwnd, msg, wp, lp);
        }
    }
    return g_ansiWindow ? CallWindowProcA(g_originalProc, hwnd, msg, wp, lp)
                        : CallWindowProcW(g_originalProc, hwnd, msg, wp, lp);
}

BOOL CALLBACK FindGameWindow(HWND hwnd, LPARAM result) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
        return TRUE;
    *reinterpret_cast<HWND*>(result) = hwnd;
    return FALSE;
}

DWORD WINAPI WindowFinderThread(void*) {
    HWND hwnd = nullptr;
    while (!hwnd) {
        Sleep(200);
        EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&hwnd));
    }
    char cls[128] = {};
    GetClassNameA(hwnd, cls, sizeof cls);
    g_ansiWindow = !IsWindowUnicode(hwnd);
    g_hwnd = hwnd;
    g_originalProc = reinterpret_cast<WNDPROC>(
        g_ansiWindow ? SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)ModWndProc)
                     : SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)ModWndProc));
    Log("main: subclassed window %p class '%s' ansi=%d", hwnd, cls, g_ansiWindow);
    // Wake the window so the timer starts even before the first input.
    PostMessageW(hwnd, WM_NULL, 0, 0);
    return 0;
}

void StartWindowFinder() {
    HANDLE t = CreateThread(nullptr, 0, WindowFinderThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}

DWORD WINAPI StartupThread(void*) {
    Log("main: Bejeweled 3 Access %s starting", WideToUtf8(kModVersion).c_str());
    SettingsLoad();
    SpeechStart();
    // Only the vanilla Steam build is supported (its memory layout is what
    // the mod reads). Any other build: say so and never touch the game.
    std::string hash;
    if (!GameBuildSupported(&hash)) {
        Log("main: unsupported game build (SHA-256 %s); accessibility disabled",
            hash.empty() ? "unreadable" : hash.c_str());
        Speak(L"Bejeweled 3 access: this version of Bejeweled 3 is not supported. "
              L"Only the Steam version is. Accessibility is turned off.",
              false);
        return 0;
    }
    Log("main: game build verified");
    Speak(std::wstring(L"Bejeweled 3 access ") + kModVersion + L" loaded", false);
    StartWindowFinder();
    return 0;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!LoadRealWinmm()) return FALSE;
        LogInit();
        HANDLE t = CreateThread(nullptr, 0, StartupThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    } else if (reason == DLL_PROCESS_DETACH) {
        SpeechStop();
    }
    return TRUE;
}
