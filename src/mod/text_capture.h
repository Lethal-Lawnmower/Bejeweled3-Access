#pragma once

// Captures the strings the game draws with its fonts, so text that exists
// only inside Draw() functions (dialog prompts, descriptions, tooltips) can be
// spoken. Evidence and risks: docs/re/text-drawing.md.
//
// How it works:
// - The ImageFont and SysFont DrawString vtable slots (and ImageFont's
//   DrawStringEx) are replaced, so every font draw is recorded before the
//   original runs.
// - WidgetManager::DrawScreen is detoured at its entry and its return, which
//   gives an exact frame boundary: only strings drawn inside DrawScreen are
//   kept, and the buffer is published when DrawScreen returns having drawn.
//
// Threads: recording happens only on the thread that runs DrawScreen. The
// query functions take a lock, so they may be called from the window thread
// (NavigatorTick) even if that turns out to be a different thread.

#include <cstdint>
#include <string>
#include <vector>

struct DrawnText {
    std::wstring text;   // raw string as drawn (may contain ^RRGGBB^ codes; use CleanGameText)
    int x = 0, y = 0;    // logical (WidgetManager) coordinates of the string origin:
                         // left edge and, by framework convention, the baseline
    uintptr_t font = 0;  // Font* used to draw it
    uint32_t color = 0;  // 0xAARRGGBB
    float scaleX = 1.0f, scaleY = 1.0f;  // Graphics scale at draw time (1 = none)
    bool onScreen = true;  // false: drawn into an offscreen image; x/y are then
                           // relative to that image, not the screen
};

// Installs the hooks. Idempotent. Call on the game's window thread, outside
// any game draw (e.g. from the subclassed window procedure); see the doc for
// why. Each hook is checked against the expected original bytes / pointers
// first, and the result is logged ("text: ..."). Safe to call repeatedly; it
// does nothing after the first attempt.
void TextCaptureInstall();

// True when both the font hooks and the frame boundary hook are active.
bool TextCaptureActive();

// All strings drawn on screen during the last completed frame, in draw order.
std::vector<DrawnText> TextCaptureLastFrame();

// Number of frames published so far (0 = none yet). Lets callers notice when
// the snapshot changed.
uint32_t TextCaptureFrameNumber();

// On-screen strings of the last completed frame whose origin lies inside the
// logical rectangle, sorted top to bottom, then left to right. The origin's
// y is the baseline, so pass a rectangle covering the widget's full height.
std::vector<DrawnText> TextCaptureInRect(int x, int y, int width, int height);

// Writes the last completed frame to the log (for the F12 dump).
void TextCaptureDumpLastFrame();
