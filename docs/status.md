# Status

Last update: 2026-10-30 (session 5: no mouse, any resolution). Evidence level for everything below:
runtime with keys posted by `tools/send_keys.py` and speech confirmed in the
mod log as submitted to Prism (NVDA backend). The user has not yet tested
with a real keyboard or confirmed hearing it.

## Working

| Area | What is spoken / possible |
|---|---|
| Startup | "Bejeweled 3 access loaded". Silent during the logo intro (labels drawn off screen are ignored); then "Main menu", items, "5 items", "6 items" as the orbs fly in |
| Welcome / new user | dialog header (string 509) and body (511), edit field with typing echo, the confirm button (510); empty-name warning dialog (83-85) |
| Main menu | the three orbs (labels 285, 286, 287), the player name followed by the change-user text (288), "main menu help button", "main menu quit button" (image names) |
| Play menu | 8 modes; after each move the game's own hover description (e.g. the Zen mode description, 366; for a locked mode the secret-game heading 352 followed by the formatted unlock condition 370); "overworld back button", "main menu quit button" |
| Options (main and in-game) | 3 sliders (Left/Right change 10 %, verified 50->60->50), checkboxes and resolution radio buttons with captions and state (toggle verified), in game also the how-to-play, main-menu and back buttons (308, 311, 309); closed via the options orb (285) |
| Records | the three tabs (labels 472-474) and the six mode tabs (434-439) with "selected"; Stats tab contents read after switching; high-score rows offered when present (none in the test profile, unverified) |
| Quit | quit dialog title (73) and question (74), then the framework's yes / no buttons |
| Game board | "Classic board", hint/menu buttons via Tab then Enter; arrows, W/A/S/D, Space, Enter go to the game (native keyboard play confirmed: a swap reached the game); announcements such as the start announcement (141); F6 status "score 0, level 1, progress 0 of 2500"; tutorial dialogs (an illegal-move notice and a special-gem hint) read |
| Help dialog (`HelpDialog`) | full help text in the game's draw order, start / close button (241, 240/242), the hints checkbox (262) |
| Change user (session 3) | user-selection list under its header (505) ("Test, list item 1 of 2, selected", then the add-user row 508), rename (506), delete (507), confirm and cancel buttons; new-user dialog (303, prompt 305, plus edit); delete-user confirmation (86/87) |
| Quest menu (session 3) | 8 quest orbs ("questmenu alchemy" etc. from image names) with the game's hover text (quest name and goal description from the quest data), back and quit buttons; quest briefing dialog (quest title as header, then the quest's rules text); F6 in a quest "progress 0 of 10" plus the goal text; in-game menu has the quest-menu button (310) |
| Lightning (session 3) | F6 time "0:56", time-up announcement (94/478), end-of-round dialog read as rows: stat labels (483, 484, ...) with values, final score (236) and the high-score table (rank, default player name, score), then play-again / badges / main-menu buttons (229, 228, 227) |
| Records tables (session 3) | picking a mode reads its table: the mode heading (440-445, e.g. 441 for Lightning) followed by numbered rows of name and score; rows also navigable |
| Zen (session 4) | Zen board buttons (options, hint, menu). Zen options: per section a header (section title 527+id, then ", section, off"); focusing it hovers the section so the game opens it (verified) and its description is read (the section's description literal from the exe); then its controls with the section name (e.g. title 528, caption 519, ", drop-down, ", then the selected ambient track name from the game's list); drop-down opens, lists its 7 choices, choosing returns focus to the drop-down (verified by switching between two tracks); vertical music slider, Right = up (verified 50->60->50); back button (537) |
| Badges (session 4) | 20 badges as items in grid order: badge name, then ", badge, not earned", then the game's own hover text (badge name with "(progress/target)" suffix, then the next-level text, e.g. string 12 for badge 2) (verified for 6 badges, none earned) |

## Board stage 1 + hints and cue (2026-10-30, runtime-verified by posted keys)

- Cursor speech: the game's own cursor (arrows) reads the gem under it with
  the game's colour names (strings 455-461) and special-gem titles (the
  game's tooltip function); verified colours, a hypercube and a flame gem.
- E (where am I), Q (neighbours), F6 (status) verified; R/C and X built,
  off by default (settings).
- After a move: settled cue (the game's own button sound) and a summary
  from the game's counters: points, gems cleared, cascades, specials made /
  set off by the game's names. Verified with a hypercube and a flame move.
- Selection (Space) announced; "still moving" warning on W/A/S/D when the
  touched gems have not landed (not yet exercised live).
- Hints: H presses the game's hint button; the hinted gem is identified by
  the game's hint animation on that gem, gated on the hint's idle-counter
  reset; also catches automatic hints. Verified twice, no false hints
  while moving the cursor.
- Settings file `settings.ini` with every choice (created with defaults).
- Not done: mode objects for Butterflies / Poker (locked on the test
  profile, untested); illegal moves are explained by the game's own
  tutorial on the test profile (the mod's "illegal move" line is unverified);
  settings can only be changed in the file, not in the game yet.

## 0.1 package (2026-10-30)

`tools/package.bat` builds `dist/Bejeweled3-Access-0.1.zip` (DLLs with no
local paths inside, player readme, licences incl. Prism's). The packaged
DLLs were installed and smoke-tested in the game (version check, NVDA via
Prism, cursor, Q, H). Published as GitHub release v0.1 (2026-10-30).

## Version 0.1 and game version check (2026-10-30, runtime-verified)

- The mod checks the game file's SHA-256 at startup and runs only on the
  vanilla Steam build (user decision). Verified both ways: the Steam game
  loads normally ("Bejeweled 3 access 0.1 loaded"); with a deliberately
  wrong expected fingerprint the mod announced that the version is not
  supported and installed no hooks.
- Version 0.1 in the startup speech and the log.

## Hint and noise fixes (2026-10-30, verified by posted keys)

- Hints spoken only on H (setting `autohints`, off); 35 s idle produced no
  hint speech. Hint text is the gem and position only (`hintneighbours`,
  off). Existing settings files get new options appended automatically.
- The item count ("2 items") is no longer spoken on boards when the hint
  button returns after its cooldown.
- Repeated specials say "... twice" (not yet seen live).

## User board session (2026-10-30)

The user played Classic for about 35 minutes with the board support and
called it a success. Log: 67 swaps (34 legal, 33 no match), level 3,
best move +6000 / 5 cascades, no errors. Follow-ups noticed: plural
wording ("2 FLAME GEM set off"), and the game's automatic hint is spoken
every 15 s while idle (126 times), which may be too frequent.

## Board: hint surroundings and swap messages (2026-10-30, verified by posted keys)

- Hint: the game's hinted gem plus its four neighbours, no direction
  (design principle: same information as a sighted player, never the
  solution). Verified.
- Swaps: "<gem> swapped <direction> with <gem>" then the move summary, or
  "..., no match, swapped back". Verified with W/A/S/D and Space+arrow.
  Cursor and selection chatter during a swap is held back.

## User test (2026-10-30)

The user tested the menus with a real keyboard and NVDA and reported they
work. The mod log of that session shows no errors or fallbacks.

## Session 5: independent of mouse, window and resolution (runtime-verified)

- Controls are operated through the game's own functions, not mouse
  input (`game_actions.cpp`, docs/re/game-actions.md): buttons and tabs
  (ButtonPress/ButtonDepress), checkboxes (SetChecked), sliders (SetValue +
  release handler), list rows and drop-downs (ListClicked / SetOpen), Zen
  sections (the game's select function). Hover is the game's own hover
  change (enter/leave + listener). Verified in windowed mode at the
  1200 art setting (318) with the real cursor outside the window and in
  fullscreen at the 768 art setting (317).
- Descriptions are read from game data, not by hovering
  (`hover_text.cpp`): quest names and descriptions, badge text (built by
  the badge's own functions), Zen section descriptions, Play menu
  description (after the game's hover notification).
- Board screens use the game's mode/quest title (the board's Title
  parameter) followed by ", game". Records adds the selected tab's label:
  "Records screen", then the label of the selected tab (e.g. 472).
- Rare dialogs (`dialog_text.cpp`): rank-up name, badge-award sequence,
  end-screen special gem counts by icon name, Zen intro picture name,
  quest-mode-complete note. Verified: saved-game prompt (75/76), end-screen
  badges dialog (237, badge grid). Rank-up, badge award, unlock, Zen intro:
  static only (need progress / a new profile).
- Art settings: all three resolution options (316: 800x600 layout, 317,
  318: 1600x1200 layout) verified; every pixel distance scales with the layout size.
- Fallbacks still present: edit fields are clicked through the game's
  mouse handling (posted messages + a GetCursorPos/WindowFromPoint import
  override inside the game process; the real cursor never moves). Any
  refused direct call also falls back to that click and is logged.
- The game's own keys (docs/re/screen-names-and-keys.md): Escape opens the
  in-game menu on boards and acts as the main-menu button on end screens; Space
  selects gems once the keyboard cursor has been shown (it only pauses
  before that); Enter is not used by boards; Alt+Enter switches window
  mode; F1 is free. No menu control has native Enter/arrow handling.

## How names are found (no hard-coded menus)

In order: widget label -> text the game draws on the control -> the game's
resource name of the button image -> "unlabeled <class> <id>". Captions for
checkboxes/sliders are the text the game draws beside them. Dialog text is
the dialog's own fields plus what it draws. The only table-like rule in use is
`controls::IsRadioCheckbox` (Options ids 8-10 form the resolution radio
group, from the game's CheckboxChecked handler). `ui_names` text tables and
`HudBoardButtonRole` exist but are not used.

Zen section titles are the game's strings 527+id, the ids its own section
drawing uses (docs/re/zen-options.md); the colour-based title guess was
removed in session 4. Reading order now groups centres within 16 px into a
row (was 40) so stacked controls keep their order.

Layout heuristics in use (all reading the game's drawn text, documented in
`navigator.cpp`): dialog text from the dialog's header onward in draw order;
left-aligned columns of 3+ strings read as table rows; list rows positioned
where the game draws them; section titles = topmost text in a container
drawn in a different colour from its controls' captions (Zen options);
U+2588 filler stripped from score fonts.

## Controllers

The game creates an XInput gamepad driver at startup (static: Bej3App ctor
FUN_0046b440 stores it at gApp+0x18; loads xinput1_1.dll). Whether menus or
the board react to a controller is untested; the readme lists only keyboard
and mouse.

## Known gaps

- Board phase scope (user decisions 2026-10-30): not supported for now:
  Lightning, Ice Storm, Diamond Mine and the timed quests. First targets:
  Classic, Zen, Butterflies, Poker and move-based quests.


- Screens without a drawn title use the class name ("Play menu", "Records
  screen", "Classic board", "Zen options dialog").
- Badges: earned level names (strings 1-4) untested (no badges earned); the
  badge names come from the game's badge-name table in memory.
- Zen options: the music slider has no drawn caption ("unlabeled, vertical
  slider"); the binaural section's safety link (579) and section 2's
  sub-controls are untested. Items reorder while a section opens; focus stays on the same
  item.
- Zen intro dialog has no text (a picture); the mod says its picture name,
  by user preference for now (may change; see decisions.md).
- Quest orbs are named by image ("questmenu alchemy" for most); the real
  quest name comes in the hover text right after.
- Not exercised: rank-up, badge award and unlock dialogs (need progress),
  other modes' end screens (same dialog family as Lightning), the developer
  Menu/SecretMenu, multi-page quest menu (only one page unlocked).
- Game board grid (gem positions/colours, reticle) is not spoken yet.

## Test state

Throwaway profile "Test" used (sessions 2 and 3) and set aside in
`private/test-profile-users-20261030/`; the original save folder (no
profiles) was restored after each session. Zen options section 0 (title
527) was enabled in the Test profile only. Options touched in testing were
set back (music slider 50 %, custom-cursors checkbox off).

## Incident (session 4)

A test script pressed Enter while focus had landed (via the mouse
position) on the Options fullscreen checkbox (320), switching the game to
windowed 1024x768. Restored by clicking it again; the window is back at
2560x1440. Test helper `tools/focus_item.sh` now moves focus by name (with
Tab on boards) before pressing Enter or Space. Options settings may be
game-wide rather than per profile.

## Next steps

0.1 is released (GitHub release v0.1) and in use. Candidates for 0.2:

1. **T key** (user request, 2026-10-30): T speaks the countdown in timed
   modes such as Lightning, "1:05 left"; "no time limit" on untimed
   boards; "time not available" if the read fails. **Implemented
   2026-10-30, runtime-verified by posted keys** on the user's profile
   (save backed up and restored identical): Lightning T "0:52 left",
   F6 "time 0:50", T "0:49 left", later "0:43 left"; Classic T "no time
   limit". In Lightning, Right then Space selected the gem ("selected")
   and did not pause (pause fade 0). On-screen rounding was not compared
   by eye. It uses `TicksLeft` in
   `src/mod/hud.cpp` (Board vtable+0x2EC, as the game's DrawTimer) and the
   new `HudTimeText`, which rounds up to whole seconds like DrawTimer.
   F6 now uses the same helper; before, it truncated and could say one
   second less than the screen. The game's board does not handle T
   (`docs/re/board-input.md`). Timed modes stay unsupported until their
   gameplay is worked out. Space pauses timed boards only while the
   keyboard cursor has never been shown (static research, board-input.md);
   the select case above is confirmed, the pause case is not tested.
2. Done for 0.2 (user-approved 2026-10-30, runtime-verified by posted keys
   on the user's profile, save restored): Enter selects like Space (Enter
   after Tab still presses a board button); F1 key list in menus and on the
   board; X on by default and repeated X goes on to the next special gem;
   0.1 settings files are updated once (`settingsversion=2`).
3. In progress: mode support (Poker, Butterflies, Ice Storm, Diamond Mine,
   Lightning objects, quests) by three subagents in `src/mod/mode_*.cpp`
   and `docs/modes/`, through `src/mod/mode_info.h` (cell text, F6 status,
   M key details, events). The lead has wired the hooks; the full build
   waits for the mode files. Not live-tested: Poker, Butterflies and
   Ice Storm are locked on the user's profile.
4. Listing in the Accessibility Mod Manager: the author tool
   (amm-author 0.30.0) is downloaded locally, outside the repository; its
   publishing docs are not read yet.
5. Exercise the untested screens and modes (Butterflies, Poker, move-based
   quests).
