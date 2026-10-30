# Screen names and the game's own keyboard handling

Date: 2026-10-30. Target: Steam `Bejeweled3.exe` (image base 0x400000).
Method: **static only** (Ghidra 12.0.4 headless on the copy in
`private/ghidra-titles`, plus offline reads of the exe's vtables and
literals). Nothing here has been checked at runtime. Game text is not
quoted; strings are named by their `id` in `private/pak/properties/default.xml`
or by the config file entry they come from.

Code: `src/mod/screen_names.h` / `.cpp` (`ScreenName`, `ScreenSection`,
`GameHandlesKey`, `GameAppHandlesKey`). Not yet wired into the navigator.

Scratch outputs (git-ignored, `private/ghidra-titles/`): `keys.c` (all
KeyDown/KeyChar overrides), `wmk.c` (WidgetManager key dispatch), `proc.c`
(message processing), `dbg.c` / `drvdbg.c` (debug keys), `keyovr.txt`
(every vtable overriding slots 47-49), `qdp.c` (quest parser), `tctx.txt` /
`t27c.txt` (board "Title" uses), `rec.c` (records tabs), `py/vt.py`
(offline vtable diff). Wrappers `dec.sh`, `dis.sh`, `grep.sh`, `vt.sh` run
the existing `tools/ghidra/*.java` scripts on this copy.

## A. Screen names

### What the game associates with each screen

| Screen class | Game text | Source | Evidence | Level |
|---|---|---|---|---|
| Every `Board` subclass | board parameter `TitleText`, else `Title` | board memory: `std::map<std::string, std::wstring>` at Board+0xC8 | see "Board titles" | static |
| `Dialog` and all `Bej3Dialog` subclasses | `mDialogHeader` (+0xD8) | dialog memory | `docs/re/menus.md`; runtime-confirmed for Welcome/warning dialogs earlier | static + runtime (some) |
| `QuestHelpDialog` | header = the board's `TitleText` | ctor 0x006CCFD0 reads `[gApp+0xBE8]+0xC8 ["TitleText"]` into +0xD8 | covered by the Dialog rule | static |
| `PlayMenu` | string 287 | label of MainMenu orb 2, whose ButtonDepress 0x006A2DE0 opens the play menu | `menus.md` | static |
| `RecordsScreen` | string 286; selected tab = label of the selected tab button | MainMenu orb 1 opens it; tabs see below | `menus.md`, `rec.c` | static |
| `QuestMenu` | string 347 | PlayMenu orb 3 start function FUN_00773B70 creates the QuestMenu (ctor 0x00693760) | `modes.c` | static |
| `SecretMenu` | string 477 | its own heading, drawn by SecretMenu::Draw 0x00677580 | `menus.md` | static |
| `ZenOptionsDialog` | string 513 (**hypothesis**) | header of the hint tooltip ZenBoard::Update 0x007116C0 shows (513 header, 514 body, at a fixed board position, FUN_00677D00) | `zt.c`, `tip.txt` | hypothesis: that the tooltip points at the options button is not proven |
| `MainMenu` | none | Draw uses only 288-290 | `menus.md` | static |
| `Menu` (developer) | none | only button labels | `menus.md` | static |
| `EndLevelDialog` family, `HintDialog`, `BadgeAwardDialog`, `ZenIntroDialog` | none in the header | header-less Bej3Dialogs; their text is drawn | `menus.md` | static |
| `HighScoresTab` mode sub-tabs | drawn heading 440-445 for the shown mode | HighScoresTab::Draw 0x0069D900 | `fn-map.txt` | static (already reachable through drawn text) |

"Opening button label" rows use a small class -> string id table
(`kScreenStrings` in `screen_names.cpp`). This is a table, so under the
project rule it must be listed in `docs/status.md` if the lead adopts it;
each entry is backed by the game's own code using that string for the
button or heading that leads to / heads that screen.

Not found: no per-screen window title (SetWindowTextA/W are called only by
framework window code FUN_00552600, FUN_005AB400, FUN_005BA000), no Steam
rich presence strings in the exe.

### Board titles (mode and quest names)

- Board+0xC8 is a `std::map<std::string, std::wstring>` of mode parameters
  (head node at +0xE0 = map+0x18). QuestBoard init FUN_006DDC70 looks up
  `ColorCount`, `RecordHighScores`, ... in it with find FUN_00409830 and
  operator[] FUN_00403180; the same instantiation as the app's named string
  table, so the node layout from `menus.md` applies (key std::string +0xC,
  value std::wstring +0x28, `_Isnil` +0x45).
- Constructors set `params["Title"]` to a compiled wide literal:
  ClassicBoard ctor 0x0073B390 (`0073b3dc`: key, `0073b43f`: value, 7
  chars), ZenBoard ctor 0x007126F0 (`00712b27`/`00712b5e`, 3 chars),
  SpeedBoard ctor 0x00718090 (`00718304`/`0071833e`, 9 chars). Their text is
  the mode name; read at runtime from the map, not from the literal.
- Quest and secret-mode boards are built from config entries
  (`quest.cfg`, `secret.cfg`; FUN_00774060 indexes 0x3C-byte entries, sets
  `gApp+0xBC4` = index, board+0x3234 = index or index+1000, board+0x3184 =
  secret-mode flag). The parser's command handler FUN_00791C80 stores the
  `Quest "<name>"` argument under `Title` and, if the entry has no
  `TitleText`, copies it to `TitleText` (`00791e40`, `0079208a`,
  `00792118`).
- How the game uses them: Board vtable+0x27C (0x00741660; quest boards
  0x006C7D10 prefixes a compiled word when +0x3184 is set) returns `Title`
  for the event log lines `GameStart Title=...` (0x0074E7C2),
  `GameOver Title=...` (0x0074E0E3), `GameLoaded Title=...` (0x00765BE8);
  quest boards draw `TitleText` (0x006D3FB0, via FUN_00434F50);
  QuestHelpDialog uses `TitleText` as its header (0x006CCFD0). The earlier
  runtime test heard the quest's name as the briefing header for a quest,
  which is consistent with this.
- `ScreenName` therefore speaks `TitleText` when present (what the player
  sees on quest boards), else `Title` (Classic, Zen, Lightning; for secret
  modes both are the `secret.cfg` entry name). The prefixed form from
  vtable+0x27C is not used.

### Records tabs

FUN_0069C760 (tab switch, EBX = RecordsScreen) loops i = 0..2: page
`[this+0xC4+4i]` gets Widget vtable+0x7C (0x0046A780 = SetVisible, writes
+0x64), tab button `[this+0xD0+4i]` gets vtable+0x94 (0x0046AAC0 =
SetDisabled, writes +0x66); the selected index gets 1. Tab buttons are
Bej3DialogButtons from FUN_0069CD80 with labels 472-474. `ScreenSection`
returns the label (`mLabel` +0xC0) of the tab whose page is visible.
Correction to `menus.md`: +0xC4.. are the pages (as written there), and the
tab buttons are at +0xD0...

## B. The game's keyboard handling

### Dispatch

- WindowsAppDriver message processing FUN_005B9670 (driver vtable
  0x00878E4C; app at driver+4) handles queued messages:
  - WM_KEYDOWN / WM_SYSKEYDOWN: if key = Enter and message = WM_SYSKEYDOWN
    (Alt+Enter) and app+0x3AA = 0, app+0x3AB = 0, app+0x6E0 != 0: app
    vtable+0x104 (switch screen mode; argument = !app+0x3A7) and the key
    stops there. If key = 'D' with WidgetManager mKeyDown[Ctrl] and
    [Alt] (+0x131, +0x132): plays a system sound and toggles app+0x6DC
    (debug keys). Then driver DebugKeyDown (vtable+0x84 = 0x005B8DF0);
    if it returns false, WidgetManager::KeyDown 0x00585EB0.
  - WM_KEYUP / WM_SYSKEYUP: WidgetManager::KeyUp 0x00585EF0.
  - WM_CHAR: WidgetManager::KeyChar 0x00585E60.
- DebugKeyDown 0x005B8DF0: **F1 is always consumed** (cycles app+0x840 only
  if app+0x686); other keys go to app vtable+0x144 only while app+0x6DC is
  set, which reaches FUN_005B8E50: F2 (perf timing), F3 (FPS / overdraw
  display; with Ctrl an overdraw report), F8 (3D mode / vsync toggles,
  with Shift/Ctrl/Alt variants), F11 with Shift (FUN_005AF190), Ctrl+Alt+W
  (toggles app+0x844), Ctrl+Alt+B / P (only with app+0x844; B executes a
  debugger break instruction, which would likely crash the game without a
  debugger). None act in normal play.
- WidgetManager::KeyDown 0x00585EB0 sets mKeyDown[key] (+0x120+key) and
  calls the **focus widget** (+0xA0) vtable+0xC0. KeyChar calls focus
  vtable+0xBC (Ctrl+Tab character goes to the widget at +0x64 instead).
  KeyUp clears mKeyDown and calls vtable+0xC4. Only the focus widget hears
  keys; no parent fallback.
- Widget slots (from Board's and EditWidget's overrides): 47 (+0xBC)
  KeyChar, 48 (+0xC0) KeyDown, 49 (+0xC4) KeyUp. Base Widget: KeyChar and
  KeyUp empty (0x005AD6C0), KeyDown 0x0046AB70.

### Every override (all widget-sized vtables, `keyovr.txt`)

| Class | Slot | Function | Keys and effect |
|---|---|---|---|
| Widget (default for menus, buttons, checkboxes, sliders, lists, MainMenu, RecordsScreen, ZenOptionsDialog) | KeyDown | 0x0046AB70 | Tab: `WidgetManager->SetFocus` (vtable+0x20) to mTabNext +0x9C, or mTabPrev +0x98 with Shift. Nothing else. |
| Board and every subclass (Classic, Speed, Zen, Quest, Dig, Inferno, Poker, Butterfly, TimeBomb, ...) | KeyDown | 0x007472B0 | Ignored while +0x1410. **Escape**: ButtonDepress(1) on the board's listener = menu button -> in-game Options (unless +0x2978, set by FUN_00756730, meaning unknown). **Arrows**: if a gem is selected (FUN_0073BF00), swap it in that direction (vtable+0x308); otherwise move the keyboard cursor (+0x2C44 column, +0x2C48 row, clamped 0-7; first press puts it near the centre). **W/A/S/D**: select the gem under the cursor and swap it up/left/down/right. **Space**: deselect the selected gem, or select the gem under the cursor. Movement also needs +0xE28 = 0 and vtable+0x188 true. **Enter is not handled.** |
| Board (Classic, Speed, Zen share it) | KeyChar | 0x0073B670 | Space character toggles user pause (+0x12C4) only while the keyboard cursor was never shown (+0x2C44 = -1) and vtable+0x1AC (can pause) or already paused, and +0x1410 clear. |
| Quest boards | KeyChar | 0x006C5430 etc. | forward to the quest goal (+0x3180) vtable+0xC8, then Board KeyChar. Goal handlers are empty except HiddenObjectGoal 0x006A9910 (calls `tolower` and discards it: a removed cheat). |
| DigBoard | KeyChar | 0x00736D10 | as quest boards, plus a discarded `tolower` when app+0x6DC (removed cheat). |
| OptionsDialog | KeyDown | 0x00673A10 | **Escape**: ButtonDepress(0) = back button (label 309, close). |
| EndLevelDialog and subclasses (Classic, Speed, Poker, Butterfly, Inferno, DigGoal) | KeyDown | 0x006A5F20 | **Escape**: ButtonDepress(0) = close and leave the game (board vtable+0x3A0), the main-menu button (labels 224/227). |
| Bej3Dialog and all subclasses without their own (Help, Hint, NewUser, Unlock, UserSelection, Welcome, BadgeAward, ZenIntro, RankUp, QuestHelp, EditEndScore) | KeyDown | 0x00769F00 | **Escape** only if +0x201: presses the No button (+0xD0), else Yes (+0xCC), via its listener with its id. +0x201 is cleared by the ctor and set only in DoSavedGameCheck 0x0076CDE0 (`0076d3f4`), so in practice only the "resume saved game" dialog. |
| EditWidget | KeyDown | 0x0059F9A0 | keys outside A-Y go to ProcessKey 0x0059EB20 (Left, Right, Home, End, Backspace, Delete; Shift selects; **Enter** calls EditListener slot 0); then Widget KeyDown (Tab). |
| EditWidget | KeyChar | 0x0059F9F0 | typing; Ctrl+C/X/V/Z as control characters. |
| WelcomeDialog, NewUserDialog | EditListener | 0x00673B80 | Enter in the name field = ButtonDepress(1000) (the confirm button, e.g. label 510). |
| PlayMenu, QuestMenu | KeyChar | 0x006738A0, 0x006734C0 | empty. |
| Menu, SecretMenu (developer) | KeyChar | 0x00679F10, 0x006799A0 | 'r': rebuild the menu. |

Framework `Dialog` has no KeyDown of its own: no Enter = OK and no Escape =
Cancel outside the cases above. No ButtonWidget, Checkbox, Slider,
ListWidget or ScrollbarWidget class overrides the key slots, so no native
Enter/Space activation or arrow movement exists in any menu.

### Conflicts with the mod's keys

The mod's window hook sees keys before the game. Current mod keys:
Up/Down/Tab/Home/End/Enter/Space (menus), Left/Right on sliders, F5/F6/F12
everywhere, Tab and Enter-after-Tab on boards.

- **No direct conflict** for F5, F6, F12, Up, Down, Home, End: the game
  never reacts to them in menus or dialogs (Home/End only inside an edit
  field, which the mod already leaves alone).
- **Tab**: the game's own Tab moves its focus only along tab links
  (+0x98/+0x9C); the mod consumes Tab in menus and on boards. Nothing lost
  unless some screen sets tab links (none found; not searched exhaustively).
- **Space / Enter in menus**: consumed by the mod; the game has no
  activation key, so nothing is lost. On edit fields both pass through
  (Enter submits there, as the game intends).
- **Board**: the mod passes Space, arrows, W/A/S/D through (correct; they
  are the game's controls). `docs/status.md` says Enter also goes to the
  game on the board; statically **the board ignores Enter**, so Enter after
  something other than Tab does nothing.
- **Space on timed boards**: Space pauses only until the arrows have shown
  the keyboard cursor; afterwards Space selects gems instead. Worth telling
  the user.
- **Escape** is not used by the mod and reaches the game: on a board it
  opens the in-game Options; in Options it is the back button; on an
  end-of-game dialog it is the main-menu button (leaves the results screen). Speech for these
  transitions comes from the normal context change.
- **Alt+Enter** switches between fullscreen and windowed. Windowed mode is
  the known trouble case for hover (status.md "Known gaps").
- **Ctrl+Alt+D** silently toggles the framework debug keys (plays a system
  sound); afterwards F2/F3/F8/F11 change rendering. The mod could swallow
  Ctrl+Alt+D if the user's screen reader setup makes it easy to hit.
- **F1** is always eaten by the framework; free for the mod if wanted.

### Helper

`GameHandlesKey(focusWidget, vk, &what)` reads the focus widget's KeyDown
pointer (vtable+0xC0) and applies the table above (board flags, Bej3Dialog
+0x201, tab links). `GameAppHandlesKey(vk, alt, ctrl, &what)` covers the
framework keys (reads app+0x6DC, assuming driver+4 is the same object as
`[0x008E1730]`: hypothesis). Suggested use: log `what` for each key the mod
passes through, and decide whether to swallow Escape on end-level dialogs.

## Integration steps (for the lead)

1. In `navigator.cpp`, where a new context is announced: if
   `DialogSpeech` is empty, try `ScreenName(context, &name)` before
   `FriendlyClassName`; for `RecordsScreen` add `ScreenSection`.
   For boards decide on wording (e.g. name alone, or name + "board").
2. Add `screen_names.cpp` to `tools/build.bat`.
3. List the `kScreenStrings` table (287, 286, 347, 477; 513 only after the
   runtime check) in `docs/status.md` as an evidenced table.
4. Optionally log `GameHandlesKey` for passed-through keys, and correct
   status.md's "Enter goes to the game on the board" after a runtime check.

## Runtime checklist

1. Classic, Zen, Lightning: `ScreenName` of the board gives the mode name
   (source `board Title`); log the map size (`[board+0xC8+0x1C]`) once.
2. A quest and a secret mode: source `board TitleText`,
   text equals the quest briefing header and the title drawn on the board.
3. Play menu, Records, Quest menu, Secret menu names (opening labels 287,
   286, 347; heading 477).
4. Records: switch tabs; `ScreenSection` follows the selected tab.
5. Zen board: does the 513/514 hint tooltip appear at the options button?
   Only then enable the ZenOptionsDialog name.
6. Keys (with a throwaway profile): Escape on the board opens Options;
   Escape in Options goes back; Escape on an end-of-game dialog goes to the
   main menu; Enter on the board does nothing; Space pauses Lightning
   before arrows are used and selects a gem after.
7. `GameHandlesKey` agrees with each observation above (log `what`).

## Open questions

- Meaning of Board +0x1410 (all keys blocked), +0x2978 (Escape blocked),
  +0xE28 and vtable+0x188 (movement allowed).
- Whether any screen sets Widget tab links (+0x98/+0x9C).
- The exact effect of the debug keys (not needed for the mod).
- Whether gamepad input reaches the same KeyDown paths (XInput driver at
  gApp+0x18; not traced).
