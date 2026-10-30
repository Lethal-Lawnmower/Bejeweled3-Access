# Interactive controls: Checkbox, Slider, lists, scrollbars, tabs

Pass date: 2026-10-30. Scope: static analysis only (Ghidra 12.0.4 headless,
project copy `private/ghidra-controls`, read-only) of the Steam
`Bejeweled3.exe` (SHA-256 `d5516244...88c3`, base 0x00400000). The game was
not run. Nothing here is runtime-verified; see "Runtime checklist" at the end.

Code: `src/mod/controls.h`, `src/mod/controls.cpp` (compile-checked with
`tools/compile-one.bat`, /W4 clean; the slider and list planning maths was
also checked in a scratch test against a model of the decompiled formulas,
which only shows the code matches my reading, not the game).

Tools added: `tools/ghidra/ControlsDisasm.java` (prints the disassembly of
the function containing each address; used where the decompiler dropped x87
arguments). Raw output (proprietary-derived, git-ignored) is in
`private/ghidra-controls/*.c` and `*.asm`.

Framework reference for field order: PeggleCommunity/PopCapFramework v1.34
`Checkbox.h`, `Slider.h`, `ListWidget.h`, `ScrollbarWidget.h` (copies in
`private/framework-ref`). This build differs in places (noted below).

## Vtable slot notes (this build)

MSVC places overloaded virtuals in reverse declaration order. Confirmed from
the Checkbox/Slider vtables (0x00876BDC / 0x00876D2C) and base Widget
(0x00871D84):

| Slot (offset) | Function | Evidence |
|---|---|---|
| 17 (+0x44) | MarkDirty | called after every state change in SetChecked/SetValue |
| 19 (+0x4C) | MarkDirty variant used by Slider | SetValue/MouseDrag |
| 31 (+0x7C) | SetVisible(bool) | FUN_0046a780 writes +0x64 |
| 37 (+0x94) | SetDisabled(bool) | FUN_0046aac0 writes +0x66 |
| 40 (+0xA0) | Resize(x,y,w,h) | ctors call it with layout values |
| 52 (+0xD0) | MouseMove(x,y) | ListWidget FUN_0059d740 |
| 53 (+0xD4) | MouseDown(x,y,btn,clicks) | Checkbox FUN_005a1740, ListWidget FUN_0059d860 |
| 54 (+0xD8) | MouseDown(x,y,clicks) | dispatcher thunk FUN_0046abf0 calls +0xD4 |
| 57 (+0xE4) | MouseUp(x,y) | Slider FUN_005a2d50 |
| 58 (+0xE8) | MouseDrag(x,y) | Slider FUN_005a2c60, Scrollbar FUN_005a0f60 |
| 59 (+0xEC) | MouseWheel(delta) | ListWidget FUN_0059d940 |
| 81 (+0x144) | first class-specific virtual | Checkbox SetChecked, Slider SetValue, ListWidget GetSortKey |

## Checkbox (vtable 0x00876BDC)

| Field | Offset | Evidence (static) |
|---|---|---|
| mListener (CheckboxListener*) | +0xBC | ctor FUN_005a14e0 stores arg 4; SetChecked calls `[[+0xBC]]` |
| mId | +0xC0 | ctor arg 3; first argument to the listener |
| mChecked (bool) | +0xC4 | IsChecked FUN_005a15f0 returns it; Draw tests it |
| mUncheckedImage / mCheckedImage | +0xC8 / +0xCC | ctor args 1, 2 |
| mCheckedRect / mUncheckedRect | +0xD0 / +0xE0 | zeroed in ctor, Draw tests +0xD8 |

- `SetChecked(bool checked, bool tellListener)` slot 81 FUN_005a15b0: sets
  +0xC4, calls `listener->CheckboxChecked(mId, checked)` if asked, MarkDirty.
- `MouseDown(x,y,btn,clicks)` FUN_005a1740: toggles +0xC4 and always calls
  `CheckboxChecked(mId, newValue)`. It toggles on button **down** and ignores
  which button. A single click therefore toggles exactly once.
- CheckboxListener (0x0086E0B4) has one slot: `CheckboxChecked(int id, bool)`.

## Slider (vtable 0x00876D2C)

| Field | Offset | Evidence (static) |
|---|---|---|
| mListener (SliderListener*) | +0xBC | ctor FUN_005a26a0 arg 3; MouseUp calls it |
| mVal (double) | +0xC0 | SetValue FUN_005a2730 stores and clamps to [0, 1.0]; constant 0x007CD048 = 1.0 (read from file) |
| mId | +0xC8 | ctor arg 2; listener first argument |
| mTrackImage | +0xCC | ctor arg 1; Draw tiles it |
| mThumbImage | +0xD0 | ctor stores global 0x008D8D98; Draw/MouseDown use its cel size |
| mDragging (bool) | +0xD4 | MouseDown sets 1 on thumb hit; MouseUp clears |
| mRelX / mRelY | +0xD8 / +0xDC | MouseDown stores press offset inside the thumb |
| mHorizontal (bool) | +0xE0 | ctor 1; ZenOptionsDialog sets 0 for its vertical slider |

This build's ctor takes (trackImage, id, listener); the framework's takes
(track, thumb, id, listener). Image cel size: `Image+0x2C width / +0x38
numCols` (FUN_0045b1e0), `+0x30 height / +0x34 numRows` (FUN_0045b1d0).

Behaviour (disassembly `private/ghidra-controls/slider.asm`):

- Thumb position (Draw, MouseMove, MouseDown):
  `thumbX = _ftol(mVal * (mWidth - thumbCelW))` (vertical: height/celH).
- `MouseDown(x,y,clicks)` FUN_005a2aa0: if `thumbX <= x < thumbX+thumbCelW`
  then mDragging = 1, mRelX = x - thumbX (no value change). Otherwise
  `SetValue(x / mWidth)` (FILD x; FIDIV mWidth). The y coordinate is not
  checked for a horizontal slider. **SetValue does not notify the listener.**
- `MouseDrag(x,y)` FUN_005a2c60: while dragging,
  `mVal = (x - mRelX) / (mWidth - thumbCelW)`, clamped to [0,1]; if changed
  calls `listener->SliderVal(mId, mVal)` and MarkDirty.
- `MouseUp(x,y)` FUN_005a2d50: mDragging = 0, cursor reset, then listener
  slot 0 `SliderVal(mId, mVal)` and listener slot 1 (this build has a second
  SliderListener virtual; the framework has only SliderVal) with the same
  arguments. OptionsDialog uses slot 1 to play a preview sound.

## OptionsDialog (vtable 0x0086EA6C, ctor FUN_00681370)

Secondary vtables: +0xBC button listener, +0x228 SliderListener
(0x0086EBEC: SliderVal FUN_00673a40, release FUN_006740a0), +0x22C
CheckboxListener (0x0086EBF8: CheckboxChecked FUN_00684ae0).

Layout (positions in units of 1/1200 of the app's resolution height
`gApp+0x9E8` = 600, 768 or 1200, so 1024x768 logical uses factor 768/1200):
sliders at x = `OPTIONS_DLG_SLIDER_X` (default 200), first y 130, step 60,
width `OPTIONS_DLG_SLIDER_WIDTH` (default 700), height = track image height.

| Member | Control id | Label string id | Evidence |
|---|---|---|---|
| +0x250 Slider | 0 | 312 | SliderVal id 0 -> gApp vtable +0xDC (music volume, hypothesis by order) |
| +0x254 Slider | 1 | 313 | id 1 -> gApp +0xE0; release plays a sound (FUN_0076aaf0) |
| +0x258 Slider | 2 | 314 | id 2 stores gApp+0x990 and sets sound-manager channel 1; release FUN_0076ab60 (voice sample, hypothesis) |
| +0x230 Checkbox | 8 | 316 (group 315) | ctor: checked iff res == 600 |
| +0x234 Checkbox | 9 | 317 (group 315) | checked iff res == 768 |
| +0x238 Checkbox | 10 | 318 (group 315) | checked iff res == 1200 |
| +0x23C Checkbox | 4 | 319 | ctor: = FUN_0046f830 (3D enabled); listener 3D-acceleration messages |
| +0x240 Checkbox | 5 | 320 | listener id 5 shows the windowed-mode box (header 322); apply sets windowed = !checked |
| +0x244 Checkbox | 6 | 321 | apply passes IsChecked to FUN_0046e9d0 (custom cursors, hypothesis by label) |
| +0x248 Checkbox | 7 | 562 | listener id 7 shows a box saying 3D acceleration is required (563-565, body 564); apply stores to gApp+0x9E0 |
| +0x24C button | how-to-play button (label 308) | | Draw tests +0x24C |

Label strings come from `OptionsDialog::DrawAll` (string comments in
`private/pak/properties/default.xml`, ids 312-321 and 562). Slider/checkbox
id-to-label pairing for the checkboxes is strong (listener messages and
resolution values); for the three sliders it rests on draw order
(312, 313, 314: music, sound, voice volume) plus the sound behaviour on
release.

What the game does on change (CheckboxChecked FUN_00684ae0, this = +0x22C):

- +0x25C is a re-entrancy guard; when set, only the resolution radio logic runs.
- 8/9/10 (resolution) act as a radio group. Choosing a different one opens
  a confirmation dialog (headers 337 / 342, with a restart warning) or an
  error, and re-sets the three boxes to the current resolution; the actual
  change happens from that dialog.
- 4 (3D) and 5 (fullscreen) may open warning dialogs and revert the box.
- After a normal change it calls FUN_006847c0, which **applies settings
  immediately**: fullscreen/windowed switch (gApp vtable +0x100, releases
  mouse capture, clears WidgetManager +0xA4/+0xA8), custom cursors,
  animated background, resolution change (restart path).
- SliderVal applies the volume immediately; the release slot plays a sample.
  No config save was seen in either listener (probably on dialog close; not
  checked).

## ZenOptionsDialog (vtable 0x0086F55C, ctor FUN_0068bb50)

Not a `Dialog` (Widget subclass). Secondary vtables: +0xBC Bej3ButtonListener,
+0xC0 SliderListener (SliderVal FUN_0068b6c0), +0xC4 CheckboxListener
(FUN_0068b870), +0xC8 ListListener (ListClicked FUN_0068b420). Four
`ZenCategoryContainer` children at +0xD8..+0xE4 (ids 0..3; ctor
FUN_004298c0), each with enable checkbox at container+0x13C, optional second
checkbox +0x140, DropListWidget +0x144, Slider +0x148, HyperlinkWidget +0x168.
Dialog +0xD4 is a vertical Slider id 4; +0xEC the back button.

Profile = `[gApp+0xB80]`. Ids from the listeners (static); label ids are a
**hypothesis** from the order of `ZenOptionsDialog.cpp` source lines in
default.xml (515-526, headings 527-530), not from code linking each label.

| Control | Id | Stored to | Label (group) |
|---|---|---|---|
| Checkbox | 0 | profile+0x238 | 515 (527, section 0) |
| Checkbox | 6 | profile+0x239 | 516 (527) |
| Slider | 0 | profile+0x23C (float) | 517 (527) |
| Slider | 7 | gApp+0x9B0, sound channel 4 | 518 (527) |
| Checkbox | 1 | profile+0x240 | 515 (528, section 1) |
| DropList | 1 | ambient track list gApp+0xCD8 | 519 (528) |
| Slider | 1 | gApp+0x998, channel 2 | 520 (528) |
| Checkbox | 2 | profile+0x280 | 515 (529, section 2) |
| Checkbox | 5 | profile+0x2A0 (subliminal) | 521 (529) |
| DropList | 2 | mantra list gApp+0xCA8 | 522 (529) |
| Slider | 2 | profile+0x2A8 if subliminal else +0x2A4 | 523 or 524 (529) |
| Checkbox | 3 | profile+0x260; first enable shows the binaural safety dialog (FUN_006771c0) and unchecks | 515 (530, section 3) |
| DropList | 3 | binaural pattern | 525 (530) |
| Slider | 3 | gApp+0x9A8, channel 3 | 526 (530) |
| Slider (vertical) | 4 | music volume = 1 - value (gApp vtable +0xDC) | none found |

Checkbox id 4 has a handler (music on/off) but no checkbox with id 4 was seen
in the ctor. Every Zen handler ends with FUN_0068b350 (likely save/refresh;
not decoded).

## ListWidget (vtable 0x008766F4, ctor FUN_0059bc40)

| Field | Offset | Evidence (static) |
|---|---|---|
| ScrollListener vptr | +0xBC | ctor |
| mId | +0xC0 | ctor writes 0; first ListClicked argument (FUN_0059d860) |
| mFont | +0xC4 | MouseMove calls font vtable +0x14 (GetHeight) when item height is -1 |
| mScrollbar | +0xC8 | MouseWheel FUN_0059d940, AddLine sets its max |
| mJustify | +0xCC | UserSelectionDialog sets 1 |
| wheel step (int) | +0xD0 | ctor 5, DropList sets 1; MouseWheel scrolls by it (not in framework) |
| mLines begin / end | +0xE0 / +0xE4 | std::vector<std::wstring> at +0xD4, element 0x1C (GetLineCount FUN_0059cb90) |
| mLineColors begin / end | +0xF8 / +0xFC | element 0x10 (SetLineColor FUN_0059ccf0) |
| mPosition (double) | +0x108 | ScrollPosition FUN_0059d6c0 stores it |
| mPageSize (double) | +0x110 | ctor |
| mHiliteIdx | +0x118 | SetHilite FUN_0059d710 |
| mSelectIdx | +0x11C | SetSelect FUN_0059d900 |
| mListListener | +0x120 | ctor arg |
| mParent / mChild | +0x124 / +0x128 | linked-list walks in SetSelect etc. |
| mItemHeight | +0x134 | ctor = font height or -1; UserSelectionDialog sets 50/1200 of res |

- Row under the cursor (MouseMove disassembly):
  `row = _ftol((y - 4) / itemHeight + mPosition)`, -1 if outside 0..count-1.
- `MouseDown` FUN_0059d860 calls MouseMove(x,y) then, if a row is hilited,
  `listener->ListClicked(mId, row, clickCount)`. It does **not** select the
  row itself; listeners call SetSelect.
- ListListener (0x0086EF3C): slot 0 ListClicked(id, idx, clicks), slot 1
  ListClosed(id), slot 2 ListHiliteChanged(id, old, new) (SetHilite calls +8).
- Used by `UserSelectionDialog` (list at +0x260; ListClicked FUN_00683710:
  last row = the add-user entry (string 508) opens the new-user dialog; other rows
  SetSelect + copy the name; a double click also presses the dialog's
  button 1000).

## DropListWidget (vtable 0x0086F3DC, ctor FUN_0067a1f0)

| Field | Offset | Evidence (static) |
|---|---|---|
| ListListener vptr | +0xBC | ctor; receives the child list's clicks |
| mId | +0xC0 | ctor arg; forwarded as the ListClicked id |
| max visible rows | +0xC8 | ctor 6; Resize FUN_006798d0 |
| selected index | +0xCC | ctor -1; set by ListClicked FUN_006732b0 |
| child ListWidget* | +0x130 | ctor creates and AddWidgets it |
| owner ListListener* | +0x134 | ctor arg |

- Open state = child list `mVisible` (+0x64). `MouseDown(x,y,clicks)`
  FUN_00673300 calls `SetOpen(!list->mVisible)` (slot 85, +0x154).
- Clicking a row of the open child list: its ListClicked (FUN_006732b0)
  closes the list, `list->SetSelect(idx)`, stores idx at +0xCC and calls the
  owner's `ListClicked(mId, idx, idx)`.
- The child list is placed below the header (Resize: y = mHeight + [+0xDC]).
  Whether clicks outside the header's own rectangle reach it depends on the
  WidgetManager's clipping (runtime check).

## ScrollbarWidget (vtable 0x008769E4, ctor FUN_0059ffe0)

| Field | Offset | Evidence |
|---|---|---|
| ButtonListener vptr | +0xBC | ctor |
| mUpButton / mDownButton | +0xC0 / +0xC4 | ctor creates two ScrollbuttonWidgets |
| mInvisIfNoScroll | +0xC8 | SetInvisIfNoScroll FUN_005a0230 |
| mId | +0xCC | ctor (1); ScrollPosition first argument |
| mValue / mMaxValue / mPageSize (double) | +0xD0 / +0xD8 / +0xE0 | SetValue FUN_005a0500, SetMaxValue FUN_005a04a0, SetPageSize FUN_005a04d0 |
| mHorizontal | +0xE8 | SetHorizontal FUN_005a0280 |
| mPressedOnThumb .. mLastMouseY | +0xF0 .. +0x110 | MouseDown FUN_005a0e60 |
| mScrollListener | +0x114 | ctor arg; SetValue notifies slot 0 |

- `SetValue` clamps and calls `ScrollListener::ScrollPosition(mId, value)`;
  ListWidget's handler stores mPosition.
- Up/down buttons: `ButtonPress(id)` FUN_005a0c40 does value -/+ 1.0
  (id 0 = up). Click on the track pages by mPageSize. Holding repeats.
- ListWidget MouseWheel: value -/+ wheel step.
- No Bej3 screen creating a scrollbar was traced in this pass.

## RecordsScreen and tabs

RecordsScreen (vtable 0x0086CD84, ctor FUN_006a05c0):

- +0xC4 HighScoresTab, +0xC8 BadgesTab, +0xCC StatsTab (all `TabPage`).
- +0xD0..+0xD8 tab buttons (Bej3Button ctor FUN_00404160 via FUN_0069cd80),
  ids 0, 1, 2, labels 472, 473, 474.
- ButtonPress (secondary vtable 0x0086CECC slot 1, FUN_0069c870): plays a
  sound, then FUN_0069c760(id): page i visible + its button **disabled** for
  the chosen tab; others hidden and enabled. So the selected tab is the
  disabled button. The navigator currently skips disabled buttons and would
  lose it; `controls::ReadTabButton` reports it as selected.

HighScoresTab (vtable 0x0086C94C, ctor FUN_0069e320):

- +0xC0..+0xD4 six mode buttons (Bej3Button ids 0..5, FUN_0069d440); labels
  are 434-439 (HighScoresTab ctor strings); the draw shows 440-445 as the
  heading of the current mode.
- +0xD8..+0xEC six score sources (FUN_0078aa00; class not identified), +0xF0
  ten ScoreEntryWidgets (FUN_0069cc70), +0x118 current mode.
- ButtonDepress(id) FUN_0069c8c0: refreshes source[id] (vtable +0x24), sound,
  FUN_0069c800: +0x118 = id and only that button disabled.
- Update FUN_0069cb10 assigns source entries to the ScoreEntryWidgets.

ScoreEntryWidget (vtable 0x0086CEEC, ctor FUN_0069bfb0, Draw FUN_0069c230):
`+0xC4` entry pointer (0 = empty row). Entry: `+0x04` name (std::wstring,
font-filtered by FUN_00712fa0 for drawing), `+0x20` rank (drawn with string
448), `+0x24` score (drawn comma-separated by FUN_005a8f10). The entry's
vtable +4 decides whether an avatar is drawn. Entry class unknown (may be
a Steam leaderboard entry; unverified).

StatsTab (vtable 0x0086CC1C, ctor FUN_006a04d0): one child at +0xC0
(FUN_00682f90, probably RankBarWidget; not confirmed). StatsTab::Draw
FUN_0069e3e0 computes every value from the profile `[gApp+0xB80]`
(+0xC0, +0xD4, +0x158 seen) and formats strings 449-474 at draw time; no
widget holds the values. Decoding them was out of scope; a text-draw hook
or a separate profile pass is needed. BadgesTab Draw FUN_0069c7c0 is tiny;
badges are BadgeWidget children (not decoded). RankBarWidget Draw
FUN_00686ba0 uses strings 427/428 (not decoded).

## Changing values: chosen approach

Preferred: **the game's own mouse path**, the same hover-then-click the
navigator uses for buttons. Reasons from the code:

- Checkbox: MouseDown is the only user path and does exactly
  toggle + CheckboxChecked. Direct `SetChecked(!v, true)` would be
  equivalent (same listener call), so it is a fallback if mouse delivery fails.
- Slider: the listener runs on MouseUp (SliderVal + release slot that plays
  the preview sound). A click on the track sets `x / width`; reaching 1.0 or
  a value under the current thumb needs a drag. Writing +0xC0 alone would
  change neither the volume nor play the preview; a direct equivalent would
  be `SetValue(v)` then listener slots 0 and 1, which is what MouseUp does.
- Lists/drop lists: the listener decides selection; clicking the row is the
  faithful path. ListWidget clicks need the row visible (scroll first via
  the scroll buttons or WM_MOUSEWHEEL).
- Options checkboxes apply immediately and can open game dialogs or switch
  fullscreen/resolution (with restart). The navigator should speak the new
  state only after re-reading it, and expect a dialog to appear.

`PlanSliderSet` returns: track click (Move with hover wait, Down, Up) when
possible; otherwise drag (Move to thumb centre with hover wait, Down, Move
to destination, Up). It overshoots by 2 px for 0 and 1 (the game clamps).
`predicted` is the value under the plan's logical coordinates; real
coordinates round-trip through the remap, so re-read mVal afterwards.

## Integration notes for the lead

1. Snapshot: add `controls::Classify(w.ptr)` for visible widgets; include
   Checkbox, Slider, DropList, List rows (when the list is visible) and
   TabButtons (even when disabled) as navigator items.
2. Speech (no game text is hard-coded in source): label = resolved string
   id from `LabelStringId(dialogClass, kind, id)` (and `GroupStringId` for
   context), state from the reader: "checked/not checked", `SliderPercent`
   + " percent", drop list selected line via `ReadListLine(list, selected)`,
   tab "selected". The string ids need the game's string table at runtime
   (FUN_00475e30 looks ids up in a map; not decoded) or a text-draw hook.
3. Actions: Enter/Space on checkbox -> `PlanCheckboxToggle`; Left/Right on
   slider -> `PlanSliderSet(value -/+ 0.05 or 0.1)`; Home/End -> 0/1.
   Execute steps in order: Move = post WM_MOUSEMOVE (wParam MK_LBUTTON if a
   Down is in progress), wait for `waitHover` widget's mIsOver; Down =
   WM_LBUTTONDOWN; Up = WM_LBUTTONUP. Map with
   `controls::LogicalToClientCentre` (pixel centre; the existing
   LogicalToClient truncates and can land one logical pixel short).
4. After an action, re-read the state next tick and speak it; if it did not
   change, say so instead of assuming.
5. Move the offsets in `controls::off` to `src/mod/sexy.h` and the evidence
   rows into `docs/reverse-engineering.md` once runtime-confirmed. Add this
   file to `docs/index.md`.

## Runtime checklist (not done; requires the game)

1. Options dialog open: dump each Checkbox +0xC0/+0xC4 and Slider
   +0xC0/+0xC8/+0xD0/+0xE0; ids should be 8,9,10,4,5,6,7 and 0,1,2 with
   checked states matching the screen; slider values 0..1.
2. Thumb image cel width from `[+0xD0]+0x2C/+0x38` is plausible (tens of px).
3. Click a checkbox centre: exactly one toggle; listener effect visible.
4. Slider track click at a known x: mVal == x/width after release; preview
   sound plays on the sound and voice sliders (ids 1, 2); drag works, i.e. a posted WM_MOUSEMOVE with
   the button down reaches Slider::MouseDrag (the app's WM_MOUSEMOVE ->
   MouseDrag routing and any GetAsyncKeyState check were not traced).
5. Coordinate round trip: compare WidgetManager +0x10C/+0x110 (last mouse
   position, used by SetDisabled FUN_0046aac0) with the intended logical x/y.
6. Records: tab buttons ids 0..2, current one disabled, page visibility
   matches; HighScoresTab +0x118 and ScoreEntry rank/name/score read sanely
   (names match the screen).
7. Zen options: control ids per table; label pairing by on-screen position.
8. UserSelectionDialog list: +0x134 item height, line strings, row click
   selects the user.
