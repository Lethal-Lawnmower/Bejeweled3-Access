# Board state: grid, pieces and the settled-board signal

Scope: the logical gem grid, the `Piece` object, the board's per-tick
update order, and how a mod can tell that the board is settled (safe to read
and to act). Swaps/legality details belong to `board-input.md`, match and
special-gem events to `board-events.md`/`board-objects.md`, level flow to
`board-flow.md`; this document only covers what those need from the board
model.

Date: 2026-10-30. Agent 1 of 6 (board research phase, research only).
Source: Steam `Bejeweled3.exe` (image base 0x400000, the exe in
`private/ghidra/input/`). Method: **static only** - Ghidra 12.0.4 headless on
`private/ghidra-b1` (read-only), new script `tools/ghidra/BStateRange.java`
(decompiles every function in address ranges), plus capstone/pefile helper
scripts and outputs in `private/ghidra-b1/` (`board_all.c` = all functions
0x0073A000-0x00770000, `piece.c` = 0x0071F000-0x00722000, `curved.c` =
CurvedVal 0x00436900-0x00438000, `subs.c` = subclass overrides,
`board_vt.txt` = Board vtable). Nothing here was checked at runtime.

Labels: **static** = read from this exe's code; **community** = third-party
claim; **hypothesis** = inferred meaning or name, not proven. Offsets are
byte offsets. Function addresses are Ghidra `FUN_xxxxxxxx` entry points.
Calling conventions: many helpers here are MSVC custom conventions (object
in EAX/EDI/ESI/EBX); a mod should read fields rather than call them unless
noted.

## Summary for design

- The grid is a fixed **8x8 array of `Piece*`** at `Board+0xF8`, index
  `col + 8*row` (row 0 at the top). NULL = empty cell. Same in every mode
  (all grid loops are hard-coded 8x8 and shared by every Board subclass).
- Each piece knows its **logical cell** (`+0x08` col, `+0x0C` row), its
  **colour** (`+0x220`, -1 = none), **flags** (`+0x228`) and a stable
  **id** (`+0x04`). Its **visual** position is separate (`+0x10` x, `+0x14`
  y, 128 units per cell).
- The logical grid runs **ahead of the visuals**: refills move survivors and
  create new pieces (with their colours) in their final cells immediately;
  they then fall visually. Matched gems stay in the grid (with colour) until
  their shrink animation ends.
- **Settled signal: `Board::IsBoardStill`, vtable `+0x2E8`** (base
  `0x00763E20` -> `0x0075FCA0`; SpeedBoard override `0x007183E0`). True when
  no fill countdown, every piece is individually still, no 3-in-a-row exists
  and no lightning effect runs. The game uses it to start level-complete,
  game-over checks, the idle hint timer and more.
- **Input is not gated on the whole board.** The board-level gate is
  vtable `+0x188` (`0x0073D4F0`, "can play"); each swap only needs the two
  pieces involved to be still (vtable `+0x304`, `0x0075FAE0`). The player
  may swap in one corner while a cascade finishes elsewhere.
- For a blind player: announce/describe the board when `IsBoardStill`
  becomes true (rising edge), allow acting whenever "can play" is true and the
  target pieces are still, and treat every other moment as "animating".

## 1. Grid

| Item | Location | Evidence | Label |
|---|---|---|---|
| Grid | `Board+0xF8`, `Piece*[64]`, index `col + 8*row` | CreatePiece `0x007415C0`: `[board + 0xF8 + (col + row*8)*4] = piece`; DeletePiece `0x00755B80` clears `[...+(col+row*8)*4]`; ShowHint `0x0075AEB0` (hud.md) | static; community `[Board+0xF8+4*col+32*row]` **confirmed** |
| Dimensions | 8 columns x 8 rows, fixed | every grid loop: `0x3E..0x7D` dwords = 64 cells (e.g. `0x0073BA40`, `0x0075FCA0`, `0x0073F050`); pixel->cell `0x0073CBE0`/`0x0073CC10` (128 per cell, stop at 0x400) | static |
| Same in all modes | yes | the key loops are base-class functions not overridden by any of the 17 Board vtables (table in section 3) | static |
| Row 0 = top | yes | falling loop `0x0073DD70` walks each column from slot row 7 (`y` limit 0x380 = 7*128) upwards; refill creates pieces above row 0 | static |
| Empty cell | NULL pointer | DeletePiece sets the cell to 0; all readers test `!= 0`; FillInBlanks refills | static |
| Cell encoding in byte streams | `row*8 + col` (saves/replays) | `0x0073CC80` decodes `(b & 7) + (b & 0xF8)`; recorders write `row*8+col` (`0x00755B80`, `0x00763E90`) | static |
| Lowest refillable row | `Board+0x22E4` int, = 7 | set only in the Board ctor `0x0074AE30` (`0x0074B333`); FillInBlanks iterates rows `[+0x22E4]..0` | static (meaning hypothesis) |
| Board origin on screen | x = `[+0x2C4C] + 0x20B`, y = `[+0x2C50] + 0x4B` | vtable `+0x2AC` `0x0073BD80`, `+0x2B0` `0x0073BD70`; units are the 1600x1200-style design space (scaled by gApp `+0x9E8` / 1200 elsewhere) | static (scale: hypothesis) |
| Piece at a design-space point | `0x0073BC50` (EDI = board) tests `piece.x <= px < x+128`, same for y | static |
| Piece by id | `std::map<int, Piece*>` at `Board+0x238`; lookup `0x0073E370` | Piece init inserts `(id, piece)` (`0x007202B9..0x007202C6`); Piece dtor `0x00720040` calls board vtable `+0x2C4(id)` | static (map layout: same MSVC map as menus.md, not re-verified) |
| Next piece id | `Board+0xF4` int, post-incremented | Piece ctor `0x007203B0`: `id = [board+0xF4]++` (`0x0072046D`) | static |

How new pieces enter (static, FillInBlanks = vtable `+0x32C`, base
`0x00763E90`; quest boards `0x006DD4D0` defer to the quest goal first):

1. Does nothing while any piece has a pending detonation `+0x230 > 0`, or
   while a hypercube "board blast" piece is active (`Board+0x2EF8`).
2. For each column, bottom to top: each surviving piece above a hole is moved
   one row down **in the grid** (`row += 1`, cell pointers swapped), marked
   `+0x17E = 1`, and given a starting fall velocity `+0x1C` if it had none.
   Repeated until no holes.
3. For each top hole a new Piece is created by CreatePiece `0x007415C0`
   (alloc 0x630, sets `+0x18`/`+0x218` = board tick, x = col*128,
   y = row*128, stores it in the grid), then its y is moved above the board
   (`y` negative, staggered) with an initial velocity and `+0x188` = the move
   credit id of the column.
4. All new pieces get `flags = 0`, `colour = -1`, then colours from the
   generator (with re-rolls and mode hooks vtable `+0x2D8`, `+0x2DC`,
   `+0x2E0`, `+0x2E4`, `+0x31C`); sometimes a hypercube is placed instead.
   **Colours are final at creation**, before any piece starts falling.
5. `Board+0xE40` is set to the estimated landing time (max over pieces).

## 2. The Piece object

No RTTI/vtable (plain struct). Size **0x630** (static: `push 0x630` before
`call 0x007203B0` at `0x007415E6`). Ctor `0x007203B0` (stack: memory; EBX =
board), alternative ctor `0x007202D0(mem, board)`, init `0x00720120`
(EDX = piece, ESI = id), dtor `0x00720040`, per-tick update `0x00720A50`
(called for every grid piece from Board::Update `0x00767090`).

CurvedVal members (SexyAppFramework animation curve, 0x70 bytes each, ctor
`0x00436940`) at `+0x20, +0x90, +0x100, +0x198, +0x280, +0x2F0, +0x360,
+0x3D0, +0x440, +0x4B8, +0x528, +0x598` (static: ctor `0x007203D6..0x0072044C`).

| Field | Type | Meaning | Evidence | Label |
|---|---|---|---|---|
| `+0x00` | Board* | owning board | ctor `mov [edi], ebx` | static |
| `+0x04` | int | unique piece id (stable across frames; key of board map) | ctor/init | static |
| `+0x08` | int | logical column 0-7 | CreatePiece; swaps exchange it | static |
| `+0x0C` | int | logical row 0-7 | CreatePiece; FillInBlanks `row += 1` | static |
| `+0x10` | float | visual x, board units (rest = col*128) | CreatePiece; swap interpolation `0x00759BA0`; motion loop UpdateGame `0x007691F6` | static |
| `+0x14` | float | visual y (rest = row*128; negative = above the board while entering) | same; falling `0x0073DD70` | static |
| `+0x18` | int | board tick at creation | CreatePiece | static |
| `+0x1C` | float | fall velocity (units/tick, >0 downwards; 0 = not falling) | falling `0x0073DD70`: `y += v`, `v += 0.275*speed`; stop at slot row or piece below; landing sound if v > 2 | static |
| `+0x20` | CurvedVal | scale; constant 1.0 normally; set to a shrink curve when matched; when it reaches 0 and stops, the piece is deleted | init SetConstant(1.0); draw `0x00757E40`; UpdateGame `0x00768FA9..0x00769017` -> vtable `+0x2C0` DeletePiece | static |
| `+0x24` | int | state; value 6 excludes the piece from "still", matching and falling | `cmp [piece+0x24], 6` in `0x00755AD0`, `0x0073DE66`, match finder | static (meaning unknown; no writer found in Board code) |
| `+0x90` | CurvedVal | alpha/brightness used by the draw (init 1.0) | draw `0x00757E5B..0x00757E84` | hypothesis |
| `+0x170` | float | selection pulse phase (0..1, advances 0.02/tick while selected) | UpdateGame `0x007690CA` loop | hypothesis |
| `+0x17C` | byte | special already triggered | TriggerSpecial `0x0074C570` returns early if set | static |
| `+0x17E` | byte | "matchable/landed"; 0 excludes from still and matching (unless flag 0x10000) | IsPieceStill `0x00755AD0`; set 1 by FillInBlanks; set 0 in match code `0x0075D4DE` | static (name hypothesis) |
| `+0x17F` | byte | can be swapped (init 1) | CanSwap `0x0075FAE0` | static |
| `+0x183` | byte | in a match, shrinking | set 1 with the shrink curve (`0x0075D3A4`), cleared by `0x00759BA0`/`0x00755620` | hypothesis |
| `+0x184` | int | short countdown (25 ticks after becoming special), decremented each tick | `0x007494D0`, `0x00749570`; UpdateGame | static (purpose hypothesis: glow) |
| `+0x188` | int | move-credit id (which player move caused it); -1 none | init -1; FillInBlanks; move tracking `0x00759840` | static (name hypothesis) |
| `+0x18C` | int | previous move-credit id | `0x00759840` | static |
| `+0x198` | CurvedVal | slide curve: when non-zero, position = lerp(home, target `(+0x208,+0x20C)*128`) | UpdateGame `0x00769189..0x00769231`; IsPieceStill requires value 0 and not running | static |
| `+0x208/+0x20C` | int | slide target col/row | same | static |
| `+0x210` | int | tick it became a hypercube | `0x00749570` | static |
| `+0x214` | int | tick of last swap | `0x00759BA0` | static |
| `+0x218` | int | tick it last moved/was created | CreatePiece; falling | static |
| `+0x21C` | int | previous colour (before turning colourless) | `0x00749570`, `0x0074C570` | static |
| `+0x220` | int | colour 0-6, -1 = none (hypercube) | init -1; match finder compares it; community enum 0 red,1 white,2 green,3 yellow,4 purple,5 orange,6 blue | static; colour names community |
| `+0x228` | uint | special/type flags (enum: see `board-objects.md`; community flame 1, hypercube 2, star 4, multiplier 0x10, bombs 0x20/0x40, butterfly 0x80) | many | static bits; names community |
| `+0x22C` | uint | flags the piece may not receive (mask) | `0x00749570`, `0x00763E90` test it before adding flags | hypothesis |
| `+0x230` | int | **pending detonation countdown** (ticks); explodes when it reaches 0 | set by TriggerSpecial `0x0074C570` and the match finder; decremented in UpdateGame `0x00768480` loop | static |
| `+0x234` | int | id of the piece that triggered it | `0x0074C570` | static |
| `+0x238` | uint | flags inherited from the triggerer | `0x0074C570` | static |
| `+0x23C` | byte | selected | GetSelectedPiece `0x0073BF00` | static |
| `+0x244` | int | counter (bomb/countdown number) | `0x0073C710` (vtable `+0x350`), init 0 | static; meaning see `board-objects.md` |
| `+0x258/+0x25C` | float | shake offset x/y (from `+0x264` amount) | Piece update `0x00720A50`; draw `0x00757F13` | static |
| `+0x26C/+0x270` | float | rotation angle / speed | UpdateGame | hypothesis |
| `+0x440` | CurvedVal | hint highlight (set by hint `0x00748480`, with `+0x280`/`+0x2F0`) | hud.md | static |
| `+0x608/+0x60C/+0x610` | float | free-flight velocity x / y / y-acceleration (used only while `+0x198` is 0; normally 0) | UpdateGame `0x0076923F..0x00769268` | static |
| `+0x614`, `+0x620/+0x624` | vectors | attached effect objects | ctor, `0x0071FFD0` | static |

Not found as fields: a separate "matched" bitmask or "explosion scale" per
piece other than the curves above, and no per-piece logical "falling" flag -
falling is `+0x1C != 0` or `y != row*128`.

### IsPieceStill (`0x00755AD0`, EAX = piece, stack arg board) - static

All of:
1. `+0x1C == 0.0` (not falling);
2. slide curve `+0x198` not running (`0x00437F30` false);
3. `+0x14 == row*128` (visually in its cell);
4. `+0x24 != 6`;
5. not part of an unfinished swap (`0x0073CAA0(0)`, scans `Board+0xC30`);
6. `+0x17E != 0` or flag `0x10000`;
7. `+0x230 == 0` (no pending detonation);
8. slide curve value == 0.0;
9. flag `0x2000` clear.

A twin `0x00755A70` omits 1-3 (used by the move finder).

## 3. Board state machine and the settled signal

Board per-tick order (static, base classes):

- `Board::Update` `0x00767090` (vtable `+0x3A8`): returns early while
  certain app dialogs are up, while `Board+0x2B40` > 0 (a countdown), in
  replay playback (`+0x12F8` -> `0x00761850`); updates every piece
  (`0x00720A50`); executes queued swaps from `Board+0xC60` whose tick equals
  `Board+0x38` (via vtable `+0x30C`), then calls the game update.
- `UpdateGame` `0x00767E20` (vtable `+0x39C`): `Board+0xE38` (game timer)
  advances unless vtable `+0x194` says time is frozen; then
  - `0x0073C630` temporarily applies in-progress swaps to the grid, calls
    the match finder vtable `+0x324` (`0x0075AF60`, which only matches
    still pieces), and `0x0073C630` again to undo it;
  - unless a hypercube board blast runs (`+0x2EF8`): FillInBlanks (if no
    lightning), move tracking `0x00759840`, UpdateSwapping `0x00759BA0`,
    falling vtable `+0x358` (`0x0073DD70`);
  - per-piece timers (`+0x184`, `+0x230` detonations, counters), shrink
    curves -> DeletePiece, slide/free-flight motion, FillInBlanks again;
  - `if IsBoardStill && no game over && !vtable+0x184 && no move found
    (0x0075A8E0) && !level-complete pending` -> vtable `+0x284`
    (`Board::GameOver` `0x007602C0`);
  - `if vtable+0x2C8` -> `Board+0xE3C` += 1 (still-tick counter);
  - level complete: if `+0x288C` and IsBoardStill and `+0x26B8` and
    `+0xC60` empty -> level-complete announcement and a Hyperspace widget at
    `+0x2890` (`HyperspaceFallback` vtable `0x0086616C`, ctor `0x00723320`,
    or `HyperspaceUltra` `0x00866014`, ctor `0x00726C40`);
  - countdowns `+0xE40` and `+0x2880` decrement.

### Board-level predicates

| Name (hypothesis) | Vtable | Base impl | Returns true when (static) | Overrides |
|---|---|---|---|---|
| **IsBoardStill** | `+0x2E8` | `0x00763E20` -> `0x0075FCA0` (EDI = board) | `[+0xE40] == 0` and every non-NULL cell IsPieceStill and no 3-in-a-row (`0x0073BA40`) and lightning vector `+0x125C..+0x1260` empty. Side effect: the first time, sets `+0x1190 = 1` and calls vtable `+0x230` | SpeedBoard `0x007183E0`: always true while `[+0x30AC] != 0` |
| still-or-swapping | `+0x2C8` | `0x0075FC10` | as above but pieces in a swap are tolerated, and also `[+0x2880] == 0`; no side effects | none |
| **CanPlay** (input gate) | `+0x188` | `0x0073D4F0` | no announcement in the queue `+0x3040` blocks play (byte `+0x181`), board settled once (`+0x1190`), `[+0x3070] == 0`, `[+0xD20] == 0`, `[+0xD1C] == 0` (not game over), time left (vtable `+0x2EC` non-zero), no 0x60-flag bomb at counter 0, no Hyperspace (`+0x2890 == 0`) | quest family `0x006C6310` (also QuestGoal `+0x3180` vtable `+0x114`), MoveLimit `0x006C6500` (moves left), BadMoveLimit `0x006C6C40`, Butterfly `0x006C6BD0`, Filler `0x006C6B10`, Inferno `0x006D3F00`, Poker `0x006C6470` |
| **CanSwap(piece,row,col)** | `+0x304` | `0x0075FAE0` | lightning vector empty, `piece+0x17F`, piece and target IsPieceStill, neither has flag 0x100, vector `+0x26B8` empty, adjacent (or hypercube) | none |
| QueueSwap | `+0x308` | `0x00740E80` | CanSwap, then pushes a 0x14-byte record (tick, piece id, row, col, flags) to `+0xC60` | Butterfly `0x006C8430` |
| time frozen | `+0x194` | `0x0073C330` | `+0x3070 > 0`, `+0x2880 > 0`, `+0xD20 != 0`, dialog 0x12 or 0x27, or lightning running (replay: bit 1 of `+0x130C`) | quest family `0x006C5560`, Poker `0x006C5A00` |
| falling enabled | `+0x198` | `0x0073C2D0` | no dialog 0x12, `+0xE24 == 0`, lightning empty | Poker `0x006C59F0` |

### Board fields that describe activity

| Field | Meaning | Evidence | Label |
|---|---|---|---|
| `+0x38` | board tick counter | compared with queued moves, stored in pieces | static; hypothesis: framework widget update count (100/s) |
| `+0xC30/+0xC34` | vector of swaps in progress, 0x100 bytes each (+0 piece A, +4 piece B or NULL, +8/+0xC direction col/row, +0xF4 int, +0xF8 done, +0xF9, +0xFA) | `0x0073CAA0`, `0x00759BA0`, `0x00755B80` | static (element names hypothesis) |
| `+0xC48/+0xC4C` | vector of moves being resolved, 0xD8 bytes (+0x38 bool, +0x3C move id); an entry is erased when no piece or column still carries its id | `0x00759840` | static ("cascade finished" meaning hypothesis) |
| `+0xC60/+0xC64` | queued swaps, 0x14 bytes | `0x00740E80`, `0x00767090` | static |
| `+0x218 + 4*col` | per-column pending move id | DeletePiece `0x00755B80`, `0x00759840` | static |
| `+0x1F8 + 4*col` | per-column float offset decaying to 0 | UpdateGame, `0x0073DD70` | hypothesis (visual) |
| `+0x125C/+0x1260` | lightning effects (objects 0x2E8 bytes, ctor `0x00789A90`, field +0x34 = piece id) - star/hypercube detonations | `0x0073C9D0`, `0x0074C570` | static (class name unknown) |
| `+0x26B8/+0x26BC` | vector of 0x68-byte items blocking swaps and level completion | CanSwap, `0x00759840`, UpdateGame | static (meaning unknown) |
| `+0xE3C` | ticks the board has been still (vtable `+0x2C8`) | UpdateGame `0x00768423` | static |
| `+0xE40` | fill landing countdown (ticks) | FillInBlanks, UpdateGame | static |
| `+0x2880` | 200-tick countdown set by `0x0075EE20` | blocks still-or-swapping and time | static (purpose unknown) |
| `+0x1190` | board has settled once this game | IsBoardStill side effect; cleared at `0x0074E775` (hypothesis: new game) and restored from saves | static |
| `+0x288C` / `+0x2890` | level complete pending / Hyperspace widget | UpdateGame; hud.md | static |
| `+0x1410`, `+0x1414`, `+0x12F8` | replay active / rewinding / replay playback (button id 3 `0x00742E60` sets the first two) | `0x00763A30`, `0x00767090` | hypothesis (matches hud.md's "suspended") |
| `+0x2B40` | Update countdown that skips game updates | `0x00767090` | static (purpose unknown) |

## 4. Logical grid vs visuals

| Event | Grid changes | Visuals | Evidence |
|---|---|---|---|
| Swap | cells exchanged when the swap animation completes (`0x00759BA0`, static); a bad swap appears to be exchanged back by the same loop (hypothesis, see `board-input.md`). During the animation the grid holds the old positions; inside UpdateGame only, `0x0073C630` swaps them temporarily for the match scan | x/y interpolated from the swap curve | static |
| Match | matched pieces stay in their cells with colour; `+0x20` shrink curve starts | shrink, then DeletePiece -> cell NULL | static |
| Special triggered | `+0x230` countdown, then the explosion deletes pieces | effects | static |
| Refill | survivors moved down and new pieces (with colours) placed in the grid at once | pieces fall over many ticks (`y < row*128`, `+0x1C > 0`) | static |
| Cascade | found only after the fallen pieces are still | step by step | static |

So the mod can read the **next board after each refill immediately**,
before the falls finish (that grid has no holes and final colours, and stays
true unless a later step changes it). It **cannot** know the final
post-cascade board early: cascades are only detected once pieces land, and
refill colours are random at refill time. Read step by step, or wait for
IsBoardStill.

## 5. Polling safety

- The mod's tick is `WM_TIMER` on the game window thread
  (`src/mod/mod_main.cpp`). SexyAppFramework runs its updates on the same
  thread (framework knowledge, **hypothesis** for this exe), so a read
  between messages sees a consistent board: the temporary swap in
  `0x0073C630` and FillInBlanks' half-shifted columns are never seen.
- Read the whole grid in one pass and copy values; do not keep `Piece*`
  across ticks (DeletePiece frees them). Track identity with `+0x04` id.
- Several game updates may run between two timer ticks: transient states
  (a shrink, a pending detonation) can be missed. Use `+0xE3C`, the
  moves vector `+0xC48` and counters instead of expecting every frame.
- Stable while animating: grid pointers, `+0x08/+0x0C`, `+0x04`, `+0x220`,
  `+0x228` (except at the moment a piece transforms). Transient: `+0x10`,
  `+0x14`, `+0x1C`, curve values, `+0x230`, `+0x184`, `+0x170`.
- CurvedVal reads: the cached value at curve `+0x38` (double) is only
  refreshed when the game calls GetOutVal (`0x00437C60`); prefer the
  logical fields above.
- Calling vtable `+0x2E8` from the mod is possible (thiscall, no args,
  returns AL) but has the first-settle side effect; replicating it from
  fields (as a mod-side `IsBoardStill`) is safer.

## 6. Reading recipe (for a later design)

```
board = [gApp + 0xBE8]                       ; hud.md
for row 0..7, col 0..7:
    p = [board + 0xF8 + 4*(col + 8*row)]
    if p == 0: empty
    else: id [p+4], colour [p+0x220], flags [p+0x228],
          falling = [p+0x1C] != 0 or [p+0x14] != row*128,
          detonating = [p+0x230] > 0, selected = byte [p+0x23C]
settled   = replicated IsBoardStill (section 3) ; announce on rising edge
can_input = replicated vtable+0x188 for the mode, or (simpler) settled
            plus no game over / hyperspace / blocking announcement
```

## Uncertainties

- Names of `+0x17E`, `+0x183`, `+0x22C`, `+0x24`, `+0x26C`, `+0x90`.
- Meaning of `+0x2880`, `+0x26B8`, `+0x2B40`, `+0x3070`, `+0xE24`, `+0x30AC`
  (Speed), replay fields.
- That `+0x38` ticks at 100/s and that updates never run off the window
  thread.
- Quest boards (FillInBlanks override `0x006DD4D0`, Dig falling
  `0x00736D60`) may leave holes or move pieces differently (Dig scrolling
  is `board-flow.md`'s).

## Runtime checklist (throwaway profile, Classic first)

1. Grid: dump 64 cells; all non-NULL at rest; `[p+8] == col`, `[p+0xC] ==
   row`, `[p+0x10] == col*128`, `[p+0x14] == row*128`; colours 0-6 match the
   screen (community colour names).
2. Piece size/ids: ids unique; `[board+0xF4]` > every id; ids survive falls.
3. Make a match: during the shrink the matched cells still hold pieces with
   colour and `+0x183 = 1`; then NULL briefly (or never visible at timer
   rate); new pieces appear with negative `+0x14`, `+0x1C > 0` and colours
   already set.
4. Settled: replicate IsBoardStill each tick and log its edges with
   `+0xE40`, `+0xE3C` (should increase only while still), lightning vector
   size `+0x1260 - +0x125C`; confirm true at rest, false from swap to the
   end of the cascade.
5. Input during cascades: start a long cascade and swap two still gems
   elsewhere (with the keyboard); confirm the game accepts it.
6. Specials: trigger a flame gem; watch `+0x230` count down and `+0x17C`.
7. Level up (Classic): `+0x288C` then `+0x2890` non-zero (Hyperspace);
   CanPlay false meanwhile.
8. Lightning: on the timer screen, confirm the timer stops (`+0xE38`
   frozen) while a star/hypercube effect runs (`+0x125C` non-empty).
9. Check `+0x24` values seen at runtime and whether 6 ever appears.

> Correction (2026-10-30, docs/re/board-flow.md): `0x00767090` is Board::DoUpdate (vtable +0x3A8); Board::Update is `0x00759170` (+0x58); UpdateGame is `0x00767E20` (+0x39C).
