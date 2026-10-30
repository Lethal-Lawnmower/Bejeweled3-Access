#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

#include "text.h"

namespace {
CRITICAL_SECTION g_lock;
HANDLE g_file = INVALID_HANDLE_VALUE;
}  // namespace

void LogInit() {
    InitializeCriticalSection(&g_lock);
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 40) return;
    lstrcatW(dir, L"\\Bejeweled3Access");
    CreateDirectoryW(dir, nullptr);
    lstrcatW(dir, L"\\mod.log");
    g_file = CreateFileW(dir, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Log(const char* fmt, ...) {
    if (g_file == INVALID_HANDLE_VALUE) return;
    char buf[2048];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int len = _snprintf_s(buf, sizeof buf, _TRUNCATE, "%02d:%02d:%02d.%03d ", t.wHour,
                          t.wMinute, t.wSecond, t.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    int body = _vsnprintf_s(buf + len, sizeof buf - len - 2, _TRUNCATE, fmt, args);
    va_end(args);
    len = body < 0 ? (int)strlen(buf) : len + body;
    buf[len++] = '\r';
    buf[len++] = '\n';
    EnterCriticalSection(&g_lock);
    DWORD written;
    WriteFile(g_file, buf, len, &written, nullptr);
    LeaveCriticalSection(&g_lock);
}

void LogWide(const char* prefix, const std::wstring& text) {
    Log("%s%s", prefix, WideToUtf8(text).c_str());
}
