# Text drawing: how the game draws strings, and how the mod captures them

Date: 2026-10-30. Target: Steam `Bejeweled3.exe` (SHA-256 `d5516244...88c3`,
image base 0x00400000, no ASLR). Evidence level for everything here is
**static** (Ghidra 12.0.4 decompilation/disassembly of
`private/ghidra/input/Bejeweled3.exe`, plus raw bytes read from the file)
unless marked **hypothesis**. Nothing here has been checked at runtime yet;
see "Runtime checks for the lead" at the end.

Implementation: `src/mod/text_capture.h`, `src/mod/text_capture.cpp`.
Tools: `tools/ghidra/TextDisasm.java` (disassembly plus all references to a
function's entry), run on the private copy `private/ghidra-text` via
`sh private/ghidra-text/disasm.sh OUT ADDR...`; decompilation with
`tools/ghidra/decompile.sh`. Raw Ghidra output is in `private/ghidra-text/*.c,
*.txt` (private, not for commit). Framework layout reference:
PeggleCommunity/PopCapFramework v1.34 `Font.h`, `ImageFont.h`, `Graphics.h`,
`WidgetManager.h` (used for names and virtual order only; this build differs).

## Call chain

```
Widget::Draw (game code, e.g. WelcomeDialog::Draw FUN_0067f890)
  Graphics::WriteWordWrapped  FUN_00435370   (word wrap)
    FUN_00435320              (one line)
      Graphics::WriteString   FUN_00434f50   (splits on ^RRGGBB^ colour codes, justification)
        Graphics::DrawString  FUN_00433040   (called directly from many draw functions; Ghidra lists 70+ call sites)
          mFont->vtable[11]   Font::DrawString           <- hooked
            ImageFont: FUN_0058ddf0 -> vtable[14] DrawStringEx FUN_0058cfd0  <- hooked
            SysFont:   FUN_005a1430 -> inner font (+0x14) vtable[11]         <- hooked
              ... Graphics::DrawImage FUN_00433080 -> render device ([g+0x78] vtable +0x34)
```

Evidence for each link:

- `WelcomeDialog::Draw` = FUN_0067f890: the only function referencing the
  UTF-16 fallback literal of string 511 at 0x007D2F00 (string table id 511
  names `WelcomeDialog::Draw`). It calls FUN_00432580 (SetFont),
  FUN_00432590 (SetColor), then FUN_00435370 with a rect.
- `MainMenu::Draw` = FUN_006a0ca0 (references the fallback literal of
  string 288 at 0x007D4878); it draws that text via FUN_00434f50 with
  justification args.
- FUN_00434f50 (WriteString): walks the string, handles `^` colour codes
  (`^oldclr^` and hex), calls FUN_00433040 for each segment and font
  vtable +0x20 (StringWidth) to advance x. So one visible sentence may arrive
  as several captured fragments (per colour segment and per wrapped line).
- FUN_00433040 (Graphics::DrawString(str, x, y)), disassembly: Graphics* in
  **EAX** (LTCG register convention), stack `str, x, y`, `RET 0xC`. If
  `[g+0x68]` (mFont) is non-null it pushes `&g->mClipRect (g+0x20)`, a colour
  (`g+0x48` when the vector at `g+0x3C..0x40` is non-empty, else `g+0x58`),
  `str`, `y`, `x`, `g` and calls `[[g+0x68]]+0x2C` with ECX = font.
- Font vtable slot 11 (+0x2C) is DrawString. Font base `Font` 0x0087DF8C
  slot 11 = FUN_00556a80 (`return;`, an empty default). ImageFont 0x00875854
  slot 11 = FUN_0058ddf0: `MOV EAX,[ECX]; CALL [EAX+0x38]` with the six
  arguments plus `0, 0`, then `RET 0x18`. SysFont 0x00876BA4 slot 11 =
  FUN_005a1430: may rebuild itself (FUN_005a13b0), then
  `MOV ECX,[ESI+0x14]; JMP [[ECX]+0x2C]` (tail call, same arguments).
- ImageFont slot 14 (+0x38) = FUN_0058cfd0, DrawStringEx(g, x, y, str,
  color, clipRect*, drawnAreas, width*), `__thiscall`, 8 stack args. Ghidra
  finds **only DATA references** to FUN_0058cfd0 (0x0087588C, the vtable) and
  to FUN_0058ddf0 (0x00875880): no direct calls exist, so every ImageFont
  draw goes through the patched slots.
- Neighbouring slots that confirm the numbering: slot 8 (+0x20) StringWidth
  (ImageFont FUN_0058c920 loops the wstring calling +0x28), slot 9 (+0x24)
  CharWidth (ImageFont FUN_0058cc80 calls +0x28(ch, 0); Font FUN_005f4470
  builds a one-char string and calls +0x20), slot 10 (+0x28) CharWidthKern
  (FUN_0058c990). Slots 1-7 were not identified individually. Slot 12 is
  Duplicate (Font base 0x0064F003, presumably `_purecall`; **hypothesis**).
  The framework v1.34 order puts DrawString at index 10, so this build has
  one extra virtual before it.

Fonts in the RTTI list: `Font`, `ImageFont`, `SysFont`, `FontData`,
`ActiveFontLayer`, `FontRes`. No other vtable has a class name containing "Font"; derived classes with other names were not searched (**hypothesis**: none).

### Calling convention of the hooked slots

| Slot | Function | Convention | Stack args | Callee pops |
|---|---|---|---|---|
| 11 ImageFont | FUN_0058ddf0 | thiscall (ECX = font) | g, x, y, const wstring*, const Color*, const Rect* | 0x18 |
| 11 SysFont | FUN_005a1430 | thiscall | same | 0x18 (via the inner font) |
| 14 ImageFont | FUN_0058cfd0 | thiscall | g, x, y, wstring*, Color*, Rect* (may be 0), RectList*, int* width | 0x20 |

The hooks are `__fastcall` with a dummy EDX parameter (the standard way to
receive `__thiscall` in a free function); the compiled hooks end in
`ret 18h` / `ret 20h` (checked in a scratch compile with dumpbin).

x/y are integers relative to the Graphics translation. By framework
convention y is the text **baseline** (ImageFont::DrawStringEx subtracts
the layer ascent: `y + (layer+0x110 - layer+0x124) + char offset` in the
decompilation); treat that as **hypothesis** until checked at runtime.

## Graphics fields used

From the Graphics(Image*) constructor FUN_00431ff0, the copy constructor
FUN_00432130, Translate FUN_00434330 and DrawImage FUN_00433080:

| Field | Offset | Evidence |
|---|---|---|
| vtable | +0x00 | 0x0086FF28 stored by both ctors |
| mDestImage | +0x04 | ctor stores the image argument |
| mTransX, mTransY (float) | +0x08, +0x0C | ctor zeroes them; Translate: `FILD [ESP+4]; FADD [EAX+8]; FSTP [EAX+8]` (same for +0xC); DrawImage adds `_ftol(mTransX)` to x |
| mScaleX, mScaleY (float) | +0x10, +0x14 | ctor stores 1.0f (0x3F800000); DrawImage takes the unscaled path only when both are 1.0 |
| mClipRect {x,y,w,h} | +0x20 | DrawImage intersects with `g+0x20`; WriteWordWrapped compares lines with `g+0x24`/`g+0x2C` |
| colour (Color {r,g,b,a} ints) | +0x58 | SetColor FUN_00432590 copies 4 ints there; Color::ToInt FUN_004362d0 = `a<<24 | r<<16 | g<<8 | b` |
| mFont | +0x68 | SetFont FUN_00432580; GetFont FUN_00432570 |
| mColorizeImages (byte) | +0x70 | getter FUN_00432640 / setter FUN_00432630 |
| render device | +0x78 | DrawImage calls `[[g+0x78]]+0x34` |

The Graphics methods use LTCG register conventions (`this` in EAX, EBX or
ESI depending on the function), so they are not hooked directly; the font
vtable slots are ordinary `__thiscall`.

## Logical coordinates

WidgetManager::DrawScreen = FUN_005856b0 (only caller: FUN_005b5f50, the
app's DrawDirtyStuff, at 0x005B60D0). Decompilation:

1. `mCurG` (`wm+0x68`) = the root Graphics on the screen image.
2. A copy `g` is translated by `(-[wm+0xE8], -[wm+0xEC])` (FUN_00434330),
   the logical mouse remap rectangle origin (runtime 2026-10-30: -102, 0).
3. For each top-level widget with mDirty (`+0x3C`) and mVisible (`+0x64`):
   a per-widget Graphics copy is translated by the widget's mX/mY (+0x40,
   +0x44) and its DrawAll (vtable +0x6C) is called; mDirty is cleared.
4. Overlays: FUN_00584d60(wm, 0x7FFFFFFF) applies the same
   `-remap` translation plus the overlay widget's position.

Hence for text drawn on screen inside DrawScreen:

    logicalX = (int)g.mTransX + x + [wm+0xE8]
    logicalY = (int)g.mTransY + y + [wm+0xEC]

which is the same space as `WidgetInfo::absX/absY` from `widgets.cpp`
(absolute positions summed from the WidgetManager at 0,0). The truncation
matches DrawImage's `_ftol`. Not covered: matrix/rotation transforms and
scaled drawing (scale is recorded so such entries can be recognised).

## Frame boundary

DrawScreen is detoured (it is not virtual, so no vtable slot exists):

- Bytes at 0x005856B0: `55 8B EC 83 E4 F8` (`push ebp; mov ebp,esp;
  and esp,-8`), read from the file and re-checked live before patching.
  The detour writes `E9 rel32 90`, saves all registers and flags, calls
  `OnDrawScreenEnter(EDI = WidgetManager, return address)`, replays the
  six bytes and jumps to 0x005856B6.
- The caller's return address is replaced by `ReturnThunk`, which calls
  `OnDrawScreenExit(EAX)` and returns to the saved address with all
  registers and flags preserved. DrawScreen ends in plain `RET` and returns
  AL = "a widget was drawn" (`MOV AL,[ESP+0xD]` at 0x00585972).
- Strings are recorded only while DrawScreen is active, on its thread. On
  exit with AL != 0 the buffer becomes the published "last frame"; with
  AL == 0 the screen was not redrawn, so the previous frame stays current.
- Why not the font hook alone with the 50 ms timer as the boundary: a tick
  spans about three frames, which would duplicate and mix animated text.

Text drawn outside DrawScreen (DrawDirtyStuff's debug performance-recording text, glyph
generation in ImageFont FUN_0058bc50) is ignored. Text drawn inside
DrawScreen into an offscreen image (dest image differs from mCurG's) is
kept with `onScreen = false` and image-relative coordinates.

## 3D / accelerated drawing

The hook sits above the renderer: ImageFont::DrawStringEx ends in
Graphics::DrawImage, which dispatches to the render device at `g+0x78`.
So hardware and software paths both pass through the hooked slots. Missed:
text baked into images, and any text drawn by a renderer path that does not
use a Font object (none found, not searched exhaustively).

## Thread safety and install timing

- ImageFont::DrawStringEx takes a critical section (0x008E2BB8) around
  static buffers, which suggests fonts may be used on other threads
  (probably resource loading). The hooks only record on the DrawScreen
  thread; other threads pass straight through.
- The vtable writes are single aligned 4-byte stores (safe at any time).
- The DrawScreen patch rewrites 6 bytes of code; it is only safe while no
  thread is executing those bytes. Installing from the window procedure is
  safe **if** DrawScreen runs on the window thread (expected for
  SexyAppFramework, whose main loop pumps messages and draws on one thread;
  **hypothesis** until the log confirms it).
- Published frames are guarded by an SRW lock, so readers may be on any
  thread. Install and the hooks' recording state are not locked and assume
  the single draw thread.

## Risks

- Partial redraws: DrawScreen only redraws widgets with mDirty set. If the
  game does not mark every widget dirty each frame, a frame contains only
  the text of the redrawn widgets. Check at runtime (below). If it happens,
  the fix is to keep per-top-level-widget text (attribute strings to the
  widget being drawn) instead of replacing the whole frame.
- An exception unwinding through DrawScreen skips ReturnThunk; the return
  stack then holds a stale entry (bounded at 8; after that the detour stops
  redirecting returns and frames stop publishing, which the frame counter
  shows). The game would normally crash in that case anyway.
- Steam overlay, another mod or a different exe build: each hook verifies
  the expected bytes/pointers first and logs and skips on mismatch.
- Cost: per string one guarded 24-byte copy of the Graphics header, one
  guarded string read (allocation), one colour read; capped at 1024
  strings per frame.

## Runtime checks for the lead (not yet done)

Log lines (`%LOCALAPPDATA%\Bejeweled3Access\mod.log`):

1. `text: hooks installed: DrawScreen=1 ImageFont.DrawStringEx=1
   ImageFont.DrawString=1 SysFont.DrawString=1 (install thread N)`.
   Any `not hooked` / `prologue differs` / `BLOCKER` line means a mismatch.
2. `text: DrawScreen runs on thread M`: M must equal N. If not, the patch
   was installed from another thread than the drawing one (race risk); move
   the install or suspend the draw thread while patching.
3. `text: first frame published (frame F, S strings, remap -102,0)`: the
   remap must match the WidgetManager remap rectangle in the F12 dump.
4. Welcome dialog F12 dump: expect a `text screen` entry containing
   string 511 with a position inside the WelcomeDialog's rectangle from
   the widget dump, and button labels (e.g. 510) near their buttons'
   rectangles. A constant offset of about 102 px
   would mean the remap correction is wrong (sign or double-counting).
5. Press F12 twice on a static screen (main menu, no animation): the frame
   number should increase and the string count should stay the same. A
   count that varies between dumps on a static screen indicates partial
   redraws.
6. Check whether y is the baseline: compare a label's y with its button's
   top and height.
7. Note entries with `offscreen` or scale != 1 (animated or cached text).

## Open questions

- Which text in the menus is drawn as images (not capturable here):
  compare the dump with what is on screen.
- Tooltips (TooltipManager) and the in-game HUD use the same path
  presumably; not traced individually.
- Slots 1-7 of Font are not identified (not needed for capture).
