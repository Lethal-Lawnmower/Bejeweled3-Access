#include "settings.h"

#include <windows.h>

#include <string>

#include "log.h"

namespace {

Settings g_settings;
std::wstring g_path;

const char kDefaultFile[] =
    "; Bejeweled 3 Access settings. Edit while the game is closed.\r\n"
    "; Every option here is a default chosen before the board was played;\r\n"
    "; change anything that does not suit you.\r\n"
    "[board]\r\n"
    "; What the cursor says for the gem under it:\r\n"
    ";   colour     colour and special type (default)\r\n"
    ";   position   colour, special type and position\r\n"
    ";   where      position only (read the gem with E)\r\n"
    "cursor=colour\r\n"
    "; How positions are said: columnrow (column 3, row 5), rowcolumn, chess (C5)\r\n"
    "coordinates=columnrow\r\n"
    "; After a move: summary (points, gems, cascades, specials), settled, off\r\n"
    "aftermove=summary\r\n"
    "; Say \"still moving\" when you swap gems that have not landed yet: on, off\r\n"
    "stillmoving=on\r\n"
    "; Say when a gem is selected or deselected: on, off\r\n"
    "selection=on\r\n"
    "; Extra reading keys: R reads the row, C reads the column: on, off\r\n"
    "rowcolumnkeys=off\r\n"
    "; X says where the next special gem is: on, off\r\n"
    "nextspecialkey=on\r\n"
    "; Enter selects the gem under the cursor, like Space: on, off.\r\n"
    "; (Tab, then Enter still presses the board's buttons.)\r\n"
    "enterselects=on\r\n"
    "; Short game sounds as cues (board settled, illegal move): on, off\r\n"
    "soundcues=on\r\n"
    "; Also speak the game's automatic hints (every 15 s when idle): on, off.\r\n"
    "; Hints you ask for with H are always spoken.\r\n"
    "autohints=off\r\n"
    "; Add the four surrounding gems to a hint (Q reads them anyway): on, off\r\n"
    "hintneighbours=off\r\n"
    "; Format of this file, used to update it for new versions; do not change.\r\n"
    "settingsversion=2\r\n";

std::wstring Get(const wchar_t* key, const wchar_t* fallback) {
    wchar_t buf[64];
    GetPrivateProfileStringW(L"board", key, fallback, buf, 64, g_path.c_str());
    std::wstring v(buf);
    for (wchar_t& c : v) c = (wchar_t)towlower(c);
    return v;
}

bool On(const wchar_t* key, bool fallback) {
    std::wstring v = Get(key, fallback ? L"on" : L"off");
    return v == L"on" || v == L"yes" || v == L"true" || v == L"1";
}

}  // namespace

void SettingsLoad() {
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 40) return;  // keep defaults
    g_path = std::wstring(dir) + L"\\Bejeweled3Access\\settings.ini";
    if (GetFileAttributesW(g_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE f = CreateFileW(g_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(f, kDefaultFile, sizeof kDefaultFile - 1, &written, nullptr);
            CloseHandle(f);
            Log("settings: created %s with defaults", "settings.ini");
        }
    }
    // Files from 0.1 (no settingsversion) get the 0.2 default for X, which
    // was off in 0.1; the version line appended below marks them as updated.
    if (Get(L"settingsversion", L"1") == L"1" && Get(L"nextspecialkey", L"?") == L"off") {
        WritePrivateProfileStringW(L"board", L"nextspecialkey", L"on", g_path.c_str());
        Log("settings: updated nextspecialkey to on (0.2 default)");
    }
    // Options added after the file was created are appended with their
    // explanation, so an existing settings file shows every option.
    struct Later { const wchar_t* key; const char* text; };
    const Later later[] = {
        {L"autohints",
         "; Also speak the game's automatic hints (every 15 s when idle): on, off.\r\n"
         "; Hints you ask for with H are always spoken.\r\n"
         "autohints=off\r\n"},
        {L"hintneighbours",
         "; Add the four surrounding gems to a hint (Q reads them anyway): on, off\r\n"
         "hintneighbours=off\r\n"},
        {L"enterselects",
         "; Enter selects the gem under the cursor, like Space: on, off.\r\n"
         "; (Tab, then Enter still presses the board's buttons.)\r\n"
         "enterselects=on\r\n"},
        {L"settingsversion",
         "; Format of this file, used to update it for new versions; do not change.\r\n"
         "settingsversion=2\r\n"},
    };
    for (const Later& opt : later) {
        if (Get(opt.key, L"?") != L"?") continue;
        HANDLE f = CreateFileW(g_path.c_str(), FILE_APPEND_DATA, 0, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD written;
        WriteFile(f, opt.text, (DWORD)strlen(opt.text), &written, nullptr);
        CloseHandle(f);
    }
    std::wstring cursor = Get(L"cursor", L"colour");
    g_settings.cursor = cursor == L"position" ? CursorSpeech::WithPosition
                        : cursor == L"where"  ? CursorSpeech::PositionOnly
                                              : CursorSpeech::ColourAndSpecial;
    std::wstring coords = Get(L"coordinates", L"columnrow");
    g_settings.coords = coords == L"rowcolumn" ? CoordStyle::RowColumn
                        : coords == L"chess"   ? CoordStyle::Chess
                                               : CoordStyle::ColumnRow;
    std::wstring after = Get(L"aftermove", L"summary");
    g_settings.afterMove = after == L"settled" ? AfterMove::SettledOnly
                           : after == L"off"   ? AfterMove::Off
                                               : AfterMove::Summary;
    g_settings.stillMovingWarning = On(L"stillmoving", true);
    g_settings.announceSelection = On(L"selection", true);
    g_settings.readRowColumnKeys = On(L"rowcolumnkeys", false);
    g_settings.nextSpecialKey = On(L"nextspecialkey", true);
    g_settings.enterSelects = On(L"enterselects", true);
    g_settings.soundCues = On(L"soundcues", true);
    g_settings.autoHints = On(L"autohints", false);
    g_settings.hintNeighbours = On(L"hintneighbours", false);
    Log("settings: cursor=%d coords=%d aftermove=%d stillmoving=%d selection=%d rowcol=%d "
        "nextspecial=%d enterselects=%d sounds=%d",
        (int)g_settings.cursor, (int)g_settings.coords, (int)g_settings.afterMove,
        g_settings.stillMovingWarning, g_settings.announceSelection,
        g_settings.readRowColumnKeys, g_settings.nextSpecialKey, g_settings.enterSelects,
        g_settings.soundCues);
}

const Settings& CurrentSettings() { return g_settings; }
