# Menus, dialogs and string tables: reverse-engineering evidence

Date: 2026-10-30. Target: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`,
image base 0x400000). Method: **static only** (Ghidra 12.0.4 headless on the
copy in `private/ghidra-menus`, plus offline reads of the exe bytes). Nothing
here has been checked at runtime yet unless stated. Game text is never
quoted; strings are referred to by their `id` in
`private/pak/properties/default.xml`.

Tools added for this pass:

- `tools/ghidra/MenusDisasm.java` + `tools/ghidra/menus-disasm.sh`: whole
  function, `ADDR:N` instructions, `callers:ADDR` or `refs:ADDR` with the 12
  instructions before each site (to recover pushed ids and register args).
- `tools/ghidra/decompile.sh` (existing) with
  `GHIDRA_PROJECT='D:\dev\Bejeweled3-Access\private\ghidra-menus'`.
- Scratch outputs (not committed, in `private/ghidra-menus/`): `ls-callers.txt`
  (all 561 calls of the string lookup), `fn-map.txt` (function address ->
  string ids, joined with the xml comments), `res.txt` (resource loaders).

## String tables

### Numeric table (what the game's UI code uses)

- Every UI string is fetched by `FUN_00475e30(id, &defaultLiteral)`: custom
  register convention, **EAX = map**, **ESI = output std::wstring**, stack =
  `int id`, `const wstring* fallback`. If `id` is missing the fallback
  (the English literal compiled at the call site) is copied instead.
- Every one of the 561 call sites loads the map as `MOV EAX,[0x008E1730];
  ADD EAX,0x7F4` (checked by script over `ls-callers.txt`). So the table is
  `std::map<int, std::wstring>` at **gApp+0x7F4**.
- Tree layout from `FUN_00476450` (find): head node pointer at map+0x18,
  root = head+4, node `_Left +0, _Parent +4, _Right +8`, key `int` at +0xC,
  value wstring at +0x10, `_Isnil` byte at +0x2D. The element count at
  map+0x1C is inferred from the MSVC 2008 `_Tree` layout (head, then size),
  not seen in code.
- Filled from the properties loader: `FUN_0046e050` opens
  `properties/default.xml`; `FUN_005ce730` handles `<String>`; the setter
  `FUN_0046e510` stores into the named map (gApp+0x750, see below) and, when
  the id parses as an integer (`FUN_0043ad70`), also into gApp+0x7F4 via
  `FUN_00475ec0` (`ADD EDX,0x7f4` at 0046e60e). No other writers were looked
  for; the maps are assumed constant after startup.
- 568 of the 579 ids in default.xml are referenced by a lookup call; the
  other 11 (8, 172, 263, 264, 306, 376, 475, 580-583) are probably used with
  a computed id or are dead. Id 376 (PlayMenu locked text) is fetched in
  `FUN_0068e0e0` through a code path the push scan did not catch.

Implementation: `src/mod/game_strings.cpp` walks the tree with guarded reads
(depth bound 64) instead of calling the game function (its register
convention cannot be called from C++ without assembly, and reading needs no
game state). Thread: run it on the game window thread like the rest of the
navigator; reads are SEH-guarded anyway.

### Named table

- `FUN_0046e460` = SexyAppBase::GetString(std::string id, default): map at
  **gApp+0x750** (`std::map<std::string, std::wstring>`, head at +0x768).
  Framework Dialog uses it for `DIALOG_BUTTON_YES/NO/OK/CANCEL` (Dialog ctor
  `FUN_00583870`, seen in `private/ghidra/ctors.c`).
- Node layout from `FUN_00403a50` (lower_bound) and `FUN_00409830`: key
  `std::string` at +0xC (buffer +0x10, size +0x20, capacity +0x24, SSO 15),
  `_Isnil` at +0x45; value wstring at +0x28 (inferred: right after the
  0x1C-byte key).
- Contains every `<String id=...>` from the loaded properties files, numeric
  ones as decimal text too.

### The comments in default.xml are not fully reliable

The `<!-- Class::Function:line -->` comments match the decompiled function
in almost every case (`fn-map.txt`), but not always: ids 239-242 are tagged
`HelpWindow::SetVisible` yet are used in the `HelpDialog` constructor
`FUN_00696100`. Strings tagged only `Line N` come from constructors.

## Image resource names (for image-only buttons)

- `ButtonWidget::mButtonImage` is at +0xE4: MainMenu (`FUN_006a3530`),
  PlayMenu (`FUN_00680760`) and QuestMenu (`FUN_00693760`) store image
  resource globals there.
- The generated resource code keeps `void* gResources[0xA4C]` at
  **0x008CF5C8** (addresses of every resource global, NULL after the last)
  and `GetStringIdById` at **0x006E8CA0** (`CMP EAX,0xA4B; JMP
  [EAX*4+0x006ECA80]`, each case `MOV EAX,name; RET`). `FUN_006efb20` loops
  over both with the same index. Offline check: all 2636 jump targets have
  the `B8 imm32 C3` form.
- Names found this way agree with the loader functions (`FUN_006f2450`,
  `FUN_006fb660`: `MOV ECX,name; CALL load` then store to the global):

| Global | Index | Resource id | Used by |
|---|---|---|---|
| 0x008D8AD8 | 781 | IMAGE_MAIN_MENU_QUIT_BUTTON | MainMenu 3, PlayMenu 101, QuestMenu 3 |
| 0x008D7AF0 | 782 | IMAGE_OVERWORLD_BACK_BUTTON | PlayMenu 100, QuestMenu 2 |
| 0x008D7758 | 1036 | IMAGE_MAIN_MENU_HELP_BUTTON | MainMenu 7 |
| 0x008D6E24 | 1442 | IMAGE_QUESTMENU_PREV | QuestMenu 0 |
| 0x008D784C | 1438 | IMAGE_QUESTMENU_NEXT | QuestMenu 1 |
| 0x008D84A0 | - | IMAGE_DIALOG_BACKGROUND | Bej3Dialog backgrounds |

Implementation: `GameImageResourceName` (game_strings.cpp) scans the 2636
pointers for the one whose global holds the image, then decodes the name
from the jump table. `ButtonImageWords` (ui_names.cpp) turns the id into
lower-case words.

## Listener vtable slots

ButtonListener secondary vtables (at object+0xBC) have 7 slots; by matching
functions named in the string comments (`PlayMenu::ButtonDepress`
0069b360 = slot 2, `PlayMenu::ButtonMouseEnter` 0068e0e0 = slot 4):
slot 1 ButtonPress(id, clickCount), slot 2 **ButtonDepress(id)**, slot 4
**ButtonMouseEnter(id)**, slot 5 ButtonMouseLeave. Per class (slot 2 / 4):
MainMenu 006a2de0 / 006a0860 (sound only), PlayMenu 0069b360 / 0068e0e0,
Menu 0069bd20, SecretMenu 00697620, QuestMenu 00697640 / 00673fa0 (sound
only), RecordsScreen 00409210 (ButtonPress 0069c870), OptionsDialog
00697460, ZenOptionsDialog 00683160, HelpDialog 00674f00 (thunk of
Bej3Dialog 00769f60), UserSelectionDialog 00674850, WelcomeDialog
00674820, UnlockDialog 0068cf10, RankUpDialog 0078fb70, ZenIntroDialog
0078d590, EndLevelDialog 006a91e0, Bej3Dialog 00769f60 (also Hint, NewUser,
BadgeAward, QuestHelp), Dialog 005849d0.

## Dialogs: titles come from the game already

- `Bej3Dialog` ctor `FUN_0076a2c0` forwards header/lines/footer to the
  framework Dialog ctor `FUN_00583870`, so every Bej3Dialog subclass has its
  title in `Dialog::mDialogHeader` (+0xD8), which `navigator.cpp` already
  speaks (runtime-confirmed for WelcomeDialog and the warning dialog).
- Its buttons are `Bej3DialogButton`s created by `FUN_0076a190(id, width,
  listener)` (0x1C0 bytes, ctor `FUN_00404160`) and get their `mLabel` from
  the string table, so they are spoken from `mLabel`.
- Generic message boxes: `gApp->vtable[0x10C](dialogId, modal, header,
  lines, footer, buttonMode)` (e.g. PlayMenu locked-mode boxes, Options
  warnings, Bej3App::AskQuit `FUN_0076b900`). Also Dialog fields.

Dialog ids (pushed to the Bej3Dialog ctor; `callers:0x0076a2c0`) and the
Bej3App::DialogButtonDepress (`FUN_00771a70`) branches:

| Dialog id | Class (ctor) | Header id | Buttons (id: label id) | Notes |
|---|---|---|---|---|
| 0x13 | HelpDialog (00696100) | 239 | 1000: 240, 241 or 242 | label chosen from the current Board state (`[gApp+0xBE8]`); exact conditions not traced |
| 0x15 | OptionsDialog (00681370) | 307 | 2: 308, 0: 309, 1: 311 or 310 (quest) | see Options below |
| 0x12 | HintDialog (00682bf0) | - | 265, 266 | Draw 267 = checkbox text |
| 0x17 | BadgeAwardDialog (0067f4f0) | - | 69 | Draw 70 |
| 0x04 | UserSelectionDialog (0067fb80) | 505 | 10: 506, 11: 507 | list entry 508 (ResetListWidget 006765b0); 11 -> delete confirm (dialog 3, 86/87) |
| 0x27 | UnlockDialog (006803b0) | 501 | 502 | Draw 503, 504 + mode name |
| 0x2C | ZenIntroDialog (0078d6f0) | - | 575 | |
| 0x28 | QuestHelpDialog (006ccfd0) | - | 418 | Draw 419 (timer format) |
| 0x2B | RankUpDialog (0078ec50) | 429 | 430 | Draw 431, 432, 433 + rank |
| 0x26 | EndLevelDialog (006a7c80) | - | ids 0-3: 224-229 | ButtonDepress 006a91e0: 3 -> badges 237/548 |
| 1 (inferred) | WelcomeDialog (00683600; uses id 509, not a direct Bej3Dialog ctor caller) | 509 | 1000: 510 | Draw 511; empty name -> 83/84/85 |
| 2 (hypothesis) | NewUserDialog (006824b0) | 302 / 303 | | Draw 304 / 305; existing name -> 91-93 |

Bej3App::DoSavedGameCheck (`FUN_0076cde0`): dialog with 75/76 and buttons
77/78/79 (one id 0x3EA). Header columns marked "-" were not traced.

## Screens (non-dialog widgets)

### MainMenu (vtable 0086C35C, ctor 006a54b0, extra buttons 006a3530)

| Id | Widget | Action (ButtonDepress 006a2de0) | Text |
|---|---|---|---|
| 0 | CrystalBall (`FUN_0078ab50`, ECX = id) | options screen transition, `FUN_00673ac0` | label 285 |
| 1 | CrystalBall | records transition | label 286 |
| 2 | CrystalBall | play transition (PlayMenu is child, `FUN_00680760`) | label 287 |
| 3 | Bej3Button (`FUN_00791b80`) | Bej3App::AskQuit | image quit; dialog header 73 |
| 4 | ButtonWidget, no draw | `FUN_0076fbd0` opens UserSelectionDialog | Draw 006a0ca0 draws 288 at this button's y when it is hovered |
| 7 | Bej3Button | new HelpDialog (0x300 bytes, `FUN_00696100`) | image help; dialog header 239 |

ButtonMouseEnter only plays a sound. Draw also uses 289/290 while loading.
The exact effects of 0/1 (animation code) are not fully traced.

### PlayMenu (vtable 0086E38C, ctor 00680760, child of MainMenu)

- Mode buttons 0-7 (CrystalBall-like, labels 344-351, created in the ctor
  loop at index `+0x32`): ButtonDepress 0..3 start modes
  (`FUN_007737d0`, `FUN_00773100`, `FUN_00773440`, `FUN_00773b70`); 4..7
  check an unlock flag at `[gApp+0xB80]+0x180..0x183` and either show a
  locked box (ids 353-364) or start the mode (`FUN_00774060(app, 0..3)`).
- **100**: Bej3Button, image IMAGE_OVERWORLD_BACK_BUTTON; ButtonDepress
  calls a virtual on the parent MainMenu (`[MainMenu+0x6E0]->vtbl[0x148]`)
  and plays the return animation. Hypothesis: back to the main menu.
- **101**: Bej3Button, image IMAGE_MAIN_MENU_QUIT_BUTTON; Bej3App::AskQuit.
- ButtonMouseEnter `FUN_0068e0e0`: per id 0..7 fetches the description
  (0:365, 1:366, 2:367, 3:368, 4: 369 or locked 370, 5: 371/372, 6: 373/374,
  7: 375/376), formats it, and stores it in the std::wstring at
  **PlayMenu+0x1C8** (`LEA ECX,[ESI+0x10C]` with ESI = listener at +0xBC,
  0068ea89). `PlayMenu::Draw` (00689d20) draws `this+0x1C8` centred, plus 352
  when the flag at +0x1E4 is set and 566 as a hint.

### QuestMenu (vtable 0086DF4C, ctor 00693760)

| Id | Widget | Action (00697640) | Image |
|---|---|---|---|
| 0 | ButtonWidget | page/selection change via `FUN_00674060` | IMAGE_QUESTMENU_PREV |
| 1 | ButtonWidget | as 0 | IMAGE_QUESTMENU_NEXT |
| 2 | Bej3Button | back: `FUN_007716e0` (same call as the pause Menu's main-menu button 0x3EE) | IMAGE_OVERWORLD_BACK_BUTTON |
| 3 | Bej3Button | Bej3App::AskQuit | IMAGE_MAIN_MENU_QUIT_BUTTON |
| >= 1000 | QuestMenuBtn | start the selected quest | - |

Draw 006922d0 draws 425 and a percentage (426). Update 00697d40 shows
420, 421, 422-424 and 570/571. QuestMenuBtn tooltips were not traced.

### Menu (vtable 0086D8A4, InitWidgets 00678ce0)

Labelled ButtonWidgets 1000-0x3FC with labels 293-300 and others with
literal-only labels (no string id); ButtonDepress 0069bd20 starts modes,
0x3EC quits, 0x3EE goes to the main menu (`FUN_007716e0`), 0x3FC
`FUN_0076c2a0`. Looks like a developer/test menu; how it is opened was not
found. Labels are in `mLabel`, so no table is needed.

### SecretMenu (vtable 0086DA0C, InitWidgets 00678560, Draw 00677580)

Buttons 1000 and others; back label 476. Draw heading 477
(`UiScreenTitle`). How it is reached: hypothesis, the PlayMenu secret flag
(+0x1E4, text 352).

### RecordsScreen (vtable 0086CD84, ctor 006a05c0)

Tabs are Bej3DialogButtons from `FUN_0069cd80(index)` with ids 0/1/2 and
labels 472/473/474; ButtonPress `FUN_0069c870` calls `FUN_0069c760`, which
shows tab page `[this+0xC4+4*i]` (HighScoresTab / BadgesTab / StatsTab) and
hides the others. Tab text: HighScoresTab 434-448, StatsTab 449-474,
BadgeWidget 71/72, RankBarWidget 427/428 (all drawn, not stored).

### OptionsDialog (vtable 0086EA6C, ctor 00681370, DrawAll 0068ed30)

- Buttons (Bej3DialogButton, labelled): 2 how-to-play (only in a game),
  0 back, 1 main menu or quest menu. ButtonDepress 00697460: 0 calls
  `FUN_0076a670` (probably close), 1 calls `[gApp+0xBE8]->vtbl[0x3A0]` (the
  board, i.e. leave the game) then the same, 2 opens HelpDialog.
- Sliders (ctor `FUN_005a26a0(image, id, listener)`): ids 0, 1, 2 at
  increasing y. DrawAll draws 312, 313, 314 at increasing y beside them.
- Checkboxes (ctor `FUN_005a14e0(img, img, id, listener)`): 8, 9, 10, 4, 5,
  6, 7. DrawAll draws group label 315 then 316, 317, 318, 319, 320, 321, 562.
  CheckboxChecked 00684ae0: 8/9/10 are a radio group (sets `+0xC4` of the
  three); 10 is refused when the screen has fewer than 1200 lines
  (`gApp[0x27A] < 0x4B0`) -> 318; 5 -> windowed-mode box 322-324 -> 320;
  7 -> 563-565 -> 562; 4 -> 3D acceleration boxes 325-333 -> 319; 6 -> 321
  by elimination (needs runtime confirmation).
- Widget layouts (static, from the ctors): Checkbox mListener +0xBC,
  **mId +0xC0**, mChecked +0xC4 (byte, written by CheckboxChecked), images
  +0xC8/+0xCC. Slider mListener +0xBC, **mId +0xC8**, image +0xCC.

### ZenOptionsDialog (vtable 0086F55C)

Built in `FUN_00429bd0` / `FUN_0042ab20` (ids 515-540, 576-579);
ButtonDepress 00683160 (0 saves settings, 1 closes). The binaural warning
580-583. Not mapped further in this pass.

## Not covered / open

- ZenOptionsDialog widgets, QuestMenuBtn tooltips, HintDialog checkbox id,
  EndLevel subclasses' extra stats (drawn text only; see `fn-map.txt`).
- Which dialog ids belong to WelcomeDialog (1) and NewUserDialog (2) is
  inferred from Bej3App::DialogButtonDepress strings, not from the ctors.
- How `Menu` and `SecretMenu` are opened.
- Everything is static; first runtime checks are listed in `docs/ui-map.md`.

> Correction (2026-10-30): ZenOptionsDialog button id 1 is the binaural section's safety link (label 579), which opens a safety dialog; it does not close the panel (docs/re/rare-dialogs.md).
