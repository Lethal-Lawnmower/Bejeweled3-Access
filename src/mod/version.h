#pragma once

#include <string>

// Mod version, spoken at startup and written to the log.
constexpr const wchar_t* kModVersion = L"0.2";

// True if the running game is the one build this mod was made for: the
// vanilla Steam Bejeweled3.exe (user decision for 0.1: no other versions,
// no modified games). Every memory location the mod uses belongs to that
// build. `hashHex` receives the file's SHA-256 for the log.
bool GameBuildSupported(std::string* hashHex);
