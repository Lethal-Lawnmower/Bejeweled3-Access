# Reverse-engineering notes

Target: Steam `Bejeweled3.exe`, SHA-256
`d55162445511fcbf711ab7922e57d9bdc27ba2fa72b3bde6ab553e0756e688c3`.
Evidence levels: **static** = Ghidra decompilation; **runtime** = confirmed in
a live dump on 2026-10-30 (2560x1440 fullscreen); **community** = third-party
claim not yet checked by us. The code constants live in `src/mod/sexy.h`.

## Globals

| Item | Value | Evidence |
|---|---|---|
| gApp (`Bej3App*`) | `[0x008E1730]` | static (Dialog ctor), runtime (RTTI class `Bej3App`), community |
| WidgetManager | `[gApp+0x360]` | runtime (found by vtable `0x008751A4`) |
| Logical screen | 1024x768 (WidgetManager width/height) | runtime |

## WidgetContainer / Widget (ctors FUN_00474450, FUN_0046a500)

| Field | Offset | Evidence |
|---|---|---|
| mWidgets std::list head node / size | +0x18 / +0x1C | static, runtime (tree walk) |
| list node {next, prev, Widget*} | +0 / +4 / +8 | runtime |
| mWidgetManager / mParent | +0x20 / +0x24 | static |
| mX, mY, mWidth, mHeight | +0x40..+0x4C | static, runtime (positions plausible) |
| mVisible, mMouseVisible, mDisabled, mHasFocus, mIsDown, mIsOver | +0x64..+0x69 (bytes) | static, runtime (visible/disabled match screens; mIsOver set after mouse move) |

## ButtonWidget (ctor FUN_0049c490)

| Field | Offset | Evidence |
|---|---|---|
| mId | +0xBC | static, runtime |
| mLabel (std::wstring) | +0xC0 | static, runtime (labels 287, 285, 510 read correctly) |
| mButtonListener | +0x13C | static |

`SexyString` is `std::wstring` (MSVC 2008: +4 inline buffer or heap pointer
when capacity > 7, +0x14 size, +0x18 capacity).

## Dialog (ctor FUN_00583870)

mYesButton +0xCC, mNoButton +0xD0, mDialogHeader +0xD8, mDialogFooter +0xF4,
mDialogLines +0x110 (all std::wstring). Static; runtime: welcome header
(string 509), and the empty-name warning's header / lines / button
(83 / 84 / 85).

## EditWidget (ctor FUN_0059dc10)

mString (std::wstring) +0xC0. Static; runtime (typing echo).

## WidgetManager (ctor FUN_00584a10)

| Field | Offset | Evidence |
|---|---|---|
| mApp | +0x6C | static |
| mFocusWidget | +0xA0 | runtime (held the focused EditWidget) |
| mBaseModalWidget | +0xAC | runtime (held the WelcomeDialog) |
| mouse remap rect, logical {x,y,w,h} | +0xE8 | runtime (-102,0,1228,768) |
| mouse remap rect, client | +0xF8 | runtime (0,0,2560,1440) |

+0xA4 and +0xA8 are probably mLastDownWidget and mOverWidget (framework
order) but are unconfirmed.

## Screens seen at runtime

Top level: `Background`, `MainMenu` (CrystalBall buttons with labels 285,
286, 287; two image-only `Bej3Button`s ids 7 and 3), `PlayMenu` (eight mode
buttons, labels 344-351; image-only ids 100/101), `RecordsScreen`
(HighScoresTab, BadgesTab, StatsTab), `OptionsDialog` (3 Sliders,
7 Checkboxes), `UnderDialogWidget`, `WelcomeDialog` (EditWidget + button
510), `TooltipManager`, `WindowedButtonWidget` (id 1001), `ResizeWidget`.

Many visible texts are drawn directly in `Draw()` functions instead of
being stored in widgets, e.g. `WelcomeDialog::Draw` draws string 511 and
`MainMenu::Draw` draws string 288 (from the string table comments in
`properties\default.xml`). Reading those needs a draw hook.

## Session 2 runtime confirmations (2026-10-30)

Static findings from the subagent passes (docs/re/*.md) confirmed live:

- Text capture hooks install and run on the window thread; drawn positions
  line up with widget rectangles (e.g. string 288 inside MainMenu
  ButtonWidget id 4).
- Widget::mMouseVisible +0x65 (hit test behaves as the game routes clicks).
- Checkbox checked +0xC4 and Slider value +0xC0 read correctly and change
  as predicted by the planned mouse sequences.
- Image resource names: MainMenu 7 "main menu help button", 3 "main menu
  quit button", PlayMenu 100 "overworld back button" (100 returns to the
  main menu, observed), board buttons "ingameui hint/menu button classic",
  "ingameui replay button".
- String table at gApp+0x7F4: 579 entries.
- PlayMenu description wstring +0x1C8 (static) and the hover text agree.
- Board `[gApp+0xBE8]`, score +0xD24, level, level target (2500 at level 1)
  read correctly; the announcement queue produced the start announcement
  (string 141).
- RecordsScreen tab buttons: current tab is disabled (selected).

## Board (community, unverified here)

From FatBrother1/bejeweled3-deck-bot: Board `[gApp+0xBE8]`, piece pointer
grid `[Board+0xF8+4*col+32*row]`, colour `[piece+0x220]`, flags
`[piece+0x228]`, counter `[piece+0x244]`, score `[Board+0xD24]`, level
`[Board+0xE0C]`. Flag bits from Bejeweled3PlusExtender (MIT). Board
vtables in this build: `Board` 0x00864CB4, `ClassicBoard` 0x0086511C,
`SpeedBoard` 0x008665C4, `ZenBoard` 0x00866B6C, `QuestBoard` 0x00866FBC.
