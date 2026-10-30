# Hover-only text without a mouse

Pass: 2026-10-30, static analysis only (Ghidra 12.0.4 copy
`private/ghidra-hover`, Steam `Bejeweled3.exe` SHA-256 d5516244...88c3, plus
offline reads of the exe bytes). **Nothing here is runtime-verified.** Code:
`src/mod/hover_text.{h,cpp}` (compile-checked with `tools/compile-one.bat`,
not built, not integrated).

Goal (user, session 5): the mod must not depend on screen, resolution,
window state or a mouse. Today `FinishHoverProbe` (`navigator.cpp`) moves
the game's mouse over an item and diffs the drawn text. This document lists,
per kind of hover text, where the game keeps or builds it and how the mod can
get it directly.

Labels: **static** = read in decompilation/disassembly; **hypothesis** =
inferred, not proven; **runtime** = observed by the lead (quoted from the
task, not re-checked here).

Tools: `tools/ghidra/HoverScan.java` (new; lists every instruction whose text
contains a substring such as `+ 0xcf0]`, used to trace struct fields that have
no symbol). Wrappers and raw output in `private/ghidra-hover/` (`dec.sh`,
`dis.sh`, `scan.sh`, `vt.sh`; `rd.py` / `vt.py` read literals and vtables from
the exe). Game text is not quoted here; string ids refer to
`properties/default.xml`.

gApp: `[0x008E1730]` and `[0x008E170C]` hold the same Bej3App pointer
(`docs/re/hud.md`); code below often uses the second.

## Summary

| Text | Source | Needs a hover first? | Function |
|---|---|---|---|
| Play menu mode description | PlayMenu+0x1C8, written by ButtonMouseEnter | yes: the game builds it only in its listener; call `game_actions::HoverWidget(modeButton, true)` (no mouse), then read | `PlayMenuDescription` |
| Locked-mode heading | string 352, drawn when PlayMenu+0x1E4 is set | same | `PlayMenuDescription(..., &locked)` + `GameString(352)` |
| Quest orb name/description | quest data `[gApp+0xBCC]+0x94`, entry name + map["Description"] | no | `QuestOrbText`, `QuestText` |
| Badge tooltip title/body | badge names table + Badge vtable slots 4/6/7 | no | `BadgeTooltip` (calls game code) |
| Zen section description | English literal in the exe, address in the game's own PUSH | no | `ZenSectionDescription` |
| Main menu tip line | tips vector `gApp+0xCFC`, index `gApp+0xD78` | no | `CurrentTip` |
| Bej3DialogButton tooltip | button +0x184 / +0x1A0 | no | `DialogButtonTooltip` |
| Any active tooltip | TooltipManager `[gApp+0xBF0]` entries | yes (diagnostics) | `ReadActiveTooltips` |

## Play menu

ButtonMouseEnter `FUN_0068e0e0` (PlayMenu listener, ECX = PlayMenu+0xBC),
static:

- Per id 0..7 it fetches a string with `FUN_00475e30(id, fallback)` from the
  numeric table: 0:365, 1:366, 2:367, 3:368 (no lock test); 4..7 test
  `[[gApp+0xB80]+0x180+(id-4)]` (unlock flag) and use 369/371/373/375 when
  set, else the locked strings 370/372/374/376 formatted with
  `FUN_0043b940` (StrFormat) using a level number or score pushed as an
  immediate (5 at 0068e51e and 0068e694; 100000 = 0x186A0 at 0068e7b3,
  thousands-formatted by `FUN_0043b2e0`) and a colour given as a narrow
  string built from the literal at 0x007D10F4 (`PUSH 6; PUSH 0x7d10f4` at
  0068e149). Id 7's locked text has only the colour argument.
- At 0068ea3c..0068ea9b: if the curve at PlayMenu+0xE8 evaluated at the
  double at +0x138 is not 0.0 and the built string is non-empty, it assigns
  the string to **PlayMenu+0x1C8** (`LEA ECX,[ESI+0x10C]`, ESI = listener)
  and stores the locked flag (BL, 1 only on the locked branches) at
  **PlayMenu+0x1E4** (`MOV [ESI+0x128],BL`). Otherwise (ids 100/101 back and
  quit, or the curve at 0) it calls `FUN_00684660` and leaves +0x1C8 as it
  was (stale text from the previous mode).
- `PlayMenu::Draw` `FUN_00689d20` draws string 0x160 = 352 when +0x1E4 is set
  and then `this+0x1C8` with WriteString (`FUN_00434f50`) centred.

So the description is not stored per mode; producing it for a mode without
the listener would mean re-implementing the locked formatting with the
game's numbers and colour, i.e. a code-derived lookup table. Not done
(decisions.md: last resort). **Integration:** call
`game_actions::HoverWidget(modeButton, true)` (it runs the game's
MouseEnter -> `ButtonMouseEnter(id)`, which writes the field synchronously,
plus the hover sound the mouse would make), then
`hover_text::PlayMenuDescription(playMenu, &text, &locked)`; when `locked`,
prefix `GameString(352)`. The runtime example for locked mode 4 (the
heading followed by the unlock condition, formatted with level 5) is
exactly 352 + formatted 370.
`PlayModeUnlocked(id)` reads the same flags the listener tests.

## Quest orbs

Quest data (static):

- `QuestMenu::ButtonDepress` `FUN_00697640`: ids >= 1000 start a quest:
  `slot = id - 1000`, or `id - 2000` when that is still >= 1000;
  `FUN_00774060(app, slot + [this+0x90]*8, [gApp+0xBCC], 0)`; the listener is
  at QuestMenu+0xBC, so the page is **QuestMenu+0x14C** (also written by
  buttons 0/1: `(page +- 1) % 5`).
- `QuestMenu` ctor `FUN_00693760` creates two QuestMenuBtn (vtable
  0x0086E4F4) per slot 0..7 and stores them at **+0x4CC+4*slot** and
  **+0x4EC+4*slot** (`puStack_4c = this + 0x13b`, `puStack_4c[-8]`).
  QuestMenu::Update `FUN_00697d40` makes the first visible when the profile's
  completion byte `[[gApp+0xB80]+0x158+page*8+slot]` is 0, the second when it
  is set (0069879a..006987f5).
- QuestMenuBtn adds no text fields: its vtable (70 slots listed with
  `vt.sh`, compared with ZenCategoryContainer 0x0086F274, whose own
  overrides are only 0/22/26/27) is ButtonWidget's plus dtor 004044C0, Draw
  0067BED0 (draws images only) and DrawAll 00791BA0; its extra fields
  +0x170..+0x17C (cel rectangle), +0x180 (overlay image), +0x184/+0x188
  (overlay offset) are written by QuestMenu::Update from image data.
- Tooltip (Update, 00698cea..00699130), for each slot when the button is
  visible (+0x64) and mIsOver (+0x69) and the page object is idle:
  `quests = [gApp+0xBCC]`, vector **begin +0x94, end +0x98**, element size
  **0x3C** (`IMUL 0x88888889`, `SAR 5` = /0x3C; `index*15*4`); index =
  `slot + page*8` (`LEA EDI,[EAX+EDX*8]`).
  - body: `FUN_00403180(entry+0x1C, "Description")` (key literal 0x007D1AF0,
    11 chars) then copy (`FUN_0043ac10` -> `FUN_0043ab50`, wstring copy),
    then `FUN_00406740` prefixes the wide literal 0x007D1AFC (a colour code).
  - title: wstring copy of **entry+0** (the quest name).
  - `FUN_00677d00(title, body, ...)` with `[gApp+0xBF0]` (TooltipManager).
- `FUN_00403180` is `std::map<std::string, std::wstring>::operator[]`: it
  uses `FUN_00403a50` (lower_bound, same as the named string table), head at
  map+0x18, compares the key's narrow string at node+0x10/+0x20/+0x24 and
  returns **node+0x28** (value). Missing keys would be inserted, so the mod
  uses its own read-only tree walk (same node layout as
  `game_strings.cpp`'s named table: key +0xC, value +0x28, `_Isnil` +0x45).
- Data origin: `properties/defaultquest.cfg` in the pak has `Quest "<name>"`
  blocks with `Description = "..."` (one of the "Goal" key, literal
  0x007D1AE8, is also read here for the progress image). The gem count in
  the runtime tooltip of one quest (120) does not match that file's
  value for the same quest (100), so the game probably loads another config; the mod reads memory, so it does
  not matter, but see the checklist.

Mod: `QuestIndexForOrb(questMenu, orb)` (id -> slot, checks the pointer is
one of the 16 orb slots, adds the page), `QuestText(index, &name,
&description)`, `QuestOrbText` (both). `QuestCount()` for diagnostics.

## Badges

Static, extending `docs/re/badges.md`:

- Title `FUN_00778430` (ECX = out, EDX = badge): calls Badge vtable +0x10
  (slot 4) with ECX = badge and one pushed pointer (hidden return
  std::wstring, 0077845b..0077846d), then `names[id] + result` via
  `FUN_00403d50` (assign + append) with `names` = vector<wstring> at
  gApp+0xD2C/+0xD30 (`IMUL 0x92492493` = /0x1C). Then frees the temporary
  with `CMP [cap],8; JC; PUSH buffer; CALL 0x0064DBCF` (007784c2).
- Body `FUN_00776f90` (ESI = out, EDI = badge): level count = `FUN_00776940`
  (entry `[gApp+0xD44] + id*0x18`, `(end - begin) >> 2` of the int vector at
  entry+0xC/+0x10); `badge+8 < count` -> slot 6 (+0x18), else slot 7
  (+0x1C). Its register convention is custom, so the mod repeats this test
  instead of calling it.
- Slot functions (21 vtables 0x00863F08 base, 0x00863F38..0x008642C8
  derived, 11 slots each, listed by `private/ghidra-hover/vt.py`) are
  `__thiscall(Badge*, std::wstring* out)` returning `out` with **RET 4**
  (checked: 0077d77e, 00776f4d, 00776f68, 00776f88). Base slots 6/7
  (00776F70, 00776F50) return an empty string; slot 4 is `FUN_0077d600`
  (string 0, a two-integer progress format, formatted from slots 9/10 and the previous
  threshold; empty when level == count) or, for score badges, a function
  returning an empty string (00776F20 etc.). Sampled slot 6/7 bodies
  (`FUN_0077cae0`, `FUN_0077c6c0`, `FUN_0077c580`) only read the string
  table, level names (`FUN_00778130`) and the badge's target/value slots,
  then StrFormat; no writes to game state were seen.
- `FUN_0064DBCF` is `operator delete`: `MOV EDI,EDI; PUSH EBP; MOV EBP,ESP;
  POP EBP; JMP 0x0064F8B0` (`_free`), cdecl, caller pops.

Mod `BadgeTooltip(badge, &title, &body)`: checks `IsKindOf(badge, "Badge")`,
reads id/level, the name and level count, calls slot 4 and slot 6/7 through
a `__fastcall(self, unusedEdx, out)` pointer (same ECX + one callee-popped
stack argument as thiscall) inside `__try`, initialises the out buffer as an
empty wstring first, copies the text and frees a heap buffer (capacity >= 8)
with 0x0064DBCF exactly like the game. The slot address must lie in .text
(0x00401000..0x007BE575). Faults are logged and return false. The body is
returned without the "^FFAACC^" colour prefix the tooltip adds. Runtime
example: badge 2's name with a 0-of-25 progress suffix, and string 12
formatted with 25 and the first level name.

## Zen section descriptions

`ZenCategoryContainer::DrawAll` `FUN_0042ab20` (static) builds a local
`std::wstring desc[4]` with `FUN_00403650(literal, length)` and selects
`desc[[container+0xC4]]` (0042b1ad `MOV EAX,[ESI+0xC4]`, `LEA EAX,[ESP+EDX*4+0x100]`
with EDX = id*7, i.e. id*0x1C from the same base as the first element built
at 0042b07a; ESP differs by the one PUSH in between). Construction sites, in
section order (bytes checked offline):

| Section (title id) | Length push | Literal push | Length | Literal |
|---|---|---|---|---|
| 0 (527) | 0042B06E `6A 78` | 0042B075 `68 30 D4 86 00` | 120 | 0x0086D430 |
| 1 (528) | 0042B0A8 `6A 6D` | 0042B0AC `68 28 D5 86 00` | 109 | 0x0086D528 |
| 2 (529) | 0042B0E1 `68 8C 00 00 00` | 0042B0EE `68 08 D6 86 00` | 140 | 0x0086D608 |
| 3 (530) | 0042B119 `6A 72` | 0042B11D `68 28 D7 86 00` | 114 | 0x0086D728 |

Each literal is NUL-terminated exactly at its length (checked offline). The
section-1 literal begins like the section-1 description seen at runtime. This
supersedes the hypothesis in `zen-options.md` that the second wrapped
rectangle holds the description: the array is indexed by section id.

Mod `ZenSectionDescription(id, &text)` reads the two PUSH operands from the
running game's code (verifying the opcodes `6A`/`68`), then the wide literal
from .rdata, and requires the terminator at the given length. No section has
to be open and no mouse is used. These four instruction addresses are a
code-backed fixed point (list in status.md per decisions.md); the text
itself is never copied into the mod.

## Main menu tip line

Static:

- `FUN_007748e0` (app startup) reads `properties\tips.txt` (narrow literal
  0x00861A64) line by line and appends each line as a wstring with
  `FUN_00404b00` to the vector object at gApp+0xCF0 (`ADD EBX,0xcf0` at
  00774d5a). The Bej3App ctor `FUN_0076d9e0` constructs 0x18-byte vector
  objects at gApp+0xCA8, 0xCC0, 0xCD8, **0xCF0**, 0xD08, 0xD20, 0xD38, 0xD50
  (`FUN_0041aef0`); like the badge-name table (object 0xD20, begin 0xD2C),
  begin/end are at object+0xC/+0x10, i.e. **gApp+0xCFC / +0xD00**.
- TipIdx: int at **gApp+0xD78**, loaded/saved under the registry name
  "TipIdx" (`FUN_0076d450` 0076d993, `FUN_0076fe40` 00770281), incremented
  by `FUN_0076c910` (`INC [ESI+0xD78]`; caller not traced).
- `MainMenu::Draw` `FUN_006a0ca0` (vtable 0x0086C35C slot 26): if the tip
  count `(end-begin)/0x1C` is non-zero and `[gApp+0xD78] > 0` (006a25c5..
  006a25f1) and a fade value at MainMenu+0x708 passes a test, it computes
  `index = (TipIdx - 1) % count` (unsigned `DIV`, 006a27a7..006a27bc) and
  draws `tips[index]` with WriteString at x = width/2,
  y = S*0x48D/1200 (006a2811..006a2820).

Mod `CurrentTip(&text, &index, &count)`. The fade state is not decoded, so
the tip may be read while it is faded out; speak it when the main menu is
announced, not as a change.

## Bej3DialogButton tooltips

`Bej3DialogButton` (vtable 0x00864364) Update `FUN_00404260` (slot 22):
when mIsOver (+0x69) and the wstring size at +0x1B4 is non-zero, it requests
a tooltip with body = literal 0x008642F4 (colour code) + wstring **+0x1A0**
and title = wstring **+0x184**, anchored above the button centre. Which
buttons set these fields was not traced. Mod: `DialogButtonTooltip`.

## TooltipManager (generic)

`FUN_00677d00` (custom convention; the TooltipManager `[gApp+0xBF0]` and the
requesting widget are the two pushed arguments, title/body std::wstrings by
value on the stack) first aborts if a modal widget above the requester is
in front, then looks for an entry with the same anchor (+0/+4) in the
vector at **manager+0xD4/+0xD8** (0x78-byte entries) and only sets its
refresh byte **+0x5C**; otherwise it builds a new entry and appends it
(`FUN_0042bef0`). Entry layout from the local frame: +0 x, +4 y,
+8..+0x14 rectangle, +0x18 arrow direction (0..3), **+0x1C title**,
**+0x38 body**, +0x54/+0x58 parameters, +0x5C refreshed, +0x60/+0x64
fonts, +0x68.. colour. The title/body order is a **hypothesis** consistent
with every call site (title pushed last). Also stores the last anchor at
manager+0xBC/+0xC0.

Callers of `FUN_00677d00`: BadgeWidget Update 0068fb50, QuestMenu Update
00697d40 (orbs, and three more with string ids 420, 570/571), Bej3DialogButton
Update 00404260, PokerBoard::UpdateTooltip 006c90e0, ZenBoard::Update
007116c0, DigGoalEndLevelDialog::Update 00736010, and 006b76c0 / 006c2ad0
(WallBlast / Dig goals, by vtable proximity, unconfirmed) and 007428a0
(board, not identified). Gem tooltips come from `Board::GetTooltipText`
`FUN_007476e0` (string ids 117-136): board phase.

Mod `ReadActiveTooltips` (checks the RTTI class "TooltipManager"). Entries
only exist while something is hovered, so this is a diagnostic or fallback
reader, not a no-hover source.

## Integration steps (lead)

1. Add `src\mod\hover_text.cpp` to `tools/build.bat`.
2. Play menu: replace the mouse hover by `game_actions::HoverWidget(button,
   true)`; then `hover_text::PlayMenuDescription(playMenu, &text, &locked)`;
   speak `GameString(352)` first when locked. (`ui_names`'s
   `PlayMenuHoverDescription` reads the same field.)
3. Quest menu: for an orb item, `QuestOrbText(questMenu, orb, &name, &desc)`
   and speak the name as the item label (instead of the image name) and the
   description after it; no hover needed. The page changes with buttons 0/1.
4. Badges: per cell `BadgeTooltip(cell.badge, &title, &body)` (badge pointer
   from `badges.h`'s `ReadBadgeGrid`); no hover needed.
5. Zen: `ZenSectionDescription(containerId, &text)` from the container's
   +0xC4; speak it with the section header without waiting for the
   expansion. Selecting the section remains `game_actions::SelectZenSection`.
6. Main menu: `CurrentTip` when announcing the main menu (optional).
7. Keep `FinishHoverProbe` only as a logged fallback while each direct path
   is verified, then remove it per decisions.md (session 5).
8. Status.md: list the Zen literal sites and the badge slot calls as
   code-backed fixed points; index.md: link this file.

## Runtime checklist

- [ ] Play menu: `HoverWidget(orb i, true)` then `PlayMenuDescription`: the
      text equals what the game draws, for 0..7 and for locked modes
      (`locked` = 1, 352 drawn). Hover back/quit: field unchanged (stale).
      Check during the menu's slide-in (curve at 0 -> no update).
- [ ] `PlayModeUnlocked(4..7)` agrees with the lock shown.
- [ ] Quest menu: `QuestCount()` (record it; pages are 0..4 by the page
      buttons' `% 5`, so up to 40 orbs), page 0 orbs:
      `QuestOrbText` name/description equal the drawn tooltip title and
      body. Change page and repeat. Completed orb
      (id 2000+) gives the same quest.
- [ ] Badges: `BadgeTooltip` for all 20 on the Records tab: no fault logged,
      title is the badge name plus progress suffix, body equals the drawn tooltip; with an
      earned badge, level name in the body; at max level the slot-7 text and
      an empty suffix.
- [ ] Badges: memory does not grow when calling it repeatedly (free path).
- [ ] Zen: `ZenSectionDescription(0..3)` equals the drawn description of
      each section, with no section open.
- [ ] Main menu: `CurrentTip` equals the bottom tip line; note TipIdx and
      whether it changes per visit.
- [ ] `ReadActiveTooltips` while hovering a badge: one entry, title/body as
      above (confirms the 0x78-entry hypothesis), refreshed byte toggling.
- [ ] `DialogButtonTooltip`: find a Bej3DialogButton with non-empty +0x1A0
      (none known yet).

## Uncertainties

- All static. The PlayMenu curve condition is described, not read by the
  mod (evaluating it needs the game's CurvedVal code with a custom
  convention).
- Badge slot functions were sampled (3 of 60 bodies decompiled); all 21
  vtables point into .text and the base slots are trivial, but a slot with
  side effects cannot be excluded without runtime checks.
- The quest config file actually loaded was not identified (defaultquest.cfg
  numbers differ from the runtime tooltip).
- TooltipManager entry title/body order and Bej3DialogButton writers.
- The MainMenu tip fade (+0x708) and who calls the TipIdx increment.
