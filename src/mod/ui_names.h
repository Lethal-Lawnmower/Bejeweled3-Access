#pragma once

// Names for widgets and screens that carry no text of their own, taken from
// the game wherever the game itself provides them. Text is always resolved
// at runtime from the game's string table (game_strings.h); this file holds
// only string ids and memory offsets, never game text.
//
// Preferred order for an unlabeled button (see docs/ui-map.md):
//   1. its own mLabel (already read by widgets.cpp);
//   2. text the game itself puts in memory for it at runtime, e.g. the
//      PlayMenu hover description (PlayMenuHoverDescription);
//   3. the evidenced last-resort table below (UiWidgetText): a string the
//      game draws next to / over that widget, or the title of the dialog
//      the button's own handler opens;
//   4. the game's own resource id of the button image (ButtonImageWords),
//      e.g. IMAGE_OVERWORLD_BACK_BUTTON -> "overworld back button".
// Every table entry cites the decompiled function that links the string
// to the widget; docs/re/menus.md has the details.

#include <cstdint>
#include <string>

enum class UiTextSource {
    DrawnNearWidget,    // the class's Draw renders this string at the widget
    OpensDialogTitled,  // the widget's handler opens a dialog with this header
};

enum class UiWidgetKind { Button, Checkbox, Slider };

// Last-resort table lookup by RTTI class name of the owning screen/dialog
// (e.g. "MainMenu"), widget kind and widget id. Returns the raw game text
// (pass through CleanGameText). False if there is no entry or the string
// could not be read; never returns a guessed name.
bool UiWidgetText(const std::string& ownerClass, UiWidgetKind kind, int widgetId,
                  std::wstring* out, UiTextSource* source = nullptr);

// Heading a non-dialog screen draws itself (Dialog subclasses already have
// their header in Dialog::mDialogHeader). False if the class draws none.
bool UiScreenTitle(const std::string& screenClass, std::wstring* out);

// Words from the game's resource id of the button's normal image
// (ButtonWidget::mButtonImage), e.g. "main menu quit button". False if the
// button has no named image.
bool ButtonImageWords(uintptr_t button, std::wstring* out);

// The description the game's PlayMenu::ButtonMouseEnter stores for the
// hovered mode button (already formatted, e.g. with the unlock level), as
// PlayMenu::Draw renders it. Only valid after the game received a hover on
// that button; empty or stale otherwise.
bool PlayMenuHoverDescription(uintptr_t playMenu, std::wstring* out);
