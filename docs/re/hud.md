# In-game HUD, announcements and board buttons

Scope: the UI around the gem grid (score, level/progress, multiplier,
timer, mode counters, pause/game-over/level-complete state, hint, the
board's own buttons, Announcement texts). Gem-grid navigation is covered
elsewhere.

Date: 2026-10-30. Source: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`,
the exe in `private/ghidra/input/`). Method: static analysis only - Ghidra
headless decompilation (`private/ghidra-hud/`, outputs `*.c` there) plus a
capstone linear sweep of `.text`. Nothing here has been checked at runtime
yet; every item below is **static** unless marked **community** or
**hypothesis**. Constants live in `src/mod/board_layout.h`; the reader is
`src/mod/hud.{h,cpp}` (compile-checked with `tools/compile-one.bat`, not
linked or run).

## gApp -> Board, mode detection

- `Board* = [gApp + 0xBE8]`. Bej3App code (e.g. `0x007733AC`,
  `0x0077353E`, `0x00774564`) stores each new board there, calls its
  `Resize` (vtable+0xA0) and `WidgetManager(gApp+0x360)->AddWidget`. It is
  removed and zeroed at `0x007596CC` and zeroed in the Bej3App ctor
  (`0x0076DD30`). gApp is `[0x008E1730]` (= `[0x008E170C]`, set by
  `new Bej3App` at `0x00791C2D`).
- Community **0xBE0** (redstone59 offsets.json, HyperNervie autosplitter,
  bognarit80 PlusExtender) is for other exe builds: their gApp global and
  vtables differ, and their Board fields are shifted (e.g. their score
  0xD20/0xD28 vs our 0xD24). Do not use 0xBE0 here. FatBrother1 deck-bot
  (same gApp `0x008E1730`) uses 0xBE8 and score 0xD24, which match; its
  "level" 0xE0C is actually the multiplier in this build.
- The board is a normal widget (child of the WidgetManager), so mode is the
  RTTI class (`ClassNameOf`): ClassicBoard = Classic, ZenBoard = Zen,
  SpeedBoard = Lightning, PokerBoard = Poker, ButterflyBoard = Butterflies,
  InfernoBoard = Ice Storm (its strings are the ice combos), DigBoard =
  Diamond Mine (derives from TimeLimitBoard), and the other QuestBoard
  subclasses (Balance, MoveLimit, TimeLimit, Filler, NoLose, BadMoveLimit,
  TimeBomb, RealTimeBomb, QuestBoard) are quests. Poker/Butterfly/Inferno/Dig
  also host quests; byte `+0x3184` distinguishes endless mode
  (**hypothesis**, from getters that return no target when it is set).
  Hierarchies checked from RTTI (`IsKindOf(b, "QuestBoard")` works).

## HUD fields (Board base, all modes)

| Field | Offset | Evidence |
|---|---|---|
| score | `+0xD24` int | AddPoints `0x0074A311` adds points (cap 0x7FFFFFFF); reset `0x0074AAE5`; community deck-bot agrees |
| displayed score | `+0xD64` int | Board::DrawScore `0x00746F20` formats it (animates toward score) |
| level (0-based) | `+0xE04` int | HyperspaceEvent `0x00760F8A` formats the level announcement (string 541) with value+1 |
| level points | `+0xE00` int | AddPoints `0x0074A2E6`; getter vtable+0x26C `0x0073BF60` |
| multiplier | `+0xE0C` int | DrawPointMultiplier `0x00752600` formats it as a multiplier; reset to 1 `0x0074AADB`; Ice Storm multiplier text (string 561) `0x006CC2E7` |
| game over | `+0xD8C` byte | set in Board::GameOver `0x007602C0` |
| game-over count | `+0xD1C` int | set to 1 in GameOver, 0 while playing |
| level complete pending | `+0x288C` byte | UpdateGame (~`0x0076976B`) creates the level-complete announcement (string 139) when set and the board is still |
| user pause | `+0x12C4` byte | KeyChar `0x0073B670` toggles on space (timed boards only) |
| pause overlay alpha (string 147) | `+0x12CC` float | DrawOverlay `0x00756CB0` |
| hint cooldown | `+0x12C8` float | ButtonDepress case 0 requires 0 |
| "score of target" flag | `+0x269C` byte | DrawScore |
| quest description | `+0x2680` wstring | QuestBoard init `0x006DEBC8` (`Description` param); DrawBottomWidget `0x006C9D40` |
| update counter | `+0xE38` int | base timer, 100 per second (**hypothesis** from `(t+99)/100`) |
| stats | `+0xC84 + 4*id`, 0x26 ints | AddStat `0x0073D170`, MaxStat `0x0073CFD0` |

Getters (vtable byte offsets) are evaluated in `hud.cpp` by replicating the
reviewed implementation for each target address; anything else is unknown:

- `+0x268` level target: Board/Classic `0x0073BF70` = 2500 + 750*level;
  Zen `0x0070E450` = same with level capped at 30; Speed `0x007140D0` =
  `[+0x30FC]*[+0x3084]+[+0x30F8]`; Balance `0x006C53A0` = 2*`[+0x3248]`;
  Poker `0x006C4D70` = endless ? 0 : `[+0x3968]`; Inferno `0x006C4F70` =
  endless ? float`[+0x3270]`*10000 : goal; Quest family `0x006C4BD0` =
  `QuestGoal(+0x3180)->vtable+0x58`. All 19 goal getters are trivial reads
  (most `[goal+0x110]`; Clear `0x006AA320` base+level*`[+0x114]` capped by
  `[+0x118]`; Hidden/HiddenGrid `[+0x118]`; HiddenObject `[+0x1F4C]`; Dig
  `[+0x3BC]`; Detonator 20; Butterfly 0 when endless; base 0).
- `+0x26C` level points: Board `0x0073BF60` = `[+0xE00]`; Speed `0x007140B0`
  = `[+0xE00]` - (int)double`[+0x30D0]`; Inferno `0x006C5D00` = endless ?
  float`[+0x326C]`*10000 : `[+0xE00]`.
- `+0x1A0` time limit (s): Board `0x0073C250` = 0; Speed `0x007140F0` = 60;
  Dig/TimeLimit `0x006C4A30` = `[+0x3248]`.
- `+0x2EC` ticks left (1/100 s, -1 untimed), used by DrawTimer `0x007516A0`
  as `(t+99)/100` -> m:ss: Board `0x0073BB80` = `[+0x1308]` if byte
  `[+0x12F8]`, else clamp(limit*100 - max(0, `[+0xE38]`-250), 0, limit*100);
  Speed `0x00713FA0` = same clamp on float `[+0x3284]`, then min with
  `[+0x30F4]` (x87 rounding replicated as truncation - **verify**); Dig
  `0x00736B50` = min(base, `[+0x3274]`) when nonzero.

Mode counters:

- Diamond Mine gold = level points `+0xE00` (DigGoal draw `0x006BDEDF` reads
  `goal->board(+0x104)->+0xE00` for the gold amount text). Depth: DigGoal::DrawOverlay
  `0x006BD55D` draws (int)(double`[goal+0x4F0]` + curve(`goal+0x518`) *
  step), step = `[goal+0x4FC]` doubled when byte `[goal+0x647]`. hud.cpp
  reports only the double base truncated (**partial**; the displayed metres
  may differ - PlusExtender multiplies the base by 10 in its build).
- Butterflies freed = stat 0x1C (`+0xCF4`): ButterflyGoal `0x006AB2F0` adds
  it when a piece with flag 0x80 is cleared.
- Moves left (MoveLimitBoard) = `+0x324C` (DrawBottomFrame `0x006CBA10`).
- Lightning time / end-of-round bonus phase: time via `+0x2EC`. The bonus
  phase text (string 482) is drawn by SpeedBoard::DrawFrame2 (~`0x00716572`) guarded by a CurvedVal at
  `+0x33E0`; no plain flag found. Not implemented.
- Poker: target (hands) via `+0x268`. Hand counts and skull state not
  verified: the autosplitter's 0x3964/0x39D8.. are from another build and
  PokerBoard field accesses here are mostly indexed. **Open.**
- Ice Storm columns: community 0xCD0 (other build) would be stat 0x14
  (`+0xCD4`) here, but AddStat(0x14) is called from Board code
  (`0x0075A56E`), not InfernoBoard. **Open, not read.**
- Zen: level/progress as Classic (target capped at level 30). No extra
  counters read.

## Pause, menu, game over, level complete

- IsGamePaused, vtable+0x19C `0x0073C260`: (user pause `+0x12C4` or (byte
  `gApp+0x63E` and gApp has dialog 21 or 22)) and the board is timed and
  not suspended (`+0x1410`). Dialog 21 is the in-game OptionsDialog opened
  by the menu button; dialog 22 is the reset confirmation (string 155).
  Untimed boards never pause.
- Game over: Board::GameOver `0x007602C0` sets `+0xD8C`=1, `+0xD1C`=1, calls
  GameOverAnnounce (vtable+0x278) which queues the time-up announcement
  (94, timed) or the game-over announcement (95); ClassicBoard overrides it
  with its out-of-moves announcement (164, `0x0073AFE0`). The
  end-level dialogs (EndLevelDialog and subclasses) derive from
  Bej3Dialog -> Dialog, so the existing navigator layer logic finds them.
- Level complete: `+0x288C` pending -> level-complete announcement (139);
  the new level shows the level announcement (541, HyperspaceEvent);
  `+0xE04` increments.

## Board buttons

Created in `0x00745240` as `Bej3Button` (vtable `0x0086498C`, ButtonWidget
subclass) children of the board, listener = board+0xBC, so the navigator
already lists them (image-only, hence "unlabeled"). `HudBoardButtonRole`
maps a widget pointer to its role:

| id | field | handler (ButtonDepress `0x0074BC40`) | exists when `[+0xC1C]` |
|---|---|---|---|
| 0 | `+0x286C` | ShowHint (vtable+0x328), only if cooldown `+0x12C8` == 0 | 0, 1, 2 |
| 1 | `+0x2870` | opens OptionsDialog as dialog 21 (in-game menu) | 0, 1, 2 |
| 2 | `+0x2878` | reset confirmation (155), dialog 22 (only if `+0xD1C` == 0) | 1, 2, 3 |
| 3 | `+0x2874` | `0x00742E60`: sets `+0x1410`/`+0x1414`, plays a sound (purpose **unknown**) | 0, 2 |

`[+0xC1C]`: 0 default (Classic, Zen), 1 SpeedBoard and endless Dig, 3
QuestBoard. All handlers first require vtable+0x33C (`+0xD20` and `+0xD1C`
both <= 0) and `+0x1410` == 0. ZenOptionsDialog is a plain Widget, not a
Dialog, so the navigator will not treat it as a layer.

## Hint

ShowHint `0x0075AEB0`: if from the button, `+0x26A0` = 250; `+0x26A4` = 0;
finds a move (`0x0075AE10` -> `0x0075A8E0`), takes the piece at
`grid[col + 8*row]` (a hypercube partner if present) and calls
`0x00748480` with ECX = that Piece*, args (Board*, bool). That function
starts CurvedVals on the piece (`+0x440`, `+0x280`, `+0x2F0`). The hint
location is **not stored** in the board. Options: (a) hook `0x00748480`
(thiscall, ECX = piece) and read the piece's grid position; (b) after
pressing the hint button, scan the grid for the piece whose `+0x440` curve
became active (needs CurvedVal layout, not done); (c) compute a hint
in the mod from the grid model. Recommend (a) or (c).

## Announcements

- Class `Announcement` (vtable `0x00863BD8`, not a widget), size 0x190,
  ctor `0x0078C7F0` (ESI = this, stack: std::wstring text by value, Board*).
  Text is `std::wstring` at `+0x10`; `+0x184` Board*; bytes `+0x180/+0x181`
  (hypothesis: darken / block play).
- If Board* is non-null the ctor push_backs into `std::vector<Announcement*>`
  at board+0x3034 (begin `+0x3040`, end `+0x3044`). Update `0x0078CD80`
  erases `begin()` when done; DrawOverlay draws only the front. So the front
  element is what is on screen.
- Creation sites (string id): Board GameOverAnnounce time up 94 / game over
  95; ClassicBoard out of moves 164; SpeedBoard time up 478, 480; Board
  get ready 140, start 141, level complete 139, level number 541;
  QuestBoard quest completed 417; PokerBoard safe hand 384; QuestMenu
  quest mode complete 421 (Board* = 0, stored at QuestMenu+0x178, **not**
  in a board queue).
- Spoken praise words are voice samples, not text.
- Design implemented: `HudNewAnnouncements()` polls each tick and returns
  the front text when the front pointer or text changes (empty queue resets
  the tracker). Text is the game's runtime string (localised), cleaned of
  `^RRGGBB^` codes and line breaks. Alternative if polling misses fast
  ones: hook the ctor `0x0078C7F0` (custom convention, ESI = this) - not
  recommended.

## Lead: integration

1. Add `hud.cpp` to `tools/build.bat`.
2. In `NavigatorTick`: speak each string from `HudNewAnnouncements()`
   (normal priority; game over / time up (95, 94, 478, 480) should interrupt).
3. Add a key (lead's binding choice) to speak `ReadHud` values: score,
   level and "points of target", multiplier, time left `(ticks+99)/100` as
   m:ss, mode counters. Only speak known fields.
4. Name board buttons via `HudBoardButtonRole(widget)`.
5. Add `HudDescribe` to the F12 dump.
6. Update `docs/index.md` (link this file) and `docs/reverse-engineering.md`
   (point its "Board (community, unverified)" section here).

## Lead: runtime verification checklist

- `[gApp+0xBE8]` non-zero with class ClassicBoard etc. in each mode; zero
  (or not visible) on the main menu.
- Score/displayed score/level/multiplier match the screen in Classic;
  points/target match the level bar (Classic level 1 target 2500).
- Lightning: ticksLeft/100 matches the drawn timer (Speed getter replication
  is the least certain item); target/points sensible.
- Poker/Butterflies/Ice Storm/Diamond Mine: endless flag `+0x3184` = 1 from
  the play menu, 0 in quests; butterflies freed counts up; Diamond Mine gold
  matches the drawn gold amount; compare digDepth with the drawn depth.
- Announcements: 140 / 141 at Lightning start, 139 and 541 in Classic,
  94 / 95 / 164 at the end.
- Pause: space in Lightning toggles `+0x12C4` and the pause overlay (147);
  menu button opens OptionsDialog (navigator layer).
- Board buttons: ids 0..3 found as Bej3Button children of the board.

## Open questions

- Poker hand counts / skulls, Ice Storm columns, Lightning end-of-round
  bonus phase (482) and exact Diamond Mine depth (needs CurvedVal value layout, or the
  text-draw hook).
- Button id 3 purpose; meaning of `+0x1410`, `+0xD20`, `+0x3234` (quest id).
- Hint position (hook design above).

> Correction (2026-10-30, docs/re/board-flow.md): `+0x12C8` is the pause fade amount (0..1), not a hint cooldown; the hint cooldown is `+0x26A0` (docs/re/board-input.md). `+0x3070`/`+0x3074` are the start countdowns.
