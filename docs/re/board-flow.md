# Board flow and time: lifecycle, timers, pause, speed, focus

Agent 5 of the board research phase (brief: `board-research-brief.md`).
Date: 2026-10-30. Target: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`).
Method: **static only**. Ghidra 12.0.4 headless on `private/ghidra-b5`
(full decompilation of `0x00660000-0x00795000` in `private/ghidra-b5/all.c`,
made with `tools/ghidra/BFlowDumpRange.java`), a capstone listing of `.text`
(`private/ghidra-b5/text.lst`), vtable diffs (`private/ghidra-b5/vt.txt`,
`goalvt.txt`). Helpers in that folder: `run.sh`, `fn.sh` (print a function
from `all.c`), `dis.py`, `rd.py` (read constants), `vt.py`. Nothing here was
run against the game. Labels: **static** (read in code), **hypothesis**
(inferred), **community** (third-party). Text is named by string id from
`private/pak/properties/default.xml`; its comments also give the original
function names (e.g. id 140 comes from `Board::DoUpdate`), which I use to name
functions.

Offsets are byte offsets from the Board (`[gApp+0xBE8]`) unless stated.
Ghidra's `param_1[N]` on an `int*` board is offset `4*N`.

## 0. Corrections and name fixes for other docs

- `0x00767090` is **Board::DoUpdate** (string ids 140/141 are attributed to
  `Board::DoUpdate` in default.xml and are created there), vtable `+0x3A8`.
  `0x00767E20` is **Board::UpdateGame** (id 139, vtable `+0x39C`).
  `0x00759170` is **Board::Update** (Widget vtable `+0x58`, ZenBoard
  `0x007116C0` matches id 513's `ZenBoard::Update`). board-state.md calls
  `0x00767090` "Board::Update"; the call chain it describes is right.
- `+0x12C8` is not a hint cooldown (hud.md): it is the **pause fade amount**
  (0..1, see section 3). The hint button needing it at 0 means "only when not
  paused or fading".
- `+0x3070` / `+0x3074` (Ghidra `[0xC1C]`/`[0xC1D]`) are the start
  countdowns, not the button-layout field `+0xC1C` from hud.md.
- `0x00792BF0` is the CRT float-to-int (`_ftol`) helper, not game logic.

## 1. The tick model (all modes)

| Item | Fact | Evidence | Level |
|---|---|---|---|
| Logical tick | **100 ticks = 1 second**. Every timer in ticks is shown as `(t+99)/100` seconds; time limits are `seconds*100` | DrawTimer `0x007516A0` (hud.md); `0x0073BB80` uses `limit*100`; app field `gApp+0x560` = 10 set in the SexyAppBase ctor `0x005AD880` and read by the driver loop `0x005BB11C..` (frame time 10 ms) | static; the 10 ms meaning is hypothesis |
| Per frame | `Board::Update` `0x00759170` computes a speed factor, adds it to the float accumulator `+0xF0`, and calls `DoUpdate` (vtable `+0x3A8`) **floor(accumulator)** times, keeping the fraction (`0x00759436-0x00759475`) | disassembly | static |
| Speed factor | global float `[0x008D2DA8]`, smoothed each Update: `speed = target*(1-k) + speed*k` (k 0.95, or 0.98 in replay), snapped when within 0.01, then multiplied by vtable `+0x1E0` | `0x00759170` | static |
| `+0x1E0` | returns 1.0 (`0x0073B5E0`); no override in any of the 18 Board classes' vtables | `vt.txt` and second diff | static |
| Target speed | 1.0 always, except during **replay playback**: `+0x1410` set and `+0x1412` clear -> 0.3 for the first 10 ticks after `+0x140C`, then 0.85; 0.03 for 3 ticks if `+0x1411`; 0.4 while swaps are in progress (`+0xC30` vector). `+0x1414` forces 1.0 | `0x00759170` | static (replay meaning from board-state.md: hypothesis) |
| Board tick counter | `+0x38` (Widget update count) increments once per DoUpdate via `0x00475660` | DoUpdate `0x00767C65` | static |
| Game clock | `+0xE38` increments once per UpdateGame unless vtable `+0x194` ("time frozen") is true | UpdateGame `0x00767E20` | static |

So the game logic is a fixed 100 Hz simulation, independent of render frame
rate (hypothesis for the framework part; the board part is static). There is
**no user-facing speed or slow-motion setting**: the only non-1.0 speeds are
the replay feature above.

## 2. Timers

### 2.1 Fields and getters

| Getter / field | Meaning | Implementation | Level |
|---|---|---|---|
| vtable `+0x1A0` | time limit in seconds (0 = untimed) | Board `0x0073C250` = 0; Speed `0x007140F0` = 60; TimeLimit/Dig `0x006C4A30` = `[+0x3248]` | static (hud.md) |
| vtable `+0x2EC` | ticks left, -1 untimed | Board `0x0073BB80`: `clamp(limit*100 - max(0, [+0xE38] - 250), 0, limit*100)`; replay playback returns `[+0x1308]`. **The first 250 ticks (2.5 s) of the clock do not count.** Speed `0x00713FA0`: same on float `[+0x3284]`, capped by `[+0x30F4]`. Dig `0x00736B50`: min(base, `[+0x3274]`) when nonzero | static |
| `+0xE38` | elapsed clock ticks | UpdateGame | static |
| `+0x3284` (Speed) | Lightning's scaled elapsed clock (float) | SpeedBoard::Update `0x007173F0`: each time `+0xE38` changes, `+= rate`, rate = `[+0x3288]` if nonzero else `min(1, 0.7 + 0.3*ticksLeft/600)`. **The Lightning clock runs slower in its last 6 seconds (down to 70 %)** | static (constants `0x00881770`=600, `0x00880EF8`=0.3, `0x008813C8`=0.7) |
| `+0x3288` (Speed) | rate override, set at the end of SpeedBoard::Update from `ticksLeft / x` while float `+0x1188` > 0 | `0x007173F0` tail | static, meaning of `+0x1188` unknown |
| vtable `+0x368` "time low" | Board `0x0073B7C0`: timed and ticksLeft < 1000 (limit <= 60 s) or < 1500 (longer limits). Speed `0x00714130`: false while banked time `+0x30AC` != 0 | static |
| vtable `+0x2F4` / `+0x2F0` | time fraction; per-tick timer housekeeping | Board `0x00744920` / `0x00744B10`; Speed `0x00716F70` | static |

### 2.2 Warnings near the end

- While "time low" is true, `0x00744920` (Board) / `0x00716F70` (Speed)
  play a sound (`0x0076AAF0` = play sound, hypothesis) every `35 - k` ticks
  with a pitch rising as time runs out (last 10 s, or 15 s for limits over
  60 s). static.
- SpeedBoard::Update additionally plays a sound once per whole second in the
  last 8 s (`ticks % 100 == 0 && ticks-1 < 800`), alternating pan. static.
- No spoken/text warning was found; the warnings are audio only (the timer
  bar drawing may also change; not traced).

### 2.3 Time-up

- Generic timed boards (quests with a time limit, Diamond Mine): vtable
  `+0x360` `0x0073B890`, called every UpdateGame through `0x00755620`: when
  the time fraction <= 0 **and vtable `+0x2E8` IsBoardStill** and the
  `+0x26B8` vector is empty and `+0xD1C == 0`: byte `+0xE28 = 1` (time-up
  flag; KeyDown refuses moves while set) and GameOver(1). **Time-up waits
  for cascades to finish.** static.
- Lightning: `0x00716F70` does the same and calls SpeedBoard::GameOver
  `0x00719350`, which is a round end, not necessarily the game end
  (section 4.3).

### 2.4 Time added by events

| Mode | Event | Effect | Evidence |
|---|---|---|---|
| Lightning | time gem (piece flag `0x20000`) cleared | SpeedBoard::PieceTallied `0x00715360`: `+0x30AC += piece[+0x244]` (banked seconds), `+0x30B0` total. **Banked, not added to the running clock**; used when the clock hits 0 (4.3). String 479 is the pop-up | static |
| Lightning | round end with bank > 0 | `0x00719350`: `+0x30F4 = bank*100`, `+0xE38 = max(0,(60-bank)*100)`, `+0x3284` likewise, bank = 0, multiplier `+0xE0C` += 1 | static |
| Diamond Mine | scroll (rows dug) | DigBoard::DoTimeBonus `0x00736F20` (strings 170/171): limit `+0x3248 += [+0x324C]` (or `[+0x3278]` for the double "mega" scroll); then the clock is rebased: `+0x3274` = new ticks left; if < 9001: limit 90, `+0xE38 = 9200 - ticks`, else limit = ceil(ticks/100), `+0xE38 = 200` | static |
| Time-limit quests | quest bonus/penalty | TimeLimitBoard::DoQuestBonus / DoQuestPenalty (strings 499/500) | string table only, code not traced |
| Speed config | `properties/speed.cfg` (pak) keys `TimeStart`, `LevelTimeStep`, `TimeStep`, `5SecChanceCurve`, `10SecChanceCurve` | data file (read-only copy) | static (data) |

### 2.5 What freezes time

Three separate mechanisms:

1. **DoUpdate returns early** (the whole board stops: no clock, no
   animation of pieces, no queued swaps). `0x00767090`: while dialog 0x29,
   0x2B (rank-up), 0x27 (unlock) or 0x13 (help, also used for the tutorial
   pop-ups from `Board::CheckForTutorialDialogs` `0x00765D50`) is open;
   while dialog 0x17 (badge award) is open; while `+0x2B40 > 0`; during
   replay playback `+0x12F8` (`0x00761850` instead). static (dialog ids from
   the `push` before each `gApp->vtable+0x110` call; 0x29 is unidentified).
2. **Pause** (pausable boards only, section 3): the board freezes once the
   pause fade reaches 0.75.
3. **Clock-only freeze**, vtable `+0x194` `0x0073C330` (quests: goal
   `+0x94` or this; Poker `0x006C5A00` adds two curves): `+0x3070 > 0`
   (get-ready countdown), `+0x2880 > 0`, `+0xD20 != 0` (quest complete),
   HintDialog 0x12 or UnlockDialog 0x27 open, or the special-gem lightning
   effect vector `+0x125C` non-empty. **The clock stops while star/hypercube
   lightning effects play**, and during the get-ready phase. Pieces still
   move. static.

## 3. Pause

| Item | Fact | Evidence | Level |
|---|---|---|---|
| User pause flag | byte `+0x12C4`, toggled by the Space **character** (KeyChar `0x0073B670`) only if `+0x1410 == 0`, the keyboard cursor is hidden (`+0x2C44 == -1`) and the board can pause (vtable `+0x1AC`) or is already paused | static |
| Cursor reset | the cursor becomes hidden again (`+0x2C44 = +0x2C48 = -1`) on a mouse press (`0x0073F140`) and at game over (`0x007602C0`), so Space pauses again after those | static |
| Can pause (`+0x1AC`) | Board `0x0073C220` = time limit != 0: Lightning yes; Classic, Zen, QuestBoard base, Butterflies, Poker, MoveLimit, Filler, NoLose no. Always yes: TimeLimit and Dig (`0x006C4A20`), Inferno/Ice Storm (`0x006C50B0`), Balance (`0x006C53B0`), RealTimeBomb (`0x006C4AB0`). TimeBomb: byte `+0x3262` (`0x006C4A90`) | static |
| IsGamePaused (`+0x19C`) | `0x0073C260`: (user pause **or app lost focus** (`gApp+0x63E == 0`) **or** dialog 0x15 (in-game Options) **or** 0x16 (reset confirmation)) and can pause and `+0x1410 == 0` | static (hud.md, refined) |
| DoUpdate pause branch | same conditions plus dialog 0x28 (quest help) while some curve is at 1.0. When paused and pausable: `+0x12C8` += 0.035 per DoUpdate up to 1.0; `+0x12CC` (overlay alpha) += 0.035 if `gApp+0x39C == 0` and `+0x2B40 == 0`; **when `+0x12C8 >= 0.75`, DoUpdate returns** (frozen). Not paused: both -= 0.075 per DoUpdate to 0 | `0x0076735E-0x00767455`, constants `0x00881A18` = 0.035, `0x008810FC` = 0.75, `0x00881730` = 0.075 | static |
| Board hidden | **Yes.** vtable `+0x2BC` `0x0073BD10` = `(1 - [+0x12C8]) * vtable+0x2B8`, and the gem drawing (`0x00757DF0`) and other board layers use it as the colour alpha (`SetColor(255*alpha)`). At full pause the gems are drawn fully transparent. The model in memory is unchanged | static |
| Overlay text | DrawOverlay `0x00756CB0` draws string 147 while can-pause and `+0x12CC > 0` | static (hud.md) |
| Timing | fade-in to the freeze point: about 22 DoUpdates (~0.22 s at 100/s, during which the board still runs); fade-out about 14 ticks | arithmetic from the constants | static |
| Input while paused | mouse presses are ignored (`0x0073F140` returns if `+0x12C4` or `+0x12CC > 0.5`). **KeyDown `0x007472B0` does not check pause**: arrows/WASD still pass CanPlay (vtable `+0x188`, which has no pause test) and can queue a swap; the queue (`+0xC60`, keyed on tick `+0x38`) is only executed by DoUpdate, which is frozen, so the swap would happen on resume | static; the resume behaviour is hypothesis |
| Untimed modes | Classic and Zen never pause: Space does nothing there, the Options dialog does not hide or stop the board. Nothing in them is time-driven, so a blind player can think freely | static |
| Options / reset dialogs | Escape or the menu button opens dialog 0x15 (in-game Options) -> pauses and hides pausable boards, same as Space | static |

## 4. Lifecycle per mode

Common building blocks (static unless noted):

- **New game / resume** `Board vtable+0x21C` (base `0x0074E410`): resets
  fields; vtable `+0x210` + `0x0074E1F0` load a saved game -> byte
  `+0x1191 = 1`. Fresh game: `+0x3070 = 0`, `+0x3074 = 25`. Resumed and
  pausable: `+0x3070 = +0x3074 = 120`. Logs `GameStart` / `GameLoaded`.
- **Start announcements** (DoUpdate): once the board has settled once
  (`+0x1190`, set by IsBoardStill `0x0075FCA0`) and the game is not over:
  `+0x3070` counts down; at 110 the get-ready announcement (140) is queued.
  `+0x3074` counts down; at 0 the go announcement (141). For timed boards the
  last step waits (`+0x3074 < 2`) while a curve is non-zero (the board
  drop-in, hypothesis). So **every game shows 141; only resumed games of
  pausable modes show 140 first**, and their clock and input are blocked
  until `+0x3070` reaches 0 (about 1.2 s).
- **CanPlay** (vtable `+0x188`, board-state.md): needs `+0x1190`,
  `+0x3070 == 0`, `+0xD20 == 0`, `+0xD1C == 0`, ticks left != 0, no
  Hyperspace `+0x2890`, no blocking announcement. KeyDown also needs
  `+0xE28 == 0`.
- **No moves -> game over**: UpdateGame: IsBoardStill and `+0xD1C == 0` and
  not vtable `+0x184` (Board `0x0073C3B0` = false) and FindMove
  `0x0075A8E0` finds nothing and no level-up pending -> GameOver vtable
  `+0x284`. **Detected only when the board is still; the grid is intact and
  readable at that moment.**
- **GameOver** `0x007602C0`: only once (`+0xD8C`); hides the cursor, sets
  `+0xD8C = 1`, `+0xD1C = 1`, queues the end announcement (vtable `+0x278`:
  94 timed / 95 untimed; Classic 164; Lightning 478), calls vtable `+0x288`.
  UpdateGame then increments `+0xD1C` every tick; at >= 400 (4 s), when no
  curve is active, vtable `+0x3A4` creates the mode's EndLevelDialog (id
  0x26). The board widget stays and its grid can still be read until the
  board is removed (`gApp+0xBE8` zeroed at `0x007596CC`, hud.md).
- **Level up** (untimed level modes): vtable `+0x364` `0x0073B810`: level
  points >= target **and** the displayed level bar `+0xD68` >= 1.0 and float
  `+0x1188 == 0` -> vtable `+0x270` `0x0074E190`: sets `+0x288C` (logs
  `LevelUp`). UpdateGame: when `+0x288C`, IsBoardStill and no queued items:
  announcement 139, and a Hyperspace widget at `+0x2890` (3D
  `HyperspaceUltra` ctor `0x00726C40` or `HyperspaceFallback`
  `0x00723320`), `+0x288C = 0`.
- **HyperspaceEvent** vtable `+0x274` `0x00760AC0`, called by the
  Hyperspace widget with phases 0..5:

  | Phase | Board effect |
  |---|---|
  | 0 | starts the transition (curves, vtable `+0x290`) |
  | 1 | **the new board is built here, in place**: `0x007604D0` mirrors every piece (`col = 7-col`, `row = 7-row`, also `+0x208/+0x20C`), rewrites `grid[+0xF8]`, then randomly swaps pieces (up to 20 tries) until FindMove finds a move and there is no ready match; positions `+0x10/+0x14 = col*128,row*128`. Same piece objects, so the new grid is readable right after this phase |
  | 2 | level `+0xE04` += 1, level points `+0xE00 = 0`, `+0xE34 = 0` |
  | 3 / 4 | visual (byte `+0x2EFC`, curves; flag-1 pieces get `0x0071FE40`) |
  | 5 | removes the Hyperspace widget (`+0x2890 = 0`), autosaves (`0x00742F10`), queues the level announcement (541, level+1). Play allowed again |

  ClassicBoard `0x0073B280` adds an unlock check at phase 5 (level > 4);
  ZenBoard `0x007120E0` adds rank points (rank-up dialog flag, rare-dialogs.md).
  The phase timing (seconds) is inside the Hyperspace classes and was not
  traced; the order 0..5 is assumed to be chronological (hypothesis).

### 4.1 Classic (ClassicBoard)

| Stage | Logical state | How to observe |
|---|---|---|
| start | board drops in, settles (`+0x1190`), 141 after 25 ticks | announcement queue `+0x3040` |
| play | untimed, no pause | CanPlay |
| level up | `+0x288C` -> 139 -> Hyperspace; new grid after phase 1; `+0xE04` after phase 2; 541 at phase 5 | `+0x288C`, `+0x2890 != 0`, `+0xE04` |
| no moves | GameOver -> 164 -> `+0xD1C` counts to 400 -> ClassicEndLevelDialog (`0x0073B100`) | `+0xD8C`, `+0xD1C` |

### 4.2 Zen (ZenBoard)

Untimed, never pauses, levels as Classic (target capped at level 30).
**No game over**: ZenBoard GameOver `0x0070E4A0` picks a random cell and
calls vtable `+0x29C` `0x00749570` on it, which sets its flags to 2 and
colour to -1 (turns it into a hypercube: hypothesis from the flag value,
see board-objects.md). static.

### 4.3 Lightning (SpeedBoard)

| Stage | Logical state | Evidence |
|---|---|---|
| start | limit 60 s (`0x007140F0`); 141 | static |
| play | clock `+0x3284` (2.1); pausable; hint auto after 5 s idle (vtable `+0x1A8` = 5, Board 15; UpdateGame counts `+0x26A4`) | static |
| "level" | vtable `+0x270` `0x00714280`: stage `+0x3084` += 1, level points reset, no hyperspace | static |
| clock 0 | waits for IsBoardStill (Speed `0x007183E0`: always true while bank > 0), sets `+0xE28 = 1`, SpeedBoard::GameOver `0x00719350` | static |
| bank > 0 | new round: clock = bank seconds, multiplier +1, `+0xE28 = 0`, play continues | static |
| bank = 0: **last hurrah** | first time: byte `+0x3096 = 1`, cursor hidden, time-up announcement 480. Every special piece (flags `0x17` or `0xA1800`) gets a staggered detonation countdown `+0x230` (25 ticks apart, +150 unless `+0x30A4`). **The player cannot act**: ticks left is 0 (CanPlay false) and `+0xE28` is set. Each time the board is still again, GameOver repeats; when no specials remain, `0x00714430` and Board::GameOver; `+0xD1C` starts at 200 (end dialog 2 s later) | static |
| end | 478 / dialog SpeedEndLevelDialog (`0x00716AB0`) | static |
| display | string 482 drawn by `0x00715CB0` while curve `+0x33E0` runs (hud.md). Poll `+0x3096` for "last hurrah started" | static |

### 4.4 Diamond Mine (DigBoard, TimeLimitBoard family, DigGoal at `+0x3180`)

- Timed (limit `+0x3248`), pausable, time-up as 2.3.
- **Scroll trigger** (DigGoal::Update `0x006C2AD0`): when the dig condition
  holds and the board is still, no queued swaps, no `+0x26B8` items and no
  dialog: starts the scroll curve, calls DoTimeBonus (2.4). In endless mode
  the double scroll is chosen by `0x006AAE50` (rows 0-5 have no piece with
  flag `0x10000`), stored in `goal+0x647`.
- **Scroll steps** `0x006BE730`: while the curve runs, each time its integer
  value advances (up to `DigCountPerScroll` `goal+0x4FC`, doubled for
  mega), one **row shift** `0x006BE3E0` runs:
  - row 0 pieces leave the grid (stored in `goal+0x444`, their flags/colours
    remembered at `goal+0x4A8`, cell set to NULL);
  - for rows 1..7: `grid[row-1][col] = grid[row][col]`, piece row `+0xC` =
    row-1, `y = (row-1)*128`; the visual offset is applied separately by
    `0x006B9690` from the curve;
  - new bottom row built (`0x007415C0`, recoloured up to 25 times to avoid
    a ready match; `0x006BBB10(7)` fills row 7);
  - if no move exists afterwards, one row-0 cell is turned into a hypercube
    (vtable `+0x29C`).
  After the curve ends, the depth `goal+0x4F0 += rows`.
- So **indices change by exactly one row per step**, several steps per
  scroll; a mod should re-read the whole grid after each step (or after the
  curve ends). Input is blocked during the scroll: Dig CanPlay goal `+0x114`
  `0x006AA580` is false while the curve runs.
- Game over: time-up; DigBoard GameOver `0x00737500` copies gold `+0xE00`
  into the score, then the quest game-over path.

### 4.5 Ice Storm (InfernoBoard)

Real-time but untimed (limit 0, ticks -1), always pausable. Loss:
`0x006E0CC0` (called from Inferno's DoUpdate override `0x006E5850`) calls
GameOver when, after a full column pass, a flag on `+0x3CC8` is >= 0 and the
board is still; Inferno UpdateGame override `0x006E82A0` calls GameOver(1)
from a piece scan (pieces with `+0x20C < 0` outside row 0). Ice reaching the
top is the visible cause (hypothesis); the exact ice fields belong to
board-objects.md.

### 4.6 Butterflies, Poker, quests

- Butterflies (ButterflyBoard): untimed, **not pausable**, move based. The
  loss check (butterfly reaching the spider) was **not located**: not in
  ButterflyGoal's overrides (`goalvt.txt`), ButterflyBoard's overrides, or
  the bomb path. Game over still runs through QuestBoard GameOver
  `0x006D8E30` -> `0x007602C0`.
- Poker: `0x006CDCE0` (hand tally) decrements `+0x396C` and calls GameOver
  when it goes below 0. static.
- Move limit: MoveLimitBoard Update `0x006E00E0`: GameOver(1) when the board
  is still and moves left `+0x324C == 0`. static. Filler: GameOver via
  `0x006DC770`. Time bombs: a piece with flags `0x60`/`0x100` whose counter
  `+0x244` is 0 on a still board -> vtable `+0x28C` `0x00755E50` -> GameOver
  (UpdateGame); counter decrement `0x0073C710`. static.
- **Quest success**: QuestBoard `+0x270` `0x006CA350`: goal met
  (`+0x3185`), not endless: `+0xD8C = 1`, countdown `+0xD20 = [+0x318C]`.
  QuestBoard::Update `0x006DD8A0` decrements `+0xD20`; at
  `[+0x3190] - 2` (default 200) it queues 417; at 0, vtable `+0x3A4`
  `0x006DED60` ends (quest menu / endless handling). While `+0xD20 != 0`
  the clock is frozen and input refused.
- **Quest failure** = normal GameOver (`+0xD1C`); at `+0xD1C == 300`
  QuestBoard::Update starts the return transition (`0x006DD760`).

### 4.7 Saved game resume

Bej3App::DoSavedGameCheck `0x0076CDE0` asks (strings 75-79). On resume the
board loads (4, "New game / resume"); pausable modes then show 140/141 with
the clock frozen for 120 ticks. The game autosaves at each level-up
(phase 5, `0x00742F10`) and via the button-3 handler `0x00742E60`
(`0x00741710`). Exactly when else it saves was not traced.

## 5. Game speed and extra time (facts only)

- No option in the in-game Options dialog concerns time or speed (labels
  312-321, 562: volume, resolution, 3D, fullscreen, cursors, background).
  Zen's "speed" (517) is the mantra speed (zen-options.md).
- Command line (Bej3App parser `0x0076F9D0`): `-zen`, `-classic`, `-speed`
  set `gApp+0x9DD/+0x9DE/+0x9DF`, and MainMenu (`0x006A45B3`) then starts
  Zen (`0x00773100`), Classic (`0x007737D0`) or Lightning (`0x00773440`)
  directly. `-quest` is accepted but sets nothing here. Others: `-600`,
  `-768`, `-1200`, `-wide`, `-testbkg`, `-webroot`, framework `-crash`,
  `-screensaver`, `-changedir`, `-nod3d9`, `-version`, `-minidump`,
  `-ultradump`. **None changes speed or time.** static, untested.
- Framework debug keys (Ctrl+Alt+D, then F2/F3/F8...) change rendering
  only (screen-names-and-keys.md).
- The only untimed play is Classic and Zen (and the move-based quests,
  Butterflies, Poker); Ice Storm is real time but pausable. Timed modes have
  no built-in extra-time setting; pause stops time and hides the board.

## 6. Focus loss

| Item | Fact | Evidence | Level |
|---|---|---|---|
| Fields | `gApp+0x63A` active, `+0x63B` minimized, `+0x63E` has focus = active && !minimized | `0x005BF7B0`; `+0x63A` written from the window procedure (`0x005B7426`, wParam != 0) and `0x005BAA0B` (foreground check); `+0x63B` from the size message (`0x005B9893`, wParam == 1) | static (message identities hypothesis: activate / size) |
| On change | gained: app vtable `+0x138` (Bej3App `0x0076AC80` restores sound channel volumes 2, 3, 4); lost: `+0x13C` (`0x0076AC20` sets them to 0), `ReleaseCapture`, WidgetManager reset; `+0x479` set also mutes all sound | `0x005BF7B0` | static |
| Board | **timed/pausable boards auto-pause and hide** while the window lacks focus (IsGamePaused and DoUpdate test `+0x63E`). They resume, with the fade-out, when focus returns; the user-pause flag is not set, so no key is needed | `0x0073C260`, `0x00767090` | static |
| Untimed boards | unaffected logically | same | static |
| Framework | when minimized, the smooth-update call is skipped (`0x005B3E3C`) and drawing is skipped; when inactive, redraws are throttled to about once per 1000 ms (`0x005B5131`) | static; whether logic updates continue while minimized is not established |

For the mod: the screen reader taking focus does not steal window focus, but
any Alt+Tab or a speech dialog in another window will pause timed boards.

## 7. How a mod can read the flow (design notes)

Poll once per tick (all fields are written only on the game thread):

- Mode: RTTI class (hud.md). Pausable: call vtable `+0x1AC`. Timed: `+0x1A0`.
- Time left: vtable `+0x2EC` (seconds = `(t+99)/100`); "time low":
  vtable `+0x368`.
- Paused: vtable `+0x19C`; user pause `+0x12C4`; fade `+0x12C8`
  (board invisible near 1.0); frozen when >= 0.75.
- Start: `+0x1190` (settled once), `+0x3070` (get-ready), announcements.
- Level-up: `+0x288C` (pending), `+0x2890` (Hyperspace active; board
  rearranged after phase 1; read the grid again when `+0x2890` returns to 0),
  `+0xE04`. Hook point if needed: HyperspaceEvent vtable `+0x274`
  (thiscall, ECX = board, arg = phase).
- Game over: `+0xD8C` (over), `+0xD1C` (ticks since), `+0xE28` (time-up),
  Lightning `+0x3096` (last hurrah) and `+0x30AC` (banked seconds).
- Quest: `+0xD20` (complete countdown), `+0x3185` (goal met, hypothesis).
- Diamond Mine: scroll in progress = DigGoal CanPlay false
  (`goal+0x114`); depth `goal+0x4F0`.
- Stable reading points: IsBoardStill (vtable `+0x2E8`), after Hyperspace
  removal, after each Dig row shift.

## 8. Uncertainties

- Dialog 0x29 (DoUpdate early return) is unidentified.
- `+0x1188`, `+0x2880`, `+0x2B40`, `gApp+0x39C`, `gApp+0x380` (DoUpdate
  awards branch) meanings unknown.
- Hyperspace phase timing and order are assumed from the switch.
- Butterflies' loss condition not located; Ice Storm described only at the
  call level.
- Keyboard swaps during pause (queued, executed on resume) are inferred,
  not observed.
- The replay interpretation of `+0x1410/+0x1411/+0x1412/+0x1414` follows
  board-state.md and the "replay" button image name; static facts are only
  the speed targets.
- Framework update behaviour while minimized/inactive is not established.

## 9. Runtime checklist (throwaway profile, read-only memory)

1. Classic: log `+0x38`, `+0xE38` over 1 s of wall time (expect ~100 each);
   `[0x008D2DA8]` = 1.0; `+0xF0` in [0,1).
2. Lightning: at start `+0x2EC` = 6000 and stays 6000 for ~2.5 s; drop rate
   ~100/s, slower below 600; Space before arrows toggles `+0x12C4`; watch
   `+0x12C8` rise by 0.035/tick to 1.0 and `+0xE38` stop; confirm visually
   (sighted helper or screenshot) that gems vanish while paused.
3. Lightning: Alt+Tab away -> `gApp+0x63E = 0`, `+0x12C8` rises, clock
   stops; return -> resumes without a key. Escape (Options) does the same.
4. Lightning: collect a time gem -> `+0x30AC` rises, clock unchanged; at 0 a
   new round with `+0xE0C` += 1; without bank -> `+0x3096 = 1`, `+0xE28 = 1`,
   specials detonate one by one, then `+0xD8C`, `+0xD1C` from 200.
5. Arrow keys during Lightning pause: does a swap queue (`+0xC60`) and run
   on resume?
6. Classic level-up: `+0x288C` -> `+0x2890` non-zero; snapshot the grid each
   tick: one big change (mirror + swaps) then stable; `+0xE04` after; 541.
7. Classic no moves: GameOver while still; grid readable; dialog 0x26 ~4 s
   later.
8. Zen with no moves: a random cell becomes flag 2, colour -1.
9. Diamond Mine: during a scroll, grid rows shift one at a time; depth
   `goal+0x4F0` increases; `+0x3248`/`+0x2EC` jump at scroll start.
10. Resume a saved Lightning/Diamond Mine game: 140 then 141; `+0x3070`
    from 120 to 0 with the clock frozen.
11. Quest completion: `+0xD20` counts down, 417 near the start.
12. Launch with `-classic` / `-zen` / `-speed` only on the test profile:
    mode starts directly (optional).
