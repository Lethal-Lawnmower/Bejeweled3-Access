#pragma once

// Read-only view of the game's SexyAppFramework widget tree.

#include <cstdint>
#include <string>
#include <vector>

struct WidgetInfo {
    uintptr_t ptr = 0;
    std::string className;
    int depth = 0;
    int parentIndex = -1;       // index of the parent in the snapshot, -1 for top level
    bool mouseVisible = false;  // Widget::mMouseVisible: can receive mouse hits
    int absX = 0, absY = 0, width = 0, height = 0;  // game (logical) coordinates
    bool visible = false;       // this widget and all ancestors visible
    bool disabled = false;      // this widget or an ancestor disabled
    bool isOver = false;
    bool isButton = false;      // ButtonWidget or subclass
    bool isDialog = false;      // Dialog or subclass
    bool isEdit = false;        // EditWidget or subclass
    std::wstring editText;      // EditWidget::mString when isEdit
    int buttonId = -1;
    std::wstring label;         // raw ButtonWidget::mLabel (uncleaned)
    bool labelRead = false;
};

// Finds the WidgetManager inside gApp. Returns 0 until the game creates it.
uintptr_t FindWidgetManager();

// Logical screen size (WidgetManager width/height). False if unreadable.
bool GetLogicalSize(uintptr_t wm, int* width, int* height);

// Maps logical widget coordinates to window client pixels using the
// WidgetManager's own mouse remap rectangles.
bool LogicalToClient(uintptr_t wm, int lx, int ly, int* cx, int* cy);

// The widget that owns keyboard focus in the game (WidgetManager mFocusWidget).
uintptr_t GameFocusWidget(uintptr_t wm);

// Depth-first walk of the widget tree in list (draw) order: later entries
// are drawn on top. Stops at 2000 widgets to bound a corrupt list.
std::vector<WidgetInfo> SnapshotWidgets(uintptr_t wm);

// Index of the widget that would receive a click at logical (x, y): the last
// visible, mouse-visible widget in draw order whose rectangle contains the
// point (children are drawn after their parent). -1 if none.
int HitTest(const std::vector<WidgetInfo>& all, int x, int y);

// True if `index` is `ancestor` or lies inside its subtree.
bool IsWithin(const std::vector<WidgetInfo>& all, int index, int ancestor);

// Writes the full tree with fields to the log (F12 debug dump).
void DumpWidgetTree(uintptr_t wm);
