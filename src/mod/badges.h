#pragma once

// Read-only view of the Records > Badges grid (one BadgeWidget draws all 20
// badges; there are no per-badge child widgets). Evidence and the runtime
// checklist: docs/re/badges.md. Everything is static analysis unless the doc
// says otherwise.
//
// What the game does (static, Update FUN_0068fb50 / Draw FUN_00685b90):
// - BadgeWidget+0xC0 is a badge manager whose +0x04 + 4*id holds Badge* for
//   ids 0..19; the grid index equals the badge id.
// - Badge centre (relative to the widget) is computed from the art scale
//   S = [gApp+0x9E8] and the widget size (FUN_00673ba0): 5 columns, 4 rows,
//   the last row (ids 15..19) pushed down a little.
// - Hover is not stored as an index: every Update tests the WidgetManager's
//   mouse position against each badge's cel rectangle (cel 0 of the image at
//   [0x008D8814], centred on the badge) and, when inside, ramps a per-badge
//   float and asks the TooltipManager to show the badge's text. So the mod
//   hovers a cell centre and captures the tooltip the game draws.
//
// No game code is called; all reads are guarded. Call from the game's window
// thread (NavigatorTick). A failed read leaves the field unknown.

#include <cstdint>
#include <string>
#include <vector>

struct BadgeCell {
    int id = -1;                   // grid index == Badge id (0..19)
    int centerX = 0, centerY = 0;  // logical, absolute (widget absX/absY added)
    // Hover rectangle the game tests (absolute, logical). hitKnown is false
    // when the cel image could not be read; then only the centre is known.
    bool hitKnown = false;
    int hitX = 0, hitY = 0, hitWidth = 0, hitHeight = 0;
    uintptr_t badge = 0;           // Badge* (0 if unreadable)
    std::string badgeClass;        // RTTI class, e.g. "ChromaticBadge" (diagnostics only)
    // Level as drawn by the widget (BadgeWidget+0xC8+4*id): 0 = not earned,
    // 1.. = earned level. levelKnown false if unreadable.
    bool levelKnown = false;
    int level = 0;
    // Number of levels this badge has (size of the game's threshold vector
    // for this id), -1 if unknown. level == levelCount means max level.
    int levelCount = -1;
    // Badge name from the game's own name table (gApp+0xD2C vector, index
    // id), raw (may contain colour codes). Empty if unreadable.
    std::wstring name;
    // Name of the earned level from the string table (ids 1..4, the same ids
    // FUN_00778130 looks up), raw. Empty when level is 0 or unreadable.
    std::wstring levelName;
    float hoverFade = 0.0f;        // BadgeWidget+0x118+4*id: 0..1, rises while hovered
};

// True if the widget is the game's BadgeWidget (RTTI class name).
bool IsBadgeWidget(uintptr_t widget);

// Reads all 20 cells. absX/absY: the widget's absolute logical position (as
// WidgetInfo reports it). Returns false only if the widget is not a
// BadgeWidget or its size / the art scale cannot be read; individual cells
// may still have unknown fields.
bool ReadBadgeGrid(uintptr_t badgeWidget, int absX, int absY, std::vector<BadgeCell>* out);

// The badge the game currently treats as hovered: the same test Update does
// (widget mIsOver, WidgetManager mouse position inside the badge's cel
// rectangle). Returns the id, -1 if none, -2 if unknown (read failure).
int HoveredBadgeId(uintptr_t badgeWidget, int absX, int absY);

// Writes the grid to the log (F12 dump).
void DumpBadgeGrid(uintptr_t badgeWidget, int absX, int absY);
