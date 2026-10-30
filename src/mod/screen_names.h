#pragma once

// Names for screens taken from the game itself, and a static model of which
// keys the game's own widgets react to. Evidence for every rule here is in
// docs/re/screen-names-and-keys.md (static analysis, 2026-10-30; nothing
// here is runtime-verified yet). No game text is stored in this source: the
// names are read from the game's string table (by the ids the game's own
// code uses) or from the game objects' memory.

#include <cstdint>
#include <string>

// Where a screen name came from, strongest first. Only the first five are
// returned by ScreenName(); HintTooltip is a weaker association that needs
// a runtime check before it is spoken (see the doc).
enum class ScreenNameSource {
    None,
    BoardTitleText,      // board params "TitleText": the quest title the game
                         // draws on quest boards and uses as the quest
                         // briefing dialog's header
    BoardTitle,          // board params "Title": the mode name the game
                         // attaches to the board (Classic, Zen, Lightning,
                         // each quest and secret mode)
    DialogHeader,        // Dialog::mDialogHeader (+0xD8)
    DrawnHeading,        // a string the screen's own Draw paints as its heading
    OpeningButtonLabel,  // label of the menu orb whose handler opens the screen
    HintTooltip,         // header of a game hint tooltip about the screen
};

const char* ScreenNameSourceName(ScreenNameSource source);

// Game text naming the screen widget (the navigator's context widget:
// a menu, a dialog or a board). False when the game associates no text
// with this screen (MainMenu, the developer Menu, end-level dialogs, ...)
// or a read failed; the caller then keeps its own fallback.
bool ScreenName(uintptr_t screenWidget, std::wstring* out);

// As ScreenName, also reporting the source. allowHypotheses additionally
// enables HintTooltip names (ZenOptionsDialog only).
bool ScreenNameWithSource(uintptr_t screenWidget, bool allowHypotheses, std::wstring* out,
                          ScreenNameSource* source);

// The selected part of a tabbed screen, read from the game's tab button
// label (RecordsScreen: the selected High Scores / Badges / Stats tab).
// False for other screens or when no tab is selected/readable.
bool ScreenSection(uintptr_t screenWidget, std::wstring* out);

// True if the game itself reacts to this key (WM_KEYDOWN virtual-key code)
// when it reaches the given focused widget (WidgetManager+0xA0), judged by
// which KeyDown implementation the widget's vtable holds. `what`, if not
// null, receives a short English description for the log. Unknown
// implementations return false with what = "unknown KeyDown". This only
// models the widget dispatch; app-level keys (Alt+Enter, F1, the
// Ctrl+Alt+D debug toggle) are reported by GameAppHandlesKey.
bool GameHandlesKey(uintptr_t focusWidget, int vk, const char** what);

// Keys the framework handles before any widget sees them (static:
// WindowsAppDriver message processing FUN_005b9670 and its DebugKeyDown
// FUN_005b8df0). alt/ctrl reflect the modifier state at the key press.
bool GameAppHandlesKey(int vk, bool alt, bool ctrl, const char** what);
