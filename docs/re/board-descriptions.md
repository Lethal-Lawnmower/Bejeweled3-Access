# Board descriptions, community cross-check, board save format

Pass: 2026-10-30, agent 6 of 6 (board research phase, see
`board-research-brief.md`). Static analysis only: Ghidra 12.0.4 copy
`private/ghidra-b6` (wrappers `dec.sh`, `dis.sh`, `scan.sh` there; they call
`tools/ghidra/decompile.sh`, `menus-disasm.sh` and `HoverScan.java`), offline
reads of `Bejeweled3.exe` (Steam build, gApp `[0x008E1730]`), and a read-only
parse of the throwaway profile's `classic.sav` / `zen.sav` with
`private/ghidra-b6/sav.py`. **Nothing here is runtime-verified.** No code was
changed. No new Ghidra script was needed (no `BDesc*` files).

Labels: **static** = read in decompilation/disassembly of this build;
**file** = seen in the throwaway save files (static parse, no game run);
**community** = from a community project, not checked here unless stated;
**hypothesis** = inferred, not proven. Game text is referred to by
string-table id (`private/pak/properties/default.xml`); the table's own
comments name the source function of each id, which helped locate code.

## Summary for design

| Need | Best source | How |
|---|---|---|
| Name + description of a special gem / object under the cursor | `Board::GetTooltipText`, Board vtable +0x394 | call it through the board's vtable on the game thread (convention below); returns false for plain gems |
| Gem colour name in the game's words | string **455 + colour** (colour = `piece+0x220`, 0..6) | code-backed by StatsTab favourite colour (below); strip the `^RRGGBB^` colour prefix |
| Tutorial hint (special gem made, coin, time gem, ice, dark rock) | HintDialog (dialog id 0x12) shown by `Board::CheckForTutorialDialogs` | already read as a dialog by the mod (status.md); the trigger table below says which piece it is about |
| Board state without memory polling | `classic.sav` / `zen.sav` buffer | same piece fields as memory; written only at safe points, so a cross-check, not a live source |

## 1. The game's own descriptions

### 1.1 Board::GetTooltipText `0x007476E0` (static)

- **Slot:** Board vtable byte offset **+0x394** (slot 229). Base Board,
  ClassicBoard, SpeedBoard, ZenBoard use `0x007476E0`; DigBoard, QuestBoard
  and every other QuestBoard subclass (Balance, BadMoveLimit, Butterfly,
  Filler, Inferno, MoveLimit, NoLose, Poker, RealTimeBomb, TimeBomb,
  TimeLimit) use the override `0x006CD7F0` (checked by reading +0x394 in all
  17 board vtables listed by `tools/rtti_vtables.py`).
- **Convention:** `__thiscall bool (Board* this, Piece* piece,
  std::wstring* title, std::wstring* body)`, callee pops 12 (`RET 0xC` at
  `0x007480E7`). Evidence: piece = `[EBP+8]` (`0x00747712`), title =
  `[EBP+0xC]`, body = `[EBP+0x10]` (assign calls at `0x00747762`,
  `0x007477F8`); the only direct caller `0x006CD911..16` pushes body, title,
  piece and sets `ECX = board`. Return: `AL = 0` (`XOR AL,AL` at
  `0x0074771B`) when no recognised flag, then the out strings are untouched;
  `MOV AL,1` (`0x007480D0`) otherwise. The base function does not use
  `this`; it reads only `piece+0x228` (flags) and, for the time gem,
  `piece+0x244`.
- **Strings:** each text is fetched with `FUN_00475e30(id, fallback)` on the
  string table `gApp+0x7F4` (fallback = an English literal in .rdata), then
  assigned with `FUN_004018B0` (wstring assign) into the out string.
- **Flag tests, in order** (`ECX = [piece+0x228]`):

| Test | Title / body ids | Community name (PlusExtender enum) |
|---|---|---|
| bit 0x1 and bit 0x4 | 117 / 118 | FLAME+STAR = supernova (deck-bot: "flags 5") |
| bit 0x1 only | 119 / 120 | FLAME |
| bit 0x4 | 121 / 122 | STAR |
| bit 0x2 | 123 / 124 | HYPERCUBE |
| bit 0x10 | 125 / 126 | MULTIPLIER |
| bit 0x800 | 127 / 128 | DETONATOR |
| bit 0x1000 | 129 / 130 | SCRAMBLER |
| bit 0x400 | 131 / 132 | COIN |
| bit 0x80 (sign of low byte) | 133 / 134 | BUTTERFLY |
| bit 0x20000 | 135 / 136, body formatted with `piece+0x244` (thousands-formatted by `FUN_0043b2e0`, StrFormat `FUN_0043b940`) | TIMEBONUS (time gem; `+0x244` = seconds) |
| bit 0x60 (either) | 137 / 138 | MBOMB / TBOMB (time bomb) |
| none of these | returns false | plain gem |

  Order matters: a flame+star piece is reported only as supernova, a flame
  hypercube (PlusExtender "Hypernovify" = 0x1+0x2) only as flame.
- **Quest override `0x006CD7F0`:** if `[board+0x3180]` (QuestGoal, see
  hud.md) is non-null it first calls the goal's vtable +0x11C with the same
  arguments; only if that returns false does it call `0x007476E0`. All goal
  classes use the stub `0x006A99F0` (`XOR AL,AL; RET 0xC`) except
  **DigGoal::GetTooltipText `0x006AC360`**, which acts only on pieces with
  flag 0x10000 (GROUND, `0x006AC39B`), looks the piece id (`piece+4`) up in a
  map at `goal+0x228` (`FUN_004224c0`) and switches on the dig object type
  (strings 184-203: artifact, gold, diamonds, buried hypercube, dirt/rock/
  stone/dark rock/boulder blocks). Its full branch table was not decoded.
- **Game caller:** `Board::UpdateTooltip` `0x007428A0` (vtable +0x390; Poker
  overrides with `0x006C90E0`). It runs only when vtable +0x17C
  (`0x00763990`) allows tooltips (board has a parent `+0x20`, not sliding,
  mouse over `+0x69`, no modal `gApp+0x380`, bytes `+0x288C`/`+0x2890` clear),
  finds the piece under the mouse with `FUN_0073BC50` (scans the grid at
  `board+0xF8 + 4*(row*8+col)`, piece rectangle `x,y = piece+0x10/+0x14`
  floats, size 128.0 = double `0x00880EA8`), calls +0x394, then hands
  title/body to the TooltipManager `FUN_00677D00` with the colour prefix
  literal `0x0085FED0`. So the game itself never builds the text without a
  mouse hover; the mod has to call +0x394.
- **Mod use (proposal, not implemented):** like `hover_text::BadgeTooltip`:
  construct two empty MSVC 2008 wstrings (0x1C bytes: buffer +4, size
  +0x14, capacity +0x18 = 7), call `vtable[0x394/4]` of the live board as
  `__fastcall(board, 0, piece, &title, &body)` inside `__try` on the game
  thread, free a buffer with capacity >= 8 through `0x0064DBCF`, check the
  slot address lies in .text. Calling through the vtable keeps the quest and
  Diamond Mine overrides. The text keeps any `^RRGGBB^` colour codes the
  string table has. The returned bool is itself useful: false = no special
  description.
- **Side effects:** base function: none seen (string allocations only).
  DigGoal path: `FUN_004224c0` on a map keyed by piece id was not checked;
  if it is `operator[]` it could insert a default entry for an unknown id
  (**uncertain**; only reached for GROUND pieces in Diamond Mine).
- **Reading the strings directly** is possible with `GameString(117..138)`,
  but the flag-to-id mapping would then be a code-derived table; calling the
  game function avoids that (decisions.md "last resort" rule).

### 1.2 Tutorial hints: Board::CheckForTutorialDialogs `0x00765D50` (static)

Strings 96-113 (source comment `Board::CheckForTutorialDialogs`). Mechanism:

- **Queue:** `QueueTutorial` `0x00745BF0` (custom convention: `ECX` = board,
  `EDI` = tutorial type, one pushed piece pointer) appends a 0x68-byte entry
  to the vector at **board+0x26AC** (begin `+0x26B8`, end `+0x26BC`), and
  sets bit `1<<type` in **board+0x26A8** and, unless byte `board+0x1412`,
  in the profile word **`[gApp+0xB80]+0x21C`** (persists per profile, so each
  hint appears once). Entry +0 = type, +4 = piece id (looked up again with
  `FUN_0073E370`, board map piece-id -> piece).
- **Gate before queueing:** callers first ask Board vtable +0x20C
  (`0x0073C4C0`; quest family `0x006C6460`): type bit not yet set in
  `+0x26A8`, bit 19 (0x80000 = hints disabled) clear, float `+0x1188` == 0,
  byte `+0xE28` == 0.
- **Display:** CheckForTutorialDialogs requires no dialog 0x12 or 0x13 open
  (gApp vtable +0x110), `board+0xD20` == 0, `board+0xD1C` (game-over count)
  == 0, bit 0x80000 clear, queue non-empty, byte `+0xE28` == 0. If the
  queued piece no longer exists (`FUN_0073E370` returns 0) it clears the bit
  in both words (`0x00765E75..0x00765E9B`) and drops the entry, so the hint
  can come back later. An entry whose piece id is -2 opens the HelpDialog
  (`FUN_00696100`, 0x300 bytes) instead. Otherwise it switches on
  `type - 1` (jump table `0x0076704C`, 16 entries) and builds a HintDialog
  (`FUN_00682BF0`, 0x238 bytes, dialog id 0x12, a Bej3Dialog) with header =
  title and body, positioned below the piece when `piece+0xC` (row) < 4,
  above otherwise.

| Type | Bit | Title / body ids | Queued by (call site, piece) |
|---|---|---|---|
| 1 | 0x2 | 96 / 97 (flame) | `0x0075C46D` in `FUN_0075AF60` (special-gem creation, see board-events) |
| 2 | 0x4 | 98 / 99 (star) | `0x0075BC47` in `FUN_0075AF60` |
| 3 | 0x8 | 100 / 101 (hypercube) | `0x0075C3D3` in `FUN_0075AF60` |
| 4 | 0x10 | 104 / 105 (multiplier) | `0x00714FC4` in `FUN_00714DD0` (increments `+0x3090`) |
| 6 | 0x40 | 102 / 103 (supernova) | `0x0075C2E2` in `FUN_0075AF60` |
| 8 | 0x100 | 106 / 107 (coin) | no `QueueTutorial` caller found (**open**) |
| 9 | 0x200 | 108 / 109 (time gem) | `0x00716E90` in `FUN_00716C60` (then tests `piece+0x244 == 5`) |
| 13 | 0x2000 | 110 / 111 (vertical match, ice) | `0x006E2456` in `FUN_006E2250`, then shows at once |
| 16 | 0x10000 | 112 / 113 (dark rock) | `0x006C35FA` in DigGoal `FUN_006C2AD0` (`EDI = -1+0x11`; pending piece id at goal+0x2C0) |
| 5, 7, 10-12, 14, 15 | - | no dialog | |

  Other bits of the same words are set directly, without a dialog (bit 11
  `0x00710CEE`, `0x00711BEA`; bit 12 `0x006C33C2`; bit 14 `0x006DF129`).
  Bit 19 (0x80000) is the hints-disabled switch: `FUN_006746E0` sets or
  clears it (through `FUN_0073B990` when a board exists).
- **Mod use:** the mod already reads HintDialogs as dialogs (status.md). The
  type tells which piece the hint is about (entry +4 = piece id); speaking
  the piece's cell with the hint would be the design addition. Watching
  `board+0x26A8` bits appearing is a cheap "special gem first created" event
  (but only the first time per profile).

### 1.3 Other descriptive text (pointers, not re-researched)

- Illegal move notices 114-116 (Board::UpdateSwapping) and 152-154
  (Board::MouseDown); count popups 142/143; multiplier 144-146; pause 147;
  timer 148/151; score 157; level complete 139; get ready/go 140/141. See
  `hud.md` and the other board docs for the code.
- Help pages 243-261 (HelpDialog::SetupHelp), by mode; Poker hand tooltips
  386-400 and 568/569 (PokerBoard::UpdateTooltip `0x006C90E0`); Ice Storm
  combo names 549-560; quest description `board+0x2680` (hud.md).

## 2. Colour names (static)

The game names gem colours only in the Records **StatsTab** ("favourite gem
colour"), strings **455..461**, each starting with a colour code
(`^RRGGBB^`). String 454 is the "none" text.

Evidence chain:

1. `StatsTab::Draw` `0x0069E3E0` builds a local array of 7 wstrings from ids
   0x1C7..0x1CD (455..461) at consecutive 0x1C-byte slots (`LEA ESI` targets
   `ESP+0x2BC, +0x2D8, ... +0x364` at `0x0069EC91..0x0069EF97`, frame base
   `ESP+0x2B4`), then takes the arg-max over 7 ints at
   **`[gApp+0xB80]+0xD4 + 4*i`** (`0x0069EFD4..0x0069EFF4`) and copies
   `array[i]` (`LEA EDX,[ESP+ECX*4+0x2B8]` with `ECX = 7*i`, `0x0069EFFD`).
2. Those ints are profile stats `0xC0 + 4*stat` (AddStat `0x0073D170` adds to
   board `+0xC84+4*stat` and profile `+0xC0+4*stat`), i.e. stats 5..11.
3. The piece-cleared path `0x0073D2B0` adds stat 4 (gems matched, profile
   `+0xD0`, StatsTab label 449) and then stat **`[piece+0x220] + 5`**
   (`MOV EAX,[EDI+0x220]; ... LEA ECX,[EAX+5]; CALL 0x0073D170` at
   `0x0073D301..0x0073D318`), skipped when colour is -1 or the piece has
   flag 0x10000.

So **colour value c (0..6) at `piece+0x220` <-> string 455+c** is backed by
the game's own code. Community enums (deck-bot `COLOR2GLYPH`, PlusExtender
`PieceColor`) list the same order: 0 red, 1 white, 2 green, 3 yellow,
4 purple, 5 orange, 6 blue (community labels), and the string ids' colour
codes agree. Colour -1 = no colour (hypercube, dirt). The mod should use
`GameString(455 + c)` with the colour code stripped, and list this mapping in
status.md as a code-backed fixed point. No other colour-name source was found
(tooltips do not name colours; the hypercube text speaks of "the same
colour" generically).

## 3. Community offsets vs this build

Sources read (facts only, no code copied): FatBrother1/bejeweled3-deck-bot
`bot/reader_mem.py` (same gApp `0x008E1730`, i.e. this build);
bognarit80/Bejeweled3PlusExtender `mods/gamefunctions.{h,cpp}`,
`mods/sandboxfunctions.cpp` (MIT; gApp `0x008A67AC`, another build: its Board
vtable `0x00829B4C` = ours `0x00864CB4` - 0x3B168, same delta for
ClassicBoard); HyperNervie/Bejeweled-Autosplitters `Bejeweled3.asl`,
`bonus/Bej3 Poker/Bej3-Poker.asl` (module offset `0x487F34`, another build).

### Piece fields (all three agree with each other; the Piece struct looks identical across builds)

| Field | Community | This build | Evidence here |
|---|---|---|---|
| grid | `[Board+0xF8+4*col+32*row]` (deck-bot) | **match** | `FUN_0073BC50` index `0x3E+row*8+col` dwords; save/load loops start at `LEA EBX,[ESI+0xF8]` (`0x00764B8E`), 8x8 row-major |
| id | - | `+0x4` | save writes `[piece+4]` first (`0x007465DF`); `FUN_0073E370` id -> piece |
| col / row | `+0x8` / `+0xC` (PlusExtender GetGemX/Y) | **match** | load stores the loop col at `+0x8`, row at `+0xC` (`0x00764C3B..46`) |
| x / y | - | `+0x10` / `+0x14` floats, 128 per cell | `FUN_0073BC50`; file: (col*128, row*128) |
| previous colour | `+0x21C` (deck-bot "precolor") | **type matches** (signed byte saved) | file: -1 on both hypercubes and on plain gems, 5 on a star of colour 5; deck-bot's "hypercube keeps its colour here" is **not** supported by this save |
| colour | `+0x220`, -1..6 | **match** | tooltip-free code `0x0073D301`; save sbyte `0x00764DCB`; file values -1..6 |
| flags | `+0x228` | **match** | GetTooltipText `0x00747715`; save `0x00764DDB` |
| flag bits | PlusExtender enum (1 flame, 2 hypercube, 4 star, 0x10 multiplier, 0x20/0x40 bombs, 0x80 butterfly, 0x400 coin, 0x800 detonator, 0x1000 scrambler, 0x10000 ground, 0x20000 time bonus, ...) | **match for every bit GetTooltipText tests** (table 1.1); 0x10000 = ground confirmed by DigGoal tooltip and AddStat skip | deck-bot "supernova = 5" matches the 0x1+0x4 test |
| destroy / "+0x230" | int (PlusExtender DestroyGem) | short in the save | `0x00764DE8` |
| counter | `+0x244` | **match** | time-gem seconds in GetTooltipText `0x00747F59`; `CMP [ESI+0x244],5` `0x00716E9A`; saved as signed byte |
| shake / spin | `+0x264`, `+0x26C`, `+0x270` floats | **type matches** (floats saved there) | `0x00764E84..0x00764F23` |
| counter flag | deck-bot calls 0x20000 "COUNTER"; PlusExtender COUNTER = 0x200 | 0x20000 is the **time gem** here | tooltip ids 135/136 |

### Board / app fields

| Field | Community | This build | Status |
|---|---|---|---|
| board pointer | deck-bot `gApp+0xBE8`; PlusExtender / autosplitter `+0xBE0` | `+0xBE8` | deck-bot **match**; others other build (hud.md) |
| clock | deck-bot / poker asl `Board+0x38` | **match** | save writes `[ESI+0x38]` right after the header (`0x0074659B`); file 10158 / 12369 (centiseconds, hypothesis) |
| score | deck-bot `+0xD24`; asl `+0xD20`; PlusExtender `+0xD28` | `+0xD24` | deck-bot **match**; load sets `+0xD24` and `+0xD64` from one int (`0x0076535F`) |
| shown score | asl `+0xD60` | `+0xD64` | asl other build (+4) |
| game over | asl `+0xD88` | `+0xD8C` | asl other build (+4); load reads a bool into `+0xD8C` |
| level complete pending | asl `+0x288C` | **same offset** `+0x288C` | save writer refuses when set (`0x00746559`); hud.md. So the +4 shift of the asl build is not uniform |
| "level" | deck-bot `+0xE0C` | `+0xE0C` is the multiplier | hud.md; save loads a short into `+0xE0C` and the level short into `+0xE04` |
| progress | deck-bot `+0xE00` | **match** (level points) | hud.md; save int at `+0xE00` |
| multiplier | PlusExtender `+0xE14` | `+0xE0C` | other build |
| level | PlusExtender `+0xE0C` (+1) | `+0xE04` (0-based) | other build |
| quest goal | PlusExtender `+0x31F0` | `+0x3180` | other build (`0x006CD7F0` reads `+0x3180`) |
| quest id / real-time bomb / balance | PlusExtender `+0x32A4`, `+0x32CA`, `+0x32B8..C8` | not checked | other build, likely shifted |
| stats | PlusExtender butterflies `+0xCF0` (stat 0x1C) | `+0xCF4` (`+0xC84+4*0x1C`) | other build (+4); AddStat `0x0073D170` |
| gem count | PlusExtender `+0x250` | `+0x250` is used by the piece-id map here (`FUN_0073E370`) | not comparable |
| profile | asl / PlusExtender `gApp+0xB78`, completed quests `+0x158`, secrets `+0x180` | `gApp+0xB80`; `+0x158`, `+0x180` **match** (hover-text.md) | app offsets +8 in this build |
| QuestMenu | asl `gApp+0xBD0`, page `+0x14C` | `gApp+0xBD8` (rare-dialogs.md), page `+0x14C` **match** | +8 |
| Poker fields | poker asl `+0x3788..+0x3A20` | not checked | other build (hud.md) |

## 4. The board in the save files

### 4.1 Functions (static)

- File name: Board vtable **+0x160** (`GetSavedGameName`): Classic
  `0x0073AFB0` returns narrow std::string literal at `0x0085E520`, Zen
  `0x0070E590` literal `0x00838674`; other modes have their own literals
  (`0x007D835C` .. `0x00839230`). Empty name = no save.
- Path: `FUN_0071B6B0(..., [gApp+0xB80]+0x60)` (profile name) + file name.
- **Write to buffer:** Board vtable +0x220 = `0x00746510` for all modes
  (base part); refuses (returns early) unless the vector `+0x125C/+0x1260`
  is empty, byte `+0x288C` (level complete pending) is 0 and `+0x2890` is 0.
  Mode-specific data follows through vtable +0x22C (Board `0x0073C1C0`, Zen
  `0x0070E4F0`, Speed `0x00718FE0`, Quest family `0x006C6400`).
- **Save to file:** `0x00742F10` (sets byte `+0xD28` during the call), using
  `FUN_00741710`: it takes the buffer of the last entry of the vector at
  `+0xC48/+0xC4C` (0xD8-byte move records, buffer at entry+0x14) or the
  board's own buffer at `+0x1270`, i.e. the saved state is the snapshot
  taken at the last settled move, not the frame being drawn. Writes with
  `FUN_0046CF40`. Delete: `0x00743120`; exists check: `0x007432A0`.
- **Load:** `0x0074E1F0` reads the file (`FUN_0046CF70`) and calls vtable
  +0x224 (Board `0x00764AB0`; Classic `0x0073B4D0` just calls it). Called from
  board init `0x0074E410` when vtable +0x210 says a saved game should be
  resumed.
- **Buffer primitives** (PopCap Buffer, little-endian, byte cursor at
  buffer+0x20): ReadByte `0x0054FD80`, ReadBoolean `0x0054FE40`, ReadShort
  `0x0054FE50`, ReadLong `0x0054FE80`, ReadBytes(EBX = n) `0x00550390`
  (used for 4-byte floats), ReadDouble `0x0073E3F0`, ReadString
  `0x00550050` (short length + bytes); writers `0x0054F750` byte,
  `0x0054F7F0` bool, `0x0054F810` short, `0x0054F830` long, `0x0054FC70`
  bytes. CurvedVal: read `0x0074A6E0` / write `0x00745FD0`.

### 4.2 Layout (static, checked against both throwaway saves)

Header: long magic **0x93AB**, long version **0x11** (17; writer constant,
the loader's board-level part requires exactly 17 at `0x0076532A`), long
**7** (checked, `0x00764B5B`), long `board+0x38` (clock).

Then 64 records, **row-major** (row 0 col 0 first; the loader stores the
loop row in `piece+0xC` and column in `piece+0x8`):

| Bytes | Field |
|---|---|
| bool | cell occupied (0 = empty, nothing else follows) |
| long | piece id `+0x4` (passed to the Piece ctor `FUN_007202D0`, 0x630 bytes) |
| float, float | `+0x10` x, `+0x14` y |
| long | `+0x18` |
| float | `+0x1C` |
| CurvedVal x3 | `+0x20`, `+0x90`, `+0x100` |
| float | `+0x170` |
| bool x8 | `+0x17C, +0x17D, +0x17E, +0x180, +0x181, +0x182, +0x183, +0x17F` (this order) |
| short | `+0x184` |
| long x3 | `+0x188, +0x18C, +0x190` |
| CurvedVal | `+0x198` |
| byte, sbyte | `+0x208` (unsigned), `+0x20C` (signed) |
| long x3 | `+0x210, +0x214, +0x218` |
| sbyte | `+0x21C` previous colour |
| sbyte | **`+0x220` colour** |
| long | **`+0x228` flags** |
| short | `+0x230` |
| long x2 | `+0x234, +0x238` |
| bool | `+0x23C` |
| sbyte | **`+0x244` counter** |
| short | `+0x248` |
| float x9 | `+0x258 .. +0x278` |

CurvedVal encoding: byte mode; 0 = nothing; 1 = one double; 2 = string
(curve data), byte, byte, 3 doubles, long (time offset from
`[gApp+0x598]`), 4 doubles, 6 bools, 2 doubles.

After the grid: 8 floats `board+0x1F8..`, 8 longs `board+0x218..`, a
string of 2500 bytes in both files (= 624+1 dwords, the size of a
Mersenne-twister state; read into `board+0x258` via `FUN_00586500` - the
RNG is saved, **hypothesis** from the size), a short count of pending swap
records (`+0xC24`: two piece ids, two sbytes, two CurvedVals, bool, short,
three bools), a short count of move records (`+0xC3C`: long, long, byte,
byte, [version 1 only: extra], long, 38 longs), a long count for the list at
`+0xC54` (`FUN_00741320`), then (version 17) 38 longs = board stats
`+0xC84`, long score -> `+0xD24` and `+0xD64`, and more board fields
(`+0xE00` long, `+0xE04` short level, `+0xE0C` short multiplier, CurvedVals,
...; read sequence `0x0076535F..0x00765BC0`, not all decoded), ending with
the next piece id `+0xF4`, `+0x2880`, the game-over bool `+0xD8C`, then the
mode part via vtable +0x22C. The loader clears the tutorial queue
(`+0x26A4 = 0`, vector `+0x26AC`).

File check (`private/ghidra-b6/sav.py`, read-only): both files parse to
offset 11350 of 11683 with consistent values: ids 0..132, positions exactly
(col*128, row*128), colours -1..6; `classic.sav` holds two hypercubes
(flags 0x2, colour -1) and one star (0x4, colour 5); stats show score 100
(= stat 1 and the score long), stat 4 = 5 and stat 10 (colour 5) = 4, which
fits a five-gem L/T match of colour 5 that left a star of colour 5.
The last 333 bytes (board tail and mode part) were not parsed.

### 4.3 What the save tells a design

- The piece record order is a second confirmation of the field types; it
  adds the id (`+4`), the row/col fields and the float coordinates.
- Use for the mod: a resume check ("a saved game exists, this many specials")
  or offline tests. Not a live source: written only when the board is
  settled and on leaving, from the last move snapshot.

## Uncertainties

- Everything is static or file-parsed; no call of GetTooltipText was made.
- `FUN_004224c0` in DigGoal::GetTooltipText may insert into the map
  (operator[] vs find not checked). DigGoal branch table not fully decoded.
- Coin hint (type 8): no queue call site found; it may be queued by inlined
  code or never.
- Meaning of `board+0xD20`, `+0x1188`, `+0xE28`, `+0x1412` (gates); `+0x1412`
  also suppresses profile stats in AddStat, so it is probably a replay/
  demo/no-save board flag (**hypothesis**).
- Save: version-conditional branches of the loader not fully mapped; board
  tail after the stats only partly decoded; RNG-state reading is a
  hypothesis from its size.
- Clock `+0x38` unit (centiseconds) is a hypothesis (deck-bot says so; the
  file values 10158/12369 fit a ~2 minute session).
- Community "precolour" `+0x21C` for hypercubes contradicts this save file
  (both hypercubes -1); maybe set only while a hypercube is being created.

## Runtime checklist (lead, throwaway profile)

- [ ] In Classic, for a flame, star, hypercube and a plain gem: call
      `vtable[0x394/4](board, piece, &t, &b)`; expect true + ids 119/120,
      121/122, 123/124 text, false for the plain gem; no fault, no leak.
- [ ] Same in a quest or Diamond Mine board (override `0x006CD7F0`); on a
      dirt block (flag 0x10000) expect DigGoal text 192/193 etc.; compare
      the DigGoal map size before/after (insertion side effect).
- [ ] Time gem (Lightning): flag 0x20000, `+0x244` = the number in the
      tooltip body.
- [ ] `piece+0x220` for each colour matches what the user (or a screenshot
      check) sees, and `GameString(455+c)` names it.
- [ ] Profile `[gApp+0xB80]+0xD4+4*c` grows by the gems of colour c cleared;
      StatsTab favourite colour equals the arg-max.
- [ ] Make the first flame of a fresh profile: `board+0x26A8` gets bit 0x2,
      profile `+0x21C` too, queue `+0x26B8/+0x26BC` holds one 0x68 entry
      (type 1, piece id), then the HintDialog (id 0x12) appears.
- [ ] Toggle the HintDialog checkbox: bit 0x80000 in both words.
- [ ] `classic.sav` written after a move: run `python -I
      private/ghidra-b6/sav.py <copy of classic.sav>` and compare the grid
      with live `[board+0xF8]` colours/flags/ids.
- [ ] Board `+0x38` increments ~100 per second while playing (unit check).
- [ ] Hypercube `+0x21C` value during and after creation (deck-bot claim).
