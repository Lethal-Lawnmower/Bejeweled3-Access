#include "speech.h"

#include <windows.h>

#include <deque>

#include "log.h"
#include "prism.h"
#include "text.h"

namespace {

struct Utterance {
    std::string utf8;
    bool interrupt;
};

CRITICAL_SECTION g_lock;
HANDLE g_wake = nullptr;
HANDLE g_thread = nullptr;
volatile bool g_quit = false;
std::deque<Utterance> g_queue;

// prism.dll is delay-loaded; a missing DLL raises an SEH exception here
// instead of preventing the game from starting. Kept free of C++ objects
// so __try is allowed.
bool OpenPrism(PrismContext** ctx, PrismBackend** backend, DWORD* exception) {
    __try {
        PrismConfig cfg = prism_config_init();
        *ctx = prism_init(&cfg);
        if (*ctx) *backend = prism_registry_acquire_best(*ctx);
        return true;
    } __except (*exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

DWORD WINAPI SpeechThread(void*) {
    PrismContext* ctx = nullptr;
    PrismBackend* backend = nullptr;
    DWORD exception = 0;
    if (!OpenPrism(&ctx, &backend, &exception)) {
        Log("speech: BLOCKER prism.dll could not be loaded (exception 0x%08X). "
            "No speech this session.", exception);
        return 1;
    }
    if (!backend) {
        Log("speech: BLOCKER Prism found no usable backend. No speech this session.");
        return 1;
    }
    Log("speech: Prism backend '%s'", prism_backend_name(backend));

    while (!g_quit) {
        WaitForSingleObject(g_wake, INFINITE);
        for (;;) {
            EnterCriticalSection(&g_lock);
            if (g_queue.empty()) {
                LeaveCriticalSection(&g_lock);
                break;
            }
            Utterance u = std::move(g_queue.front());
            g_queue.pop_front();
            LeaveCriticalSection(&g_lock);
            PrismError err = prism_backend_output(backend, u.utf8.c_str(), u.interrupt);
            if (err != PRISM_OK) Log("speech: output error %d", (int)err);
        }
    }
    prism_backend_free(backend);
    prism_shutdown(ctx);
    return 0;
}

}  // namespace

void SpeechStart() {
    InitializeCriticalSection(&g_lock);
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, SpeechThread, nullptr, 0, nullptr);
}

void SpeechStop() {
    g_quit = true;
    if (g_wake) SetEvent(g_wake);
}

void Speak(const std::wstring& text, bool interrupt) {
    if (text.empty()) return;
    LogWide(interrupt ? "say! " : "say  ", text);
    EnterCriticalSection(&g_lock);
    if (interrupt) g_queue.clear();
    g_queue.push_back({WideToUtf8(text), interrupt});
    LeaveCriticalSection(&g_lock);
    SetEvent(g_wake);
}
