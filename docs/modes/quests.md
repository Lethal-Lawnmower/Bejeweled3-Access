# Quest boards (QuestBoard family)

Source: `src/mod/mode_quest.cpp` (`QuestHooks()`, see `src/mod/mode_info.h`).
Date: 2026-10-30. Exe: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`).

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Not linked into a build, not run in the game, not heard through
Prism.** Every field below is a static read of the decompilation; none is
runtime-verified.

Method: the full read-only decompile `private/ghidra-b4/all.c` (extractor
`private/ghidra-b4/fn.sh`, `callers.sh`, vtable map `vtmap.txt`; goal and
board override tables written to `private/ghidra-quest/goal_overrides.txt`
from `vtmap.txt`), capstone disassembly `private/ghidra-b4/py/dis.py`, quest
definitions in `private/pak/properties/quest.cfg`, string ids from
`default.xml`. Builds on `docs/re/board-objects.md` (Quest boards and
goals) and `docs/re/hud.md`. No new Ghidra run was needed.

## Scope and dispatch

`mode_info.cpp` sends every QuestBoard subclass except Poker, Butterfly,
Inferno and Dig boards here. Inside, two things are looked up once per
board/goal pointer:

- **goal class**: RTTI of `[Board+0x3180]`, accepted only if the goal's
  `+0x104` points back at the board;
- **board class** (the quest's Challenge): TimeBombBoard, RealTimeBombBoard,
  BalanceBoard, FillerBoard, MoveLimitBoard (IsKindOf), else plain.

A ButterflyGoal or DigGoal on one of these boards is passed on to
`ButterflyHooks()` / `DiamondMineHooks()` (owned by their mode files); their
text is appended.

Goal progress and target, moves left and time left are already spoken by
F6 / T (`hud.cpp`) and are not repeated.

## What is spoken

| Hook | Says (examples) |
|---|---|
| `cellText` | bomb: `5 moves left` / `40 seconds left`; Alchemy: `gold` / `not gold`; Alchemy III (flip): `not flipped` / `lead` / `gold`; Wall Blast: `wall` (nothing on cells without wall) |
| `briefStatus` (F6) | Time Bomb: `3 TIME BOMB, lowest 4 moves left` or `no TIME BOMB`; Balance: `Red 5 of 15, Blue 7 of 15, Blue side down 30 percent` (or `level`); Avalanche: `41 of 64 squares filled, almost full`; Alchemy: `12 squares not gold` (flip: `20 squares not gold, 8 of them lead`); Wall Blast: `9 wall pieces left` |
| `details` (M) | bombs lowest first with positions: `TIME BOMB 2 moves left, column 4, row 6. TIME BOMB 9 moves left, ...`; Balance as F6; Avalanche: count plus `Gems per column, left to right: 3, 5, ...`; Alchemy / Wall Blast: count plus `row 3: columns 3, 4, 5, 6; row 4: column 2` (chess coordinates: `C3, D3, ...`); Move Limit: `Moves Left: 3` while under 4 |
| `tick` events | `new TIME BOMB, 20 moves left, column 4, row 1`; `TIME BOMB 3 moves left, column 4, row 6` (move bombs at 3, 2, 1; time bombs at 10, 5, 3, 2, 1); `11 squares not gold` / `8 wall pieces left` when the count drops; `Blue side down 75 percent` (rising past 50, 75, 90; re-armed 10 below); `board almost full`; `Moves Left: 3` (each change while under 4) |

Game words via `GameString()`: 137 TIME BOMB (tooltip title), 301
`Moves Left: %i` (MoveLimitBoard::DrawBottomFrame), colours 455 (Red) and
461 (Blue). Mod words: `moves left`, `seconds left`, `lowest`, `new`,
`gold`, `not gold`, `lead`, `not flipped`, `wall`, `wall pieces left`,
`squares not gold`, `side down N percent`, `level`, `squares filled`,
`almost full`, `Gems per column`, positions.

## Quests covered (quest.cfg, 40 quests)

| Quest (Challenge / Goal) | Coverage |
|---|---|
| Challenge "Time Bomb", Goal Clear Bombs (e.g. Time Bomb) | covered: bomb counters on cells, F6, M, warnings |
| Challenge "Time Bomb (Real time)", Goal Clear Bombs (e.g. Time Bomb II) | covered; counter unit "seconds" is a hypothesis (below) |
| Alchemy, Alchemy II (Gold Lines; Time Limit or Move Limit) | covered: square state, count, list |
| Alchemy III (Move Limit / Gold Lines Flip) | covered: three states |
| Wall Blast (Time Limit + No Lose / Wall Blast) | covered: wall squares, count, list |
| Challenge Balance (Balance, Balance II, ...) | covered: red/blue counts and the scale |
| Avalanche (Filler / Clear Gems) | covered: how full, per-column heights, danger |
| Move Limit + Clear Gems (e.g. Stratamax) | nothing extra on cells; low moves warning; progress via F6 |
| Buried Treasure, Sandstorm (Time Limit / Hidden Objects) | **not covered** (below) |
| Butterflies, Gold Rush/Dig, Poker, Inferno quests | other mode files (board class dispatch) |
| Gold, Gold Yellow (defaultquest.cfg only) | covered by the Gold grid if they are loaded |
| Arsenal, Detonator, Firestorm, Preserver, Score, Survival, Hidden Gems, Hidden Grid | not in quest.cfg; no cell objects; progress via F6 only |

## Fields

All **static** (this exe), none runtime-verified.

| Field | Meaning | Evidence |
|---|---|---|
| Board `+0x3180` | QuestGoal* | board_layout.h kQuestGoal; goal getters read the board through goal `+0x104` |
| piece `+0x228` `0x20` / `0x40` | bomb counting moves / time | setter `0x0073CCC0` (flags 0x220 or 0x240), RealTimeBomb drop `0x006C6200` (0x240); Board `0x006D0930` / `0x006D0970` lower the bomb count when one is tallied |
| piece `+0x244` | bomb counter | setter stores BombCountdown (`+0x3248`); a move bomb is lowered after each swap (`0x0074C070` -> `0x0073C7E0` -> vt+0x350 spin, decrement in UpdateGame `0x00767E20`); a time bomb every `piece+0x250` (= 100, piece init `0x00720120`) updates; counter 0 on a still board -> vt+0x28C (game over) |
| GoldLines(Flip)Goal `+0x134 + 0x20*(col+8*row)` int | square state: Gold Lines 0 plain, 1 gold; Flip -1 plain, 0 lead, 1 gold | gild vt+0x120 `0x006B78F0` (both classes; flip slot `0x006B7DD0` is a JMP to it) called as (row, col) from per-piece `0x006AA220` and the delayed queue `0x006ABEB0`; raises the value while it is below 1 and adds a level point when it becomes positive; init `0x006B04F0` writes 0, flip init `0x006B0F20` writes -1; draw `0x006ABFC0` has three looks (< 0, 0, > 0) |
| GoldGoal/GoldYellowGoal `+4 + 4*(col+8*row)` int | 1 = gold | `0x006AA140` (GoldYellow, piece col/row) and `0x006AB820`; draw `0x006AC200`; Perimeter pre-gilds edges (init `0x006AE200`) |
| WallBlastGoal `+0x118 + 4*(col+8*row)` byte | wall present | init `0x006B0FB0` copies the quest grid in reading order and counts walls into `+0x110` (target); hit `0x006AD740` (EAX col, arg row; x = col*0x80) clears it and adds a level point |
| BalanceBoard `+0x3260` / `+0x325C` int | red / blue collected | `0x006D06A0`: colour 0 (or pre-hypercube colour) -> `[0xC98]`, colour 6 -> `[0xC97]`, each capped at `[0xC92]` |
| BalanceBoard `+0x3248` int | Gems (cap per colour, target = 2x) | init `0x006E4C80`; getter `0x006C53A0` |
| BalanceBoard `+0x3250` float | scale, positive when blue is heavier; over 1.0 either way loses | update `0x006E3C90`: += clamp((blue-red)/Balance(+0x324C), -1, 1) / RollSpeedDivisor(+0x3254); GameOver when abs > 1.0 (constant `0x00880D44` = 1.0) |
| FillerBoard `+0x3248` int | DropPerMove | per move `0x006C52A0` adds it to the waiting count `+0x324C` |
| FillerBoard vt+0x368 `0x006C5240` | danger: gems on board + DropPerMove - 3 > 63, `+0xD1C` == 0 | replicated only when the slot is that address |
| MoveLimitBoard vt+0x368 `0x006C4DA0` | danger: moves left (`+0x324C`) < 4 | replicated only when the slot is that address |

## Not covered, and why

- **Hidden Objects** (Buried Treasure, Sandstorm): the goal keeps an 8x8
  map of 0x78-byte cell records (`goal + 0x110 + 0x78*col + 0x3C0*row`,
  `+4` = object index or -1, `+0` zeroed at setup) and object records of
  0x48 bytes in the vector `goal+0x1F20..+0x1F24` (count = target
  `+0x1F4C`), built by `0x006AEC80`. Speaking the object index would reveal
  treasure the sand still hides, which breaks the design rule. What the
  screen shows (how much sand is cleared per square, which parts of an
  object are visible) was not decoded; next step: find the per-cell reveal
  value in the per-move function `0x006C24C0` and the draw `0x006B9CA0`.
- Wall Blast special-drop countdown (`goal+0x218`, reload DropThreshold
  `+0x114`, `0x006A9980`): not shown on screen as far as found; not read.
- Time Bomb next-drop countdown (`+0x3258`) and bombs-on-board counter
  (`+0x325C`): not drawn as far as found; the mod counts bombs from the
  grid instead.
- Avalanche gems waiting (`+0x324C`): not known to be drawn; not read.
- TimeLimitBoard bonus / penalty texts (499 / 500): not traced; if they are
  Announcements, hud.cpp already speaks them.

## Uncertainties

- Board class per Challenge: "Time Bomb (Real time)" is assumed to create
  RealTimeBombBoard and "Time Bomb" TimeBombBoard (factory not traced). Cell
  text reads the bomb flag itself, so it is right either way.
- **Time bomb unit**: one count per 100 updates; "seconds" assumes 100
  updates per second (hud.md hypothesis).
- **Balance direction**: the sign rule is static; that a positive scale is
  drawn as the blue side going down is a hypothesis; the percent is the
  scale's distance to the losing value 1.0.
- Gold Lines grid layout now traced (board-objects.md listed it as a
  hypothesis); the records' other fields (delay queue at record +0xC) are
  not read.
- Event wording and thresholds (bomb warnings, balance steps) are mod
  defaults, not game behaviour; candidates for settings.

## Interface notes for the lead

- `board_access.cpp` `Gem()` still appends the bare counter for bombs and
  time gems (`TIME BOMB 5`), so the cursor says `Red TIME BOMB 5, 5 moves
  left`. Suggest dropping the bare number when the mode hook has
  `cellText`.
- `QuestHooks().reset` also calls the Butterfly and Diamond Mine resets (it
  may have delegated to them).

## Runtime checklist (throwaway profile, read-only)

1. Each quest: `ClassNameOf([board+0x3180])` gives the expected goal class
   and the goal's `+0x104` equals the board.
2. Time Bomb: bombs have `0x220` (moves) or `0x240` (real time); `+0x244`
   equals the drawn number; after a move it drops by 1; warnings come once
   per value; time bombs drop once per second.
3. Alchemy: cells at `goal+0x134+0x20*i` turn 1 where the drawn square
   turns gold; F6 count = 64 - level points. Alchemy III: -1 / 0 / 1 match
   the three looks.
4. Wall Blast: bytes at `goal+0x118+4*i` are 1 exactly on the drawn wall;
   count matches target - progress.
5. Balance: red/blue counts follow the matched gems; the scale sign and the
   drawn tilt agree; the game ends at about 100 percent.
6. Avalanche: `board almost full` appears when the game's own warning does.
7. Move Limit: `Moves Left: 3`, `2`, `1` as the counter drops.

## Live check (lead, 2026-10-30, user's profile)

Time Bomb quest (TimeBombBoard): F6 "progress 0 of 10, 1 TIME BOMB, lowest
20 moves left"; M "TIME BOMB 20 moves left, column 4, row 4"; after three
moves M "TIME BOMB 17 moves left". Butterflies and Gold Rush quests use
ButterflyBoard and DigBoard (see those docs). Balance, Avalanche, Alchemy,
Buried Treasure and Stratamax were not played.
