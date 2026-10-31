#pragma once

#include <windows.h>

// Keyboard navigation and speech for the game's menus and dialogs.
// All functions run on the game's window thread (from the subclassed
// window procedure), so game memory is read between frames.

void NavigatorTick(HWND hwnd);

// Returns true if the key was consumed by the mod.
bool NavigatorKeyDown(HWND hwnd, WPARAM key);

// The key the game should receive for a WM_KEYDOWN / WM_KEYUP the mod did
// not consume. On the board Enter reaches the game as Space (setting
// enterselects), so Enter selects the gem under the cursor.
WPARAM NavigatorGameKey(UINT msg, WPARAM key);

// Returns true if this WM_CHAR belongs to a key the mod consumed.
bool NavigatorChar(WPARAM ch);

// Called for every WM_MOUSEMOVE the window receives.
void NavigatorMouseMove(LPARAM pos);

// True while the mod is driving the mouse, so WM_MOUSELEAVE caused by the
// real cursor being outside the window must not reach the game.
bool NavigatorWithholdMouseLeave();
