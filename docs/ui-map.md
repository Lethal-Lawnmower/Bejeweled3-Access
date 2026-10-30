# Bejeweled 3 screen map

What each screen and dialog is, how you get there, and what its controls do,
for the accessibility mod. Text is named by string id from the game's
`properties/default.xml`; the mod reads the actual words from the running
game (`src/mod/game_strings.h`), so nothing here is spoken from this file.
Evidence and addresses: `docs/re/menus.md`. Status: **static analysis only
(2026-10-30)**; nothing below is runtime-verified except where noted.

## How the mod should find names (game first)

1. A button's own label (`ButtonWidget::mLabel`) covers most buttons: the
   main menu balls, play menu modes, every dialog button, records tabs.
2. Dialog titles and messages are in the Dialog header/lines/footer fields
   (already spoken; runtime-confirmed for the welcome and warning dialogs).
3. Play menu mode descriptions: the game writes the hovered mode's
   description into the play menu object (`PlayMenu+0x1C8`) when the mouse
   enters a mode button, and draws it from there. Hover the button (the
   mod's existing mouse-move), then read it with `PlayMenuHoverDescription`.
   This includes the "how to unlock" text for locked modes, already
   formatted by the game.
4. Image-only buttons: the game has no text for them. Two game-derived
   sources exist:
   - the button image's resource id (`ButtonImageWords`), e.g. the quit
     button's image is the main menu quit image, the back button's image is
     the overworld back image;
   - for a few buttons, text the game draws next to the button or the title
     of the dialog the button opens (`UiWidgetText`, a small evidenced
     table; see "Table-based entries" below).
5. Options checkboxes and sliders have no label; the game draws their labels
   beside them. `UiWidgetText` maps them by widget id (table-based).

## Screens

### Main menu (`MainMenu`)

Shown after the profile is chosen. No drawn heading.

| Control | Id | Name source | What it does |
|---|---|---|---|
| Options ball | 0 | label (285) | opens options |
| Records ball | 1 | label (286) | opens the records screen |
| Play ball | 2 | label (287) | opens the play menu |
| Quit (image) | 3 | image; quit dialog title 73 | asks whether to quit (dialog 73/74) |
| Change user (invisible) | 4 | text 288, drawn beside it on hover | opens the user selection dialog |
| Help (image) | 7 | image; help dialog title 239 | opens the help dialog |

### Play menu (`PlayMenu`, inside the main menu)

| Control | Id | Name | Description (on hover) | Action |
|---|---|---|---|---|
| Classic | 0 | label 344 | 365 | starts Classic |
| Zen | 1 | label 345 | 366 | starts Zen |
| Lightning | 2 | label 346 | 367 | starts Lightning |
| Quest | 3 | label 347 | 368 | starts Quest mode (`FUN_00773b70`; leads to the quest menu, not traced) |
| Poker | 4 | label 348 | 369, or 370 while locked | starts, or shows locked box 353-355 |
| Butterflies | 5 | label 349 | 371 / 372 | same, box 356-358 |
| Ice Storm | 6 | label 350 | 373 / 374 | same, box 359-361 |
| Diamond Mine | 7 | label 351 | 375 / 376 | same, box 362-364 |
| Back (image) | 100 | image only | - | returns to the main menu (hypothesis) |
| Quit (image) | 101 | image; quit title 73 | - | asks whether to quit |

Also drawn: 566 (a hint for new players) and 352 when a secret game is
available. Mode-button ids and labels are from the constructor; which
string id goes with which button follows creation order (strings 344-351).

### Quest menu (`QuestMenu`)

| Control | Id | Name | Action |
|---|---|---|---|
| Previous (image) | 0 | image only | changes the shown quest set |
| Next (image) | 1 | image only | same, other direction |
| Back (image) | 2 | image only | returns to the main menu |
| Quit (image) | 3 | image; quit title 73 | asks whether to quit |
| Quest buttons | 1000+ | not traced | start a quest |

Drawn text: 425 (goal), 426 (percentage), and messages 420-424, 570/571.

### Records (`RecordsScreen`)

Three tab buttons, ids 0/1/2, labels 472 (high scores), 473 (badges),
474 (stats). Choosing one shows that tab page. Tab contents are drawn text
(high scores 434-448, stats 449-474 range, badges via badge strings 0-68).

### Secret games (`SecretMenu`)

Heading 477 (drawn). Back button label 476. How it is reached is not
confirmed (probably the play menu's secret-game state, text 352).

### Developer menu (`Menu`)

A list of labelled buttons (labels 293-300 plus unlocalised ones) that start
modes, quit or return to the main menu. How it opens is unknown; likely not
reachable in normal play.

## Dialogs

All of these are `Bej3Dialog`s: title in the header field, buttons labelled.

| Dialog | Reached from | Title | Buttons | Other drawn text |
|---|---|---|---|---|
| Welcome (`WelcomeDialog`) | first start, no profile | 509 | 510 | 511; empty name -> box 83-85 |
| New / rename user (`NewUserDialog`) | user selection | 303 / 302 | | 305 / 304; duplicate name -> 91-93 |
| User selection (`UserSelectionDialog`) | main menu change-user area | 505 | rename 506 (id 10), delete 507 (id 11) | list item 508 for adding a user; delete asks 86/87 |
| Options (`OptionsDialog`) | main menu options ball (in-game route not traced) | 307 | how to play 308 (id 2, in a game only), back 309 (id 0), main menu 311 or quest menu 310 (id 1) | see below |
| How to play (`HelpDialog`) | main menu help, options | 239 | close/start 240-242 (id 1000) | help pages 243-261 |
| Hint (`HintDialog`) | during play (trigger not traced) | - | 265, 266 | 267 (checkbox text) |
| Quest help (`QuestHelpDialog`) | quest start | - | 418 | 419 |
| Unlock (`UnlockDialog`) | after unlocking a mode | 501 | 502 | 503, 504 |
| Rank up (`RankUpDialog`) | after a rank | 429 | 430 | 431-433 |
| Badge award (`BadgeAwardDialog`) | after a badge | - | 69 | 70 |
| Zen intro (`ZenIntroDialog`) | starting Zen | - | 575 | |
| End of game (`EndLevelDialog` and subclasses) | game over | - | ids 0-3, labels 224-229; badges 237 / close 548 | final score 236 and per-mode stats |
| Quit (`Bej3App::AskQuit`) | quit buttons | 73 | yes/no (framework) | 74 |
| Continue saved game (`Bej3App::DoSavedGameCheck`) | starting a mode with a save | 75 | 77, 78, 79 | 76 |

"-" = not traced.

### Options controls

| Control | Kind, id | Label (drawn beside it) | Evidence strength |
|---|---|---|---|
| Music volume | Slider 0 | 312 | order match |
| Sound volume | Slider 1 | 313 | order match |
| Voice volume | Slider 2 | 314 | order match |
| Resolution option 1 (600) | Checkbox 8 | 316 (group 315) | radio group in handler |
| Resolution option 2 (768) | Checkbox 9 | 317 | radio group |
| Resolution option 3 (1200) | Checkbox 10 | 318 | refused on small screens |
| 3D acceleration | Checkbox 4 | 319 | its warning boxes |
| Full-screen toggle | Checkbox 5 | 320 | windowed-mode box |
| Custom mouse cursor toggle | Checkbox 6 | 321 | elimination only |
| Background animation toggle | Checkbox 7 | 562 | its warning box |

Zen options (`ZenOptionsDialog`, strings 515-540, 576-583) are not mapped
yet.

## Table-based entries (and why)

Per the project rule "find things the way the game does", only these remain
in `src/mod/ui_names.cpp`, because the game associates the text with the
widget only by drawing it at the widget's position or by what the button
does:

- MainMenu 4 -> 288: drawn at that button's position while hovered.
- MainMenu 3, PlayMenu 101, QuestMenu 3 -> 73; MainMenu 7 -> 239: title of
  the dialog the button's own handler opens (marked `OpensDialogTitled`;
  the lead may prefer the image words instead).
- OptionsDialog sliders and checkboxes: labels drawn beside them.
- SecretMenu heading 477.

PlayMenu 100 and QuestMenu 0/1/2 have no game text at all; only their image
resource ids name them.

## First runtime checks

1. `GameStringCount()` > 0 after the main menu appears, and `GameString(287)`
   equals the play ball's label.
2. `ButtonImageWords` on MainMenu 3/7 and PlayMenu 100/101 gives the image
   ids listed above.
3. After hovering a play menu mode, `PlayMenuHoverDescription` is non-empty
   and changes per button (including a locked mode).
4. Options: toggle each checkbox and confirm the id/label pairs, especially
   id 6; read Checkbox mId at +0xC0 and Slider mId at +0xC8.
5. Confirm PlayMenu 100 returns to the main menu and QuestMenu 0/1 change
   pages.
