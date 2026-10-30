#pragma once

#include <string>

std::string WideToUtf8(const std::wstring& text);

// Turns a game display string into speakable text: removes ^RRGGBB^ colour
// codes, converts &cr; and newlines to spaces, collapses whitespace and
// strips the game's leading/trailing padding.
std::wstring CleanGameText(const std::wstring& text);
