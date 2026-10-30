#pragma once

#include <string>

// Plain-text log at %LOCALAPPDATA%\Bejeweled3Access\mod.log (UTF-8).
// Thread-safe. The log is overwritten on each game start.
void LogInit();
void Log(const char* fmt, ...);
void LogWide(const char* prefix, const std::wstring& text);
