# Ice Storm mode (InfernoBoard)

Source: `src/mod/mode_icestorm.cpp` (`IceStormHooks()`, see
`src/mod/mode_info.h`). Date: 2026-10-30. Exe: Steam `Bejeweled3.exe`
(SHA-256 `d5516244...88c3`). Repository revision at the time: `21b0d19`
plus uncommitted mode files.

**Status: compile-checked only (`tools\compile-one.bat`, no warnings at
/W4). Not linked, not run in the game, not heard through Prism.** Every
field is a static read of the decompilation; none is runtime-verified.

Method: the full read-only decompile `private/ghidra-b4/all.c`, extracted
per function with `private/ice-dig/fn.sh` (outputs in `private/ice-dig/`),
capstone disassembly `private/ghidra-b4/py/dis.py`, constants read with
`private/ghidra-b4/py/rd.py`, InfernoBoard overrides from
`private/ghidra-b4/vtmap.txt`, string ids from
`private/pak/properties/default.xml`. Builds on `docs/re/board-objects.md`
(Ice Storm section) and `docs/re/board-flow.md` 4.5; corrects and extends
both (see "Corrections").

Tool note: `private/ghidra-b4/fn.sh` compares addresses with awk `==`,
which treats addresses such as `006e0650` as numbers in exponent notation
(all equal to infinity) and prints hundreds of unrelated functions.
`private/ice-dig/fn.sh` forces a string comparison.

## How the mode works (static)

- Eight column records, stride `0x348`, at `Board+0x3D28` (board ctor
  `0x006E2D90`: `_eh_vector_constructor_iterator_(board+0x3D28, 0x348, 8,
  0x006CBE90, 0x006CAE20)`; dtor `0x006DB270` frees the same range).
- Ice rises from the bottom. The height is in rows: the column draw
  `0x006D5F10` (and the crush pass in `0x006D63B0`) translates the ice
  animation to `y = (8.0 - height) * 128` px from the board top
  (disassembly `0x006D66EC`..`0x006D671A`: `fld [0x880D38]` = 8.0, `fsub`
  height, `fmul [0x880EA8]` = 128.0, scaled by app width / 1200). Rows are
  128 px (board-flow: piece `y = row * 128`).
- A column can be one or two grid columns wide (record `+0x24`). The
  activation `0x006E0650` (args board, column, width; BL = on/off) clears
  height, sets the width, and sets `+0x1C` of every covered record to the
  left column, so only the left (lead) record draws.
- The rise is in `0x006E0CC0` (called from Inferno DoUpdate override
  `0x006E5850`); the height is written by vtable+0x42C `0x006C4F20`.
- Over the top: when height > 8.0 the column's `+0x34` counter goes from -1
  to 0 and counts updates; `0x006CE540` turns it into a crack ratio
  `min(1, (count + extra) / 100 / [Board+0x3CBC] * double[rec+0x48])`. The
  crack animation frame follows it (`0x006E0CC0`, "crack" animation, frame =
  0.9 x ratio of its frame range), plus "panicslow"/"panicfast". When a
  ratio exceeds 1.0 `+0x3CC8` is set and, once the board is still,
  GameOver (vtable+0x284) runs. So the loss rule is "a column stays over the
  top for `[Board+0x3CBC]` seconds (scaled by the rate)". Back at or under
  8.0 resets the counter to -1.
- Danger (vtable+0x368 `0x006C5C20`): any record height > 8.0 (else the
  base time rule).
- A vertical match on an active column crushes it: `0x006DB1A0` sets
  `+0x46`, plays a sound, calls IncrementColCleared `0x006D6D60` and
  AddStat(1). Matches in a column push it down (`0x006DD580`,
  `0x006CAF30`, accumulator `+0x10`).
- Combo: IncrementColCleared counts consecutive crushes in
  `[Board+0x36C8]` (reset when the window `+0x36BC + +0x36C4` has passed).
  From 3 on, in endless mode (byte `+0x3184`) and when `0x006C4EC0` is
  true, it shows two floating texts: string 559 formatted with the combo
  name (count 3: empty; 4..12: strings 549..557; 13 and more: 558) and
  string 560 formatted with the count.
- Multiplier: DoUpdate raises `Board+0xE0C` (`param_1[899]++`) when the
  progress `[+0x369C] / [+0x36A0]` reaches 1 (endless), with the
  "multiplierup" animation; RefreshMultiplierText `0x006CC010` uses string
  561.

## What is spoken

| Hook | Says (examples) |
|---|---|
| `cellText` (after the gem name) | `in ice` (cell fully below the ice top); `partly in ice` (at least half) |
| `briefStatus` (F6) | `highest ice column 4, 3 and a half rows`; `highest ice columns 5 and 6, 8 and a half rows, over the top, cracked 40 percent`; `no ice` |
| `details` (M) | `Ice. column 1, 2 rows. columns 3 and 4, 5 and a half rows. column 7, breaking. Highest columns 3 and 4. No ice in columns 2, 5, 6, 7, 8` |
| `tick` events | `new ice, column 2`; `column 2 near the top, 6 rows`; `column 2 over the top`; `column 2 crushed`, in endless with the game's combo texts appended (strings 559 + 549.., 560); the game's multiplier text (561) when `+0xE0C` rises |

Heights are rounded to the nearest half row. "Near the top" (6 rows, reset
below 5.5) is a mod default, not a game threshold; "over the top" is the
game's 8.0. Time left: Ice Storm is untimed (limit 0), so the T key already
says "no time limit"; nothing is duplicated.

## Fields read

All **static**; none runtime-verified. Record `R = Board + 0x3D28 + i*0x348`.

| Field | Type | Meaning | Evidence |
|---|---|---|---|
| `R+0x0C` (= `Board+0x3D34+i*0x348`) | float | ice height, rows from the bottom | draw `0x006D66EC`; danger `0x006C5C20`; setter `0x006C4F20` |
| `R+0x1C` (`+0x3D44`) | int | lead record of this slot | draw loops test `[R+0x1C] == i`; `0x006E0650` |
| `R+0x24` (`+0x3D4C`) | int | width 1 or 2 | `0x006E0650`; draw offsets `0x006D6688` |
| `R+0x34` (`+0x3D5C`) | int | updates over the top, -1 not over | `0x006E0CC0` |
| `R+0x45` (`+0x3D6D`) | byte | active (has ice) | `0x006E0650`, `0x006C4FB0` |
| `R+0x46` (`+0x3D6E`) | byte | crushed | `0x006DB1A0`; crush animation in `0x006E0CC0` |
| `R+0x48` (`+0x3D70`) | double | crack rate | `0x006E0650` copies `Board+0x3688`; `0x006CE540` |
| `Board+0x3CBC` | int | seconds over the top before loss | `0x006CE540` |
| `Board+0x36C8` | int | consecutive crushes in the combo | `0x006D6D60` |
| `Board+0xE0C` | int | multiplier | hud.md; `0x006E5850` |
| `Board+0x3184` | byte | endless | hud.md (hypothesis there) |

Strings: 549-558 combo names, 559 combo format, 560 count format, 561
multiplier format (all through `GameString`, none copied).

## Corrections to earlier notes

- board-objects.md lists the record from `+0x3D30`; the records start at
  `+0x3D28` (ctor). `+0x3D30` is `R+0x08` (previous height),
  `+0x3D38` is `R+0x10` (push-down accumulator), `+0x3D44` is `R+0x1C`.
- `+0x3D44` is the lead (drawing) record, not a general slot mapping;
  `+0x3D48` (`R+0x20`) is the record's home index and `+0x3D68`
  (`R+0x40`) the width a slot is activated with (`0x006E08A0`).
- hud.md "Ice Storm columns ... Open" is answered here; the community
  stat 0x14 is not used.
- board-flow 4.5: the loss is the crack ratio rule above; `+0x3CC8` holds
  the losing column (+1 when two wide), not a flag.

## Uncertainties

- Whether a covered gem is visually "in ice" in the same way for a
  two-wide column (draw offset differs, -24/-46 vs -40/-40 px); cell
  coverage assumes the ice spans exactly the lead's `width` grid columns.
- The draw adds an animated curve (`R+0x1A8`, `edi-0x130`) to the y
  position (shake/bounce); ignored.
- Crack percent: the rate `R+0x48` and seconds `+0x3CBC` come from quest
  config (`SecondsUntilLose` key per board-flow); the formula is
  replicated, values unchecked. If either is unreadable or zero the percent
  is left out.
- Combo texts: condition `0x006C4EC0` and the points test (`+0x36B8 > 0`)
  are not replicated; the mod adds the combo texts in endless mode when the
  combo count changed with a crush and is at least 3.
- Whether the multiplier text is shown in quests; the event fires on any
  rise above 1.
- A crushed column is said "breaking" in details until reactivation; when
  it is cleared exactly (crush animation end, `0x006E0650` with BL = 0) was
  not traced.

## Runtime checklist (lead; throwaway profile, read-only)

1. Endless Ice Storm: dump `Board+0x3D28 + i*0x348` +0x0C, +0x1C, +0x24,
   +0x45, +0x46 for i = 0..7. Columns with visible ice have active 1 and
   lead = i; heights rise over time.
2. Compare a column's height with the screen: height 4.0 should reach the
   middle of the board (ice top between rows 4 and 5).
3. Find a two-wide column (if the mode has one) and check both records
   point to the left one.
4. Let a column pass the top: `+0x34` counts from 0; the crack grows; check
   the cracked percent against the crack animation and the time to loss
   against `+0x3CBC`.
5. Make a vertical match on a column: `+0x46` becomes 1, "column N
   crushed" is spoken; two or three quick crushes show the combo texts and
   the mod repeats them.
6. Watch the multiplier text and `+0xE0C`.
7. Cursor on a gem low in an iced column: "in ice" is added after the gem
   name.

## Live check

None: Ice Storm is locked on the user's profile (2026-10-30). Everything
here is static only.
