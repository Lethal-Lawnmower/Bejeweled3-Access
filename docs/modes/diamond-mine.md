# Diamond Mine mode (DigBoard + DigGoal)

Source: `src/mod/mode_diamondmine.cpp` (`DiamondMineHooks()`, see
`src/mod/mode_info.h`). Date: 2026-10-30. Exe: Steam `Bejeweled3.exe`
(SHA-256 `d5516244...88c3`). Repository revision at the time: `21b0d19`
plus uncommitted mode files.

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Not linked, not run in the game, not heard through Prism.** Every
field is a static read of the decompilation; none is runtime-verified.

Method: `private/ghidra-b4/all.c` extracted with `private/ice-dig/fn.sh`
(outputs `private/ice-dig/dig*.c`), capstone disassembly of the DigGoal
range (`private/ice-dig/dig-dis.txt`), constants via
`private/ghidra-b4/py/rd.py`, string ids from `default.xml`. Builds on
`docs/re/board-objects.md` (Diamond Mine), `docs/re/hud.md` (gold, depth)
and `docs/re/board-flow.md` 4.4.

## How the mode works (static)

- Tiles are grid pieces with flag `0x10000`. Their details are a
  `std::map<int, int[8]>` keyed by piece id (`piece+4`) at
  `DigGoal+0x228` (goal = `Board+0x3180`). MSVC 2008 map: head pointer at
  `map+0x18` (= `goal+0x240`, the end compared after find `0x004228C0`),
  root = `head+4`; node `+0` left, `+4` parent, `+8` right, `+0xC` key,
  `+0x10` value, `_Isnil` byte `+0x31` (operator[] `0x004224C0`).
- Record (setter `0x006B3700`): `[0]` strength or treasure size, `[1]`
  starting value, `[2]` kind (0 artifact, 1 block, 2 treasure), `[3]`
  random, `[4]` sparkle effect, `[7]` object; byte `node+0x29` (value +
  0x19) is set when a block's strength reached 0 (`0x006B59F0`).
- Damage (`0x006B59F0`, a match next to a tile): artifacts and treasure
  are collected; a block loses 1 strength per hit, 2 while it is above 4;
  dark rock (4) is not hit by plain matches; with the function's 4th
  argument set (hypothesis: a special gem blast) it is first set to 1. Strength 0 marks the block broken.
- Buried hypercube: a block whose id is in `std::vector<int>` at
  `goal+0x450` (first `+0x45C`, last `+0x460`); `0x006BB6A0` turns it into
  a hypercube when uncovered.
- Tooltip (GetTooltipText `0x006AC360`, goal vtable+0x11C): kind 0 ->
  184; kind 2 -> 186 (size < 4) or 188; kind 1 -> 190 if buried hypercube,
  else by strength 1..5 -> 192, 194, 196, 198, 200, other -> 202.
- Depth as drawn (endless only, DrawOverlay `0x006BD535`..`0x006BD5A4`):
  `trunc(double[goal+0x4F0] + curve(goal+0x518) * [goal+0x4FC] * (byte
  [goal+0x647] ? 2 : 1)) * 10`, as three digits; at 1000 or more the
  letters 545, 546, 547. The curve's last evaluated value is the double at
  `goal+0x550` (CurvedVal GetOutVal `0x00437C60` stores it at `+0x38`; the
  draw calls it every frame). The truncation is `_ftol2` (`0x00792BF0`,
  `cvttsd2si`).
- The scroll start (DigGoal::Update `0x006C2AD0`) sets the mega flag
  `+0x647`, adds to the depth base and calls DoTimeBonus `0x00736F20`,
  which shows string 171 with `[Board+0x324C]` or 170 with
  `[Board+0x3278]` (mega).
- Gold total = `Board+0xE00`, drawn with string 173 (`0x006BDEDF`).
- Pieces leaving the top during a scroll go to `std::vector<Piece*>` at
  `goal+0x438` (first `+0x444`, last `+0x448`).

## What is spoken

| Hook | Says (examples) |
|---|---|
| `cellText` (after the gem name) | `DIRT, 1 hit`; `ROCKS, 2 hits`; `BOULDER, 4 hits`; `DARK ROCK, special gems only`; `GOLD`; `DIAMONDS`; `ARTIFACT`; `HYPERCUBE` (names are the game's strings) |
| `briefStatus` (F6, endless only) | `depth 40, $1250` (gold with string 173) |
| `details` (M) | `depth 40, $1250. row 6: columns 1 to 3 DIRT, 1 hit; column 4 GOLD. row 7: columns 1 to 8 ROCKS, 2 hits. row 8: ...`; `no tiles on the board` |
| `tick` events | `GOLD dug up`, `ARTIFACT dug up`; `plus 250, $1500`; after a scroll has settled (10 ticks): `+30 SECOND BONUS, depth 50` (string 171/170 text, then depth) |

Hits are given only where the count is certain from the damage rule
(strength 1-3: that many; 5: 4, which the game's own text 201 states);
other strengths say nothing. Time left: the T key (hud.cpp) already reads
the timer; not duplicated. In quests depth and gold are not spoken (F6
already reads the goal progress); the tiles are.

## Fields read

All **static**.

| Field | Type | Meaning | Evidence |
|---|---|---|---|
| piece `+0x4`, `+0x228` | int, flags | id, ground flag `0x10000` | board-objects.md; `0x006AC360` |
| `Board+0x3180` | DigGoal* (RTTI checked) | goal | hud.md, board-flow 4.4 |
| `goal+0x228` map (head `+0x240`) | std::map | tile records | `0x006AC360`, `0x004224C0`, `0x006AAC00` |
| record `[0]`, `[2]`, byte `+0x19` | int, int, byte | strength/size, kind, broken | `0x006B3700`, `0x006B59F0` |
| `goal+0x45C`/`+0x460` | int* | buried hypercube ids | `0x006AC360`, `0x006BB6A0` |
| `goal+0x444`/`+0x448` | Piece** | pieces scrolled off | `0x006BB6A0`, board-flow 4.4 |
| `goal+0x4F0` | double | depth base | `0x006BD55D` |
| `goal+0x550` | double | scroll curve value (cache) | `0x006BD535`, `0x00437C60` |
| `goal+0x4FC`, byte `goal+0x647` | int, byte | rows per scroll, mega | `0x006BD542`..`0x006BD54C` |
| `Board+0xE00` | int | gold | `0x006BDEDF` (hud.md) |
| `Board+0x324C`, `+0x3278` | int | time bonus, mega bonus (s) | `0x00736F20` |
| `Board+0x3184` | byte | endless | hud.md (hypothesis there) |

## Corrections to earlier notes

- hud.md / hud.cpp `digDepth` is the raw base `trunc(goal+0x4F0)`; the
  screen shows that plus the scroll curve term, times 10. F6 in hud.cpp
  ("depth N") therefore says a value 10 times smaller than the screen and
  possibly one scroll behind. The mode's `briefStatus` uses the drawn
  formula.
- board-objects.md "Record [2] = 2 treasure: [0] < 4 gold" is confirmed;
  treasure and artifacts are tiles themselves, not hidden under blocks.

## Uncertainties

- The scroll curve cache at `goal+0x550` is the value of the last draw; if
  the board was not drawn it may be stale. Whether the curve rests at 0 or
  1 between scrolls is not known; the formula is the draw's in both cases.
- Strength 6 or above (big boulder) behaviour: the damage rule takes it
  through 4 (dark rock); hits are left out.
- "dug up" assumes a treasure/artifact piece leaving the grid (or losing
  its map record) without being in the scrolled-off vector was collected;
  a failed map read could cause a false event.
- The value text 172 (`$%d`, DigGoal::DrawPiecePre) was not located in
  code, so treasure values on tiles are not spoken; the gold total event
  gives the amount after collection.
- Time bonus is spoken at the end of the scroll (depth settled), not when
  the game first shows it; a zero bonus is left out.
- Tile colour: tiles may carry a colour; board_access's colour and
  special-name words are spoken before cellText (see lead notes).

## Lead notes (board_access interaction)

- `board_access.cpp` `SpecialName` caches the tooltip title by
  (board, flags). All Dig tiles share flags `0x10000`, so the first tile's
  title would be reused for every tile. Suggest: skip the colour and
  SpecialName for pieces with `0x10000` on DigBoard and let `cellText`
  name the tile, or key the cache by piece id for ground pieces.
- `hud.cpp` digDepth: see Corrections.

## Runtime checklist (lead; throwaway profile, read-only)

1. Endless Diamond Mine: cursor on each tile kind; compare the spoken name
   with the game's mouse tooltip for the same tile.
2. Hit a ROCKS tile once: it should say DIRT, 1 hit (strength 2 -> 1).
3. Hit a BOULDER (5): the next name is STONES (5 -> 3); check 4 hits total.
4. Dump the map: walk `goal+0x240` head, count nodes; ids match grid pieces
   with `0x10000`.
5. Compare `depth N` with the depth drum before and after a scroll; check
   `goal+0x550` between scrolls (0 or 1) and correct the doc.
6. Collect gold: "GOLD dug up" and "plus N, $total" with the drawn total.
7. Let a tile scroll off the top: no "dug up".
8. A Dig quest: tiles named, F6 adds nothing from this mode.

## Live check (lead, 2026-10-30, Gold Rush quest, user's profile)

The Gold Rush quest board is a DigBoard. M listed "row 6: columns 1 to 8
DIRT, 1 hit. row 7: column 1 DIRT, 1 hit; columns 2 to 3 ROCKS, 2 hits;
columns 4 to 5 GOLD; columns 6 to 7 ROCKS, 2 hits; column 8 DIRT, 1 hit.
row 8: columns 1 to 8 GOLD", and E on column 5, row 8 said "tile, GOLD".
**Compared with a screenshot: tiles and cursor matched exactly.** Not seen:
digging, gold collected, depth (quest has no depth; endless mode locked).
