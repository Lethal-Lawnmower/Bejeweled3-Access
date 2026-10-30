# Zen options panel (ZenOptionsDialog / ZenCategoryContainer)

Pass: 2026-10-30, static analysis only (Ghidra 12.0.4 copy
`private/ghidra-zen`, Steam `Bejeweled3.exe` SHA-256 d5516244...88c3). Nothing
here is runtime-verified yet; see the checklist at the end. Code:
`src/mod/zen_options.{h,cpp}` (compile-checked with `tools/compile-one.bat`,
not built or integrated).

Evidence labels: **static** = read from decompilation/disassembly;
**hypothesis** = inferred, not proven; **runtime (lead)** = from the lead's
2026-10-30 dumps quoted in the task.

Scripts: `tools/ghidra/ZenVtable.java` (lists vtable slots against base Widget
0x00871D84 and decompiles overrides), `tools/ghidra/ZenData.java` (prints
constants). Wrappers and raw output: `private/ghidra-zen/{vt.sh,dis.sh,data.sh}`,
`vt.c`, `a.c`..`g.c`, `d1.txt`..`d4.txt`.

S below is `[gApp+0x9E8]` (600, 768 or 1200); every layout value is
`S * n / 1200` with integer division.

## Summary: how a section expands

1. **Hover, not click (static).** `ZenCategoryContainer::Update` FUN_0042a960
   reads the WidgetManager's last mouse position (`WM+0x10C` x, `WM+0x110` y)
   and calls the dialog's select function FUN_00683410(container id) when
   - `mX <= mouseX < mX + width([0x008D7298])` (an Image global, its width at
     Image+0x2C; FUN_0045b1b0 returns `[EAX+0x2C]`),
   - `mY + [container+0x164] <= mouseY < mY + mHeight`, and
   - the container's own `mIsOver` (+0x69) is set, i.e. the mouse is on the
     container itself, not on one of its child controls.
   The only caller of FUN_00683410 is this Update (callers listing,
   `d1.txt`). The dialog overrides no mouse virtuals (ZenVtable: only slots
   0, 22 Update, 26 Draw, 27 DrawAll differ from Widget), and neither does
   the container. A click works only because it moves the mouse there first.
2. **Select function FUN_00683410(id)** (dialog in EDI, static):
   - clears `container+0x164` on all four sections;
   - if `id == dialog+0xE8` nothing else happens;
   - if a section is selected and its DropListWidget (`+0x144`) has its list
     open (`[drop+0x130]+0x64` visible), it **returns without switching**;
   - otherwise it starts the old section's collapse curve
     (FUN_00437730 on `old+0xC8`, from a compiled curve-definition string,
     scaled by the current value), plays a sound (FUN_0076aaf0), closes the old
     drop-down (DropListWidget vtable +0x154 with 0);
   - stores `dialog+0xE8 = id` and starts the expand curve on `new+0xC8`
     (FUN_004376f0, another compiled curve-definition string) with another sound.
   Nothing ever selects -1 again: moving the mouse away keeps the selection.
3. **Child state each Update (static, FUN_0042a960).** The first child in the
   container's widget list is the enable checkbox (caption 515; `+0x13C`, added first in
   the dialog ctor). For every *later* child the container calls
   `SetDisabled(!enable.checked)` (vtable +0x94) and
   `SetVisible(expansion > 0.0)` (vtable +0x7C), where expansion is
   `GetOutVal` (FUN_00437c60) of the CurvedVal at `container+0xC8` and the
   constant at 0x00880D58 is 0.0 (ZenData). Section 3's HyperlinkWidget
   (`+0x168`) also gets a colour (FUN_00436290 with
   0x80, 0x50 in registers and 0x40 pushed; component order not decoded) whose
   alpha is `ftol(expansion * 255.0)` (0x00880DB8 = 255.0).
4. **Layout each Update (static, dialog Update FUN_006876f0).** Two passes
   over sections 0..3 (`dialog+0xD8+4*i`): height
   `ftol(expansion * [container+0x138] + 170*S/1200)`; first pass sums them,
   start y = `600*S/1200 - total/2`; second pass calls
   `Resize(500*S/1200, y, 1000*S/1200, height)` and advances y. The back
   button (`dialog+0xEC`) gets `mY = y + 40*S/1200`. `container+0x138` is set to
   `150*S/1200` in the container ctor. At S=768: x 320, width 640, collapsed
   height 108, expanded 204; collapsed start y 168, matching the lead's dump
   (y 168/276/384/492). Section 0 is at the top.
   The same Update closes the dialog (parent RemoveWidget, gApp vtable +0x84)
   when `[board+0x30F8] == 1.0f` and `[board+0x33B0] == 0` (meaning not
   decoded).

So there is no scroller and no accordion "click": the vertical Slider at
`dialog+0xD4` (id 4) is the **music volume** (SliderVal FUN_0068b6c0 id 4 →
gApp vtable +0xDC with `1 - value`; ListClicked re-reads it from gApp vtable
+0xCC as `1 - volume`). The "re-flowing" the lead saw is the centred layout
growing by 150*S/1200 around the selected section.

## ZenOptionsDialog (vtable 0x0086F55C, ctor FUN_0068bb50)

| Field | Offset | Evidence (static) |
|---|---|---|
| Bej3ButtonListener / SliderListener / CheckboxListener / ListListener vptrs | +0xBC / +0xC0 / +0xC4 / +0xC8 | ctor |
| ZenBoard* (ctor arg 2) | +0xCC | ctor stores its second argument in this field (dword index 0x33) |
| music volume Slider (vertical, id 4) | +0xD4 | ctor sets 0 then the slider; SetValue `1 - volume` |
| sections[4] (ZenCategoryContainer*, index = id) | +0xD8..+0xE4 | ctor loop id 3..0 stores downward from +0xE4; select function indexes `[+0xD8 + 4*id]` |
| selected section, -1 none | +0xE8 | ctor writes -1; FUN_00683410 compares/stores |
| back button | +0xEC | ctor stores it in this field (dword index 0x3B), Update moves it |
| unknown byte, ctor 1 | +0xF0 | ctor |

Listeners (checkbox FUN_0068b870, slider FUN_0068b6c0, list FUN_0068b420):
as in `controls.md`. New details: ListClicked for drop-down id `n` (1..3)
sets the **next** section's `+0x164 = 10*S/1200 - next.mY + mouseY`, so the
mouse left over the next section after picking from an open list (the list
overlaps it) does not immediately select that section; the next select call
clears it again. Enabling section 0 (checkbox id 0, on) also checks that
section's second checkbox (id 6, caption 516; `[+0x140]` SetChecked(1,1)).

## ZenCategoryContainer (vtable 0x0086F274, ctor FUN_004298c0, 0x170 bytes)

| Field | Offset | Evidence (static) |
|---|---|---|
| owning dialog | +0xC0 | dialog ctor stores its own pointer here (dword index 0x30); Update passes it in EDI |
| id 0..3 | +0xC4 | ctor arg; Draw/DrawAll switch on it |
| expansion CurvedVal | +0xC8 | ctor FUN_00436940 on +0xC8; Update/DrawAll/dialog Update call GetOutVal on it |
| CurvedVal cached output (double) | +0xC8+0x38 = +0x100 | FUN_00437c60 stores the value it returns at this+0x38 |
| extra expanded height `150*S/1200` | +0x138 | ctor |
| enable Checkbox (first child) | +0x13C | dialog ctor stores it here (dword index 0x4F), added first |
| second Checkbox | +0x140 | dialog ctor (section 0: id 6, caption 516). Section 2's checkbox id 5 (caption 521) store not traced |
| DropListWidget or 0 | +0x144 | ctor 0; dialog ctor (section 1 id 1, section 2 id 2, section 3 id 3) |
| Slider or 0 | +0x148 | ctor 0; dialog ctor (ids 0, 1, 2, 3) |
| object constructed by FUN_0041aef0 | +0x14C | ctor (not decoded) |
| hover top inset | +0x164 | ctor 0; ListClicked sets; select clears; Update uses |
| HyperlinkWidget or 0 | +0x168 | ctor 0 (dword index 0x5A); dialog ctor for section 3 |

Section 0's second slider (id 7, caption 518) is added as a child but kept in no field;
`ReadZenOptions` walks the child list instead of relying on fields.

### Text the container draws (static)

All lookups go through FUN_00475e30 = string-table GetString(id, default)
(the game falls back to the compiled English literal when an id is missing).

- **Titles**, DrawAll FUN_0042ab20: ids 0x20F..0x212 = **527 + id**
  (527 for section 0 through 530 for section 3; this confirms the earlier
  hypothesis in `controls.md`). Drawn with
  WriteString (FUN_00434f50) centred at container-relative
  `x = bgWidth/2, y = 63*S/1200` (baseline), colour from a CurvedVal.
- **Captions**, Draw FUN_00429bd0, origin `(60*S/1200, 112*S/1200)` inside the
  container, colour alpha 0x50 when the enable box is unchecked:

  | Section | Caption id (dx, dy in 1/1200 S) | Control |
  |---|---|---|
  | all | 515 (+50, 0) | enable checkbox (ids 0..3) |
  | 0 | 516 (+50, +50) | Checkbox id 6 |
  | 0 | 517 (0, +100) | Slider id 0 |
  | 0 | 518 (0, +150) | Slider id 7 |
  | 1 | 519 (0, +60) | DropList id 1 |
  | 1 | 520 (0, +120) | Slider id 1 |
  | 2 | 521 (+50, +50) | Checkbox id 5 |
  | 2 | 522 (0, +100) | DropList id 2 |
  | 2 | **523 if profile+0x2A0 (subliminal) else 524** (0, +150) | Slider id 2 (Draw also resizes it) |
  | 3 | 525 (0, +60) | DropList id 3 |
  | 3 | 526 (0, +120) | Slider id 3 |
  | 3 | literals, no id | a volume-level note, safety-information link text |

  Correction to `controls.md`: 523 is the subliminal-on caption and 524 the
  off caption (Draw tests the profile byte; earlier table had them the other
  way round by order guess).
- **Status line** (DrawAll, only when enable is checked): 535 (0x217) for
  section 0 with its second checkbox (516) on; section 1: track name, or 577
  (0x241) when the profile track is the random entry; section 2: mantra name
  + 536 (0x218) when subliminal; section 3: pattern name.
- **Descriptions** (DrawAll): four English literals built with the wstring
  constructor and **no** GetString call, so they are not in the string table.
  Drawn word-wrapped in a container-relative box at `(630, 20)` size
  `400 x 1000` (1/1200 S) on a background at `(630, 15, 420, 280)`, only while
  the expansion value exceeds a constant (0x00880D58 = 0.0). Hypothesis on
  which wrapped rect holds which string: the first `(270, 85, 280, 45)` rect is
  the status line and the second is the description (from argument order). The
  mod must read descriptions from captured drawn text, not from the table.
- DrawAll clips the container's own Draw to `mHeight - 40*S/1200` while the
  expansion is below 1.0.

### CurvedVal fields seen (static, partial)

FUN_00436970 (ctor), FUN_004376f0 (set curve), FUN_00437c90 (GetInVal),
FUN_004378f0 (evaluate), FUN_00437c60 (GetOutVal). +0x00 mode, +0x04 curve
type (6 = spline data at +0x34), +0x08 increment (double), +0x10/+0x18 out
min/max, +0x28 start update count, +0x2C pointer to gApp+0x598 (update
counter), +0x30 linked CurvedVal, +0x38 last output, +0x48/+0x50 in min/max,
+0x5D flag "in value from update counter", +0x68 stored in value.
**Unresolved:** which code advances the in value for these curves (no
IncInVal call was found in the container or dialog Update). Therefore the
module does not decide "animation finished" from CurvedVal internals; it
uses the game's own layout result (below).

## Module: `src/mod/zen_options.{h,cpp}`

- `IsZenOptionsDialog(w)`: RTTI class name.
- `ReadZenOptions(dialog, &state)`: selected index, S, background image
  width, music slider, back button, and per section: container, dialog-relative
  rect, title id 527+id and its table text (raw), enable checkbox + checked,
  every later child (kind, control id, own mVisible/mDisabled, caption string
  id from the table above), `controlsShown`, cached expansion (+0x100),
  +0x138, +0x164. `selectedListOpen` = selected section's drop-down open.
  `settled` = selected section within 1 px of `170*S/1200 + extra` and every
  other section at base height with its controls hidden.
- `PlanExpandSection(state, id, snapshot, &steps)`: returns `Ready` if already
  selected, `ListOpen` if the game would refuse, else one `MouseStep::Move`
  with `waitHover = container` at a point of the hover band that
  `HitTest(snapshot)` gives to the container itself (title row first, then
  rows in the top `170*S/1200`). Any point in the first 170*S/1200 rows of a
  section that is not mid-animation stays inside it after the relayout
  (worked out from the layout rule: the selected section's top moves up by at
  most the extra height and its bottom never moves up), so the mouse cannot
  drift onto a neighbour and select it.

## Integration steps (for the lead; not done by this pass)

1. Add `src\mod\zen_options.cpp` to `tools/build.bat`.
2. In the navigator, when the modal base widget `IsZenOptionsDialog`, build
   the item list from `ReadZenOptions` instead of the generic section
   heuristic: per section a header item (title from `GameString(527+id)`,
   cleaned; enable state), then, when that section is the selected one and
   `controlsShown`, its controls with captions from `GameString(captionStringId)`
   (fall back to NearbyLabel when -1, e.g. section 3's safety link), then
   the music slider and the back button. Collapsed sections' controls are hidden by the
   game, so list them only after expanding.
3. Moving focus to a section (or into a control of a non-selected section):
   run `PlanExpandSection` on a fresh snapshot, execute the steps with the
   existing hover path, then poll `ReadZenOptions` each frame until
   `selected == id && settled` (time out after about 2 s and announce the
   failure) before reading positions of its controls or clicking them.
   Description text: after expansion, read the drawn text in the
   description box (container-relative x >= 630*S/1200).
4. `ListOpen`: tell the user the list must be closed first (Escape / pick an
   item) instead of moving the mouse.
5. Any mod mouse move that ends on a container's bare area (not on a child
   or outside the panel) selects that section; hovering a control or the back button
   does not. Keep sliders/checkbox clicks on the control rectangles.
6. Name the vertical slider from the game's own music-volume handling (it has
   no drawn caption; options screen music caption id is in `controls.md`)
   and speak `1 - value` as the volume, or leave it unnamed per the
   no-hard-coded-label rule; this is a design choice for the lead/user.
7. Update `docs/status.md` (Zen gap), `docs/index.md` (link this file) and
   `controls.md` (523/524 correction, title ids confirmed) when integrated.
   The caption-id table in `CaptionStringId` is a lookup backed by the
   game's Draw code; per `decisions.md` it must be listed in status.md.

## Runtime checklist

1. Open Zen options, F12 dump: `[dlg+0xE8]` should be -1 before any hover;
   `[dlg+0xD8+4*i]` the containers with `[c+0xC4] == i`, i = 0 at y 168.
2. `[c+0x138]` = 96 at S 768; `[[0x008D7298]+0x2C]` plausibly 640.
3. Hover a section's title row only (no click): `[dlg+0xE8]` becomes its id,
   its sub-controls get mVisible 1, its height grows to 204, and the
   double at `c+0x100` goes from 0 towards 1. Note how many frames until
   `settled` (and whether the final height is exactly 204).
4. Move the mouse to empty screen space: selection must stay.
5. Hover the enable checkbox of another section: selection must **not**
   change (child gets mIsOver, not the container).
6. Uncheck enable: sub-controls stay visible but mDisabled 1.
7. Open section 1's drop-down, then hover another section: no switch
   (`ListOpen`). Pick an item over section 2: section 2's `+0x164`
   becomes non-zero and section 2 is not selected until the mouse moves above
   that inset or another section is selected.
8. Toggle section 2's checkbox id 5 (caption 521): section 2's slider caption drawn changes between
   string 524 and 523.
9. Move the vertical slider: music volume changes (expect 1 - value).
