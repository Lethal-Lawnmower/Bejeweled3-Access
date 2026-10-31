# Butterflies mode (ButterflyBoard)

Source: `src/mod/mode_butterflies.cpp` (`ButterflyHooks()`, see
`src/mod/mode_info.h`). Date: 2026-10-30. Exe: Steam `Bejeweled3.exe`
(SHA-256 `d5516244...88c3`).

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Nothing here has been run in the game or heard through Prism.** Every
field is a static read of the decompilation; none is runtime-verified.

Method: `private/ghidra-b4/all.c` (extractor `private/poker-bfly/fn.sh`),
capstone scans (`private/ghidra-b4/py/opscan.py`, `dis.py`), ButterflyBoard
overrides from `private/ghidra-b4/vtmap.txt`. Builds on
`docs/re/board-objects.md` (Butterflies section) and resolves its "move-up
code and piece `+0x20C`" uncertainty.

## How the mode works (static)

- Butterflies are grid pieces with flag `0x80` and a colour. They spawn in
  the bottom row (spawn `0x006D7C70` writes grid row 7 cells
  `Board+0x1D8+4*col`).
- ButterflyBoard UpdateGame `0x006E82A0`: when a butterfly's counter
  `+0x244` is below 1 and the board is idle, it moves to the row above
  (vtable+0x308 with row-1) and the counter is reloaded. So `+0x244` = moves
  until that butterfly moves up one row.
- A butterfly in row 0 that is due to move up escapes: `+0x208` = column,
  `+0x20C` = -1. The spider (float `+0x3288`, in columns) heads for the
  butterfly nearest the top; when it is over an escaped butterfly the game
  ends (vtable+0x284 GameOver).
- Danger (vtable+0x368 `0x006C6B50`): any butterfly in row 0.
- Matching a butterfly with same-coloured gems removes it (freed).

## What is spoken

| Hook | Says (examples) |
|---|---|
| `cellText` (after the gem name, which already contains the game's BUTTERFLY title) | `2 rows below the top row`; `in the top row, under the spider`; `escaping to the spider` |
| `briefStatus` (F6) | `5 butterflies, nearest the top column 3, row 2, spider above column 3` |
| `details` (M) | `spider above column 3. 5 butterflies, from the top: Red column 3, row 2; Blue column 6, row 4; ...` |
| `tick` events | `new Green BUTTERFLY, column 4, row 8`; `Red BUTTERFLY reached the top row, column 3, row 1`; `Red BUTTERFLY escaping to the spider, column 3`; `Blue BUTTERFLY freed` / `2 butterflies freed: Blue, Red`; `spider moves above column 5` |

Positions use the same wording and coords setting as `board_access`
(column/row 1-based, row-first or chess style); the code has its own copy
of `Position()` because board_access keeps it file-local. Game text:
colours 455-461, BUTTERFLY tooltip title 133. Mod words: counts,
`rows below the top row`, `in the top row`, `under the spider`, `spider
above`, `escaping to the spider`, `new`, `reached the top row`, `freed`.

Not spoken on purpose (not drawn, so a sighted player does not see them):

- Spawn countdown `Board+0x3258` (moves until the next spawn, decremented by
  vtable+0x2FC `0x006D7F40`, set to 0 when no butterfly is on the board).
  Only ButterflyBoard logic reads it; no draw function does.
- Each butterfly's move countdown `+0x244`: the piece draw `0x0074F130`
  reads `+0x244` only for COUNTER (`0x200`) pieces; Butterflyify
  `0x007492C0` gives butterflies `0x80` only. (The `%d` drawn from `+0x244`
  in `0x00757DF0` is behind the debug byte `+0x2738`.)
- Spawn amount / side-spawn chance floats (`+0x3274`, `+0x3278`).

## Memory fields

| Field | Meaning | Evidence |
|---|---|---|
| grid `Board+0xF8+4*(col+8*row)` | Piece* | live (board_access) |
| piece `+0x228 & 0x80` | butterfly | static: Butterflyify, spawn, danger test |
| piece `+0x220` | colour | live for gems; static for butterflies (Butterflyify keeps it) |
| piece `+0x04` | piece id, used to follow a butterfly between ticks | static (board-objects.md) |
| piece `+0x20C` == -1 | butterfly has left the top row towards the spider | static: UpdateGame `0x006E82A0` sets it; spider/GameOver logic tests it. Other values' meaning unknown (only `== -1` is used) |
| `Board+0x3288` float | spider x in columns (0 = left) | static: UpdateGame compares it with butterfly column `+0x8`, assigns it, animates it between `+0x328C` and `+0x3290`; init `0x006E3660` writes it |
| `Board+0xD8C` byte | game over: events stop | static (hud.md) |

The spider column is spoken only when the float is within 0.05 of a whole
column; while it glides it is "spider moving" (details/F6) and no event.

## Uncertainties

- "Freed" is inferred from a butterfly id leaving the grid while not
  escaping; a butterfly destroyed some other way (e.g. a special's blast)
  would also be called freed - which is what the game counts too (ButterflyGoal
  counts any cleared 0x80 piece), but not checked live.
- If a new game reuses the same board object and the lead does not call
  `reset`, the old butterflies vanishing would be announced as freed.
- Whether the piece `+0x8/+0xC` column/row always equals the grid index for
  butterflies during the move-up animation (the mod uses the grid index).
- Escaping butterflies stay in the grid at row 0 while the spider moves; the
  mod lists them as escaping.

## Runtime checklist for the lead (throwaway profile)

1. Endless Butterflies: new butterflies appear in row 8 (bottom) with flag
   `0x80`, and a `new ... BUTTERFLY` event is spoken once each.
2. Each move the butterflies rise one row; cellText's rows-below-top count
   matches the screen.
3. Spider float `+0x3288` follows the top butterfly; `spider moves above
   column N` matches the drawn spider; F6 says the same.
4. Match a butterfly: `<colour> BUTTERFLY freed`, once.
5. Let one reach row 1 (top): `reached the top row`; let it escape:
   `escaping to the spider` and the game over follows.
6. Confirm nothing is said about the spawn countdown or per-butterfly counters.

## Live check (lead, 2026-10-30, Butterflies quest, user's profile)

The quest board is a ButterflyBoard. Heard: F6 "progress 0 of 15, 1
butterfly, nearest the top column 4, row 8, spider above column 4"; M
"spider above column 4. 1 butterfly, from the top: Orange column 4, row 8";
after a move the butterfly moved to row 7 and X said "Orange BUTTERFLY, 6
rows below the top row, under the spider, column 4, row 7". Not seen: new
butterflies, freed, top row, escape. Endless Butterflies is locked on this
profile.
