# Lightning mode (SpeedBoard)

Source: `src/mod/mode_lightning.cpp` (`LightningHooks()`, see `src/mod/mode_info.h`).
Date: 2026-10-30. Exe: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`).

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Not linked into a build, not run in the game, not heard through
Prism.** Every field below is a static read of the decompilation; none is
runtime-verified.

Method: the full read-only decompile `private/ghidra-b4/all.c` (extractor
`private/ghidra-b4/fn.sh`, callers `callers.sh`, vtable map `vtmap.txt`),
capstone disassembly `private/ghidra-b4/py/dis.py`, string ids from
`private/pak/properties/default.xml`. Builds on `docs/re/board-objects.md`
(Lightning, flag bits), `docs/re/board-events.md` (speed bonus, Blazing
Speed) and `docs/re/hud.md` (timer). No new Ghidra run was needed.

## What the screen shows and what is spoken

The sighted player sees: the 60-second clock; time gems (a gem with a
number, 5 or 10) that add that many seconds when matched, with a "+5 sec"
popup; the bonus time saved up for the next round ("+0:15" beside the
clock); "N MATCH CHAIN" / "SPEED +X" while fast swaps chain; the Blazing
Speed flames; when the clock runs out with bonus time, the clock refills
and the multiplier rises; with no bonus left, TIME UP and the Last Hurrah,
where the remaining special gems go off.

The time left is the T key and score / multiplier are F6 (`hud.cpp`); the
hooks do not repeat them.

| Hook | Says (examples, English game text) |
|---|---|
| `cellText` | on a time gem: `+5 sec` (string 479 filled with the gem's seconds) |
| `briefStatus` (F6) | `bonus time +0:15, 4 MATCH CHAIN, Blazing Speed, Last Hurrah` (each part only when present) |
| `details` (M) | `2 TIME GEM: Green +5 sec, column 3, row 6; Red +10 sec, column 7, row 2. 1 MULTIPLIER GEM: Blue, column 1, row 8. bonus time +0:15` |
| `tick` events | `+5 sec` (time gem matched); `bonus time added, multiplier 3` (new round from the bonus); `3 MATCH CHAIN`; `Blazing Speed`; `Blazing Speed over`; `Last Hurrah` |

Game words via `GameString()`: 135 TIME GEM and 125 MULTIPLIER GEM
(tooltip titles), 479 `+%d sec` (SpeedBoard::PieceTallied), 481 `+%d:%02d`
(SpeedBoard::DrawFrame2, the bonus time), 482 Last Hurrah (DrawFrame2), 149
`%d MATCH CHAIN` (Board::DrawGameElements), colours 455-461. Mod words:
`bonus time`, `bonus time added, multiplier`, `Blazing Speed` (the game
only says it as a voice sample), `Blazing Speed over`, `no`, positions.
When a game string is missing the mod falls back to short English
(`plus 5 seconds`, `15 seconds`, `3 match chain`).

## Fields

All **static** (this exe), none runtime-verified.

| Field | Meaning | Evidence |
|---|---|---|
| piece `+0x228` bit `0x20000` | time gem | SpeedBoard vt+0x2E4 `0x00716C60` sets flags = 0x20000 on a plain gem (if not blocked by `+0x22C`); tooltip `0x007476E0` names it (135/136) |
| piece `+0x244` | time gem seconds, 5 or 10 | same function stores 5 or 10; tooltip formats the body with it; PieceTallied adds it to the pool |
| piece `+0x228` bit `0x10` | multiplier gem | board-objects.md (SpeedBoard vt+0x2DC `0x00714DD0`) |
| Board `+0x30AC` int | bonus time pool, seconds | PieceTallied `0x00715360`: `[0xC2B] += piece[0x91]` (piece +0x244) and shows 479 with it; GameOver `0x00719350`: if the pool is above 0 when the clock ends, next round time = `(60 - pool) * 100` ticks, pool = 0, `[899]` (+0xE0C multiplier) += 1; init `0x00718500` zeroes it; danger test `0x00714130` is off while it is non-zero |
| Board `+0x30B0` int | total bonus time this game (end dialog) | PieceTallied adds the same seconds; SpeedEndLevelDialog gets it (`0x00716AB0`). Not read |
| Board `+0x30B4` int | animated copy of the pool for drawing | Update `0x007173F0` eases it toward `+0x30AC`. Not read (the mod uses the real pool) |
| Board `+0x3096` byte | Last Hurrah started | GameOver: when the pool is below 1 and Blazing Speed is off, the first time it queues the TIME UP announcement (480) and sets this byte, then sets off every special gem (`& 0x17` or `& 0xA1800`); init `0x00718500` clears it |
| Board `+0x1080` int | speed chain | `0x0074C070` (per swap, Lightning only): +1 when the previous swap was under 301 ticks ago; DrawGameElements `0x00750320` draws 149/598 with it (or `+0x1088` when it is 0) and SPEED +((chain+1)*20 capped 200, times vt+0x1E8) |
| Board `+0x1188` float | Blazing Speed, > 0 while active | BlazingSpeed `0x0074A590` sets 1.0; UpdateGame `0x00767E20` lowers it each tick to 0 and then clears the meter `+0x1078` |
| Board `+0xE0C` int | multiplier | `board_layout.h` kMultiplier; GameOver raises it per bonus round |

## Uncertainties

- **Last Hurrah timing**: the text 482 is drawn by DrawFrame2 under a
  CurvedVal at `+0x33E0`; the mod speaks it when `+0x3096` becomes 1, which
  is when GameOver starts that phase. Whether the text shows at that moment
  (or a little later) is not checked.
- **Bonus pool shown**: DrawFrame2 formats 481 from an animated value;
  whether "+0:00" is drawn when the pool is empty is not known, so the mod
  speaks the bonus time only when it is above 0.
- **Chain speech**: every chain step from 2 up becomes an event. In fast
  play this may be too chatty next to the after-move summary; the lead may
  want a setting or a throttle.
- **Blazing Speed**: the game speaks it as a voice sample, so the start
  event may be redundant; kept because the flames and their end are only
  visible.
- `0x40000` (decaying time gem) has no creator in this build
  (board-objects.md); such a gem would still be read as a time gem.
- The x87 timer replication in hud.cpp (T key) is unrelated but still the
  least certain Lightning item (hud.md).

## Interface note for the lead

`board_access.cpp` `Gem()` appends the bare counter for flags
`0x20000 | 0x20 | 0x40` (`TIME GEM 5`); with this hook the cursor would say
`Green TIME GEM 5, +5 sec`. Suggest dropping the bare number when the mode
hook has `cellText` (Lightning and quests are the only modes with counters).

## Runtime checklist (throwaway profile, read-only)

1. A time gem: flags `0x20000`, `+0x244` equals the drawn number; cursor
   says `+N sec` (string 479 loaded).
2. Match it: `+0x30AC` grows by that number; the event `+N sec` comes once;
   F6 shows `bonus time +0:NN` matching the drawn bonus beside the clock.
3. Let the clock run out with bonus saved: `+0x30AC` becomes 0, `+0xE0C`
   +1, the clock refills; event `bonus time added, multiplier N`.
4. Fast swaps: `+0x1080` counts up with the drawn "N MATCH CHAIN"; the
   events match; resets to 0 when the chain ends.
5. Blazing Speed: `+0x1188` becomes 1.0 with the voice, falls to 0; both
   events once.
6. End: TIME UP announcement (hud), then `+0x3096` = 1 and `Last Hurrah`
   spoken while the specials go off; compare with when 482 is drawn.
7. M lists every time gem and multiplier gem with correct positions.

## Live check (lead, 2026-10-30, Lightning, user's profile)

Four hinted moves. Heard: "+10 sec" when a time gem was matched; at the end
of the round "no TIME GEM. bonus time +0:10" (M) and "bonus time added,
multiplier 2"; F6 then "multiplier 2, time 0:10"; at the end "Last Hurrah"
together with the game's TIME UP. Not seen: the time gem's cell text, chains,
Blazing Speed.
