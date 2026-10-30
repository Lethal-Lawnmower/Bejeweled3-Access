# Board objects: special gems and mode/quest objects

Scope: every kind of thing that can sit on the board in each mode, how it is
represented in memory, what it does and how a mod could read it. Agent 4 of
the board research (see `board-research-brief.md`). Grid model, settling,
events, input and flow are other agents' topics; they are referenced, not
repeated.

Date: 2026-10-30. Source: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`).
Method: static analysis only. Full read-only decompile of 0x401000-0x860000
(`private/ghidra-b4/all.c`, made with `tools/ghidra/BObjectsDumpRange.java`
via `private/ghidra-b4/run-range.sh`), capstone disassembly and RTTI vtable
maps (scratch helpers in `private/ghidra-b4/py/`: `vtmap.py` function ->
class/slot, `vtdiff.py` overrides, `strids.py` string ids per function,
`opscan.py` instruction grep, `dis.py`). Function names come from the
string-table comments in `private/pak/properties/default.xml` (each string
id names the function that loads it) and from RTTI vtables. Nothing here has
been checked at runtime. Labels: **static** (decompilation/disassembly),
**community** (third-party claim), **hypothesis** (inference not yet proven).
No game text is reproduced; string ids refer to `default.xml`.

## Piece (gem) record

Pieces have no vtable. `Piece* = [Board + 0xF8 + 4*(col + 8*row)]`
(board-state topic). Fields used by board objects:

| Field | Type | Meaning | Evidence |
|---|---|---|---|
| `+0x04` | int | piece id (unique; key of Dig and hidden-object maps) | static: DigGoal::GetTooltipText `0x006AC360` looks up `piece+4`; WallBlast/HiddenObject use it |
| `+0x08` | int | column 0-7 | static: WallBlastGoal drop `0x006AB080` accepts `+8` in 1..6; TimeBomb drop `0x006C60B0` tests `+8` in 3..4 |
| `+0x0C` | int | row 0-7 | static: TimeBomb drop tests `+0xC < 5`; DigGoal `0x006AD0D0` bounds `+0xC` by its row vector |
| `+0x10`, `+0x14` | float | draw position (pixels, animates) | static: Flamify `0x00745CA0` places effects at board origin + these |
| `+0x188` | int | match/creation id (compared with Poker "flame/star created" ids) | static: PokerBoard Flamify/Starify overrides store it |
| `+0x21C` | int | colour before becoming a hypercube | static: Hypercubeify `0x00749570` copies `+0x220` here |
| `+0x220` | int | colour, 0 red, 1 white, 2 green, 3 yellow, 4 purple, 5 orange, 6 blue, -1 none (hypercube, some Dig tiles) | static: Hypercubeify sets -1; GoldYellowGoal tests colour 3 = yellow; Balance tests 0 and 6; community FatBrother1/PlusExtender colour enum |
| `+0x228` | uint | **flags** (table below) | static: tooltip `0x007476E0`, setters below; community PlusExtender `+552` |
| `+0x22C` | uint | "disallowed flags" mask: a flag bit set here blocks setting that flag | static: every setter tests `+0x22C` first (e.g. `0x007494D0`, `0x0073CCC0`, `0x006AB080`) |
| `+0x238` | uint | copy of the flags a flame/blast gem had when it was consumed in a match | static: `0x0075AF60` saves `+0x228` to `+0x238` and clears `+0x228` |
| `+0x240` | float | hidden-gem fade (Hidden goal, 0..1) | static: HiddenGoal `0x006AA040` adds a per-move step, capped at 1.0 |
| `+0x244` | int | **counter**: bomb moves/seconds left, time-gem seconds, butterfly countdown | static: tooltip reads it for the time-gem seconds (`0x00747F59`); bomb setter `0x0073CCC0`; community `+580` |
| `+0x254` | int | butterfly move-countdown reload value | static: butterfly spawn `0x006D7C70` |
| `+0x26C`, `+0x270` | float | counter "spin" animation (set when a counter piece is ticked) | static: `0x0073C710`; community mSpinFrame/mSpinSpeed |

Helper functions (inlined at most call sites, so readers should test the
bits directly): IsFlag `0x0071FDC0` (EAX piece, CL bit index), CanSetFlag
`0x0071FDE0`, SetFlag `0x0071FE00` (EDX piece, CL bit, stack bool),
ClearFlags `0x0071FD50`, IsDetonatorOrScrambler `0x0071FF40` (`& 0x1800`).

### Flag bits (verified in this build)

| Bit | Community name | This build: what sets it, what uses it | Status |
|---|---|---|---|
| `0x1` | FLAME | Board::Flamify vtable+0x298 `0x00745CA0` (sets 1, clears 0x2E0); match of exactly 4 in a line (`0x0075AF60`); tooltip id 119/120 | static |
| `0x1 + 0x4` | (supernova) | match of 6+ in a line calls Starify then Flamify (`0x0075AF60`); tooltip shows Supernova (117/118) when both bits are set | static. Note: PlusExtender's sandbox "Hypernova" is FLAME+HYPERCUBE; that is a mod invention, not this |
| `0x2` | HYPERCUBE | Board::Hypercubeify vtable+0x29C `0x00749570` (flags = 2, colour -> `+0x21C`, colour = -1); 5 in a line; Zen no-moves fallback; tooltip 123/124 | static |
| `0x4` | STAR | Board::Starify vtable+0x2A0 `0x007494D0` (sets 4, clears 0x2E0, needs colour != -1); intersecting matches; tooltip 121/122 | static |
| `0x8` | RESERVED | no use found | static (absence) |
| `0x10` | MULTIPLIER | dropped by SpeedBoard vtable+0x2DC `0x00714DD0`; when tallied Board vtable+0x238 `0x0073D2B0` calls vtable+0x294 (multiplier up); tooltip 125/126 | static |
| `0x20` | MBOMB | bomb counting moves: `0x0073CCC0` with bool=false sets flags 0x220 (COUNTER+MBOMB) | static |
| `0x40` | TBOMB | bomb counting time: same setter with bool=true sets 0x240; RealTimeBomb drop `0x006C6200` sets 0x240 directly | static |
| `0x60` (either bomb) | | tooltip 137/138 (counter must not reach zero); ClearBombsGoal counts cleared `& 0x60`; Board `0x0073D4F0` treats a bomb with counter 0 as not settled | static |
| `0x80` | BUTTERFLY | Butterflyify `0x007492C0` (clears flags, sets 0x80, keeps colour); ButterflyBoard spawn; ButterflyGoal counts cleared; tooltip 133/134 | static |
| `0x100` | DOOM | ticked by `0x0073C790` (vtable+0x350); swap logic `0x0075FAE0` tests it; **no creator found** | static use, creator not found (hypothesis: unused in shipped modes) |
| `0x200` | COUNTER | marks pieces whose `+0x244` counts down; DecrementCounter vtable+0x350 `0x0073C710` calls GameOver (vtable+0x284) when it reaches 0 | static |
| `0x400` | COIN | collected in SpeedBoard::GameOver `0x00719350` and Board `0x0074B9E0`; tooltip 131/132 (mentions boosts); **no creator found** | static use; hypothesis: leftover, unused |
| `0x800` | DETONATOR | clickable (KeyDown `0x007472B0` and IsDetonatorOrScrambler); tooltip 127/128; **no creator found** | static use; creator not found |
| `0x1000` | SCRAMBLER | clickable; created by the Dig grid parser for grid letter `S` (`0x006BBB10`); tooltip 129/130 | static |
| `0x2000` | MYSTERY | not swappable (`0x00755A70`, `0x00755AD0` refuse it); resolved at random into a special by `0x00744830` (called from UpdateGame); **no creator found** | static use; creator not found |
| `0x4000` | SKULL | match logic `0x00761850` treats it like a plain gem; nothing else | static (minor); Poker skulls are **not** pieces (see Poker) |
| `0x8000` | INFERNOSWAP | set by `0x0075AF60` on the highest-ranked piece of a match while Board float `+0x1188` > 0, cleared in UpdateGame `0x00767E20` | static; meaning hypothesis (speed-bonus explosion marker) |
| `0x10000` | GROUND | Diamond Mine dirt/rock/treasure tiles (DigGoal parser `0x006BBB10`, `0x006B3A50`); tiles are pieces in the grid | static |
| `0x20000` | TIMEBONUS | Lightning time gem: SpeedBoard vtable+0x2E4 `0x00716C60` sets flags = 0x20000 and counter 5 or 10 | static |
| `0x40000` | DECAYING | SpeedBoard Update `0x007173F0`: every 150 ticks a decaying gem's counter drops; at < 1 both 0x20000 and 0x40000 are cleared; **no creator found** | static use; creator not found |
| `0x80000` | BLAST | set by SpeedBoard vtable+0x2DC on one piece (when byte `+0x3094` is 0); kept like flame when consumed (`0x0075AF60`) | static; meaning hypothesis |

Effect dispatch `0x00749220` (piece in EAX) picks the first of 0x10, 0x4,
0x80, 0x2, (skip 0x1800), 0x80000, 0x20000 to start its visual effect, so
the flags are combinable but one dominates visually.

Reading rule for a mod: read `+0x220` and `+0x228` of each grid piece.
Name priority as the game's tooltip does (static, `0x007476E0`): flame
(supernova if 0x4 also set), star, hypercube, multiplier, detonator,
scrambler, coin, butterfly, time gem, bomb. Then add mode objects (ground,
counter). Flags change in the same frame the special is created, while the
match animation still plays; the piece pointer of a cleared gem stays in the
grid until settling (board-state topic). Speak specials when the board is
settled, or speak "new flame gem" from the setter hooks (thiscall,
ECX = board, stack = piece): Flamify `0x00745CA0`, Hypercubeify
`0x00749570`, Starify `0x007494D0`, Butterflyify `0x007492C0`.

## Classic (ClassicBoard) and Zen (ZenBoard)

| Object | Representation | Fields | Behaviour | Evidence |
|---|---|---|---|---|
| 7 gem colours | `+0x220` 0..6, flags 0 | | match 3+ | static/community |
| Flame gem | flag 0x1 | | from 4 in a line; explodes when matched | static `0x0075AF60` |
| Star gem | 0x4 | | from intersecting (L/T) matches | static (vtable+0x174 chooses star vs flame there; base behaviour not decoded) |
| Hypercube | 0x2, colour -1, old colour `+0x21C` | | from 5 in a line; swap with a gem to clear that colour | static |
| Supernova | 0x1 + 0x4 | | from 6+ in a line | static |
| Zen no-moves | | | ZenBoard::GameOver override `0x0070E4A0` turns a random piece into a hypercube instead of ending | static |

Zen has no extra board objects; its overrides are scoring, options and
drawing (ZenBoard vtable diff). Classic out-of-moves ends the game
(announcement 164, hud.md).

## Lightning (SpeedBoard)

| Object | Representation | Fields | Behaviour | Evidence |
|---|---|---|---|---|
| Time gem | flags 0x20000 | `+0x244` = seconds (5 or 10) | dropped on a plain gem by vtable+0x2E4 `0x00716C60` after the first second; when matched PieceTallied `0x00715360` adds the seconds to `+0x30AC` and `+0x30B0` (bonus time pool) and shows string 479 | static |
| Decaying time gem | 0x20000 + 0x40000 | `+0x244` falls every 1.5 s | becomes a normal gem at 0 | static (creator not found) |
| Multiplier gem | 0x10 | | dropped by vtable+0x2DC `0x00714DD0`; matching raises the multiplier `+0xE0C` | static |
| Blast gem | 0x80000 | | dropped by the same function (condition byte `+0x3094`) | static, meaning hypothesis |
| End of round | | | SpeedBoard::GameOver `0x00719350` scans pieces with `& 0x17` or `& 0xA1800` (specials, time, coins) and detonates/collects them | static |

Player needs: where time gems are and their seconds; multiplier gems.

## Poker (PokerBoard)

No poker objects are pieces; the cards are a hand record in the board.

| Object | Representation | Evidence |
|---|---|---|
| Cards in the current hand | count `[Board+0x3720]` (0..5). Card i at `Board + 0x3248 + i*0xF8`: `+0x0` colour (0..6), `+0x4` second colour (when two pieces were added in one move), `+0x8` card kind: 0 plain, 1 hypercube (wild; colour taken from piece `+0x21C`), 2 made with a flame gem, 3 made with a star gem; `+0xF0` effect pointer | static: PokerBoard AddCard `0x006CF460` (writes `[0xC92 + n*0x3E]`, `[0xC94 + n*0x3E]`, then increments `[0xDC8]`); flame/star ids from PokerBoard Flamify `0x006CAB70` (`+0x3AA8`) and Starify `0x006CE1D0` (`+0x3AAC`) overrides |
| Card added | vtable+0x2CC `0x006CFA50` and +0x2FC `0x006CFAA0` call AddCard while the count is < 5 (one card per move; two in some hypercube cases) | static |
| Hand evaluation | `0x006C80A0` returns hand type + 1; stored at `+0x3A30`; 7 types in the order of strings 588-594 (ids 386-392 for the tooltip names) | static |
| Hand score / no score | TallyScore `0x006C7560`: last hand points `+0x3A2C`, best `+0x3980`, string 385 for no score | static |
| Skulls | not gems: hand **rows** on the score table are skulled. Rows `0..[Board+0x398C]` show a skull (DrawTopWidget `0x006D4300` draws counts only for rows above it); a completed hand whose type is not above `+0x398C` triggers `0x006CB430` (the skull coin flip, help strings 383/400) | static for the comparison; coin outcome not traced |
| Skull eliminator bar | floats `+0x3984` (filled by hand values `[+0x39C0 + 4*type]`), tooltip 568/569 | static (partial) |
| Hand counts per type | vector `+0x39A8` (begin) | static |
| Hands left | displayed as min(`+0x396C`, another value) beside string 380/567; last hand string 381 | static, partial |
| Endless | byte `+0x3184` (hud.md) | hypothesis (hud) |

Player needs: the colours of cards already in the hand and the count, which
hand types are skulled, hands left, and what each candidate match would make
(the game shows hand tooltips via UpdateTooltip `0x006C90E0`).

## Butterflies (ButterflyBoard)

| Object | Representation | Fields | Behaviour | Evidence |
|---|---|---|---|---|
| Butterfly | flag 0x80 with a colour | `+0x244` countdown, `+0x254` reload | spawns in the **bottom row** (`grid row 7`, `Board+0x1D8 + 4*col`), column 1..6, or 0/7 with SideSpawnChance `+0x3278`; match with same-colour gems to free it (ButterflyGoal `0x006AB2F0`, stat 0x1C) | static `0x006D7C70` |
| Spawn countdown | `Board+0x3258` (moves until next spawn), decremented per move by vtable+0x2FC `0x006D7F40`; reload from DropCountdown/level | | static |
| Spawn amount | float `+0x3274` accumulates `+0x3294` per spawn (SpawnCount* config) | | static |
| Spider | at the top. Its x follows the butterfly nearest the top (float `Board+0x3288`) | | static `0x006E82A0` (ButterflyBoard UpdateGame) |
| Escape / loss | any butterfly in **row 0** -> vtable+0x368 `0x006C6B50` (danger state) returns true; ButterflyBoard UpdateGame `0x006E82A0` calls GameOver (vtable+0x284) once the spider is over that butterfly | | static |
| Butterfly counter | DecrementCounter `0x0073C710` decrements `+0x244` for 0x80 pieces on the per-turn tick; the move-up code itself was not located | | static/partial |
| Quest done | when the goal is reached, every butterfly's `+0x244` is increased (stalls them) | | static `0x006D03D0` |

Player needs: each butterfly's column, row (distance to the top) and colour,
and the spawn countdown.

## Ice Storm (InfernoBoard)

Ice columns are not pieces; they are an array of 8 column records, stride
`0x348`, in the board.

| Field (column i) | Meaning | Evidence |
|---|---|---|
| float `Board + 0x3D34 + i*0x348` | ice height in rows; any column above 8.0 (top of the board) puts the board in danger | static vtable+0x368 `0x006C5C20` (constant `0x880F88` = 8.0), update `0x006E0CC0`; the actual loss rule (quest key SecondsUntilLose) was not traced |
| float `+0x3D30 + i*0x348` | previous height | static |
| float `+0x3D38 + i*0x348` | push-down accumulator (matches in that column push it down; flame/special matches push more, `+0x3288` x `+0x328C`) | static `0x006DD580`, `0x006CAF30` |
| int `+0x3D44 + i*0x348` | index of the column record mapped to slot i | static |
| byte `+0x3D6D + i*0x348` | column active (ice present) | static |
| byte `+0x3D6E + i*0x348` | column being crushed | static `0x006DB1A0` |

A vertical match on an active column crushes it (`0x006DB1A0` ->
IncrementColCleared `0x006D6D60`, strings 549-560 combo names, tutorial
110/111). Multiplier text 561. "Which column is rising": every active column
rises continuously (FireSpeed config); the danger order is by height.
Player needs: per column, active or not and its height (rows of ice), and
the highest column.

## Diamond Mine (DigBoard + DigGoal)

| Object | Representation | Evidence |
|---|---|---|
| Ground tile | piece with flag 0x10000 in the grid; details in a map keyed by piece id at `DigGoal+0x228` (8-int record) | static GetTooltipText `0x006AC360`, parser `0x006BBB10` |
| Record `[2]` = 1 block | `[0]` = strength: 1 dirt (192/193), 2 rocks (194/195), 3 stones (196/197), 4 dark rock, only specials break it (198/199, tutorial 112/113), 5 boulder (200/201), >5 boulder needing many matches (202/203). Matches adjacent to a block hit it | static |
| Record `[2]` = 2 treasure | `[0]` < 4 gold (186/187), else diamonds (188/189); value text 172/173 | static |
| Record `[2]` = 0 artifact | uncover for bonus (184/185) | static |
| Buried hypercube | block whose id is in the vector at `DigGoal+0x450` (grid letter `H`) (190/191) | static |
| Scrambler | grid letter `S` | static |
| Grid letters | `H` hypercube, `S` scrambler, `M` money, `A` artifact, `R` random, digits strength (quest.cfg comment: 9 money, 1-8 strength) | static `0x006BBB10` / config |
| Depth, gold, scrolling | see hud.md (DigGoal `+0x4F0`, gold = `+0xE00`) and the flow topic | |

Player needs: for each cell, gem or tile; tile kind and remaining strength;
treasure under it; depth.

## Quest boards and goals

QuestBoard holds the goal at `Board+0x3180` (QuestGoal*, `goal+0x104` =
Board*). Goal names are matched upper-case by the factory QuestBoard
vtable+0x21C `0x006DDC70`: CLEAR GEMS -> ClearGoal, WALL BLAST, GOLD,
DETONATOR, ARSENAL, GOLD LINES, GOLD LINES FLIP, GOLD YELLOW, CLEAR BOMBS,
SCORE, FIRESTORM, PRESERVER, HIDDEN GEMS -> HiddenGoal, HIDDEN GRID,
SURVIVAL, CATCH BUTTERFLIES -> ButterflyGoal, HIDDEN OBJECTS, DIG.
Goal **progress** is the board's level points `+0xE00` for every goal
(each goal adds 1 there and to `+0xD24`, or overwrites it); **target** is
goal vtable+0x58 (hud.md). Goal vtable+0xA0 runs per cleared piece,
+0xD8 per move.

| Goal | Objects / progress rule | Evidence |
|---|---|---|
| ClearGoal | every cleared piece +1 | static `0x006AA360` |
| ClearBombsGoal | cleared bomb (`& 0x60`) +1 | static `0x006AB280` |
| FirestormGoal | cleared flame gem +1 | static `0x006AB200` |
| ArsenalGoal | progress = number of flame gems on the board now | static `0x006AB360` / `0x0073CA40` |
| DetonatorGoal | progress = best per-move value of stat 4 (target 20) | static `0x006AB230`; stat 4 meaning not confirmed |
| PreserverGoal | progress = gems of colour `goal+0x114` on the board | static `0x006A9C50` |
| ScoreGoal, SurvivalGoal | per move: progress = score / a move counter modulo `goal+0x110` | static `0x006AA880`, `0x006AA7E0` |
| GoldGoal | 8x8 golden-square grid, int at `goal+4 + 4*(col + 8*row)` (1 = gold); a cleared piece turns its square gold | static `0x006AB820` |
| GoldYellowGoal | cleared yellow gem turns its 3x3 neighbourhood gold (same grid) | static `0x006AA140` |
| GoldLinesGoal / GoldLinesFlipGoal (Alchemy) | a matched piece (`+0x17D`) calls vtable+0x120(row, col) to gild its row/column (flip variant toggles); grid presumed same layout | static call; grid layout hypothesis |
| WallBlastGoal | wall grid at `goal+0x118`, 64 cells x 4 bytes {present, present copy, random bit, random bit}; target = wall count `goal+0x110`; every `goal+0x114` (DropThreshold) moves a special drops in a column 1..6 (10% hypercube, else flame or star) | static `0x006B0FB0`, `0x006A9980`, `0x006AB080` |
| HiddenGoal (HIDDEN GEMS) | each move raises piece `+0x240` fade toward 1.0 | static `0x006AA040` |
| HiddenGridGoal | per-square fade floats from `goal+0x11C` | static `0x006A9E70` |
| HiddenObjectGoal (Buried Treasure, Sandstorm) | target `goal+0x1F4C`; objects tracked by piece-id sets near `goal+0x1F28..0x1F44`; hypercube use reveals (`0x006B7AE0` -> `0x006B7A50`); ItemSet `goal+0x1F10`, RefillRateSec `goal+0x1F54` | static, partial |
| ButterflyGoal | see Butterflies | static |
| DigGoal | see Diamond Mine | static |

Challenge boards (quest `Challenge` key):

| Board | Objects / state | Evidence |
|---|---|---|
| TimeBombBoard / RealTimeBombBoard (Time Bomb) | bombs are pieces: COUNTER+MBOMB (move) or COUNTER+TBOMB (time); counter = BombCountdown `+0x3248`; bombs on board `+0x325C` (max `+0x3250`); next drop countdown `+0x3258` (reload `+0x324C`); a bomb reaching 0 ends the game via DecrementCounter -> GameOver | static `0x006C60B0`, `0x006C6200`, `0x006D0930`, `0x006D0970`, `0x006D8C60` |
| BalanceBoard | red (colour 0) collected `+0x3260`, blue (colour 6) `+0x325C`, each capped at Gems `+0x3248`; progress = sum, target 2 x Gems; tilt = (blue - red) / Balance `+0x324C`; scale state float `+0x3250` (sign = side) | static `0x006D06A0`, `0x006C84D0`, `0x006CEDC0` |
| FillerBoard (Avalanche) | gems waiting to drop `+0x324C` (each move adds DropPerMove `+0x3248`); danger when board count + `+0x3248` - 3 > 63 (board about to overflow) | static `0x006C52A0`, `0x006C5240` |

Board vtable+0x368 is a "danger" test (base `0x0073B7C0`: time left under
10 s, or 15 s for limits over 60 s). Subclasses override it with their own
danger rule (ice height, butterfly in row 0, Avalanche overflow), so one
poll of vtable+0x368 (thiscall, no args, returns bool) gives a mode-neutral
"in danger" signal; calling it from the mod is a hypothesis to verify.
| MoveLimitBoard | moves left `+0x324C` (hud.md) | static |
| TimeLimitBoard / DigBoard | time via hud getters; bonus 499 / penalty 500 | static |
| NoLoseBoard, BadMoveLimitBoard | no extra board objects found | static (absence) |

## Quest data caveat

`private/pak/properties/quest.cfg` (40 quests), `defaultquest.cfg`,
`secret.cfg` (endless Poker, Butterflies, Ice Storm, Diamond Mine) use these
goal and challenge names. hud.md found a runtime quest value differing from
quest.cfg, so which file the game loads is not confirmed. Per quest.cfg the
shipped quests use: Catch Butterflies, Clear Bombs, Clear Gems, Dig, Gold
Lines, Gold Lines Flip, Hidden Objects, Wall Blast, plus Balance and Poker
challenges. Arsenal, Detonator, Firestorm, Preserver, Score, Survival, Gold,
Gold Yellow, Hidden Gems and Hidden Grid exist in code but are not used by
quest.cfg (defaultquest.cfg uses Gold and Gold Yellow).

## What a player must know (design input)

- Every mode: colour of each gem; specials (flame, star, hypercube,
  supernova) with their position.
- Lightning: time gems (seconds) and multiplier gems.
- Poker: cards in hand (colours, count of 5), skulled hand types, hands left.
- Butterflies: butterfly positions and colours, rows to the top; spawn
  countdown.
- Ice Storm: per-column ice height and which columns are near the top.
- Diamond Mine: tile kind and strength per cell, treasure, depth.
- Time Bomb: bomb positions and counters (lowest first).
- Balance: red and blue counts and the tilt.
- Wall Blast / Gold / Alchemy: which cells are wall or still not gold.
- Avalanche: how full the board is and pending drops.

## Uncertainties

- Creators of DOOM, COIN, DETONATOR, MYSTERY, DECAYING not found; they may be
  unused in this build (or created through a path the scan missed, e.g. a
  computed flag write in `0x00764AB0` / `0x00761850`).
- BLAST and INFERNOSWAP meaning; Lightning bonus pool fields `+0x30AC/+0x30B0`.
- Butterfly move-up code and the exact meaning of piece `+0x20C` (-1 at the
  spider check) not traced.
- Poker hands-left formula, skull coin outcome, eliminator thresholds.
- Ice column record start (struct may begin before `+0x3D30`) and the
  slot-to-record mapping `+0x3D44`.
- Hidden objects layout; GoldLines grid layout; Detonator stat 4.
- All offsets are static reads of this exe only.

## Runtime checklist (throwaway profile, read-only memory)

1. Classic: make a 4-match; the new piece has `+0x228 == 1`; 5-match gives
   2 with `+0x220 == -1` and `+0x21C` = old colour; L-match gives 4; 6-match
   gives 5.
2. Lightning: a time gem shows `0x20000` and `+0x244` 5 or 10 matching the
   drawn number; multiplier gem `0x10`; note any `0x80000` piece and what
   it looks like.
3. Poker: after each move `[Board+0x3720]` increments; card colours at
   `+0x3248 + i*0xF8`; after skulls appear `+0x398C` >= 0 and matches the
   skulled rows.
4. Butterflies: butterflies `0x80` appear in row 7; `+0x3258` counts down
   per move; spider float `+0x3288` tracks the top butterfly.
5. Ice Storm: `+0x3D34 + i*0x348` rises over time and drops after matches;
   byte `+0x3D6D` 1 for columns with ice; a vertical match sets `+0x3D6E`.
6. Diamond Mine: tiles have `0x10000`; compare the hovered tile's tooltip
   with the record at `DigGoal+0x228` keyed by piece `+4`.
7. Time Bomb quest: bombs `0x220` or `0x240`, counter matches the drawn
   number, `+0x325C` = bombs on board.
8. Balance quest: `+0x3260`/`+0x325C` follow red/blue collected; `+0x3250`
   sign follows the drawn tilt.
9. Wall Blast: `goal+0x118` cells with byte 1 match the drawn walls.
10. Zen: when no moves remain a random gem becomes a hypercube.
