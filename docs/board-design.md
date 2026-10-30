# Board design (decided defaults, all configurable)

Decided with the user on 2026-10-30 after the board research
(`board-research-summary.md`). The user has not played the game yet, so
**every choice below is only a default and must be an option the user can
change** (user: "We should have them be optional and configurable").
Design principle (user, 2026-10-30): the same information a sighted player
perceives, never the solution. No feature may compute moves for the
player; the game's own hint is passed on as the sighted player gets it.

| Area | Default | Alternatives to offer as settings |
|---|---|---|
| Cursor speech | Colour and special of the gem under the game's cursor, e.g. a colour name plus "star" (game's own colour and gem names) | add position every move; position only |
| Coordinates | "column N, row M", from the top-left, starting at 1 | chess style (letter + number, top or bottom origin); row first |
| After a move | Short summary when the board settles: points gained, cascades, specials made or set off | just "settled"; full detail (cells cleared); off |
| Cascades | Act anytime the game allows; say "still moving" if the touched gems are not settled | wait for the whole board to settle |
| Hints | Use the game's own hint (button / auto-hint) and say which gem it points to | area clue; full move; count of moves; off |
| Reading keys | "Where am I" (position) and status; neighbours (above, below, left, right) | read row / column; next special gem (available but off by default) |
| Mode objects | Part of the cell description; status key gives mode info (spider column, Poker hand, quest goal); key events announced (e.g. butterfly near the top) | status key only |
| Sounds | Speech plus a few cues (e.g. board settled, illegal move) | speech only |

## Notes for implementation

- Keys must avoid the game's own board keys (arrows, W A S D, Space,
  Escape) and the mod's existing ones (Tab, F5, F6, F12); candidate free
  keys to confirm on a live board before choosing.
- Settings need a home: a settings file in `%LOCALAPPDATA%\Bejeweled3Access\`
  plus a way to change them in the game (a mod settings menu or keys); to be
  designed.
- Sound cues: Prism is for speech only. Preferred source is the game's own
  sound effects played through the game's sound function (docs/re/
  board-events.md, PlaySample), so no new audio files or libraries are
  needed; otherwise a separate decision on an audio approach.
- Scope: not supported for now: Lightning, Ice Storm, Diamond Mine, timed
  quests. First targets: Classic, Zen, Butterflies, Poker, move-based quests.
