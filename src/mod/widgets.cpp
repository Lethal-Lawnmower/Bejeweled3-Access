#include "widgets.h"

#include "game_memory.h"
#include "log.h"
#include "sexy.h"
#include "text.h"

namespace {

int g_wmOffset = -1;  // offset of the WidgetManager pointer inside gApp

void Walk(uintptr_t container, int parentIndex, int depth, int originX, int originY,
          bool visible, bool disabled, std::vector<WidgetInfo>* out) {
    uint32_t head, size;
    if (!ReadU32(container + sexy::kWidgetsHead, &head) ||
        !ReadU32(container + sexy::kWidgetsSize, &size) || size > 500)
        return;
    uint32_t node;
    if (!ReadU32(head, &node)) return;  // head->next is the first element
    for (uint32_t i = 0; i < size && node != head && out->size() < 2000; ++i) {
        uint32_t widget, next;
        if (!ReadU32(node + 8, &widget) || !ReadU32(node, &next)) return;
        WidgetInfo w;
        w.ptr = widget;
        w.depth = depth;
        w.parentIndex = parentIndex;
        w.className = ClassNameOf(widget);
        int32_t x = 0, y = 0, wd = 0, ht = 0;
        uint8_t vis = 0, dis = 0, over = 0, mouseVis = 0;
        bool ok = ReadI32(widget + sexy::kX, &x) && ReadI32(widget + sexy::kY, &y) &&
                  ReadI32(widget + sexy::kWidth, &wd) &&
                  ReadI32(widget + sexy::kHeight, &ht) &&
                  ReadU8(widget + sexy::kVisible, &vis) &&
                  ReadU8(widget + sexy::kDisabled, &dis) &&
                  ReadU8(widget + sexy::kIsOver, &over) &&
                  ReadU8(widget + sexy::kMouseVisible, &mouseVis);
        if (ok && !w.className.empty()) {
            w.absX = originX + x;
            w.absY = originY + y;
            w.width = wd;
            w.height = ht;
            w.visible = visible && vis != 0;
            w.disabled = disabled || dis != 0;
            w.isOver = over != 0;
            w.mouseVisible = mouseVis != 0;
            w.isButton = IsKindOf(widget, "ButtonWidget");
            w.isDialog = IsKindOf(widget, "Dialog");
            w.isEdit = IsKindOf(widget, "EditWidget");
            if (w.isEdit) ReadWString(widget + sexy::kEditString, &w.editText);
            if (w.isButton) {
                int32_t id;
                if (ReadI32(widget + sexy::kButtonId, &id)) w.buttonId = id;
                w.labelRead = ReadWString(widget + sexy::kButtonLabel, &w.label);
            }
            out->push_back(w);
            int self = (int)out->size() - 1;
            Walk(widget, self, depth + 1, w.absX, w.absY, w.visible, w.disabled, out);
        }
        node = next;
    }
}

}  // namespace

uintptr_t FindWidgetManager() {
    uint32_t app;
    if (!ReadU32(sexy::kGAppPtr, &app) || app == 0) return 0;
    if (g_wmOffset >= 0) {
        uint32_t wm, vt;
        if (ReadU32(app + g_wmOffset, &wm) && ReadU32(wm, &vt) && vt == sexy::kVtWidgetManager)
            return wm;
        return 0;
    }
    // Locate the field once by its vtable rather than hard-coding the offset.
    for (int off = 4; off < 0x2000; off += 4) {
        uint32_t p, vt;
        if (ReadU32(app + off, &p) && ReadU32(p, &vt) && vt == sexy::kVtWidgetManager) {
            g_wmOffset = off;
            Log("widgets: gApp=%08X class=%s WidgetManager at gApp+0x%X = %08X", app,
                ClassNameOf(app).c_str(), off, p);
            return p;
        }
    }
    return 0;
}

bool GetLogicalSize(uintptr_t wm, int* width, int* height) {
    int32_t w, h;
    if (!ReadI32(wm + sexy::kWidth, &w) || !ReadI32(wm + sexy::kHeight, &h)) return false;
    *width = w;
    *height = h;
    return w > 0 && h > 0;
}

bool LogicalToClient(uintptr_t wm, int lx, int ly, int* cx, int* cy) {
    int32_t l[4], c[4];
    for (int i = 0; i < 4; ++i) {
        if (!ReadI32(wm + sexy::kWmLogicalRect + 4 * i, &l[i]) ||
            !ReadI32(wm + sexy::kWmClientRect + 4 * i, &c[i]))
            return false;
    }
    if (l[2] <= 0 || l[3] <= 0 || c[2] <= 0 || c[3] <= 0) return false;
    *cx = (lx - l[0]) * c[2] / l[2] + c[0];
    *cy = (ly - l[1]) * c[3] / l[3] + c[1];
    return true;
}

uintptr_t GameFocusWidget(uintptr_t wm) {
    uint32_t p = 0;
    return ReadU32(wm + sexy::kWmFocusWidget, &p) ? p : 0;
}

int HitTest(const std::vector<WidgetInfo>& all, int x, int y) {
    // Approximation of WidgetManager::GetWidgetAt: ignores mouse insets and
    // per-widget hit overrides; the click path still confirms via mIsOver.
    for (int i = (int)all.size() - 1; i >= 0; --i) {
        const WidgetInfo& w = all[i];
        if (!w.visible || !w.mouseVisible) continue;
        if (x >= w.absX && x < w.absX + w.width && y >= w.absY && y < w.absY + w.height)
            return i;
    }
    return -1;
}

bool IsWithin(const std::vector<WidgetInfo>& all, int index, int ancestor) {
    for (int i = index; i >= 0; i = all[i].parentIndex)
        if (i == ancestor) return true;
    return false;
}

std::vector<WidgetInfo> SnapshotWidgets(uintptr_t wm) {
    std::vector<WidgetInfo> out;
    Walk(wm, -1, 0, 0, 0, true, false, &out);
    return out;
}

void DumpWidgetTree(uintptr_t wm) {
    int lw = 0, lh = 0;
    GetLogicalSize(wm, &lw, &lh);
    Log("dump: WidgetManager %08X logical %dx%d", wm, lw, lh);
    for (int off : sexy::kWmDumpFields) {
        uint32_t p = 0;
        ReadU32(wm + off, &p);
        Log("dump:   wm+0x%X = %08X %s", off, p, p ? ClassNameOf(p).c_str() : "");
    }
    for (const WidgetInfo& w : SnapshotWidgets(wm)) {
        Log("dump: %*s%s %08X pos=%d,%d size=%dx%d vis=%d mvis=%d dis=%d over=%d%s%s id=%d label=\"%s\"",
            w.depth * 2, "", w.className.c_str(), w.ptr, w.absX, w.absY, w.width, w.height,
            w.visible, w.mouseVisible, w.disabled, w.isOver, w.isButton ? " button" : "",
            w.isDialog ? " dialog" : "", w.buttonId,
            w.labelRead ? WideToUtf8(w.label).c_str() : "<unread>");
        if (w.isEdit) LogWide("dump:     text: ", w.editText);
        if (w.isDialog) {
            std::wstring s;
            if (ReadWString(w.ptr + sexy::kDialogHeader, &s)) LogWide("dump:     header: ", s);
            if (ReadWString(w.ptr + sexy::kDialogLines, &s)) LogWide("dump:     lines: ", s);
            if (ReadWString(w.ptr + sexy::kDialogFooter, &s)) LogWide("dump:     footer: ", s);
        }
    }
}
