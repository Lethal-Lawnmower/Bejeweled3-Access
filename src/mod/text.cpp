#include "text.h"

#include <windows.h>

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0,
                                nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &out[0], n, nullptr,
                        nullptr);
    return out;
}

namespace {
bool IsHex(wchar_t c) {
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
}
}  // namespace

std::wstring CleanGameText(const std::wstring& in) {
    std::wstring out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        wchar_t c = in[i];
        // Colour code: ^RRGGBB^ (also ^oldclr^ in some strings).
        if (c == L'^') {
            size_t end = in.find(L'^', i + 1);
            if (end != std::wstring::npos && end - i <= 9) {
                bool code = end - i == 7;
                for (size_t k = i + 1; code && k < end; ++k) code = IsHex(in[k]);
                if (code || in.compare(i + 1, end - i - 1, L"oldclr") == 0) {
                    i = end;
                    continue;
                }
            }
        }
        if (c == L'&') {
            if (in.compare(i, 4, L"&cr;") == 0) { c = L' '; i += 3; }
            else if (in.compare(i, 6, L"&nbsp;") == 0) { c = L' '; i += 5; }
        }
        // The score fonts put U+2588 (full block) between digits as a spacing
        // glyph; it is not part of the text.
        if (c == 0x2588) continue;
        if (c == L'\n' || c == L'\r' || c == L'\t') c = L' ';
        if (c == L' ' && (out.empty() || out.back() == L' ')) continue;
        out.push_back(c);
    }
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}
