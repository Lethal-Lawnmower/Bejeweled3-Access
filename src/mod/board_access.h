#pragma once

#include <windows.h>

#include <cstdint>

// Game board accessibility (stage 1): speaks the gem under the game's own
// keyboard cursor, reading keys, and a summary after each move. The game's
// own controls (arrows, W A S D, Space, Escape) stay untouched; the mod only
// reads the board (docs/re/board-*.md, docs/board-design.md).

// Called every navigator tick while a board is the current screen.
void BoardTick(uintptr_t board);

// Called for keys on a board before they reach the game. Returns true if the
// mod used the key (it is then not passed on).
bool BoardKeyDown(uintptr_t board, WPARAM key);

// Resets tracking when the board is left (a new game starts clean).
void BoardReset();
