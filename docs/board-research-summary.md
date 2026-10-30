# Board research summary (2026-10-30)

Combined result of the six board research agents (static analysis, details
in `docs/re/board-*.md`) and a read-only live check. Nothing is implemented
yet: this is the basis for the user's design decisions.

Evidence levels: **live** = read from the running game on the throwaway
profile with `tools/board_probe.py` (read-only); **static** = from the
game's code only.

## What is certain

### The board under the animations
- 8x8 grid in every mode, an array of gem pointers in the board object;
  empty cells are null. **live**
- Each gem record holds a stable id, its logical column and row (matching
  its cell), colour 0-6 or none, and special flags. **live** (2 hypercubes
  with no colour and an orange star, as in the save file)
- Refills put the new gems, with their colours, into their final cells at
  once; they only fall visually. **live** (cells changed immediately after
  the move)
- Matched gems stay in their cells until their shrink animation ends.
  **static**
- The final board after a whole cascade cannot be known in advance (new
  gems are random and land step by step). **static**

### When the board is settled
- The game's own "board is still" check: nothing falling, swapping or about
  to explode, no pending match, no lightning effects. A counter of still
  ticks stops during resolution and resumes when settled. **live**
- A swap only needs the two gems involved to be still, so the player may act
  while a cascade runs elsewhere. **static**

### Playing with the keyboard (built into the game)
- Arrows move a cursor (column/row readable); the first press only shows it
  at column 4, row 3 (or 3,3). **live**
- Space selects; an arrow then swaps the selected gem; W/A/S/D swap the gem
  under the cursor directly. **live** (S swapped down and scored)
- Illegal moves swap back (tutorial dialog on new profiles; some quests
  count bad moves). Escape opens the game menu. Enter is unused. **static**
- The game has a function listing every valid move; hints come from the hint
  button or automatically after about 15 s idle. **static**

### What a move did
- Matches, cascades (per-move depth), special gem creation (4 in a line:
  flame; L/T: star; 5: hypercube; 6+: supernova) and detonations, points and
  multiplier all fire at the moment of detection, before the animation.
  **live** (score rose within 0.15 s of the swap) / **static** (the rest)
- Every removed gem passes through one game function with its cause, so
  "this move cleared N gems / the hypercube cleared all blue" is knowable.
  Requires hooking the game's code. **static**
- The announcer's praise voices are the game's own audio. **static**

### The game's own words
- Special-gem descriptions: the game's tooltip function covers every special
  type and Diamond Mine objects; callable like the badge text. **static**
- Colour names: the game's strings 455-461 (used by the Records "favourite
  colour" stat). **static**
- Tutorial pop-ups refer to a specific gem, so its cell can be announced.
  **static**

### Flow and time
- No speed or extra-time setting exists. Classic, Zen, Butterflies, Poker and
  move-based quests have no clock. **static**
- Pausing hides the gems visually, but the board stays readable in memory.
  **static**
- Classic/Zen level-up rebuilds the board in place; Zen never ends (adds a
  hypercube when stuck); "no more moves" is declared only on a settled
  board. **static**

## Scope (user decisions)

Not supported for now: Lightning, Ice Storm, Diamond Mine, timed quests.
First targets: Classic, Zen, Butterflies, Poker, move-based quests.

## Design options for the user to decide

Inspiration: AccessiCrush (docs/board-inspiration.md).

1. **Cursor and moves.** Keep the game's own keys (arrows, Space, W/A/S/D).
   The mod speaks the cell under the cursor on every move: colour and
   special type, optionally the coordinates ("column 3, row 5" or chess
   style "C5"), optionally only when it changes.
2. **Reading without moving** (AccessiCrush style): keys to read the current
   row or column, the neighbours, the status (score, level progress, moves),
   and to jump to the next special gem. The keys must avoid the game's own
   (arrows, W A S D, Space, Escape); letters such as R, C, E, F, Q and the F
   keys appear free on boards (to be confirmed).
3. **After a move.** Options from minimal to detailed: (a) just "settled"
   when the board is ready; (b) a short summary, e.g. score gained, specials
   created, cascade depth; (c) full detail (which cells cleared). The game's
   praise voices already play.
4. **Acting during cascades.** Either wait for "settled" (simplest), or allow
   moves as soon as the target gems are still (the game allows it).
5. **Hints.** Use the game's own hint, or the mod's move list at a chosen
   level: full move, area only (AccessiCrush style), or number of moves
   available.
6. **Mode objects.** Butterflies (positions, spider column), Poker (hand
   being built), quest goals and progress: describe on request plus key
   events (for example a butterfly reaching the top row).
7. **Sound vs speech.** Prism speech only for text; whether to add any
   non-speech cues is a separate decision (none planned).

## Still unconfirmed

- Game update and mod timer on the same thread (assumed; the mod's reads
  were consistent in the live check).
- Tooltip and colour-name calls (planned to verify when implementing).
- Butterflies and Ice Storm loss rules; a few unexplained flags and fields
  (listed in the topic docs).
- Whether keyboard swaps are queued while paused.

## Live check log (2026-10-30, Test profile, Classic, fullscreen High)

- Board class ClassicBoard; grid consistent; specials as in the save.
- Native cursor: Right showed it at 4,3; Right then Down moved to 5,4.
- Moved cursor to 0,1 and pressed S: score 100 -> 150 within 0.15 s ->
  250 (cascade), level points 180 -> 246 -> 337, refill countdown 31 -> 74
  -> 0, still-tick counter paused then resumed, moves-in-progress list
  filled then emptied, idle counter reset; cells in columns 0-2, rows 0-2
  refilled.
- The user's own save was backed up first and restored afterwards,
  verified identical.
