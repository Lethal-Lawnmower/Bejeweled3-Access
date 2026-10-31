# Poker mode (PokerBoard)

Source: `src/mod/mode_poker.cpp` (`PokerHooks()`, see `src/mod/mode_info.h`).
Date: 2026-10-30. Exe: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`).

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Nothing here has been run in the game or heard through Prism.** Every
field below is a static read of the decompilation; none is runtime-verified.

Method: the full read-only decompile `private/ghidra-b4/all.c` (function
extractor `private/poker-bfly/fn.sh`), capstone scans with
`private/ghidra-b4/py/opscan.py` / `dis.py`, string ids from
`private/pak/properties/default.xml` (function names come from its
comments). Builds on `docs/re/board-objects.md` (Poker section) and
`docs/re/hud.md`; the hud.md "Poker hand counts / skulls: Open" item is
answered here.

## What the screen shows and what is spoken

The sighted player sees: five card slots above the board, filled one per
move with the colour of the gem matched (a hypercube card, or a flame/star
card when the match used one); a score table of the seven hand types with
each hand's value, or a skull on the skulled rows; the row of the hand the
cards make so far highlighted; "Need X or better" over the board while that
hand is skulled; in endless mode the Skull Eliminator bar; in a quest the
hands left ("N Left" or "Last Hand"). After five cards the hand's name and
points (or "No Score") appear; a skulled hand starts the Skull Coin flip.

| Hook | Says (examples, English game text) |
|---|---|
| `briefStatus` (F6) | `hand 3 of 5, Pair, 2 SKULL, 4 hands Left` (skull count and hands only when present) |
| `details` (M) | `hand 3 of 5: Red, Red FLAME GEM, Blue. so far Pair. Need Two Pair or better. Pair SKULL, Spectrum 100, Two Pair 150, ... Flush 1000. SKULL ELIMINATOR 40 percent` (eliminator in endless, hands left in quests) |
| `tick` events | `card 3, Blue, Pair, Need Two Pair or better`; `card 5, Yellow`; `Full House, plus 1500`; `Pair, No Score`; `skull coin flip`; `new SKULL, Spectrum`; `SKULL removed, Spectrum`; `3 hands Left` / `Last Hand` |
| `cellText` | none (Poker gems are ordinary gems) |

Game words come from `GameString()`: colours 455-461, hand names 588-594
(order of the type index), "Need %s or better" 377, "Left" 380 / 567,
"Last Hand" 381, "No Score" 385, "SKULL ELIMINATOR" 568, "SKULL" 597, card
specials use the gem tooltip titles 119 (flame), 121 (star), 123
(hypercube). Mod words: `hand N of 5`, `card N`, `so far`, `or`, `plus`,
`skull coin flip`, `new`, `removed`, `hands`, `percent`.

Not spoken on purpose:

- The skull coin's outcome. `0x006CB430` decides it at the start of the flip
  (byte `+0x3B90`, 0 = safe) but the screen only shows it when the coin
  lands; the game's SAFE! announcement (384, through hud announcements) or
  the game-over announcement reports it.
- The skull-appearance countdown (`+0x3994`, accumulates per hand from
  HandCountdown values in `0x006CDCE0`): not drawn anywhere found.
- The hover tooltips of the score table (UpdateTooltip `0x006C90E0`, strings
  386-400, 568/569): shown only under the mouse. The descriptions (393-399)
  could be added to details later if the user wants them.

## Memory fields

All offsets are from the PokerBoard object. "static" = read from
decompiled/disassembled code of this exe.

| Field | Meaning | Evidence |
|---|---|---|
| `+0x3720` int | cards in the hand, 0..5 | static: AddCard `0x006CF460` increments `[0xDC8]`; `0x006CDCE0` sets 0; Update `0x006DEDD0` tallies at 5 |
| `+0x3248 + i*0xF8` | card i: `+0` colour 0..6, `+4` second colour (-2 none), `+8` kind 0 plain / 1 hypercube / 2 flame / 3 star | static: AddCard writes `[0xC92/0xC93/0xC94 + n*0x3E]`; kind 2/3 when the piece's `+0x188` equals the flame/star ids `+0x3AA8/+0x3AAC`; reset `0x006CDCE0` (colours -2, kind 0); init `0x006DFA40` |
| `+0x3A30` int | hand type the current cards make, -1 none (highlighted row) | static: AddCard stores evaluator `0x006C80A0` result - 1; DrawTopWidget `0x006D4300` draws at that row when >= 0 |
| `+0x398C` int | highest skulled type, -1 none (rows 0..it show a skull) | static: DrawTopWidget row loop prints the value only for rows above it; `0x006CDCE0` +1 (cap SkullMax `+0x3990`) / -1 |
| `+0x39A8/+0x39AC` vector<int> | hand values shown on the table | static: DrawTopWidget prints `[begin+4*row]` with `%i`; `0x006CB520` passes `[begin+4*type]` to TallyScore as points |
| `+0x3984` float | Skull Eliminator fill 0..1 (bar drawn in endless only, `0x006D1660`) | static: TallyScore `0x006C7560` adds HandBuster value `[+0x39C0+4*type]` while skulls exist, capped at 1; full bar removes a skull in `0x006CDCE0`. `+0x3988` is the animated drawn value (Update eases it) |
| `+0x396C`, `+0x3978` int | quest hands remaining / starting "Hands" | static: init `0x006DFA40` loads "Hands" into both; `0x006CDCE0` decrements `+0x396C`, GameOver below 0; DrawTopWidget shows min of both with "Left", "Last Hand" when `+0x396C` < 1 (only when not endless) |
| `+0x39E4` int | skull coin flips | static: `0x006CB430` increments it; init sets 0 |
| `+0x39E8 + 4*t` int | hands of type t made | static: `0x006CB520` increments `[+0x39E4 + 4*(t+1)]` right after TallyScore; init zeroes |
| `+0x3A2C` int | points of the hand just scored, 0 = No Score | static: TallyScore |
| `+0x3184` byte | endless | hud.md hypothesis; DrawTopWidget picks the eliminator vs hands-left branch on it |

Hand type index 0..6 = the order of strings 588-594, loaded in that order by
the PokerBoard ctor `0x006DA1C0`. The game's own overlay hand name and
"Need %s or better" use a static English table (`0x008D2248`, filled by
`0x007BCF30`); the mod uses the localised 588-594 instead (identical in the
English build).

## Uncertainties

- Screen order of the table rows (the mod lists lowest type first).
- What a hypercube (wild) card is drawn as: the evaluator rewrites its
  colour to the one it chose; the mod says only "HYPERCUBE".
- A card with a second colour (two gems added in one move): spoken
  "Red or Blue" from the evaluator's use of it; how it is drawn not traced.
- The "so far" hand for 1-4 cards: the evaluator runs over count+1 slots;
  the value is spoken as stored, its exact meaning for partial hands is
  not decoded.
- 380 vs 567 ("Left") singular/plural assignment inferred from the `< 2`
  branch; both are "Left" in English.
- A scored hand's event relies on the per-type counter; if a new game reuses
  the same board object without the lead's `reset`, counters restart at 0
  and no false events are produced, but a missed tick could merge two hands.

## Runtime checklist for the lead (throwaway profile)

1. Endless Poker: after each move `+0x3720` rises by one and the spoken card
   colour matches the drawn card; a flame/star match gives kind 2/3.
2. The "so far" hand matches the highlighted table row.
3. At five cards: one `<hand>, plus N` event; N equals the drawn `+N` and
   the table value of that row.
4. When the first skull appears: `new SKULL, Pair` and the Pair row shows a
   skull; F6 / M list it. Making a skulled hand: `skull coin flip`, then
   the game's SAFE! or game over, and nothing before the coin lands.
5. The eliminator percentage follows the bar; when full a skull goes and
   `SKULL removed` is spoken.
6. A Poker quest: hands left matches the "N Left" / "Last Hand" widget.
7. Confirm `+0x3184` is 1 in endless Poker and 0 in the quest.

## Live check (lead, 2026-10-30, user's profile, save restored)

Endless Poker, five hinted moves, posted keys, speech read from the mod log
(not listened to). Heard: F6 "hand 0 of 5"; M with the full score table
("Pair 2500, Spectrum 5000, ... Flush 50000. SKULL ELIMINATOR 0 percent");
"card 1, White, Spectrum", "card 2, Red FLAME GEM, Spectrum", "card 3,
White, Pair", "card 4, Red, Two Pair", "card 5, Purple", then "Two Pair,
plus 7500". The hands match the cards spoken. Not seen: skulls, coin flip,
quest hands left. Open: whether "Spectrum" for one or two cards is what the
screen highlights (field `+0x3A30`). Not compared with a screenshot.
