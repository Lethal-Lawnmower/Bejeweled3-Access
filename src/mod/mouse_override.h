#pragma once

#include <windows.h>

// Makes the game see the mouse where the mod put it.
//
// The game decides whether the mouse is inside its window by asking Windows
// for the real cursor (GetCursorPos, then WindowFromPoint). In fullscreen the
// cursor is always inside, but in a window it usually is not, so the hover
// the mod sets with posted WM_MOUSEMOVE messages was dropped at once. While
// an override is active, the game's own imports of those two functions
// report the mod's position and window instead. Only this process is
// affected; the real cursor never moves.

// Patches the executable's import table. Call once from the window thread.
void MouseOverrideInstall();

// Reports `clientPos` (client coordinates of hwnd) to the game until cleared.
void MouseOverrideSet(HWND hwnd, POINT clientPos);
void MouseOverrideClear();
