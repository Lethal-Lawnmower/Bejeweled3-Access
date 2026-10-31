#pragma once

// Mode-specific board information: what a sighted player sees in Poker,
// Butterflies, Ice Storm, Diamond Mine, Lightning and the quests beyond the
// gems themselves. Each mode lives in its own file (mode_<name>.cpp) and
// fills a ModeHooks table; board_access and navigator call them through
// ModeHooksFor().
//
// Contract for every hook:
// - Read-only. Game memory only through game_memory.h guarded reads; no game
//   code is called and nothing is written.
// - Same information a sighted player sees, never more: no move advice, no
//   hidden state the screen does not show (docs/board-design.md).
// - Game text through GameString() (game_strings.h) where the game has it;
//   the mod's own words are short English.
// - A value that cannot be read is left out, never replaced by a plausible
//   default.
// - Runs on the game's window thread, every 50 ms tick at most; keep it cheap.

#include <cstdint>
#include <string>
#include <vector>

struct ModeHooks {
    // Words added after the gem name when the cursor lands on (col, row),
    // e.g. "butterfly, 2 rows from the spider" or "dirt, 2 hits". piece is
    // the Piece* in that cell (0 if empty). Empty string: nothing to add.
    std::wstring (*cellText)(uintptr_t board, uintptr_t piece, int col, int row) = nullptr;

    // Short mode status appended to F6, e.g. "hand: 3 cards" or "depth 40".
    std::wstring (*briefStatus)(uintptr_t board) = nullptr;

    // Full mode details for the M key, e.g. the poker hand card by card, all
    // butterflies with their positions, each ice column's height.
    std::wstring (*details)(uintptr_t board) = nullptr;

    // Called every tick; appends texts for changes a sighted player would
    // notice (card added, hand scored, butterfly reached the top row, ice
    // column in danger). Must compare against its own previous state and
    // report each change once.
    void (*tick)(uintptr_t board, std::vector<std::wstring>* events) = nullptr;

    // Forget remembered state (board left or a new game started).
    void (*reset)() = nullptr;
};

// Hooks for the board's RTTI class (PokerBoard, ButterflyBoard, InfernoBoard,
// DigBoard, SpeedBoard, any other QuestBoard subclass), or a table of null
// hooks for Classic / Zen / unknown boards.
const ModeHooks& ModeHooksFor(uintptr_t board);

// Each mode file defines one of these (declared here so mode_info.cpp can
// dispatch without a header per mode).
const ModeHooks& PokerHooks();
const ModeHooks& ButterflyHooks();
const ModeHooks& IceStormHooks();
const ModeHooks& DiamondMineHooks();
const ModeHooks& LightningHooks();
const ModeHooks& QuestHooks();  // QuestBoard and its subclasses
