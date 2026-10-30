# Decisions and approvals

Human approvals are marked **User**. Agent choices are marked **Agent** and can be
changed by the user.

## 2026-10-30

- **User:** Start a Bejeweled 3 accessibility mod. Goal is menus and UI first,
  then the game board. Research community reverse engineering; if none is
  useful, the agent may "take the wheel" on design.
- **User:** Approved using Prism, including building it from source for the
  32-bit game process (upstream ships no 32-bit Windows binary).
- **User:** Approved using Ghidra for reverse engineering.
- **Agent:** Project lives at `D:\dev\Bejeweled3-Access`, outside the Steam
  install, so the Steam folder only receives the built mod files.
- **Agent:** Injection via a `winmm.dll` proxy placed next to `Bejeweled3.exe`
  (the game imports WINMM). The game still launches normally from Steam.
  Not yet installed into the game folder.
- **Agent:** Speech only through Prism (`prism.dll`, 32-bit, built from tag
  v0.18.3). No other speech library or fallback.

- **Agent:** Keyboard scheme is provisional (Up/Down/Tab move, Enter/Space
  activate, F5 repeat, F12 dump) and awaits user review.
- **Agent:** Mod files installed into the Steam game folder (`winmm.dll`,
  `prism.dll`); remove with `tools/install.sh --remove`.
- **Agent:** Leaked PopCap source repositories are not used as references.

- **User:** "Figure out the rest of the game's menus and UI", then "use
  subagents to figure out the rest of the game's UI, reverse engineering if
  needed". Authorizes delegating static reverse engineering and code to
  subagents. Scope: menus, dialogs, in-game UI around the board. The board
  grid itself remains the next phase.
- **Agent:** Subagents do static work only (Ghidra copies, code in their own
  files). Only the lead agent runs the game, integrates code and runtime-tests.
- **Agent:** For menu exploration a throwaway game profile is used. The save
  folder `%LOCALAPPDATA%\Steam\Bejeweled3\users` (no profiles yet, only
  `users.dat`) was backed up to `private/save-backup-20261030/` and will be
  restored afterwards so the user still gets the new-player prompt.
- **Agent:** Game UI text is not copied into the source; the mod reads it at
  runtime from the game (widget labels, drawn text, string table).

- **User (2026-10-30):** No hard-coded menus or labels: "as long as the game
  natively can find it". The mod discovers screens, buttons and text from
  the game's own data at runtime. Accepted fixed points are the memory
  locations of the game's own structures (gApp, field offsets, vtables),
  which are needed to find that data at all. Any lookup table (e.g. naming
  an image-only button) is a last resort, must be backed by the game's own
  code using that text for that button, and must be listed in status.md.

- **User (2026-10-30):** "Let's finish the menus first" before the board.
- **Agent:** Naming order for picture buttons is image resource name before
  drawn text (quest orbs overlap the screen heading). Section titles use a
  colour heuristic (see status.md); revisit if it mislabels elsewhere.
- **Agent:** Badges and Zen options left partially accessible this pass,
  recorded as gaps rather than guessed.

(Superseded 2026-10-30: the user chose GPL-3.0 and a public repository; see below.)

## 2026-10-30, session 4

- **User:** "Fix badges and Zen" before the board.
- **Agent:** Two static-RE subagents (badges, Zen options); lead integrated
  and runtime-tested. Zen section titles use the game's own string ids
  (527+id) rather than the colour heuristic. Zen caption table
  (`zen::CaptionStringId`) is not used; captions come from drawn text.

## 2026-10-30, session 5

- **User:** The mod must not depend on screen mode, resolution, window state
  or a mouse; "there's no way we can actually just check the box via the
  game's code?" Decision: operate controls by calling the game's own
  functions (button pressed, checkbox set, slider value, list choice,
  section select, hover text) instead of synthesising mouse input. Mouse
  synthesis stays only as a documented fallback until each direct path is
  verified, then is removed where replaced.
- **Agent:** Interim fixes this session: pixel distances scale with the
  game's layout size (600/768/1200); a GetCursorPos/WindowFromPoint import
  override inside the game process keeps synthetic hover from being undone
  in windowed mode (the real cursor never moves).
- **User (2026-10-30):** "Set aside a few sub-agents if you need help
  figuring out more of the game's code." Two static-RE agents running:
  direct control actions (`game_actions.*`) and hover-only text
  (`hover_text.*`), each with its own Ghidra copy and files.
- **User (2026-10-30):** "Deploy another 2 agents if you really need help
  with menus. The board can wait till menus are polished." Added agents for
  screen names + the game's native keys (`screen_names.*`) and rarely seen
  dialogs (`dialog_text.*`). Board work deferred until menus are polished.
- **User (2026-10-30):** Zen intro dialog (a picture with no text): keep
  speaking only the picture's resource name for now. This is a user
  preference that may change later (alternative considered: also reading
  the four Zen section titles the picture describes, string ids 527-530).
- **User (2026-10-30):** Tested the menus with their own keyboard and NVDA:
  "testing the menus and they work." First user acceptance of the menu
  layer. Mod log of that session (13:13-13:16): 195 utterances, no
  errors, refusals or fallbacks logged.

## 2026-10-30, GitHub

- **User:** Put the project in a private GitHub repository
  (Lethal-Lawnmower/Bejeweled3-Access). Approved: create it private and
  push source, docs and tools (no game files, saves, builds or analysis
  databases).
- **User:** Before upload, replace quoted in-game text in the docs with
  string ids or descriptions (game content stays out of repositories,
  private included). Local history is rewritten so no commit contains the
  quotes.
- **User:** Licence: decide later. The private repository has no licence
  (all rights reserved) until the user chooses; required before anything
  public or any shared build.
- **Agent (2026-10-30):** Created https://github.com/Lethal-Lawnmower/Bejeweled3-Access
  (private) and pushed one commit (90 files). History was rebuilt before the
  push so no commit contains quoted game text. Verified on GitHub: 90 files,
  none under private/, deps/ or build/, no executables, paks, DLLs or saves.

## 2026-10-30, board research

- **User:** Board phase starts with research only: "use 6 sub-agents to
  reverse engineer everything about the game board ... don't implement
  anything yet." Goal: know what the board presents underneath the
  animations so a blind player can navigate it. Six static-RE agents
  (state/settling, input/cursor/hints, events, special objects per mode,
  flow/timing, game descriptions + cross-check), then a combined research
  summary with design options for the user to decide. No mod code changes
  in this phase; live checks only read memory on the throwaway profile.
- **User (2026-10-30):** "lightning mode not supported for now" for the
  board phase. Recorded as a known limitation; the game itself still plays
  Lightning normally with the menu layer. Other time-pressure modes (Ice
  Storm, Diamond Mine, timed quests) are pending the user's decision.
- **User (2026-10-30):** Mark the other time-pressure play as unsupported
  "till we figure out the gameplay". Recorded as: not supported for now in
  the board phase: Lightning, Ice Storm, Diamond Mine (both are main modes,
  not quests; the user asked whether they were quests) and the timed quests.
  Supported targets for the first board design: Classic, Zen, Butterflies,
  Poker and move-based quests. Revisit after the untimed modes work.
- **User (2026-10-30):** Board design defaults chosen (see
  `board-design.md`): cursor speaks colour and special; "column N, row M";
  short summary after a move; act anytime with a "still moving" warning;
  game's own hint with its gem spoken; reading keys for position/status and
  neighbours; mode objects in the cell description plus status and key
  events; speech plus a few sound cues. User: these are uncertain because
  they have not played yet, so **all of them must be optional and
  configurable**.
- **User (2026-10-30), design principle:** the mod conveys the same
  information a sighted player perceives, and never solves the game for
  the player ("I actually want to challenge myself", no instant hints,
  cheat or god mode). Every board feature is checked against this rule.
  Accepted now: (1) the game's own hint is announced as the hinted gem plus
  its neighbours (what a sighted player sees), never the direction; (3)
  clear swap feedback (what moved where, match or swapped back). Proposed
  and pending the user's answer: R/C row and column reading on by default,
  less repetition after pop-ups, an F1 key list. Ideas for later: shorter
  colour words for scanning, optional per-colour sounds.
- **User (2026-10-30):** First real board session by the user (own
  profile, Classic, ~14:41-15:16): "I think we struck gold." Mod log: 67
  swaps (34 legal, 33 no match), level 3 reached, best move +6000 with 5
  cascades, 126 hint announcements (mostly the game's automatic hint every
  15 s while idle), no errors or fallbacks.
- **User (2026-10-30):** Hints spoken only when the player presses H; the
  game's automatic hints are no longer announced (new setting
  `autohints`, default off). Repeated specials are worded "... set off
  twice" instead of pluralising the game's own names.
- **User (2026-10-30):** The hint says only the hinted gem and its
  position; the surrounding gems are dropped (Q still reads them). Kept as
  setting `hintneighbours`, default off. Supersedes the earlier "hint plus
  neighbours" default.
- **User (2026-10-30):** Release 0.1 supports only the vanilla Steam
  version of Bejeweled 3 ("no mods, just vanilla"). The mod checks the game
  file's SHA-256 at startup; on any other version it says so and stays
  inactive. Fixed memory locations are per game build, not per PC
  (explained to the user); pattern-based lookups for other versions are a
  possible later step if needed.
- **User (2026-10-30):** Licence GPL-3.0 (version 3 only; "or later" was
  not chosen by the user). Make the repository public; before that, squash the
  history into one commit so older commits with a local user-folder path
  and the save profile name are not published. The user plans to
  distribute through someone's mod manager (which one: to be confirmed).
