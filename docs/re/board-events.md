# Board events: matches, cascades, specials, points, feedback

Agent 3 of 6 of the board research phase (see `board-research-brief.md`).
Question: what happens after a move, expressed as events a mod can observe.

Date: 2026-10-30. Source: Steam `Bejeweled3.exe` (SHA-256 d5516244...88c3,
base 0x00400000, no ASLR). Method: **static analysis only**: Ghidra 12.0.4
headless (read-only copy `private/ghidra-b3`, script
`tools/ghidra/BEventsDumpRange.java`) plus capstone disassembly for every
register-passed argument the decompiler drops (sound ids, AddStat ids, call
argument order). The game was not run. Every claim is **static** unless
labelled **hypothesis**; there are no **community** claims in this file.

Raw material (private, not for publication): `private/ghidra-b3/core.c`
(decompiled 0x738000-0x792000), `modes1.c`/`modes2.c` (0x6A0000-0x738000),
`sounds.txt` (SOUND_ resource id -> global holding the loaded sound id),
`soundrefs.txt` (code sites reading those globals), `statcalls.txt` (every
AddStat/MaxStat call with its stat id), `board_vt.txt` (Board vtable with
Classic/Speed/Zen/Quest overrides), helper scripts in `scripts/`
(`snd3.py` sound map, `refs.py` xrefs, `callers.py`, `vcalls.py` virtual call
sites, `stats.py`, `dis.py`).

Conventions used below: "thiscall" = ECX this, stack args, callee pops
(`ret n`); "vt+X" = byte offset in the Board vtable (0x00864CB4). Piece and
Board offsets are byte offsets. Grid: `Piece* [Board + 0xF8 + 4*(col + 8*row)]`
(see `hud.md`/`board_layout.h`). Pixel positions are 128 per cell.

## Summary for design

- One function finds every match: **FindSets** `0x0075AF60` (vt+0x324).
  UpdateGame calls `FindSets(0,0,0)` every tick (cascades); the swap
  handler calls `FindSets(1, piece1, piece2)` when a swap lands.
- Specials are created by four virtuals with the piece as the only object
  argument: **Flamify** vt+0x298, **Laserify** (Star) vt+0x2A0,
  **Hypercubeify** vt+0x29C; Supernova = Laserify + Flamify on the same piece.
  Hooking these gives "special X created at cell (col,row)".
- Every removed gem passes once through **TallyPiece** vt+0x238
  `0x0073D2B0`; it carries the move id, match id and cause, so "N gems
  destroyed by this event" can be counted there.
- Every score change goes through vt+0x260 (**AddPoints wrapper**,
  Board `0x0074DF80`) -> `0x0074A210` -> **PointsManager::Add** `0x0071F030`
  (the floating number). Score `+0xD24`, level points `+0xE00`.
- Praise voices (the six escalating words) come from **DoComplement**
  vt+0x240 `0x0074A420(index 0..6)`; index is stored at Board `+0x2678`.
- Detection always happens before the visual: FindSets runs on the tick the
  gems settle, points and the popup are added in the same call; gems are
  only removed on later ticks (countdowns, shatter effects).

## Event table

Frequency: "per set" = once per detected line/shape; "per tick" = every
Board update (100 per second, `hud.md`).

| Event | Best hook (address, convention) | Fires | What the hook can read | Poll alternative | Order vs visual |
|---|---|---|---|---|---|
| Match pass (any sets) | FindSets `0x0075AF60`, thiscall `(bool fromSwap, Piece* a, Piece* b)`, `ret 0xC`; returns 0 none, 1 sets found, 2 found but deferred (hypothesis) | per tick + once per landed swap | return value; after return, pieces with `+0x183` set / new `+0x190` match ids | none reliable (sets are locals) | before shatter |
| Swap result (good / bad) | swap update `0x00759BA0` (fastcall ECX=Board, no args) calls FindSets(1,..); no match -> swap back, SOUND_BADMOVE `0x0075A3AB`, AddStat 0xF and 0x14; match -> `0x0074C070` (thiscall `(SwapData*)`, `ret 4`) | per swap | SwapData (`+0` piece1, `+4` piece2) | Board `+0xE34` move counter increments in `0x0074C070` | at the end of the swap animation |
| Set scored | vt+0x260 called inside FindSets at `0x0075D962` | per set | x, y (px), points, colour, match id, move id (see AddPoints) | score `+0xD24` | before shatter |
| Flame created | Flamify vt+0x298 = `0x00745CA0`, thiscall `(Piece*)`, `ret 4` | per creation | piece col `+8`, row `+0xC`, colour `+0x220`; flag 1 set | piece flags `+0x228` bit 0 | same tick, plays SOUND_POWERGEM_CREATED |
| Star created | Laserify vt+0x2A0 = `0x007494D0`, `(Piece*)`, `ret 4` | per creation | as above; flag 4 | `+0x228` bit 2 | SOUND_LASERGEM_CREATED |
| Hypercube created | Hypercubeify vt+0x29C = `0x00749570`, `(Piece*, bool playSound)`, `ret 8` | per creation | flag 2, colour becomes -1, old colour in `+0x21C` | `+0x228` bit 1, `+0x220` = -1 | SOUND_HYPERCUBE_CREATE |
| Supernova created | Laserify then Flamify on the same piece in FindSets (`0x0075CB60`, `0x0075CB6D`) | per creation | flags 1 and 4 | `+0x228` & 5 == 5 | both sounds |
| Flame (or other bomb) explodes | Explode vt+0x318 = `0x0074D940`, thiscall `(int x, int y)` pixel centre, `ret 8` | per explosion | centre pixel -> cell = (x-boardX)/128 (vt+0x2AC/+0x2B0 give the board origin) | `+0x230` countdown reaching 0 | the call starts the explosion effect; neighbours are flagged, removed later |
| Star / Supernova / Hypercube effect starts | LightningStorm ctor `0x00789A90` (stdcall `(storm, Board*, Piece*, int type)`, `ret 0x10`) | per detonation | type (below), colour `+0x20`, flags `+0x1C`, move id `+0x38`, match id `+0x3C`, hypercube target colour `+0x10` | storm vector Board `+0x125C..+0x1260` (Storm*) | starts the lightning animation; cells are hit step by step on later ticks |
| Hypercube detonates | DoHypercube vt+0x2D0 = `0x0074DB70`, thiscall `(Piece* cube, int colour)`, `ret 8` | per detonation | colour cleared (-1 = no colour, hypothesis: cube+cube) | as above (type 7) | before the zaps |
| Special triggered by another blast | TriggerSpecial vt+0x320 = `0x0074C570`, thiscall `(Piece*, Piece* cause)`, `ret 8`, returns 1 if it was special | per chained special | piece flags, cause piece | `+0x230`, `+0x17C` | chain reactions |
| Gem removed (any cause) | TallyPiece vt+0x238 = `0x0073D2B0`, thiscall `(Piece*, bool)`, `ret 8` | once per gem (guard byte `+0x181`) | colour, flags, move id `+0x188`, match id `+0x190`, cause id `+0x234`, cause flags `+0x238` | stats `+0xC84+4*4` (gems) and `+0xC84+4*(5+colour)` | when the gem is actually removed (after detection) |
| Points / popup | PointsManager::Add `0x0071F030`, thiscall ECX=`[Board+0x1268]`, 11 args, `ret 0x2C` | per AddPoints call | x, y, points before multiplier, match id (popups with the same id merge) | popup vector `[Board+0x1268]+0xC8..+0xCC` (Points*, `+0x218` match id, `+0x220` accumulated points) | same tick as detection |
| Multiplier up | IncPointMult vt+0x294 = `0x00749650`, thiscall `(Piece* or 0)`, `ret 4` | per multiplier gem cleared | Board `+0xE0C` incremented | poll `+0xE0C` | plays SOUND_MULTIPLIER_UP2_n |
| Cascade / combo step | inside FindSets: AddStat(0x1B) per involved move at `0x0075DBF4`; combo sound at `0x0075DB94` | per FindSets pass with sets | move stat 0x1B (see Cascades) | MoveData vector (below) | before shatter |
| Praise voice | DoComplement vt+0x240 = `0x0074A420`, thiscall `(int index)`, `ret 4`; decided by `0x0075A760` once per tick | when a move's score passes the next threshold | index 0..6 | Board `+0x2678` last index (not reset per move) | after detection of the set that crossed the threshold |
| Speed chain / speed bonus | `0x0074C070` (Lightning only, see below) | per successful swap | Board `+0x1080` chain, `+0x1084` best chain | poll `+0x1080` | after the swap lands |
| Blazing speed | `0x0074A590` stdcall `(Board*)`, `ret 4` | when the meter fills | Board `+0x1188` (float, 1.0 when started), meter `+0x1078` (double 0..1) | poll `+0x1188` | voice + sound in the call |
| Level progress | AddPoints `0x0074A210` adds to `+0xE00` | per AddPoints | | poll `+0xE00` vs target (vt+0x268) | immediate |
| Any sound | PlaySample `0x0076AAF0`: EAX=gApp, EDX=sound id, stack `(int pan, double volume, double pitch)`, `ret 0x14` | per sound | sound id (compare with `sounds.txt` globals) | none | as played |
| Any voice | PlayVoice `0x0076AB60`: ESI=gApp, EDI=sound id, EBX=voice key, stack `(double volume)`; a new voice stops the previous one | per voice | | gApp `+0xC0C` last voice sound id, `+0xC08` current SoundInstance* | as played |

## Evidence

### FindSets 0x0075AF60 (vt+0x324)

- Scan: two passes (`local_354` 0 = rows, 1 = columns), runs of equal colour
  (`+0x220`) of length >= 3 (`2 < local_358`). Pieces that are still moving
  (CurvedVal not at its end, `+0x17E`, `+0x230 != 0`, flag 0x2000) are
  skipped (`0x0075B2BA`..`0x0075B2DA`), so cascades are found when the fallen
  gems settle.
- Each set gets a new id from the Board counter `+0xE1C`
  (`param_1[0x387]` incremented at `0x0075B441`; the same counter is also
  written by `0x00744660`, hypothesis: move ids and match ids share it).
  Each matched piece gets `+0x190` = match id and `+0x188` = the highest move
  id in the set (`0x0075B9F0` area).
- Set records are locals (0x24-byte entries, piece vector at `+0xC/+0x10`,
  match id `+0x18`, move id `+0x1C`), not stored in the Board. A mod cannot
  poll them; it must hook FindSets or the virtuals below.
- Matched multiplier gems (flag 0x10) call IncPointMult (vt+0x294,
  `0x0075B948`).
- Special creation (`0x0075C81A`..`0x0075CC8F`):
  - Pieces at the crossing of two sets (L/T shapes, list `local_1c4`):
    AddStat 4 and then, because vt+0x174 returns true in Board
    (`0x0073C3E0`, no override in Classic/Speed/Zen/Quest), AddStat 0x12 and
    **Laserify** (Star). If vt+0x174 were false it would be AddStat 0x11 and
    Flamify.
  - Runs (map `local_198`, run length at `entry+0x10`): length >= 6
    (`cmp [ebx+0x10],5; jle`) -> AddStat 4, 0x11, 0x12, 0x1E, then Laserify
    and Flamify (**Supernova**, flags 1|4); length 5 (`cmp ...,4`) -> AddStat
    4, 0x13 and Hypercubeify(piece, 1); otherwise (4) -> Flamify, AddStat
    4, 0x11. The piece used is the run's map key, or `entry+0x14` when the
    key is already a Star.
  - Which cell gets the special: **hypothesis** - the gem the player moved
    (`+0x214` == current tick) or else the most recently moved gem in the run
    (`+0x218` comparison at `0x0075BE80`). Hooking the create virtuals and
    reading `+8/+0xC` avoids depending on this.
- Points per set (`0x0075D885`..`0x0075D962`): AddStat(0x1A, 1, moveId),
  then `k = max(1, moveStat(moveId, 0x1A))`, `points = (k - 3 + n) * 50`,
  plus `(n - 4) * 350` when `n >= 5`, n = set length. Then vt+0x260 with
  x = set x + 0x40, y = set y + 0x38, the gem colour from the table at
  `0x008E1D38 + 16*colour`, match id, move id. So the first 3-set of a move
  is 50 before multipliers; each further set in the same move counts one
  more (hypothesis: this is the cascade bonus).
- Double set: if more than one set was found in this pass, SOUND_DOUBLESET
  (`0x0075DA62`).
- Return value (capstone, `0x0075DDB4` and `0x0075DDD1`): 1 if any set
  was found, 0 otherwise, 2 when `fromSwap` and the found sets were marked
  pending (`local_365`) and the swapped pieces were not in them
  (hypothesis for the meaning of 2; the swap handler treats 2 like a match
  but keeps waiting, `0x0075A4C3`).

### Cascades and combos

- Board `+0xC48/+0xC4C`: `std::vector<MoveData>`, element 0xD8 bytes:
  `+0x3C` move id, `+0x38` byte (used by `0x007561A0`), `+0x40 + 4*stat`
  per-move stats (0x26 stats, same ids as the Board stats `+0xC84`).
  AddStat `0x0073D170` (ECX = stat id, ESI = Board, stack `(amount,
  moveId)`, moveId -1 = none) adds to Board stats, to the profile
  (`[gApp+0xB80]+0xC0+4*stat`, unless `+0x1410/+0x1412`) and to the
  matching MoveData. GetMoveStat `0x0073CF10` (ESI=Board, stack
  `(moveId, stat)`); MaxMoveStat `0x0073CDB0` (ESI=Board, stack `(stat)`).
- Cascade depth: FindSets adds 1 to move stat **0x1B** for every move that
  had a set in this pass (`0x0075DBEF`). So `moveStat(id, 0x1B)` is the
  number of passes in which that move produced sets (1 for the swap's own
  match, +1 per cascade level; **hypothesis** on the exact off-by-one).
- Combo sound (`0x0075DA73`..`0x0075DB94`): `level = MaxMoveStat(0x1B) + 1`,
  forced to 1 when every set in this pass contains a just-swapped gem
  (`local_2c4 == 0`), capped at 6. Sound id = `[SOUND_COMBO_1 global] +
  level` (sound ids assumed consecutive, **hypothesis**); Zen-style boards
  (vt+0x1C4 true) use SOUND_ZEN_COMBO_2 at level 1. In Lightning with a
  speed chain the SPEEDMATCH sound replaces it (below).
- Move stat **0x1A** counts sets in the move and drives points.
- Not found: a single Board field holding "current cascade depth". The
  MoveData vector is the closest poll target; when records are removed is
  **open** (candidates `0x00755940`, `0x00755620`, called after the praise
  check).

### Specials: create functions

- Flamify `0x00745CA0`: skips colour -1 or `+0x22C` bit 0 (blocked);
  `+0x228 |= 1`, clears other kinds (`&= 0xFFFFFD1F`), `+0x184 = 0x19`,
  plays SOUND_POWERGEM_CREATED (`0x00745CFC`), starts two effects.
- Laserify `0x007494D0`: `+0x228 |= 4`, SOUND_LASERGEM_CREATED,
  `0x00749050` effect.
- Hypercubeify `0x00749570`: `+0x228 = 2`, `+0x210` = tick, `+0x21C` = old
  colour, `+0x220 = -1`, sound only when the bool is set.
- Stats: 4 gems cleared (also counted for the converted gem), 0x11 flames,
  0x12 stars, 0x13 hypercubes, 0x1E supernovas (call sites listed in
  `statcalls.txt`). StatsTab strings 450-452 label flame/star/hypercube
  totals (profile copies of these stats, **hypothesis** on the exact
  mapping).

### Specials: detonation

- UpdateGame `0x00767E20` (vt+0x39C, thiscall, no args) walks the grid every
  tick. Field `+0x230` is a detonation countdown; when it reaches 0
  (`0x00768480` loop):
  - flag 0x8000: Explode via vt+0x318.
  - flag 1 (Flame, not Star) -> AddStat, a 20-point popup at the gem
    (vt+0x260), Explode vt+0x318 at its centre.
  - flag 0x80000 (**hypothesis**: a "detonate everything" gem; quest/end of
    game): 1000 * stat 0x15 points, then every special on the board gets a
    staggered countdown (`iVar14*0x19+1`).
  - flags 2/4/0x400/0x10 go to TriggerSpecial (vt+0x320) instead.
  - plain gems: shatter effect `0x0074C770`, 300 points for flags 0x1800
    (coins, hypothesis), SOUND for multiplier gems.
  - Afterwards one batch of explosion sounds (SOUND_PREBLAST,
    SOUND_GEM_SHATTERS, SOUND_BOMB_EXPLODE, SOUND_SMALL_EXPLODE are read in
    this function, `0x00768BA7`..`0x00768F29`).
- Explode `0x0074D940` -> `0x0073F8E0` (stdcall `(Board*, x, y)`): cell
  list of 9 (3x3 around the centre) or 13 when vt+0x180 is true (Board
  returns false, `0x0073C3C0`; adds 2-away cross cells). For each neighbour:
  - another Flame (flag 1): countdown `+0x230 = 15` (25 in Zen-style),
    cause copied -> chain explosion later.
  - multiplier gem (0x10): countdown 5.
  - Star/Hypercube etc.: TriggerSpecial.
  - plain gem: `+0x17D = 1`, 20 points (vt+0x260 with its match/move id),
    DeletePiece vt+0x2C0 `0x00755B80` (which calls TallyPiece).
- TriggerSpecial `0x0074C570`: flame/multiplier/0x80000 -> countdown 1;
  hypercube -> DoHypercube(piece, cause colour or old colour, else random
  0..6); star -> `+0x17C = 1`, new LightningStorm type 2.
- LightningStorm (no RTTI, 0x2E8 bytes) ctor `0x00789A90`: `+0` Board,
  `+0x1C` piece flags, `+0x20` colour, `+0x28` type (6 when the piece is
  also a Flame = Supernova), `+0x14` range = 7 (3 for type 3), `+0x38` move
  id, `+0x3C` match id. Stored in Board vector `+0x125C/+0x1260/+0x1264`.
  Updated every tick by `0x0075DEE0` (thiscall, `ret 4`; plays
  SOUND_ELECTRO_EXPLODE): type 0 = row only, 1 = column only, 2/3 = row and
  column (Star), 4 = 8 directions, 5 = expanding square ring, 6 = row and
  column three cells wide (Supernova), 7 = hypercube colour zap (target
  colour `+0x10`). Each hit cell: copies move/match id and cause; multiplier
  gems countdown 5; specials via TriggerSpecial; plain gems countdown 1
  (removed next tick). When a storm ends the source star loses flag 4 and is
  itself removed. FindSets creates a storm for a matched Star with type
  `3 - (Board+0xE8 != 0)` (`0x0075BB3C`; meaning of `+0xE8` **open**).
- DoHypercube `0x0074DB70`: AddStat(0x26+colour) when colour == -1 path
  (`lea ecx,[ebx+0x26]`, **hypothesis** cube+cube), AddStat 0xE (hypercubes
  used), vt+0x24C(colour), storm type 7. Swapping with a hypercube goes
  through TrySwap `0x0075EE20` (vt+0x30C) -> vt+0x2CC `0x00740F20`
  (SOUND_ELECTRO_PATH) -> DoHypercube.
- Counting "detonated, destroyed N gems": hook TallyPiece and group by the
  cause id `+0x234` / cause flags `+0x238` (set by Explode, storms and
  TriggerSpecial) or by match id `+0x190`. Not verified at runtime.

### Points

- vt+0x260 AddPoints wrapper: Board `0x0074DF80`, Speed `0x00715300`,
  Quest `0x006CF290`; thiscall, 13 stack args, `ret 0x34`:
  `(x, y, points, colR, colG, colB, colA, matchId, addToLevel, useMultiplier,
  moveId, force, channel)` (order from the pushes at `0x0075D908`..`0x0075D962`
  and the explosion call in `0x0073F8E0`; colour words are a 16-byte struct,
  **hypothesis** RGBA). Returns early when Board byte `+0x12F8` is set.
  Calls `0x0074A210` and adds the score delta to entry `channel` of a
  vector from `0x0040A570` (purpose open).
- `0x0074A210` (EAX = Board, 12 stack args, `ret 0x30`): total = points *
  (`+0xE0C` if useMultiplier else 1), rounded; then in chunks of up to 10:
  level points `+0xE00 +=` a value computed from the move's points so far
  (move stat 1), vt+0x1E8 and a pow() call (when addToLevel; formula not
  decoded), then `s = round(vt+0x1E8() * chunk)`, AddStat(1, s, moveId)
  and score `+0xD24 += s` (capped at 0x7FFFFFFF). vt+0x1E8: Classic
  `0x0073AF50` = level+1, Speed `0x00714090` = 5.0, Board/Zen
  `0x0073B600`/`0x0070E330` = 1.0. So in Classic the score of a set grows
  with the level. Then PointsManager::Add.
- PointsManager (RTTI `PointsManager`, vtable `0x00866474`) at
  `[Board+0x1268]` (set in the Board ctor `0x0074B60C`); Add `0x0071F030`
  merges into an existing `Points` popup with the same match id
  (`+0x218`) by adding to `+0x220`, else creates one.
- Point values seen: set (above), 20 per gem destroyed by an explosion or
  per Flame detonating, 300 for flags 0x1800 gems, 1000 * stat 0x15 for the
  0x80000 gem, speed bonus `min((chain+1)*20, 200)`.

### Speed bonus and Blazing Speed (Lightning)

- `0x0074C070` runs for each successful swap: Board `+0xE34` (moves made)
  += 1. Only when vt+0x16C is true: Board/Speed `0x0073C400` true; Classic
  `0x0073AF90`, Zen `0x0070E360`, Quest `0x006C4CE0` false. So speed chains
  and Blazing Speed are **Lightning only** (static).
- Chain: if the previous successful swap was less than 301 ticks ago
  (`[0x392]+delta < 0x12D`) or a chain is running, `+0x1080 += 1`, best in
  `+0x1084`, speed-bonus points `min((chain+1)*20, 200)` through vt+0x260
  (channel 2). FindSets then plays SOUND_SPEEDMATCH(1 + min(chain, 8)) instead
  of the combo sound (`0x0075DAF7`), or SOUND_FLAMESPEED1 while the meter
  `+0x1078` is above 0.01.
- Meter: double `+0x1078` grows with fast moves once the chain is above 8;
  when it reaches 1.0 and `+0x1188` == 0 -> BlazingSpeed `0x0074A590`:
  `+0x1188 = 1.0`, SOUND_FLAMEBONUS, voice SOUND_VOICE_BLAZINGSPEED,
  vt+0x240(6), one AddStat with a computed id (`lea ecx,[ebx+0x19]`). Decay `0x00743FF0`, reset `0x00748370`.

### Multiplier

- IncPointMult `0x00749650`: unless byte `+0xE28` is set, `+0xE0C += 1`
  (`0x00749706`), starts the multiplier animation, plays resource
  `0x990 + min(old-1, 3)` = SOUND_MULTIPLIER_UP2_1..4. Called from FindSets
  (matched multiplier gem) and TallyPiece (removed multiplier gem). Ice Storm
  `0x006E6195` and SpeedBoard `0x00719915` increment `+0xE0C` directly.
- SOUND_MULTIPLIER_APPEARS in vt+0x330 `0x0073C830` (a multiplier gem
  appears, called from `0x00763E90`); SOUND_MULTIPLIER_HURRAHED in UpdateGame
  `0x0076864D` (end-of-round special detonation, **hypothesis**).

### Praise voices (DoComplement)

- `0x0075A760` (ESI = Board, no stack args), called once per tick from
  UpdateGame `0x007693A7` after the storms: only if vt+0x2A4 is true
  (Board `0x004092A0` true; Quest override `0x00417570`). Sums vt+0x344
  (`0x0073E010`, a per-move score from move stats 0x1B cascades, 0xC, 0xD,
  0xE, 0x11, 0x12, 0x13 x2, 0x18, 4) over all MoveData; picks the highest
  index i with sum >= threshold[i], thresholds at `0x008D2D90` =
  3, 6, 12, 20, 30, 45. If i is above the last level `+0x267C` and at least
  vt+0x1D0 (Board 0, Zen `0x0070E390`), calls vt+0x240(i).
- DoComplement `0x0074A420`: plays the voice from table `0x0085E724`
  (resource ids): 0 SOUND_VOICE_GOOD, 1 SOUND_VOICE_EXCELLENT,
  2 SOUND_VOICE_AWESOME, 3 SOUND_VOICE_SPECTACULAR, 4 SOUND_VOICE_EXTRAORDINARY,
  5 SOUND_VOICE_UNBELIEVABLE, 6 SOUND_VOICE_BLAZINGSPEED; stores i in
  `+0x2678` and `+0x267C` and starts a text/visual animation (CurvedVal at
  `+0x2598`). So each praise is shown and spoken; the mod can speak the
  level name it knows from the index (no game text needed beyond the index).
- Thresholds count per move, so praise escalates within one move (several
  voices possible in one cascade).

### Other feedback sites (sound/voice -> function)

From `soundrefs.txt` (capstone; the decompiler loses these EDX/EDI args):

| Sound | Function | Meaning |
|---|---|---|
| SOUND_BADMOVE | TrySwap `0x0075EE20`, swap update `0x00759BA0`, `0x0073F140` | illegal / no-match swap |
| SOUND_SELECT | `0x0073F140` | gem selected (input agent's area) |
| SOUND_GEM_HIT | vt+0x358 `0x0073DD70` | gems landing (hypothesis) |
| SOUND_DOUBLESET, SOUND_COMBO_n, SOUND_ZEN_COMBO_2, SOUND_SPEEDMATCHn, SOUND_FLAMESPEED1 | FindSets | sets / combos / speed |
| SOUND_POWERGEM_CREATED, SOUND_LASERGEM_CREATED, SOUND_HYPERCUBE_CREATE | create functions | special created |
| SOUND_ELECTRO_EXPLODE | storm update `0x0075DEE0` | star / hypercube zaps |
| SOUND_ELECTRO_PATH | `0x00740F20` | hypercube swapped |
| SOUND_PREBLAST, SOUND_GEM_SHATTERS, SOUND_BOMB_EXPLODE, SOUND_SMALL_EXPLODE | UpdateGame, replay `0x00761850` | detonations |
| SOUND_MULTIPLIER_UP2_n / _APPEARS / _HURRAHED | see Multiplier | |
| SOUND_FLAMEBONUS + SOUND_VOICE_BLAZINGSPEED | `0x0074A590` | blazing speed |
| SOUND_VOICE_GETREADY, SOUND_VOICE_GO | `0x00767090` | round start (with announcements 140/141, hud.md) |
| SOUND_VOICE_LEVELCOMPLETE | UpdateGame `0x00769900` | level complete (announcement 139) |
| SOUND_VOICE_GAMEOVER, SOUND_VOICE_TIMEUP | GameOverAnnounce `0x0074DDB0`; Speed `0x007151E0`, `0x00719350` | game over / time up (94/95/478/480) |
| SOUND_VOICE_NOMOREMOVES | Classic `0x0073AFE0` | out of moves (164) |
| SOUND_VOICE_THIRTYSECONDS | `0x00716F70`, `0x00744920` | timer warning |
| SOUND_VOICE_CHALLENGECOMPLETE | Quest `0x006DD8A0` | quest done (417) |

Announcements (texts, front of the queue at Board `+0x3034`) are covered in
`hud.md`; the voices above are played next to them in the same functions, so
the existing announcement poll already covers round start, level complete
and game over. Praise words are voice only plus an image animation (no
Announcement object, static: DoComplement creates none).

### Replay recorder (side finding)

`0x00761850` decodes a byte stream (Board `+0x12F0` read position,
**hypothesis**: replay/undo data) with event codes 0 = piece created, 1 =
swap, 2 = set, ... and re-runs the same create/score calls. Writes happen
through `0x0054F750` when vt+0x148(n) is true (e.g. Explode writes code 10,
BlazingSpeed code 9, multiplier code 0xD, AddPoints code 3). This is a
possible single observation point for many events, but its format was not
decoded. **Open.**

## How a mod should observe (recommendation)

1. Inline hooks (5-byte jmp + trampoline) on the thiscall virtual targets,
   not vtable patches (subclasses share most entries, but Quest/Speed
   override vt+0x260; hooking `0x0074A210` covers all AddPoints paths
   except direct callers `0x006D6D60`, `0x006DEDD0`, `0x00761850` of
   PointsManager::Add). Minimal set: FindSets (enter/leave, to bracket a
   "pass"), Flamify, Laserify, Hypercubeify, Explode, LightningStorm ctor,
   DoHypercube, TallyPiece, IncPointMult, DoComplement, BlazingSpeed.
2. Queue events in the hook (no speech inside the game call); the
   navigator tick summarises per pass: "match 3 red, flame created at
   C4 R6, cascade 2, +150".
3. Pure polling fallback (no hooks): score `+0xD24`, level points `+0xE00`,
   multiplier `+0xE0C`, praise `+0x2678` (+ change of `+0x267C`), speed chain
   `+0x1080`, blazing `+0x1188`, storm count `(+0x1260-+0x125C)/4`, grid
   flags per cell (`+0x228` bits 1/2/4 appear = special created), gApp
   `+0xC0C` voice id. Polling misses repeated identical values and cannot
   attribute gems to causes.
4. Everything above fires before the matching visual ends; speech can be
   immediate without spoiling anything the screen has not already started.

## Uncertainties

- Run-length -> special rule is read from compare constants; the exact run
  set (does a 4-run inside an L count as both?) is not proven.
- Meaning of FindSets return 2, Board `+0xE8`, `+0xE28`, `+0x12F8`, flags
  0x8000/0x80000/0x1800, AddPoints `channel`, MoveData lifetime.
- Combo sound arithmetic assumes consecutive sound ids for COMBO_1..7.
- Calling conventions are from `ret n` and call sites; the register-based
  ones (AddStat ECX/ESI, PlaySample EAX/EDX, PlayVoice ESI/EDI/EBX,
  `0x0074A210` EAX, `0x0075A760` ESI) need trampolines that preserve
  registers.
- Ghidra marks several delete calls as no-return, so decompiled tails of
  FindSets and the storm update are truncated; capstone was used there.
- No runtime check of any of this.

## Runtime checklist (lead, throwaway profile, Classic then Lightning)

1. Breakpoint/log hooks on FindSets `0x0075AF60`: argument 1 = 1 right after
   a swap lands, 0 on other ticks; returns 1 when gems match.
2. Make a 3-match on Classic level 1 with multiplier 1: score `+0xD24`
   rises by 50 * `+0xE0C` * (level+1) (expected 50); a Points popup appears
   in `[[Board+0x1268]+0xC8]` with `+0x220` = 50 (check whether the popup
   shows the scaled or unscaled value).
3. Make a 4-match: Flamify `0x00745CA0` called once; that cell's piece
   `+0x228` has bit 0; stat `+0xC84+4*0x11` +1.
4. L/T match: Laserify `0x007494D0` (bit 2). 5-run: Hypercubeify
   `0x00749570`, `+0x220` = -1. 6-run if achievable: both Laserify and
   Flamify on one piece.
5. Detonate a Flame: Explode `0x0074D940` with the cell centre; TallyPiece
   `0x0073D2B0` called for each of up to 9 gems with the same `+0x234`.
6. Detonate a Star: storm ctor `0x00789A90` type 2 or 3 (record which, and
   `Board+0xE8`); storm vector size 1 until it ends.
7. Hypercube swap: DoHypercube `0x0074DB70` colour = partner colour; storm
   type 7.
8. Cascade: move stat 0x1B (`MoveData+0x40+4*0x1B`) increments per pass;
   combo sound index matches; praise index `+0x2678` follows thresholds
   3/6/12/20/30/45 of the move score.
9. Match a multiplier gem: `+0xE0C` +1, IncPointMult called.
10. Lightning: `+0x1080` grows with quick moves, speed-bonus popups,
    `+0x1188` becomes 1.0 with the blazing-speed voice; in Classic
    `+0x1080` stays 0.
11. gApp `+0xC0C` changes to the praise voice's loaded sound id
    (`[0x008D8450]` for SOUND_VOICE_GOOD etc., see `sounds.txt`).
