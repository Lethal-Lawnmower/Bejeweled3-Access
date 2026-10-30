# Board input: cursor, selection, swaps, legality, hints, move finder

Research agent 2 of 6, 2026-10-30. Static analysis only (Ghidra 12.0.4
headless, read-only copy `private/ghidra-b2`, Steam `Bejeweled3.exe`,
base 0x400000). Nothing was run against the game. Brief:
`docs/re/board-research-brief.md`.

Labels: **static** = read from decompilation/disassembly at the cited
address; **string-table** = the function name comes from the source
comment next to a string id in `private/pak/properties/default.xml`
(e.g. `<!-- Board::MouseDown:13357 -->`); **hypothesis** = inferred, not
proven; **runtime** = nothing in this doc (see checklist at the end).

Board pointer: `[gApp+0xBE8]`, gApp `[0x008E1730]` (= `[0x008E170C]`,
see `hud.md`). Piece grid `[Board+0xF8+4*(col+8*row)]`. Piece fields used
below: `+0x08` column, `+0x0C` row (static: KeyDown adds the column delta to
`+0x08` and the row delta to `+0x0C` before QueueSwap; IsSwapAllowed indexes
the grid with them), `+0x10/+0x14` drawn x/y (float, board pixels, 128 per
cell), `+0x220` colour, `+0x228` flags, `+0x23C` selected (byte).

## Key result for design

The game already has **complete native keyboard play** on every board
class: a keyboard cursor, select, and swap in four directions. Every board
class uses the same `Board::KeyDown`, `MouseDown`, `MouseUp`, and
`MouseDrag` (vtable comparison below). A mod does not need to inject swaps.
It can let the player's keys reach the game and **announce** the cursor
cell (`+0x2C44/+0x2C48`), the selected gem (`+0x23C`), and the result of
each swap. The only actions without a key are **Hint** (button id 0) and
the in-game **Reset** and Replay buttons. For extra help, the move finder
`0x0075A8E0` can list every legal move.

## 1. Native keyboard control

### Dispatch and vtable evidence (static)

WidgetManager sends WM_KEYDOWN to the focus widget's vtable `+0xC0`, and
WM_CHAR to vtable `+0xBC` (`screen-names-and-keys.md`). Vtable slots were
read from the exe with `private/ghidra-b2/vt.py`:

| class | +0xC0 KeyDown | +0xBC KeyChar | +0xD4 MouseDown | +0xE4 MouseUp | +0xE8 MouseDrag | +0x308 QueueSwap |
|---|---|---|---|---|---|---|
| Board, Classic | 0x007472B0 | 0x0073B670 | 0x0073F140 | 0x0073D990 | 0x0073C510 | 0x00740E80 |
| Speed / Zen | same | thunks 0x00714120 / 0x0070E490 to 0x0073B670 | same | same | same | same |
| Quest, Dig, BadMoveLimit, Balance, Filler, Inferno, MoveLimit, NoLose, Poker, RealTimeBomb, TimeBomb, TimeLimit | same | goal vtable+0xC8 then 0x0073B670 | same | same | same | same |
| Butterfly | same | same as Quest | same | same | same | **0x006C8430** (see 3.5) |

### Fields (static)

| field | type | meaning | written by |
|---|---|---|---|
| `+0x2C44` | int | keyboard cursor column, -1 = cursor not shown | KeyDown; set to -1 by the ctor `0x0074AE30`, MouseDown `0x0073F140`, GameOver `0x007602C0`, and `0x00719350` (the SpeedBoard vtable+0x284 handler) |
| `+0x2C48` | int | keyboard cursor row, -1 = none | same |
| piece `+0x23C` | byte | selected flag. GetSelectedPiece `0x0073BF00` returns the first grid piece with it set | KeyDown, MouseDown/Up, ExecuteSwap (player swap clears it) |
| `+0x1410` | byte | all input off: KeyDown, KeyChar, MouseDown, and ShowHint return immediately. Set by `0x00742E60` (button id 3) and cleared at the end of `0x00763A30`. Hypothesis: replay playback; see board-flow | |
| `+0x2978` | byte | Escape ignored. Set to 1 in `0x00756730` (vtable+0x3A0, leave board / return to menu) | |
| `+0xE28` | byte | time expired: cursor and swap keys ignored. Set to 1 in `0x0073B890` (vtable+0x360) when the timer fraction drops to the threshold, just before it calls vtable+0x284 | |
| `+0x12C4` | byte | user pause (`hud.md`) | KeyChar; a mouse click clears it |
| `+0x27D4` | byte | mouse button held, for drag | MouseDown 1, MouseUp 0 |
| `+0x27D8/+0x27DC` | int | mouse-down x/y | MouseDown |
| `+0x27E0` | Piece* | piece that was selected at the last mouse-up (second click deselects) | MouseUp |

**When the cursor is drawn (static).** `0x007568A0` is a board draw pass
(it also draws the swap records). It draws image global
`[0x008D95B4]` at cell (`+0x2C44`, `+0x2C48`) only while `+0x2C44 != -1`
**and no gem is selected**. The image resource id is not resolved. The
alpha comes from board vtable+0x2BC. While a gem is selected, the gem's
own selection animation (piece `+0x100` CurvedVal, started by
`0x00437760`) replaces the cursor image.

### KeyDown `0x007472B0` (thiscall, arg = virtual key, `RET 4`; static)

The code runs in this order. A later step runs only if no earlier step
returned.

1. If `+0x1410` is set, return.
2. Switch on the key:
   - **Escape (0x1B)**: unless `+0x2978` is set, calls ButtonDepress(1) on
     the board's listener subobject `[+0xBC]` slot 2, which opens the
     in-game menu (`hud.md`). This happens before the busy checks below.
   - **Space (0x20)**: if a gem is selected, clears its `+0x23C`, resets
     its selection curve, and goes on with no direction set. Otherwise it
     goes on in select mode with no direction.
   - **Arrows (0x25-0x28)**: move mode. dx/dy are -1/0/+1. dx is the
     column, dy is the row; Up is dy = -1.
   - **A / D / S / W (0x41, 0x44, 0x53, 0x57)**: select mode with the
     direction left / right / down / up.
3. Return if `+0xE28` is set (time expired) or vtable+0x188 CanPlay is
   false (see section 5).
4. **Select mode with nothing selected.** Find the target gem:
   - If the cursor is shown, take the piece drawn at the centre of cell
     (`+0x2C44`, `+0x2C48`): PieceAtPixel `0x0073CBB0` with x = col*128 +
     vtable+0x2AC() + 64 and y = row*128 + vtable+0x2B0() + 64.
   - If the cursor is -1, take the piece **under the mouse pointer**
     (WidgetManager `[board+0x20]` +0x10C/+0x110, divided by the screen
     scale).

   Then:
   - If the piece has flags & 0x1800 (Detonator / Scrambler, see 3.4) and
     the cursor is shown, the piece becomes the swap source without being
     selected.
   - Otherwise, Space with the cursor at -1 does nothing in KeyDown. If
     the piece is not in an active swap (`0x0073CAA0`), set `+0x23C` = 1.
5. With no direction and no source, return. Space has finished here.
6. The source is the step-4 piece or the selected gem. If there is one:
   - If vtable+0x194 (input blocked, section 5) is true, or **QueueSwap**
     vtable+0x308(source, source.row+dy, source.col+dx, 0, 1, 0) returns
     false: deselect the source, reset its curve (curve string
     `0x00860DB0`), and return without moving the cursor.
   - On success, go to step 7.
7. Cursor update:
   - If the cursor is -1: select mode returns. In move mode the cursor
     appears at col = 3 (+1 when moving right) and row = 3 (+1 when moving
     down). The first arrow press shows the cursor near the centre; it
     does not move it.
   - If the cursor is shown: only move mode moves it by (dx, dy), clamped
     to 0..7.

Consequence: when an arrow swaps a selected gem, the cursor follows the
gem to its new cell. After a W/A/S/D swap, the cursor stays where it was.

### KeyChar `0x0073B670` (static)

Space toggles user pause `+0x12C4` only when all of these hold:
`+0x1410` is clear, the keyboard cursor was never shown (`+0x2C44 == -1`),
and vtable+0x1AC (timed board) is true or the board is already paused.
**Once the keyboard cursor has appeared, Space no longer pauses.** It
selects instead, until a mouse click or game over resets the cursor to -1.
Quest-type boards first forward the character to the goal (vtable+0xC8).
Those handlers are empty (`screen-names-and-keys.md`); Inferno forwards it
twice (`0x006C5BD0`).

### Per-key table

| key | cursor hidden (-1), nothing selected | cursor shown, nothing selected | a gem is selected |
|---|---|---|---|
| Arrow | Shows the cursor at (3 or 4, 3 or 4) | Moves the cursor one cell, clamped at the edges | Queues a swap of the **selected gem** in that direction. Success: the cursor follows the gem (or appears, if hidden). Failure: deselects |
| W / A / S / D | Selects the gem under the **mouse pointer** and swaps it up / left / down / right. No pointer gem: nothing | Selects the gem at the cursor and swaps it in that direction. Failure deselects. The cursor does not move | Swaps the **selected** gem (not the cursor gem). Failure deselects |
| Space (KeyDown) | Nothing | Selects the gem at the cursor. On a Detonator or Scrambler: **activates it** (zero-distance swap, 3.4) | Deselects |
| Space (KeyChar) | Toggles pause on timed boards | Nothing (cursor shown) | Nothing |
| Escape | In-game menu (dialog 21), unless `+0x2978` is set | same | same |
| Enter, H, other keys | Not handled by the Board | | |

The board **has no hint key**. A hint comes only from button id 0 or the
automatic hint.

**Key repeat (hypothesis).** Board KeyDown does no repeat filtering, and
WidgetManager::KeyDown only sets `mKeyDown[key]` before it dispatches. So
Windows auto-repeat WM_KEYDOWN messages probably move the cursor once per
repeat. Not verified; the driver may drop repeats.

**Mode differences (static).** None in the key handlers themselves. They
differ only through the virtual guards (vtable+0x188 / +0x194), QueueSwap
(Butterfly), and the timer (`+0xE28`).

## 2. Mouse path

### MouseDown `0x0073F140` (string-table name; thiscall x, y, button, clicks; static)

1. Return if `+0x1410` is set.
2. If paused (`+0x12C4`, or pause alpha `+0x12CC` above a constant), the
   click **unpauses** and returns.
3. Return if vtable+0x188 CanPlay is false.
4. Set the cursor to -1. Store x/y at `+0x27D8/+0x27DC` and set `+0x27D4`
   = 1.
5. Convert the click to a cell: logical 1200-high coordinates, 128 px
   cells (`0x0073CC10` / `0x0073CBE0`). Also find the piece under the
   click.
6. Button != 0 (right or middle): deselect and stop.
7. If the clicked piece is the selected one: do nothing. The deselect
   happens on mouse-up (see MouseUp).
8. Nothing selected:
   - If the clicked piece has flags & 0x1800 (`0x0071FF40`), call
     QueueSwap(piece, its row, its col, 0, 1, 0). This **click-activates
     a Detonator or Scrambler**.
   - Otherwise select it (`+0x23C` = 1) and play the select sound.
9. Something selected:
   - If the vector `+0x125C/+0x1260` is non-empty, do nothing.
   - Otherwise QueueSwap(selected, clicked row, clicked col, 0, 1, 0).
   - If QueueSwap fails (not adjacent, piece busy, and so on), deselect
     the old gem. Then:
     - If profile stat 20 (`[[gApp+0xB80]+0x110]`, lifetime legal moves,
       see 3.3) is below 3, show HintDialog **0x12** with strings 152-154.
       These are the title, an explanation that the two gems must be
       adjacent, and OK. The dialog is positioned beside the board.
     - Otherwise **select the clicked gem instead**.
10. A piece with byte `+0x17F` = 0 (not swappable) cannot be the click
    target. It takes the deselect path.

### MouseUp `0x0073D990` (static)

- `+0x27D4` = 0.
- If the selected gem equals `+0x27E0` and is not in an active swap,
  deselect it. Clicking a selected gem a second time deselects it on
  release.
- Otherwise store the selected gem in `+0x27E0`.

### MouseDrag `0x0073C510` (static)

Needs all of: CanPlay true, a selected gem, and the button held. When the
drag passes a threshold of `40 * screenHeight / 1200` pixels on either
axis, it uses the dominant axis and calls QueueSwap(selected, row±1 or
col±1, 0, 1, 0). It stays in bounds.

### All player actions on the board (static unless noted)

- **Select** a gem, then **swap** it with an orthogonal neighbour. Keys:
  arrows after a select, or W/A/S/D. Mouse: click and click, or drag.
- **Deselect**: Space, right click, a second click, or a failed swap.
- **Hypercube**: swapping a hypercube (flag 0x02) with any neighbour
  activates it immediately with that neighbour (ExecuteSwap, 3.2). The
  same input as a normal swap; nothing special.
- **Detonator / Scrambler** (flags 0x800 / 0x1000, together 0x1800):
  activated in place. Mouse: click it with nothing selected. Keys: Space
  on it while the keyboard cursor is shown. The 0x800 = Detonator naming
  is a hypothesis: Board::GetTooltipText `0x007476E0` tests 0x800 just
  before 0x1000, and strings 127/128 name a click-to-detonate gem. 0x1000
  triggers the board-scramble branch of ExecuteSwap. Exact names are left
  to `board-objects.md`.
- **Pause and unpause**: Space (KeyChar), and any click unpauses.
- **HUD buttons** (children of the board; `hud.md`): Hint (0), Menu (1),
  Reset (2), id 3 (`0x00742E60`, sets `+0x1410`).
- Diamond Mine (DigBoard), Poker, Butterflies, Ice Storm (InfernoBoard),
  and quests use the same Board input handlers. No extra board click
  action was found in them. Quest-goal specific input (e.g. hidden-object
  quests) is **not verified**. The goal KeyChar handlers are empty, and
  goals have no MouseDown hook in the Board path.

## 3. Swap pipeline and legality

Every input path calls **QueueSwap** (vtable+0x308). Nothing is moved
there. The swap starts on the next Update, and legality (a match is made)
is decided when the swap animation reaches its midpoint.

### 3.1 IsSwapAllowed, vtable+0x304 = `0x0075FAE0` (thiscall piece, row, col; `RET 0xC`; static)

Returns false when any of these holds:

- the vector `+0x125C` is non-empty;
- piece `+0x17F` = 0;
- the piece or the target is not still (`0x00755AD0`: drawn position on
  its cell, not in a swap, `+0x17E` or flag 0x10000, `+0x230` = 0, curve
  idle, no flag 0x2000);
- the piece or the target has flag 0x100;
- the vector `+0x26B8` (0x68-byte elements) is non-empty.

Distance rule:

- normal pieces need Manhattan distance 1;
- pieces with flags & 0x1800 need distance 0 (themselves).

It does **not** check for a match.

### 3.2 QueueSwap and ExecuteSwap

- **QueueSwap**, vtable+0x308 = `0x00740E80`. Thiscall
  `(Piece*, int row, int col, bool force, bool player, bool flag6)`,
  `RET 0x18`, returns AL (static).
  - Calls IsSwapAllowed. If it passes, appends a 0x14-byte record
    `{update tick [board+0x38], piece id [piece+4], row, col, force,
    player, flag6}` to the queue at `+0xC54`.
  - Also appends it to `+0x1338` when vtable+0x1C0 is true. Board returns
    0 (`0x0073C200`), so not in practice.
  - Input handlers pass `(0, 1, 0)`.
- **Queue consumer** `0x00767090` (Update). For each record whose tick
  equals the current tick, it looks the piece up by id (`0x0073E370`) and
  calls **ExecuteSwap**, vtable+0x30C = `0x0075EE20`, with the same 6
  args. If the id is not found, it sets `+0x1413`.
- ExecuteSwap checks the args (row and col 0..7) and IsSwapAllowed again.
  Then:
  - `player`: store the piece colour in `+0x1234` and deselect the piece.
  - Piece flag 0x1000: board scramble branch.
  - Hypercube with a neighbour (`+0x182` byte on the partner):
    vtable+0x2CC(hypercube, partner) immediately. Board `0x00740F20`;
    Quest and Dig forward to goal+0x4C (`0x006C7C20`); Poker `0x006CFA50`.
  - Otherwise it builds a 0x100-byte swap record (piece, partner, dx, dy,
    flags) in the vector `+0xC30` (pushed through `+0xC24`), starts the
    swap curve, and resets the idle-hint counter `+0x26A4`.
- `force` and `flag6` are copied into the record bytes that UpdateSwapping
  reads as "skip legality" (`+0xF9`) and "single-piece move" (`+0xFA`).
  The mapping comes from the adjacent stores at `0x0075FA65..0x0075FA79`
  (static). The exact record base is ±4 uncertain.

### 3.3 Legality: Board::UpdateSwapping `0x00759BA0` (string-table name; static)

For each swap record:

- Animate both pieces.
- At the midpoint, exchange them in the grid and call vtable+0x324
  (match check; Board `0x0075AF60`, Poker `0x006D90A0`):
  - **1** = match. **Legal move.**
    - AddStat(15, +1) and AddStat(20, +1). AddStat `0x0073D170`: ECX =
      stat id, ESI = board, args (delta, -1). It writes board
      `+0xC84+4*id` and profile `+0xC0+4*id`.
    - vtable+0x250 when the same colour as the last swap is involved.
    - vtable+0x2FC **OnMoveMade**. Board `0x0073BB60`; MoveLimitBoard
      `0x006C5A60` decrements moves left `+0x324C`; Quest `0x006C5650` →
      goal+0x60.
  - **2** = wait. Keep retrying up to 400 ticks (record `+0xF4`).
    Hypothesis: pieces not yet settled.
  - **0** = no match and not forced (record `+0xF9` = 0 and vtable+0x184
    false; Board `0x0073C3B0` returns 0). **Illegal move.**
    - AddStat(15, +1) only.
    - vtable+0x300 **BadMove**(record): Board `0x0073BB50` is empty;
      BadMoveLimitBoard `0x006C6050` increments `+0x324C`; Quest
      `0x006C5670` is empty.
    - If profile stat 20 < 3, show HintDialog **0x12** with strings
      114-116 (title; explanation that a swap must make a row of three;
      OK).
    - Play a sound, clear record `+0xF0`, and start the **swap-back**
      animation. The second pass swaps the gems back.

So stat 15 counts swap attempts that reached the board, and stat 20 counts
legal moves. Bad moves = stat15 - stat20. The `+0x110` value tested before
both tutorial dialogs is profile stat 20 (`0xC0 + 4*20`). Both dialogs
therefore appear only during a profile's first two legal moves.

### 3.4 Click-activated pieces (static, names hypothesis)

Flags & 0x1800 change IsSwapAllowed to distance 0. Both MouseDown and
KeyDown then QueueSwap the piece onto its own cell. ExecuteSwap handles
0x1000 with a board scramble (loop over still pieces, up to 1000 tries,
`0x0075EE20` lines for flag 0x1000). 0x800 falls through to the normal
record path. Its effect is decided later, in the match / special code
(`board-events.md` / `board-objects.md`).

### 3.5 Mode differences in the swap path (static)

| board | difference |
|---|---|
| ButterflyBoard | QueueSwap `0x006C8430` refuses a player swap while any non-forced swap record exists. One swap at a time |
| MoveLimitBoard | CanPlay `0x006C6500` needs moves left `+0x324C` != 0. OnMoveMade decrements it |
| BadMoveLimitBoard | CanPlay `0x006C6C40` needs `+0x324C` != `+0x3248` (bad moves < limit). BadMove increments `+0x324C`. vtable+0x1F0 `0x006CCB70` formats the remaining count `+0x3248 - +0x324C`. **This is the quest bad-move penalty** |
| InfernoBoard (Ice Storm) | CanPlay `0x006D3F00` also needs `+0x3CC8 == -1` and an idle curve |
| FillerBoard | CanPlay `0x006C6B10` needs byte `+0x3251` |
| PokerBoard | CanPlay `0x006C6470` waits for five curves and hand state. Own match check `0x006D90A0` |
| Butterfly CanPlay `0x006C6BD0` | false while any piece has `+0x20C == -1` |
| Quest/Dig | CanPlay `0x006C6310` first asks goal vtable+0x114. Input blocked `0x006C5560` asks goal vtable+0x94 |

## 4. Hints and the move finder

### 4.1 FindMove `0x0075A8E0` (static)

**Convention:** EAX = Board*. Five stack args, callee-cleaned (`RET
0x14`); returns AL:

`bool FindMove(int out[4] /*or NULL*/, int index, bool reverseRows,
Piece* mustInclude /*or NULL*/, bool specialOnly)`

**What it does:**

- Scans rows 0..7, or 7..0 when `reverseRows`, and columns 0..7.
- An **eligible** piece: non-null; `+0x24 != 6`; not in a swap; `+0x17E`
  set; `+0x230 == 0`; curve idle; no flags 0x2100; `+0x17F` set.
- Each eligible piece is tried in directions (+1,0), (-1,0), (0,+1),
  (0,-1). For each, it **temporarily swaps the grid pointers**, measures
  the horizontal and vertical run of the gem that landed in the original
  cell, and swaps back. Run members must pass the same checks and lack
  flag 0x2000.
- **Valid move:** a run of 3 or more, or the moving piece is a hypercube
  (flag 0x02) with a non-null neighbour.
- With `specialOnly`: only moves that make both a horizontal and a
  vertical run of 3 (star shape), a run of 4 or more, or a hypercube move.
- With `mustInclude`: only moves that involve that piece.
- Returns the **index-th** valid move (0-based):
  `out = {col1, row1, col2, row2}`. `(col1,row1)` is the cell the
  matching gem moves into. `(col2,row2)` is the gem that makes the match.
  It is the piece the hint highlights.
- **Listing every legal move:** call with index 0, 1, 2, ... until it
  returns false. A swap that matches on both sides is reported twice
  (once from each gem), so de-duplicate unordered cell pairs.
- Results are meaningful only while the board is still. Moving or
  animating pieces are skipped.

**Callers:**

- "No more moves": Update `0x00767E20` at `0x007692DC` calls
  `FindMove(NULL,0,0,0,0)`. It runs only when the board is still
  (`0x0075FCA0`), `+0xD1C == 0`, vtable+0x184 is false, and `+0x288C ==
  0`. When no move is found it calls vtable+0x284(1), the out-of-moves
  handler:
  - Board: GameOver `0x007602C0`.
  - Classic `0x0073B2D0`: GameOver, plus a profile counter.
  - Zen `0x0070E4A0`: vtable+0x29C(random piece, 1). Hypothesis: it
    converts a gem so play continues.
  - Speed `0x00719350`.
  - Quest `0x006D8E30`.
  - Filler `0x006C50F0`: `+0x324C += 3`.
  - Inferno `0x006E0450`.
  - Dig `0x00737500`.

  Details belong to `board-flow.md`.
- Board generation and reshuffle (`0x007604D0`) and quest code use the
  same finder.

### 4.2 Hint (static)

- **ShowHint**, vtable+0x328 = `0x0075AEB0`. Thiscall `(bool fromButton)`,
  `RET 4`.
  1. Return if `+0x1410` is set.
  2. If `fromButton`: set **`+0x26A0` = 250** (button cooldown ticks).
     Always clear the idle counter **`+0x26A4`**.
  3. If `fromButton` or **`+0x1370`** (show automatic hints; 1 from the
     ctor; quests set it from the quest parameter `ShowAutohints` ==
     `TRUE`, `0x006DE45C`), run FindHintMove.
  4. FindHintMove `0x0075AE10`: EDI = `int out[4]`, stack Board*, `RET 4`.
     It picks a random direction (Rand(2)) and a random start index
     (Rand(10)), then tries indices downward, then the other direction.
  5. The hinted piece is `grid[col2,row2]`, or `grid[col1,row1]` when that
     one is a hypercube.
  6. Calls **HintFlash** `0x00748480`: ECX = Piece*, stack (Board*, bool
     fromButton), `RET 8`. It starts three curves on the piece. Zen
     (vtable+0x1C4 = 1) stretches them. With `fromButton` it also spawns
     a 0x1C8-byte effect object at the piece.
- **Hint location is not stored.** Hook `0x00748480` (ECX = piece; read
  piece `+0x08/+0x0C`), or compute it with FindMove.
- **Automatic hint:** each Update tick while the board is still, `+0x12C8`
  == 0, and CanPlay, `++[+0x26A4]`. When it equals vtable+0x1A8() × 100,
  Update calls ShowHint(false) (`0x00769401`). Delay seconds: Board 15
  (`0x0073C240`); Speed 5 (`0x00714110`); Inferno 15, or 5 when byte
  `+0x3184` is set (`0x006C5CC0`). The counter is reset by ShowHint, by
  ExecuteSwap, by `0x0074C070` (match processing), and by `0x00764AB0`
  (load).
- **Button cooldown:**
  - Update decrements `+0x26A0`. While it is above 0, the hint button
    `[+0x286C]` has mMouseVisible (`+0x65`) = 0, so it is not clickable.
  - SpeedBoard `0x007116C0` caps `+0x26A0` at 50.
  - Separately, Board::ButtonDepress `0x0074BC40` case 0 calls ShowHint
    only when float `+0x12C8` == 0 and vtable+0x33C is true (`hud.md`).
- **Penalty:** none found. ShowHint, FindHintMove, HintFlash, and the
  button handler change no score, stat, or counter other than the timers
  above. This is not exhaustive: a mode could react to the effect object.

## 5. Calling these from a mod, and the guards

| function | address / slot | convention | safe use |
|---|---|---|---|
| GetSelectedPiece | `0x0073BF00` | ESI = Board*, returns EAX | read-only; easier to scan the grid for `+0x23C` |
| PieceAtPixel | `0x0073CBB0` | ECX = x, EAX = y (board px incl. origin), EDX = Board* | read-only |
| IsSwapAllowed | vtable+0x304 `0x0075FAE0` | thiscall (piece,row,col) RET 0xC | read-only check |
| QueueSwap | vtable+0x308 `0x00740E80` (Butterfly `0x006C8430`) | thiscall (piece,row,col,0,1,0) RET 0x18 | **the entry point all input uses** |
| ExecuteSwap | vtable+0x30C `0x0075EE20` | same args | do not call directly (bypasses the queue and replay) |
| FindMove | `0x0075A8E0` | EAX = Board*, 5 stack args, RET 0x14 | **game thread only**; it writes the grid temporarily |
| ShowHint | vtable+0x328 `0x0075AEB0` | thiscall (bool) RET 4 | `true` = same as the button (sets the cooldown) |
| KeyDown / KeyChar | `0x007472B0` / `0x0073B670` | thiscall (vk / wchar) RET 4 | equivalent to real keys |

**Guards a mod should test before acting (static):**

- `+0x1410` == 0 (input off / replay).
- `+0xE28` == 0 (time not expired).
- `+0xD1C` == 0 (game over; `hud.md`).
- vtable+0x188 **CanPlay**. Base `0x0073D4F0` requires all of:
  - no object in `+0x3040` with byte `+0x181`;
  - `+0x1190` set (the board has settled once);
  - `+0x3070`, `+0xD20`, and `+0xD1C` all zero;
  - vtable+0x2EC (ticks left) != 0;
  - no bomb (flags 0x60) with counter `+0x244` == 0;
  - `+0x2890` == 0.
- vtable+0x194 **InputBlocked**. Base `0x0073C330` is true when any of:
  - `+0x3070` > 0, `+0x2880` > 0, or `+0xD20` != 0;
  - HintDialog 0x12 or UnlockDialog 0x27 is open;
  - the vector `+0x125C` is non-empty.

  It reads bit 1 of `+0x130C` when the override `+0x12F8` is set.
- Pause: user pause `+0x12C4` (a click clears it), and IsGamePaused
  vtable+0x19C `0x0073C260`.

**Recommendation:**

- Prefer real keys, delivered to the focused board. They reproduce every
  guard, the deselect and failure behaviour, and the cursor bookkeeping.
- Read state by polling: `+0x2C44/+0x2C48`, piece `+0x23C`, stats
  `+0xC84+4*15` / `+0xC84+4*20`, and `+0x26A0/+0x26A4`.
- Or hook vtable+0x300 BadMove and +0x2FC OnMoveMade per class for exact
  move results.
- If a direct swap is needed (e.g. "swap cell A with cell B"), call
  QueueSwap on the game thread after the guards. The QueueSwap return
  value only means "started". Legality arrives later (3.3).
- **Stability:** all of these fields are stable while animations play.
  `+0x23C` may be cleared at any time by a failed swap. The cursor fields
  change only on input and the resets listed in section 1.

## Uncertainties

- Meaning of `+0x1410` (replay is a hypothesis), `+0x3070`, `+0xD20`,
  `+0x2880`, `+0x2890`, `+0x1190`, the vector `+0x125C`, and the
  vector `+0x26B8`. They are only used here as guards.
- The swap record base offset (±4) on the producer side. The consumer
  offsets come from UpdateSwapping.
- Flag names: 0x100 (unswappable), 0x800 / 0x1000 (Detonator /
  Scrambler), 0x2000, 0x10000. These are lead names only.
- Whether OS key repeat reaches Board KeyDown.
- Return value 2 of the match check.
- The keyboard cursor image resource id (`[0x008D95B4]`).
- `0x00719350` (SpeedBoard vtable+0x284), which also resets the cursor:
  its trigger is not analysed.
- Quest-goal specific mouse actions were not searched exhaustively.

## Runtime checklist (lead, throwaway profile)

1. In Classic, before any key: `+0x2C44 == -1`. Press Right once: expect
   (4,3). Then Left ×10: the column clamps at 0. Click a gem: the cursor
   returns to -1.
2. With the cursor shown, press Space: the gem at the cursor gets `+0x23C`
   = 1 and the cursor image disappears. Press Space again: it is cleared.
3. In a timed mode (Lightning), Space before any arrow toggles `+0x12C4`.
   After an arrow press it does not.
4. Press W/A/S/D with the cursor hidden and the mouse over a gem: that gem
   swaps.
5. Make an illegal swap. Board stat `[+0xC84+0x3C]` (15) rises and
   `[+0xC84+0x50]` (20) does not. On a fresh profile (profile
   `[+0xB80]+0x110` < 3) HintDialog 0x12 appears.
6. Make a legal swap: both stats rise and the cursor follows the gem
   (arrow swap).
7. Press the hint button: `+0x26A0` = 250 and counts down, and the button
   `+0x65` is 0 meanwhile. Hook or break on `0x00748480` and read ECX
   `+0x08/+0x0C`.
8. Idle in Classic about 15 s: `+0x26A4` reaches 1500 and an automatic
   hint shows.
9. On the game thread, call FindMove with index 0..n and compare with the
   hint and a manual grid check. Check that the grid is unchanged
   afterwards.
10. Hold an arrow: count cursor steps to settle the key repeat question.
11. In a quest with a move limit or bad-move limit, watch `+0x324C` /
    `+0x3248` on legal and illegal swaps.

## Tools and reproduction

- Wrapper `private/ghidra-b2/run.sh SCRIPT OUT ARGS` (headless,
  read-only), with `DecompileRefs.java`, `ControlsDisasm.java`,
  `ActionsGrep.java`, and the new **`tools/ghidra/BInputRange.java`**
  (prints instruction ranges and strings).
- `private/ghidra-b2/vt.py EXE VTABLE OFFSETS..` reads vtable slots.
  `vtdiff.py EXE A B N` diffs two vtables.
- Decompilation outputs: `private/ghidra-b2/k1.c`..`k9.c`, `*.asm`,
  `g*.txt`, `r*.txt`.
