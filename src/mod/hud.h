#pragma once

// Read-only view of the in-game HUD around the board: score, level and
// progress, multiplier, timer, mode counters, pause/game-over state, and the
// game's Announcement texts (level complete, time up, ...).
//
// Everything is read from game memory (board_layout.h); no game code is
// called. Values the mod could not read, or whose getter implementation is
// not one we reviewed, stay "unknown" (known == false) and must not be
// announced as a default. Call from the game's window thread (NavigatorTick).

#include <cstdint>
#include <string>
#include <vector>

enum class HudMode {
    Unknown,
    Classic,      // ClassicBoard
    Zen,          // ZenBoard
    Lightning,    // SpeedBoard
    Poker,        // PokerBoard
    Butterflies,  // ButterflyBoard
    IceStorm,     // InfernoBoard
    DiamondMine,  // DigBoard
    Quest,        // QuestBoard and its other subclasses (Balance, MoveLimit, ...)
};

// Short English mode name for logs/speech ("Classic", "Ice Storm", ...).
const char* HudModeName(HudMode mode);

struct HudInt {
    bool known = false;
    int value = 0;
};

struct HudState {
    uintptr_t board = 0;      // Board*; 0 when no game board exists
    std::string boardClass;   // RTTI class name, e.g. "SpeedBoard"
    HudMode mode = HudMode::Unknown;
    HudInt endless;           // 1 = endless mode, 0 = quest (QuestBoard family only)
    HudInt visible;           // board widget mVisible

    HudInt score;             // actual score
    HudInt displayedScore;    // score as currently drawn (counts up to score)
    HudInt level;             // 1-based level number (Classic/Zen meaning)
    HudInt levelPoints;       // progress toward levelTarget (gold in Diamond Mine)
    HudInt levelTarget;       // points/goal needed; 0 = no target shown
    HudInt multiplier;        // score multiplier (x1, x2, ...)

    HudInt timeLimitSeconds;  // 0 = untimed board
    HudInt ticksLeft;         // remaining time in 1/100 s; unknown if untimed
    HudInt movesLeft;         // MoveLimitBoard only
    HudInt butterfliesFreed;  // stat 0x1C (Butterflies)
    HudInt digDepth;          // Diamond Mine depth base value (see hud.md)

    HudInt gameOver;          // 1 after Board::GameOver
    HudInt gameOverCount;     // ticks since game over (0 while playing)
    HudInt levelCompletePending;  // 1 while the level-complete announcement is pending
    HudInt userPaused;        // space-bar pause toggle (timed boards)
    HudInt pausedOverlay;     // 1 while the pause overlay is drawn (alpha > 0)
    HudInt hintCooldown;      // 1 while the hint button is cooling down

    std::string questGoalClass;     // e.g. "ScoreGoal", empty if none
    std::wstring questDescription;  // quest "Description" text, if any
    bool questDescriptionKnown = false;
};

// Board pointer from gApp, or 0 if none / unreadable.
uintptr_t HudCurrentBoard();

// Fills *out for the current board. Returns false (out->board == 0) when no
// board exists. Individual fields may still be unknown on success.
bool ReadHud(HudState* out);

// The time left as the game's timer draws it ("1:05"): DrawTimer rounds
// 1/100 s ticks up to whole seconds, so "0:01" lasts until time runs out.
std::wstring HudTimeText(int ticksLeft);

// Polls the current board's announcement queue and returns the texts of
// announcements that became visible since the previous call, already cleaned
// for speech (line breaks -> spaces, colour codes removed). Call once per
// tick. Returns an empty vector when nothing new appeared.
std::vector<std::wstring> HudNewAnnouncements();

// Role of a board HUD button (Bej3Button child of the board), by comparing
// the widget pointer with the board's button fields. Lets the navigator name
// the image-only buttons without guessing from position.
enum class BoardButtonRole { None, Hint, Menu, Reset, Extra };
BoardButtonRole HudBoardButtonRole(uintptr_t widget);

// One-line summary of a HudState for the log / F12 dump.
std::string HudDescribe(const HudState& s);
