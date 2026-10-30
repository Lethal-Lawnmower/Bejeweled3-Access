# Board design inspiration

Reference suggested by the user (2026-10-30) as potential inspiration for
the board phase. Design ideas only; no code or text from it is used. Its
speech uses Tolk, which this project does not use (Prism only).

## AccessiCrush (https://lillaire.net/accessicrush/, README.txt)

Screen-reader mod for Candy Crush Saga (Windows, beta 0.8.14 at the time
read). Reads the game's own data, keeps the game's audio.

- Arrow keys move a cell cursor; rows and columns numbered from 1; holes
  and irregular boards are skipped.
- Describe current cell (colour, special type and direction, blockers,
  objective layers). Separate keys read without moving: repeat cell, read
  row, read column, read neighbours (1 or 2 cells), status (moves,
  objectives, score), next/previous blocker, next/previous objective.
- Moves: select then pick an adjacent cell, or Shift+Arrow to swap in a
  direction; Backspace/Escape cancels. A board change clears a pending
  selection.
- Feedback after a move is minimal by default ("announce clears" optional,
  off); advice is to wait for the board to settle. No cascade summary.
- Clues instead of solutions: a move-area clue names one reference cell
  without giving the swap; special-match clues give shape and area. Clues
  never make moves.
- Tutorials and popups are spoken automatically; F1 key help; F3 settings.
- Timed modes are not handled (listed as untested).

## Notes for Bejeweled 3

- The game already uses arrows (cursor), W/A/S/D (swap) and Space
  (select), so AccessiCrush's letter keys (W, A, S, ...) would collide;
  reading commands need other keys (to be decided with the user).
- Bejeweled cascades are much heavier than Candy Crush's; a settled-board
  signal and an optional "what changed" summary are design questions for
  the combined board research summary.
- Hint style (full move vs. clue) is a user choice; the game has its own
  hint button.
