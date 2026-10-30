# Architecture

## Loading

`winmm.dll` (this mod) sits next to `Bejeweled3.exe`. `DllMain` only loads
the real `%SystemRoot%\SysWOW64\winmm.dll`, fills a jump table for all 192
exports (`src/proxy`), opens the log and starts a thread. Nothing else runs
under the loader lock.

The startup thread starts speech, then waits for the game's top-level window
(class `MainWindow`) and subclasses it. From then on all mod logic runs on
the game's own window thread:

- a 50 ms `WM_TIMER` calls `NavigatorTick`, which reads the widget tree
  between frames;
- `WM_KEYDOWN` / `WM_CHAR` go through `NavigatorKeyDown` / `NavigatorChar`.

If the game destroys its window (display mode switch), the mod waits for the
new one and subclasses it again.

## Speech (`speech.cpp`)

Prism only. `prism.dll` is delay-loaded on a dedicated speech thread; a
missing DLL or no backend is logged as a BLOCKER and the game continues
silently (no substitute speech). `Speak(text, interrupt)`: interrupt clears
the queue and cuts current speech, used for every focus change so stale
items are never read. Every utterance is also written to the log.

## Reading the game (`game_memory.cpp`, `widgets.cpp`, `sexy.h`)

- All reads are SEH-guarded; a failed read is "unknown", never a default.
- Classes are identified by MSVC RTTI at runtime (`ClassNameOf`,
  `IsKindOf(p, "ButtonWidget")`), so subclasses such as `Bej3Button`,
  `CrystalBall` and `Bej3DialogButton` are handled without per-class tables.
- The WidgetManager is found inside `gApp` by its vtable.
- `SnapshotWidgets` walks the tree depth-first in draw order, carrying
  absolute position, effective visibility and effective disabled state.

## Navigation (`navigator.cpp`)

- Layer: the topmost visible `Dialog` subtree if any, else the whole screen.
- Items: visible, enabled `ButtonWidget`s and `EditWidget`s with non-zero
  size, sorted into reading order (rows top to bottom, then left to right).
  The window-mode toggle button is skipped.
- Context change (new dialog, or new top-level screen owning the first
  item): speak the dialog's header/lines/footer, or the screen's class name
  as a stand-in, then the focused item with "n of m". Initial focus is the
  game's own keyboard focus when it is in the layer.
- Mouse hover also moves focus and speaks.
- Activation posts `WM_MOUSEMOVE` to the button centre (mapped with the
  WidgetManager's own remap rectangles), waits until the game marks the
  button as hovered, then posts button down/up. If hover never happens the
  user hears "Could not reach that button" and the log records it.
- Edit fields: the game's focused `EditWidget` text is compared every tick
  and additions/deletions are spoken. Keys consumed by the mod also swallow
  the matching `WM_CHAR` so they do not leak into a text field.

## Session 2 additions

- `text_capture.cpp`: hooks the font draw virtuals and
  `WidgetManager::DrawScreen` to record every string the game draws per
  frame with logical position and colour (docs/re/text-drawing.md).
- Items are found by hit test (the widget that would get a click at the
  centre), must be on screen, and are skipped while their label is drawn only
  off screen (logo intro). Dialogs restrict focus only when modal
  (WidgetManager mBaseModalWidget).
- Naming chain: label, text drawn on the control, image resource name
  (`game_strings.cpp` reads the game's resource table), "unlabeled".
  Checkbox/slider captions: nearest drawn text on the same row (right of
  checkboxes, left of sliders).
- `controls.cpp`: checkbox/slider/tab/score-row state and planned mouse
  sequences; the navigator executes them one event per tick and waits for
  hover before pressing.
- Hover probe: after a keyboard move the mouse is hovered over the item and
  new text rows are spoken (Play menu reads its description field directly).
  Change probe: after a press on the same screen, new text rows are spoken
  (Stats tab).
- `hud.cpp`: board status (F6) and the board's announcement queue.
- On a board (and no dialog) only Tab / Enter-after-Tab and F-keys are
  taken; everything else goes to the game.
- Transitions: announcements wait until the item set is stable for 300 ms;
  later growth is reported as "N items".

## Planned

- Board grid: reticle position, gems, specials, hints.
- Badges, quest menu, Zen options, end-of-game dialogs.
