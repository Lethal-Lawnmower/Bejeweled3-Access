#pragma once

// Operating UI controls by calling the game's own code: the same functions
// the game's mouse (or gamepad) handlers end in, with the same sounds,
// listener notifications and settings side effects, but without mouse
// events, coordinates, window focus or screen mode.
//
// Evidence for every address, slot and calling convention:
// docs/re/game-actions.md (static analysis of the Steam Bejeweled3.exe,
// SHA-256 d5516244...88c3). Not yet runtime-verified.
//
// Rules for callers:
// - Call only on the game's window thread and never while the game is
//   drawing (the mod's WM_TIMER / WM_KEYDOWN handlers are fine).
// - Pass a widget pointer from a fresh snapshot. Each function re-checks the
//   object's class (vtable or RTTI), that it is in a WidgetManager, visible
//   and enabled, and refuses (returns false and logs why) otherwise.
// - Game code runs inside an SEH guard; a crash in it is caught and logged
//   and the function returns false. Game state may then be inconsistent.
// - A true return means the game call completed, not that the game accepted
//   the change (listeners may refuse, revert a checkbox or open a dialog).
//   Re-read the control's state afterwards.

#include <cstdint>

namespace game_actions {

// Presses a ButtonWidget (any subclass: Bej3Button, Bej3DialogButton,
// CrystalBall, QuestMenuBtn, HyperlinkWidget...) exactly as the game's own
// gamepad path does: listener->ButtonPress(id, 1) then
// listener->ButtonDepress(id). Dialog buttons reach the dialog's
// DialogListener through the dialog's own ButtonPress/ButtonDepress.
// Refuses hidden or disabled buttons (the mouse could not press them
// either; a selected Records tab is a disabled button).
bool PressButton(uintptr_t button);

// Moves the game's "mouse over" to this widget (enter = true) or off it
// (enter = false) through the WidgetManager's own EnterWidget/LeaveWidget
// routines: sets mIsOver, calls the widget's MouseEnter/MouseLeave (which
// for buttons calls ButtonListener::ButtonMouseEnter/Leave, plays the
// hover sound of Bej3DialogButton) and updates WidgetManager::mOverWidget so
// the next real mouse move leaves it correctly. Works for any Widget.
// The game re-evaluates the hover from the last real mouse position when a
// widget is disabled (Widget::SetDisabled) or the mouse moves, which can
// undo this.
bool HoverWidget(uintptr_t widget, bool enter);

// Checkbox: ClickCheckbox toggles exactly like Checkbox::MouseDown.
// SetCheckbox calls Checkbox::SetChecked(checked, tellListener = true)
// only when the state differs (no call, returns true, when it already
// matches).
bool ClickCheckbox(uintptr_t checkbox);
bool SetCheckbox(uintptr_t checkbox, bool checked);

// Slider: Slider::SetValue(value) (clamps to 0..1) followed by
// Slider::MouseUp, which is what ends a drag: listener SliderVal(id, value)
// and this build's second "released" listener slot (Options plays its
// preview sound there). value is the raw 0..1 value (a vertical slider such
// as the Zen music slider shows 1 - value as its level).
bool SetSlider(uintptr_t slider, double value);

// ListWidget row: listener->ListClicked(mId, index, clickCount = 1), which
// is what ListWidget::MouseDown calls for the row under the mouse. The
// listener decides selection (UserSelectionDialog selects the user, the
// last row adds a new user; a drop-down's own list closes it, selects the
// row and tells the owner). index must be 0..line count - 1.
bool ChooseListRow(uintptr_t list, int index);

// DropListWidget: open or close (SetOpen, which DropListWidget::MouseDown
// calls with !open), read the open state, and choose a row of its list
// (ChooseListRow on the child list; works whether or not it is open).
bool SetDropListOpen(uintptr_t dropList, bool open);
bool ToggleDropList(uintptr_t dropList);
bool IsDropListOpen(uintptr_t dropList, bool* open);
bool ChooseDropListRow(uintptr_t dropList, int index);

// Zen options: selects (expands) section 0..3 through the dialog's own
// select routine, as ZenCategoryContainer::Update does on hover. The game
// itself refuses while the current section's drop-down list is open;
// ZenSectionSelectBlocked reports that case so the caller can say so
// (SelectZenSection returns false without calling the game then).
// Selecting the already selected section is a no-op in the game.
bool ZenSectionSelectBlocked(uintptr_t zenDialog, bool* blocked);
bool SelectZenSection(uintptr_t zenDialog, int sectionId);

}  // namespace game_actions
