# Game actions: operating controls through the game's own code

Pass: 2026-10-30 (session 5), static analysis only (Ghidra 12.0.4 headless,
project copy `private/ghidra-actions`, read-only) of the Steam
`Bejeweled3.exe` (SHA-256 d5516244...88c3, base 0x00400000). The game was
not run. **Nothing here is runtime-verified**; see the checklist at the end.

Why: the user decided (decisions.md, session 5) that the mod must not depend
on the mouse, coordinates, window state or resolution. Each action below
calls the function the game's own input handler ends in, so sounds,
listener notifications and settings side effects match a real click.

Code: `src/mod/game_actions.{h,cpp}` (compiled with `cl /c /W4` to a scratch
object and the inline-asm thunks inspected in the listing; not linked,
built or run). Tools: `tools/ghidra/ActionsGrep.java` (find instructions by
text in an address range). Wrappers and raw output in
`private/ghidra-actions/` (`vt.sh`, `dis.sh`, `dec.sh`, `grep.sh`;
`btn.c`, `btnsub.c`, `bl.c`, `ctl*.c`, `d1..d6.txt`, `g1/g2.txt`, `c7.c`).

Conventions: all virtuals and listener methods are MSVC `__thiscall`
(ECX = this, arguments pushed right to left, callee pops with `RET n`).
The mod calls them through `__fastcall` pointers with a dummy EDX argument
(same technique as `text_capture.cpp`). Three routines take their object in
a register and are called through small inline-asm thunks.

## ButtonListener (7 slots, base vtable 0x00864C8C)

Static, from the base and override vtables (`bl.c`) and the call sites in
ButtonWidget:

| Slot | Method | Evidence |
|---|---|---|
| 0 (+0x00) | ButtonPress(id, clickCount) | base FUN_00404030 = `push [esp+4]; call [vt+4]; ret 8` (forwards id to slot 1). ButtonWidget::MouseDown pushes (id, clickCount) |
| 1 (+0x04) | ButtonPress(id) | RecordsScreen FUN_0069c870, Dialog FUN_005849a0 (`ret 4`) override it |
| 2 (+0x08) | ButtonDepress(id) | ButtonWidget::MouseUp FUN_0049d140 `push [esi+0xBC]; call [vt+8]`; Dialog FUN_005849d0 |
| 3 (+0x0C) | ButtonDownTick(id) | ButtonWidget::Update FUN_0049d180 calls it while mIsDown && mIsOver |
| 4 (+0x10) | ButtonMouseEnter(id) | ButtonWidget::MouseEnter FUN_0049cf30 ends `call [vt+0x10]` |
| 5 (+0x14) | ButtonMouseLeave(id) | ButtonWidget::MouseLeave FUN_0049cfe0 |
| 6 (+0x18) | ButtonMouseMove(id, x, y) | ButtonWidget::MouseMove FUN_0049d0e0 |

Correction to `menus.md`: slot 1 is `ButtonPress(id)`, slot 0 is
`ButtonPress(id, clickCount)` (the MSVC overload order puts the two-argument
overload first). Listeners that override only slot 1 still receive the call
through the base slot 0.

`Bej3ButtonListener` (0x008650F0) slots 1, 2, 4, 5 each just play a sound
(FUN_0076aaf0). So the click and hover sounds of most screens come from the
listener, not from the button.

## ButtonWidget (vtable 0x00872724) input handlers

| Slot | Function | What it does (static) |
|---|---|---|
| 50 (+0xC8) MouseEnter | FUN_0049cf30 | animation reset, MarkDirty, `listener->ButtonMouseEnter(id)` |
| 51 (+0xCC) MouseLeave | FUN_0049cfe0 | as above, `ButtonMouseLeave(id)` |
| 53 (+0xD4) MouseDown(x,y,btn,clicks) | FUN_0049d110 | `listener->ButtonPress(id, clicks)` (slot 0), MarkDirty, `ret 0x10` |
| 55 (+0xDC) MouseUp(x,y,btn,clicks) | FUN_0049d140 | **only if mIsOver (+0x69) and WidgetManager+0x9C (has focus)**: `listener->ButtonDepress(id)`; MarkDirty |
| 69 (+0x114) gamepad down | FUN_0049d290 | button code 6: +0x140 = player, mIsDown = 1, `listener->ButtonPress(id, 1)`, MarkDirty |
| 70 (+0x118) gamepad up | FUN_0049d320 | button code 6 while mIsDown: `listener->ButtonDepress(id)`, mIsDown = 0 |

The game's own non-mouse activation (slots 69/70) is exactly
`ButtonPress(id, 1)` then `ButtonDepress(id)`, with no position or focus
check. That is the precedent `PressButton` follows. The MouseUp focus check
explains the earlier "Could not reach that control" in windowed mode: a
real click on an unfocused window never reaches ButtonDepress.

Subclass overrides against ButtonWidget (`btnsub.c`):

- **Bej3Button** (0x0086498C), **QuestMenuBtn** (0x0086E4F4),
  **WindowedButtonWidget**: no input overrides (Draw only).
- **Bej3DialogButton** (0x00864364): MouseEnter/MouseLeave call the base
  then play a sound; MouseLeave also writes gApp(0x008E170C)+0x9F0 = -10.
  Update requests a tooltip when hovered and it has tooltip text.
- **CrystalBall** (0x00865564): MouseEnter is a thunk of the base; Update
  fades a highlight while mIsOver.
- **HyperlinkWidget** (0x0087658C): own MouseEnter/MouseLeave (not decoded;
  framework: colour/cursor), presses through the same listener.

Dialog buttons: a Bej3DialogButton's listener is its dialog's ButtonListener
sub-object (dialog+0xBC). Dialog::ButtonPress/ButtonDepress (FUN_005849a0 /
FUN_005849d0) forward ids 1000/1001 to `mDialogListener` (dialog+0xC0)
slots 0/1 `DialogButtonPress/Depress(dialogId = dialog+0x174, buttonId)`,
Depress also stores mResult (dialog+0x17C). Bej3Dialog (FUN_00769fd0 /
FUN_00769f60) adds a sound and a guard byte at dialog+0x144 and handles
button 0x3EA. So pressing a dialog button through its listener gives the
full click chain with no extra code.

### `PressButton(button)`

Checks: RTTI kind of ButtonWidget; listener (+0x13C) non-null with a
vtable in the image; widget in a WidgetManager (vtable 0x008751A4 at
`[+0x20]`); mVisible (+0x64) set and mDisabled (+0x66) clear (the
WidgetManager does not deliver clicks to disabled widgets (framework behaviour, not traced in this build); the selected
Records tab and the current high-score mode button are disabled, so
"already selected" is a refusal, like the mouse).
Calls: listener slot 0 `(id, 1)`, then re-checks the button (same id,
listener, still in a WidgetManager) and calls slot 2 `(id)`. If the press
removed the screen, the depress is skipped and logged (the mouse path
would also not deliver it). Hypothesis: removal clears mWidgetManager as in
the framework's WidgetRemovedHelper (not checked in this build).

Covered by this one function: main menu orbs and buttons, Play menu modes
(listener PlayMenu 0x0086E4D4: Depress 0069b360), back/quit, quest orbs
(QuestMenu Depress 00697640, ids >= 1000), Records tabs (RecordsScreen
ButtonPress 0069c870, slot 1 reached through base slot 0) and mode buttons
(HighScoresTab Depress 0069c8c0), Options/Zen/dialog buttons.

## Hover: `HoverWidget(widget, enter)`

WidgetManager (static, `d2.txt`, `d3.txt`):

- `EnterWidget` 0x00585020: ESI = widget, `mov byte [esi+0x69],1`,
  `call [vt+0xC8]` (MouseEnter), then if `[esi+0x94]` ShowFinger(true)
  (`[vt+0x98]`). No stack arguments, plain RET, uses EAX/ECX/EDX only.
- `LeaveWidget` 0x00585050: same with mIsOver = 0, MouseLeave (+0xCC),
  ShowFinger(false).
- `MousePosition(x, y)` FUN_00585a10 (EDI = WidgetManager): stores the last
  mouse x/y at +0x10C/+0x110, finds the widget under it, and if it differs
  from **mOverWidget (+0xA8)**: clears +0xA8, LeaveWidget(old), stores the
  new one, EnterWidget(new); then MouseMove. Called from mouse moves and
  from `Widget::SetDisabled` FUN_0046aac0 (re-evaluates hover after a
  disable).

`HoverWidget` repeats the +0xA8 bookkeeping and calls the two routines
through an ESI thunk. For a button this runs the game's MouseEnter, i.e.
`ButtonMouseEnter(id)` on its listener (PlayMenu writes the mode
description there) plus the subclass hover sound. Leaving only acts when
the widget is the current mOverWidget.

Limitations (static reasoning): the next real mouse move or any
SetDisabled re-evaluates the hover from the stored mouse position and can
leave our widget. Screens that hit-test the stored mouse position
themselves (Zen sections, badges: WidgetManager+0x10C/+0x110 in Update)
are not driven by mIsOver alone; use `SelectZenSection` for Zen.

## Checkbox (vtable 0x00876BDC)

- Slot 81 (+0x144) `SetChecked(bool checked, bool tellListener)`
  FUN_005a15b0: `mov [esi+0xC4],al`; if tell and listener (+0xBC):
  `listener->CheckboxChecked(mId +0xC0, checked)` (listener slot 0); MarkDirty;
  `ret 8`. Slot 82 is IsChecked FUN_005a15f0.
- MouseDown (slot 53) FUN_005a1740: `mChecked = !mChecked`, the same
  listener call, MarkDirty, `ret 0x10`. No sound in either; sounds and
  settings come from the listener (OptionsDialog FUN_00684ae0 applies
  immediately and may open dialogs or revert the box).

`ClickCheckbox` = SetChecked(!current, true) (identical effect to
MouseDown). `SetCheckbox(cb, v)` calls it only when the state differs.
Exact vtable required (no Checkbox subclasses in the RTTI list).

## Slider (vtable 0x00876D2C)

- Slot 81 (+0x144) `SetValue(double)` FUN_005a2730: `fld [esp+4]`, store
  +0xC0, clamp to [0, 1.0], MarkDirty variant (+0x4C), `ret 8`. No listener.
- Slot 57 (+0xE4) `MouseUp(x, y)` FUN_005a2d50: mDragging (+0xD4) = 0,
  cursor reset FUN_0046e9c0(0) with EAX = `[WidgetManager+0x6C]`, then
  `listener->slot0(mId +0xC8, mVal)` and `listener->slot1(mId, mVal)`
  (double pushed with `sub esp,8; fstp [esp]`, then id), `ret 8`. Does not
  null-check the listener and ignores x, y.

`SetSlider(slider, v)` = SetValue(v) then MouseUp(0, 0): the same end
state and notifications as a completed drag (OptionsDialog slot 1 plays
the preview sound). Refuses a null listener. Raw value; the vertical Zen
music slider means `1 - value`.

## ListWidget (vtable 0x008766F4) and DropListWidget (0x0086F3DC)

- ListWidget MouseDown (slot 53) FUN_0059d860: MouseMove(x, y) to set the
  hilite (+0x118), then if hilite != -1 and listener (+0x120):
  `listener->ListClicked(mId +0xC0, hilite, clickCount)`, `ret 0x10`.
- ListListener slot 0 ListClicked(id, index, clickCount): `ret 0xC`
  (DropList FUN_006732b0). Slot 94 of ListWidget is SetSelect FUN_0059d900.
- DropListWidget::ListClicked FUN_006732b0 (this = drop+0xBC): SetOpen(0)
  (`[drop vt+0x154]`), child list SetSelect(index) (`[list vt+0x178]`),
  drop+0xCC = index, owner (+0x134) `ListClicked(drop id +0xC0, index,
  clickCount)`. Correction to `controls.md`: the third argument is the
  click count, not the index.
- DropListWidget::MouseDown (slot 54, (x,y,clicks)) FUN_00673300: base
  Widget::MouseDown, then `SetOpen(!childList->mVisible)` (slot 85,
  +0x154, FUN_00673dc0, `ret 4`: child list SetVisible, its scrollbar
  shown only when rows exceed +0xC8).
- UserSelectionDialog::ListClicked FUN_00683710 uses the passed index (last
  row = add user; otherwise SetSelect + name copy); clickCount > 1 also
  presses button 1000. Zen ListClicked FUN_0068b420 uses the mouse y only
  for the next section's hover inset.

`ChooseListRow(list, i)` calls the list's listener ListClicked(id, i, 1)
(a single click; the hilite is not set, no listener read it). Index range
from the line vector (+0xE0/+0xE4, 0x1C bytes each). It does not require
the row to be scrolled into view. `ChooseDropListRow(drop, i)` = the same
on the child list (+0x130). `SetDropListOpen/ToggleDropList` call SetOpen.

## Zen section select (ZenOptionsDialog vtable 0x0086F55C)

FUN_00683410, only caller ZenCategoryContainer::Update FUN_0042a960:
`mov eax,[esi+0xC4]; mov edi,[esi+0xC0]; push eax; call 00683410` -> **EDI
= dialog, one stack int (section id), `ret 4`**. It pushes and restores
EBX/EBP/ESI and only reads EDI. Behaviour (`d6.txt`, matches zen-options.md):
clears every section's +0x164, returns if id == +0xE8; **returns without
change if the selected section's drop-down list is visible** (0068346f..
00683489); otherwise collapses the old section (curve, sound, closes its
drop-down) and expands the new one (curve, sound), +0xE8 = id.

`SelectZenSection(dialog, id)` checks the vtable, id 0..3, four non-null
sections, the dialog reachable, and the drop-down condition first
(`ZenSectionSelectBlocked`), so the caller can tell the user to close the
list. A mouse left over another section's bare area will reselect that
section on the next Update (the container hit-tests the stored mouse
position); the mouse override should keep the stored position off the panel.

## Safety

- Each game call is inside `__try/__except`; an exception is logged
  (`actions: exception ...`) and the function returns false. Game state may
  be half-updated after such a fault.
- Call only on the game window thread outside drawing (WM_TIMER /
  WM_KEYDOWN handlers). Listeners create and remove widgets.
- Object checks: exact vtable for Checkbox, Slider, ListWidget,
  DropListWidget, ZenOptionsDialog; RTTI IsKindOf for ButtonWidget.
  Every function pointer must be inside the image.

## Integration steps (lead)

1. Add `src\mod\game_actions.cpp` to `tools/build.bat`.
2. `navigator.cpp` `Activate()`: Button/Tab -> `PressButton(item->ptr)`;
   Checkbox -> `ClickCheckbox`; DropList -> `ToggleDropList`; ListRow ->
   `ChooseListRow(listPtr, row)` (for rows of an open drop-down use
   `ChooseDropListRow(drop, row)`); Section -> `SelectZenSection(dialog, id)`
   with `ZenSectionSelectBlocked` for the "close the list first" message.
   Keep the existing report/re-read logic (`g_reportTicks`, `g_change`).
3. `AdjustSlider()`: `SetSlider(item->ptr, target)` and re-read; drop
   `SliderGoal`/corrections for this path.
4. `StartHoverProbe()`: `HoverWidget(item->ptr, true)` instead of posting
   WM_MOUSEMOVE (button hover listeners, PlayMenu description, QuestMenu
   description, Bej3DialogButton tooltip). Badges and Zen sections are
   position-based; leave those to the hover-text work.
5. Keep the mouse path as a fallback until each item below is verified,
   then remove it per decisions.md. Record results in status.md and move
   confirmed offsets to `sexy.h` / `reverse-engineering.md`; link this file
   from `docs/index.md`.

## Runtime checklist (not done)

For each, the log should show the `actions:` line and no exception.

1. Main menu: PressButton on the play orb (label 287) -> click sound(s),
   Play menu opens. On the quit button -> quit dialog; PressButton on its
   no button -> dialog closes (DialogButtonDepress path).
2. Windowed mode with the game window in the background: PressButton still
   works (no focus dependency).
3. HoverWidget(Play mode orb, true) -> hover sound and PlayMenu+0x1C8 holds
   that mode's description; HoverWidget on another orb -> first orb gets
   MouseLeave (WidgetManager+0xA8 changes).
4. Records: PressButton on the Badges tab -> page switches, tab becomes
   disabled; PressButton on it again -> refused ("visible 1, disabled 1").
   High-score mode buttons switch tables.
5. Options: ClickCheckbox on the custom-cursors checkbox (id 6) -> state
   flips and applies; on a resolution box -> the game's confirmation dialog
   appears. Do not test the fullscreen checkbox (id 5) unattended.
6. SetSlider(sound slider id 1, 0.6) -> mVal 0.6, preview sound plays once,
   volume applied; SetSlider(music slider id 0, 1.0) reaches exactly 1.0.
7. Change user: ChooseListRow(list, 0) selects the user; last row opens the
   new-user dialog.
8. Zen: SelectZenSection(dlg, 1) expands section 1 (title 528) with its sounds; with its
   drop-down open (ToggleDropList), SelectZenSection(dlg, 2) is refused;
   ChooseDropListRow(drop, 3) closes the list and changes the sound.
9. Quest orb PressButton starts the quest briefing.

## Uncertainties

- All static. Calling conventions are taken from call sites and RET
  operands; the `__fastcall` trick relies on callee-pop thiscall (true for
  every function listed).
- PressButton skips mIsDown (+0x68) handling and ButtonDownTick; a listener
  reading mIsDown during ButtonPress was not seen but not searched
  exhaustively.
- HyperlinkWidget MouseEnter/Leave and CrystalBall Update were not decoded.
- The WidgetManager focus flag (+0x9C) meaning is from the framework
  (mHasFocus).
