#pragma once

// User settings, read from %LOCALAPPDATA%\Bejeweled3Access\settings.ini.
// The file is created with the defaults and comments on first start. Every
// board behaviour is a setting because the user chose the defaults before
// playing the game (docs/board-design.md).

enum class CursorSpeech { ColourAndSpecial, WithPosition, PositionOnly };
enum class CoordStyle { ColumnRow, RowColumn, Chess };
enum class AfterMove { Summary, SettledOnly, Off };

struct Settings {
    CursorSpeech cursor = CursorSpeech::ColourAndSpecial;
    CoordStyle coords = CoordStyle::ColumnRow;
    AfterMove afterMove = AfterMove::Summary;
    bool stillMovingWarning = true;
    bool announceSelection = true;
    bool readRowColumnKeys = false;  // R / C on the board
    bool nextSpecialKey = true;      // X on the board (off by default in 0.1)
    bool enterSelects = true;        // Enter on the board acts like Space
    bool soundCues = true;
    bool hintNeighbours = false;     // add the four neighbours to a hint
    bool autoHints = false;          // speak the game's automatic hints (user: H only)
};

void SettingsLoad();
const Settings& CurrentSettings();
