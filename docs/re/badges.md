# Records > Badges tab (BadgeWidget)

Pass: 2026-10-30, static analysis only (Ghidra copy `private/ghidra-badges`,
exe SHA-256 d5516244...88c3). Nothing here is runtime-confirmed unless marked
**runtime**. Code: `src/mod/badges.{h,cpp}` (compile-checked, not built or
run). Decompilations: `private/ghidra-badges/bw.c`, `b2.c`, `b3.c`,
disassembly `d1.txt` (private, not committed).

`S` below is the art scale `[gApp+0x9E8]`; the game writes every layout
value as `S * n / 1200` (signed integer division). **Runtime-derived:** the
widget's runtime size 672x556 equals `S*1050/1200 x S*870/1200` with
`S = 768`, so S is the logical screen height at 2560x1440.

## Objects

| What | Where | Evidence |
|---|---|---|
| BadgesTab -> BadgeWidget | BadgesTab+0xC0 | static: BadgesTab ctor FUN_006a03e0 `new BadgeWidget(0, 0)` (0x340 bytes), stores it at +0xC0, Move(S*430/1200, S*205/1200) = 275,131 at S=768 (vtable +0xA4), AddWidget |
| BadgeWidget ctor | FUN_0067c370(this, awardSource, p3) | static: +0xBC = awardSource, +0xC4 = p3, size S*1050/1200 x S*870/1200 |
| +0xBC award source | 0 on the Records tab | static (ctor call above). Non-zero is an object with a vector of newly earned badge ids at +0x305C/+0x3060 and its own badge manager at +0x3068 (the end-of-game badges-button path; hypothesis: the Board) |
| +0xC0 badge manager | `new` 0x5C object FUN_007770d0 when +0xBC == 0, else `[awardSource+0x3068]` | static |
| manager+0x04+4*id | `Badge*` for id 0..19 | static: FUN_007770d0 stores each badge; Update reads `[[+0xC0]+4+4*i]` for loop index i (0x0068feba..0x0068fec9: `ECX = manager - this - 0x118`, `[ECX+EDI+4]` with EDI = this+0x118+4i) |
| +0xC8+4*id | int level as drawn (0 = not earned, drawn dimmed alpha 0x80) | static: Draw loop reads `+0xC8+4*i`; FUN_00672f90 (called from SetVisible(true) override FUN_00679850 and the ctor) copies `profile+0x1A0+4*id` when +0xBC == 0 |
| +0x118+4*id | float hover highlight 0..1 (+0.05 per Update while hovered, -0.05 otherwise) | static: Update FUN_0068fb50 |
| +0x328 / +0x32C / +0x330 | award-sequence index / effect object / level | static (award path only, not used on Records) |

Badge object (0x20 bytes, FUN_007770d0): +0x00 vtable, **+0x04 id**,
**+0x08 level** (from `profile+0x1A0+4*id`), +0x0C level copy, +0x10 owner,
+0x14/+0x18 per-class parameters, +0x1C 0. FUN_007762b0 (SetVisible(true))
refreshes +0x08/+0x0C from the profile.

Ids and classes (FUN_007770d0, RTTI): 0 Inferno, 1 Stellar, 2 Chromatic,
3 Blaster, 4 ClassicScore, 5 FinalFrenzy, 6 LightningScore, 7 PokerScore,
8 PokerFlushes, 9 IceStormScore, 10 IceStormColCombo, 11 DiamondMineScore,
12 DiamondMineArtifacts, 13 ButterfliesScore, 14 ButterfliesBigMove,
15 QuestModeFinished, 16 HypercubeAnnihilation, 17 Supernova,
18 ClassicLevel, 19 SecretHighScore. (RTTI class names with the `Badge`
suffix omitted, e.g. `ChromaticBadge`; they are diagnostics only, and the
spoken name comes from the game's name table below.)

Profile `[gApp+0xB80]`: +0x1A0+4*id level per badge (static).

App tables (static):

| Table | Where | Evidence |
|---|---|---|
| Badge names, `vector<wstring>` (0x1C per id) | begin `gApp+0xD2C`, end `+0xD30` | Draw award branch formats a compiled wide format literal (level name, a colour code, then the quoted badge name) with the level name and `names[badge->id]`; FUN_00778430 (tooltip title) bounds-checks the same vector with the id |
| Level thresholds, 0x18-byte entry per id | begin `gApp+0xD44`, end `+0xD48`; entry +0x0C/+0x10 = int vector begin/end | FUN_00776940 (level count = entry size), FUN_00776990 / FUN_00776aa0 (thresholds by level) |
| Level names | string table ids 1..4 | FUN_00778130 looks up ids 1, 2, 3, 4 (fallback literals for the four level names compiled in); `GetBadgeLevelName(badge, next)`: next = 0 -> name[level-1], 1 -> name[level] |

## Grid geometry (FUN_00673ba0, id in EBX)

Relative to the widget, `step = S*205/1200`:

    x = (id % 5) * step - S*410/1200 + width/2
    y = trunc(height/2 - S*307.5/1200 + (id / 5) * step - S*15/1200)
    if id > 14: y += S*30/1200          (the fourth row of "elite" badges)

(constants 307.5 at 0x00880F60, 1200.0 at 0x00880E18; FUN_00792BF0 is a
truncating ftol, CVTTSD2SI). At S = 768, 672x556: columns x = 74, 205, 336,
467, 598; rows y = 72, 203, 334, 484. 20 badges, 5 x 4, no scrolling or
paging (all loops are `i < 0x14`).

These are **centres**: Update builds the hover rectangle as the cel
rectangle of cel 0 of the image `[0x008D8814]` (FUN_0045b200; cel width =
img+0x2C / img+0x38 columns, cel height = img+0x30 / img+0x34 rows, from
FUN_0045b1e0 / FUN_0045b1d0) and moves it so its centre is (x, y).

Check against the runtime observation (2026-10-30): mouse at logical
(450,190) showed the tooltip of badge id 2 (ChromaticBadge). With the widget at absolute 173,131,
that is (277, 59) relative: column 2 (centre 336), row 0 (centre 72), inside
if the cel is wider than 118 px and taller than 26 px. With the widget at
276,131 it would be column 1 (badge id 1), which was not observed, so at hover
time the widget was at 173 (BadgesTab at -102 + 275). The reader takes the
absolute position from the snapshot at the time of use, so either position
works as long as the snapshot is current.

## Hover (no hovered-index field)

Update FUN_0068fb50 (BadgeWidget vtable slot 22), each frame, only when
mVisible (+0x64), mMouseVisible (+0x65), the award sequence is finished or
absent (+0xBC == 0) and +0xC4 == 0 or done:

1. reads the WidgetManager's last mouse position `[mWidgetManager+0x10C]`,
   `+0x110` (logical);
2. for i = 0..19: if mIsOver (+0x69) and `mouse - GetAbsPos()` (vtable
   +0x3C) lies in the badge's cel rectangle: fade += 0.05 (max 1), then
   requests a tooltip from the TooltipManager `[gApp+0xBF0]` via
   FUN_00677d00; else fade -= 0.05 (min 0).

BadgeWidget has **no MouseMove override** (vtable diff against Widget
0x00871D84: only slots 0 dtor, 22 Update, 26 Draw, 31 SetVisible, 54
MouseDown differ). So the hovered badge is derived, not stored:
`HoveredBadgeId` repeats the game's test; the fade floats show the same
thing with a lag.

Tooltip text (built every hovered frame, not cached on the badge):

- title: FUN_00778430 = `names[id]` + Badge vtable +0x10 (slot 4,
  FUN_0077d600: progress suffix from string 0, two integers current/target
  in parentheses, empty at max level) -> runtime: badge 2's name with a
  0-of-25 progress suffix;
- body: `"^FFAACC^"` (colour code) + FUN_00776f90: level < level count -> vtable +0x18
  (slot 6, "next level" string, e.g. ChromaticBadge FUN_0077cae0 string id
  12, formatted with the target count and the next level name), else vtable +0x1C (slot 7,
  max-level string, id 13). Slot 5 (+0x14) is the award string (id 11),
  used by the award dialog.
- position: above the badge, or below for ids >= 15 when the property
  `DISPLAY_TOOLTIPS_ABOVE_ELITE_BADGES` check (FUN_0046e280) passes.

The TooltipManager keeps the requested entries (vector of 0x78-byte
entries at TooltipManager+0xD4/+0xD8; title wstring at entry+0x1C, body at
+0x38, x/y at +0/+4) — **hypothesis** from FUN_00677d00's local layout; not
used by the mod. The text-capture hook already sees the drawn tooltip.

Progress values: slot 0 of each Badge class is the current value getter
(e.g. InfernoBadge FUN_00776870 `profile+0xF0`, ChromaticBadge FUN_007767d0
`profile+0xF8`), slot 9 the target, slot 3 whether progress is relative to
the previous threshold. These are per-class code; the mod does not decode
or call them and reads progress from the drawn tooltip instead.

## Mod reader (`badges.h`)

- `IsBadgeWidget(ptr)` - RTTI class name.
- `ReadBadgeGrid(widget, absX, absY, &cells)` - per id: absolute centre,
  hover rectangle (when the cel image is readable), Badge*, RTTI class,
  drawn level (+0xC8), level count, name (gApp+0xD2C), level name (string
  id = level, 1..4), fade.
- `HoveredBadgeId(widget, absX, absY)` - game's hit test; -1 none, -2
  unknown.
- `DumpBadgeGrid` - log lines prefixed `badges:` for F12.

## Integration (lead)

1. Add `badges.cpp` to `tools/build.bat`.
2. F12 dump: for each snapshot widget with `IsBadgeWidget(w.ptr)`, call
   `DumpBadgeGrid(w.ptr, w.absX, w.absY)`.
3. Navigator: when the focused container is a BadgeWidget, offer 20 items
   in id order (row-major, 5 per row: Up/Down could move by 5). For each
   item move the mouse (existing hover path, `LogicalToClient`) to
   `centerX, centerY`, then speak the text captured inside the tooltip
   (the game draws it at runtime; e.g. for badge 2 the title is its name
   with the progress suffix and the body is string 12 formatted with the
   target count and the first level name). Use
   `HoveredBadgeId == id` to confirm the game took the hover before
   speaking. Fallback if no tooltip text arrives: `name` + `levelName`
   (or "not earned" when level is 0) from `ReadBadgeGrid`, all from game
   data. Position text: "row r of 4, column c of 5" or "n of 20".
4. Record the evidence levels in `docs/reverse-engineering.md` once
   confirmed.

## Runtime checklist

- [ ] F12 on the Badges tab: `badges:` lines appear, 20 ids, class names
      in the order above, `[badge+4] == id` (cells have a class).
- [ ] Centres at S = 768: x = abs + 74/205/336/467/598, y = abs +
      72/203/334/484; log `[gApp+0x9E8]` = 768.
- [ ] Hit rectangle size plausible (> 118 x 26 implied by the badge-2
      hover); log `GameImageResourceName([0x008D8814])`.
- [ ] Names from gApp+0xD2C match the tooltip titles (e.g. badge 2).
- [ ] Levels: all 0 in the Test profile; level count 4 for most badges.
- [ ] Hovering each centre makes the game draw that badge's tooltip, and
      `HoveredBadgeId` returns the same id; fade rises toward 1.
- [ ] Repeat at another resolution (S changes) and while the screen slides
      in (absX changes): centres must follow.
- [ ] With an earned badge (later): level name string ids 1..4 read;
      max-level body text.

## Uncertainties

- All offsets static only. The Image field offsets (+0x2C..+0x38) come from
  two framework getters used here; not confirmed on this image.
- GetAbsPos (vtable +0x3C) is assumed to equal the snapshot's absX/absY
  (sum of parent mX/mY).
- The award source object (+0xBC) type and the end-of-game badge flow
  (BadgeAwardDialog) were not decoded.
- The tooltip entry layout is a hypothesis.
