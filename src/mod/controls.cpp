#include "controls.h"

#include <cmath>
#include <cstring>
#include <utility>

#include "game_memory.h"
#include "sexy.h"

namespace controls {

namespace {

bool ReadBool(uintptr_t addr, bool* out) {
    uint8_t b;
    if (!ReadU8(addr, &b)) return false;
    *out = b != 0;
    return true;
}

bool ReadDouble(uintptr_t addr, double* out) {
    uint32_t lo, hi;
    if (!ReadU32(addr, &lo) || !ReadU32(addr + 4, &hi)) return false;
    uint64_t bits = (uint64_t)hi << 32 | lo;
    double v;
    static_assert(sizeof(v) == sizeof(bits), "double must be 64-bit");
    memcpy(&v, &bits, sizeof(v));
    if (!std::isfinite(v)) return false;  // garbage, not a real value
    *out = v;
    return true;
}

bool ReadPtr(uintptr_t addr, uintptr_t* out) {
    uint32_t p;
    if (!ReadU32(addr, &p)) return false;
    *out = p;
    return true;
}

bool ReadSize(uintptr_t widget, int* w, int* h) {
    int32_t a, b;
    if (!ReadI32(widget + sexy::kWidth, &a) || !ReadI32(widget + sexy::kHeight, &b)) return false;
    *w = a;
    *h = b;
    return true;
}

// Image::GetCelWidth / GetCelHeight (FUN_0045b1e0 / FUN_0045b1d0).
bool ReadCelSize(uintptr_t image, int* celW, int* celH) {
    int32_t w, h, rows, cols;
    if (!ReadI32(image + off::kImageWidth, &w) || !ReadI32(image + off::kImageHeight, &h) ||
        !ReadI32(image + off::kImageNumRows, &rows) || !ReadI32(image + off::kImageNumCols, &cols))
        return false;
    if (rows <= 0 || cols <= 0 || w <= 0 || h <= 0) return false;
    *celW = w / cols;
    *celH = h / rows;
    return true;
}

double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

uintptr_t ParentOf(uintptr_t widget) {
    uint32_t p = 0;
    return ReadU32(widget + sexy::kParent, &p) ? p : 0;
}

// Finds which slot of a pointer array in `owner` holds `widget`.
int SlotOf(uintptr_t owner, int arrayOffset, int count, uintptr_t widget) {
    for (int i = 0; i < count; ++i) {
        uintptr_t p;
        if (ReadPtr(owner + arrayOffset + 4 * i, &p) && p == widget) return i;
    }
    return -1;
}

MouseStep Step(MouseStep::Type t, int x, int y, uintptr_t waitHover = 0) {
    return MouseStep{t, x, y, waitHover};
}

}  // namespace

Kind Classify(uintptr_t w) {
    if (!w) return Kind::None;
    if (IsKindOf(w, "Checkbox")) return Kind::Checkbox;
    if (IsKindOf(w, "Slider")) return Kind::Slider;
    if (IsKindOf(w, "DropListWidget")) return Kind::DropList;
    if (IsKindOf(w, "ListWidget")) return Kind::List;
    if (IsKindOf(w, "ScrollbarWidget")) return Kind::Scrollbar;
    if (IsKindOf(w, "ButtonWidget")) {
        TabButtonState t;
        if (ReadTabButton(w, &t)) return Kind::TabButton;
    }
    return Kind::None;
}

bool ReadCheckbox(uintptr_t w, CheckboxState* out) {
    int32_t id;
    bool checked;
    if (!ReadI32(w + off::kCheckboxId, &id) || !ReadBool(w + off::kCheckboxChecked, &checked))
        return false;
    out->id = id;
    out->checked = checked;
    return true;
}

bool ReadSlider(uintptr_t w, SliderState* out) {
    SliderState s;
    int32_t id;
    uintptr_t thumb;
    if (!ReadDouble(w + off::kSliderValue, &s.value) || !ReadI32(w + off::kSliderId, &id) ||
        !ReadBool(w + off::kSliderHorizontal, &s.horizontal) ||
        !ReadBool(w + off::kSliderDragging, &s.dragging) || !ReadSize(w, &s.width, &s.height) ||
        !ReadPtr(w + off::kSliderThumbImage, &thumb))
        return false;
    s.id = id;
    if (thumb && !ReadCelSize(thumb, &s.thumbCelW, &s.thumbCelH)) return false;
    *out = s;
    return true;
}

int SliderPercent(double value) { return (int)std::floor(Clamp01(value) * 100.0 + 0.5); }

bool ReadList(uintptr_t w, ListState* out) {
    ListState s;
    int32_t id, hilite, select, itemHeight;
    uint32_t begin, end;
    if (!ReadI32(w + off::kListId, &id) || !ReadU32(w + off::kListLinesBegin, &begin) ||
        !ReadU32(w + off::kListLinesEnd, &end) || !ReadDouble(w + off::kListPosition, &s.position) ||
        !ReadDouble(w + off::kListPageSize, &s.pageSize) ||
        !ReadI32(w + off::kListHilite, &hilite) || !ReadI32(w + off::kListSelect, &select) ||
        !ReadI32(w + off::kListItemHeight, &itemHeight) || !ReadSize(w, &s.width, &s.height))
        return false;
    if (end < begin || (end - begin) % off::kListLineSize != 0) return false;
    uint32_t count = (end - begin) / off::kListLineSize;
    if (count > 10000) return false;
    s.id = id;
    s.count = (int)count;
    s.hilite = hilite;
    s.select = select;
    s.itemHeight = itemHeight;
    *out = s;
    return true;
}

bool ReadListLine(uintptr_t w, int index, std::wstring* out) {
    uint32_t begin, end;
    if (index < 0 || !ReadU32(w + off::kListLinesBegin, &begin) ||
        !ReadU32(w + off::kListLinesEnd, &end) || end < begin)
        return false;
    if ((uint32_t)index >= (end - begin) / off::kListLineSize) return false;
    return ReadWString(begin + (uint32_t)index * off::kListLineSize, out);
}

bool ReadDropList(uintptr_t w, DropListState* out) {
    int32_t id, selected;
    uintptr_t list;
    bool open;
    if (!ReadI32(w + off::kDropId, &id) || !ReadI32(w + off::kDropSelected, &selected) ||
        !ReadPtr(w + off::kDropList, &list) || !list || !ReadBool(list + sexy::kVisible, &open))
        return false;
    out->id = id;
    out->selected = selected;
    out->open = open;
    out->list = list;
    return true;
}

bool ReadScrollbar(uintptr_t w, ScrollbarState* out) {
    ScrollbarState s;
    int32_t id;
    if (!ReadI32(w + off::kScrollId, &id) || !ReadDouble(w + off::kScrollValue, &s.value) ||
        !ReadDouble(w + off::kScrollMax, &s.maxValue) ||
        !ReadDouble(w + off::kScrollPageSize, &s.pageSize) ||
        !ReadBool(w + off::kScrollHorizontal, &s.horizontal) ||
        !ReadPtr(w + off::kScrollUpButton, &s.upButton) ||
        !ReadPtr(w + off::kScrollDownButton, &s.downButton))
        return false;
    s.id = id;
    *out = s;
    return true;
}

bool ReadTabButton(uintptr_t w, TabButtonState* out) {
    uintptr_t parent = ParentOf(w);
    if (!parent) return false;
    TabButtonState t;
    int slot;
    if (IsKindOf(parent, "RecordsScreen")) {
        slot = SlotOf(parent, off::kRecordsTabButtons, 3, w);
        t.count = 3;
        // RecordsScreen ctor creates the buttons with labels 472, 473, 474.
        if (slot >= 0) t.labelStringId = 472 + slot;
    } else if (IsKindOf(parent, "HighScoresTab")) {
        slot = SlotOf(parent, off::kHsModeButtons, 6, w);
        t.count = 6;
    } else {
        return false;
    }
    if (slot < 0) return false;
    bool disabled;
    if (!ReadBool(w + sexy::kDisabled, &disabled)) return false;
    t.index = slot;
    t.selected = disabled;
    *out = t;
    return true;
}

int RecordsCurrentTab(uintptr_t screen) {
    int current = -1;
    for (int i = 0; i < 3; ++i) {
        uintptr_t page;
        bool visible;
        if (!ReadPtr(screen + off::kRecordsTabPages + 4 * i, &page) || !page ||
            !ReadBool(page + sexy::kVisible, &visible))
            return -1;
        if (visible) {
            if (current >= 0) return -1;  // two visible pages: not a state we understand
            current = i;
        }
    }
    return current;
}

int HighScoresCurrentMode(uintptr_t tab) {
    int32_t mode;
    if (!ReadI32(tab + off::kHsCurrentMode, &mode) || mode < 0 || mode > 5) return -1;
    return mode;
}

bool ReadScoreEntry(uintptr_t w, ScoreRow* out) {
    uintptr_t entry;
    if (!ReadPtr(w + off::kScoreEntryData, &entry)) return false;
    ScoreRow r;
    if (entry) {
        int32_t rank, score;
        if (!ReadI32(entry + off::kEntryRank, &rank) || !ReadI32(entry + off::kEntryScore, &score) ||
            !ReadWString(entry + off::kEntryName, &r.name))
            return false;
        r.present = true;
        r.rank = rank;
        r.score = score;
    }
    *out = r;
    return true;
}

bool ReadHighScores(uintptr_t tab, std::vector<ScoreRow>* out) {
    out->clear();
    for (int i = 0; i < off::kHsEntryCount; ++i) {
        uintptr_t w;
        ScoreRow r;
        if (!ReadPtr(tab + off::kHsEntryWidgets + 4 * i, &w) || !w || !ReadScoreEntry(w, &r))
            return false;
        out->push_back(r);
    }
    return true;
}

int LabelStringId(const std::string& dialog, Kind kind, int id) {
    if (dialog == "OptionsDialog") {
        // Strings from OptionsDialog::DrawAll; ids from the ctor and listeners.
        if (kind == Kind::Slider) {
            switch (id) {
                case 0: return 312;
                case 1: return 313;
                case 2: return 314;
            }
        } else if (kind == Kind::Checkbox) {
            switch (id) {
                case 8: return 316;
                case 9: return 317;
                case 10: return 318;
                case 4: return 319;
                case 5: return 320;
                case 6: return 321;
                case 7: return 562;
            }
        }
    } else if (dialog == "ZenOptionsDialog") {
        // Hypothesis: by ZenOptionsDialog.cpp line order of the drawn strings
        // and the control creation order; see docs/re/controls.md.
        if (kind == Kind::Checkbox) {
            switch (id) {
                case 0: case 1: case 2: case 3: return 515;
                case 6: return 516;
                case 5: return 521;
            }
        } else if (kind == Kind::Slider) {
            switch (id) {
                case 0: return 517;
                case 7: return 518;
                case 1: return 520;
                case 2: return 523;  // 524 when Subliminal is off (unconfirmed)
                case 3: return 526;
            }
        } else if (kind == Kind::DropList) {
            switch (id) {
                case 1: return 519;
                case 2: return 522;
                case 3: return 525;
            }
        }
    }
    return -1;
}

int GroupStringId(const std::string& dialog, Kind kind, int id) {
    if (dialog == "OptionsDialog") {
        if (kind == Kind::Checkbox && id >= 8 && id <= 10) return 315;
        return -1;
    }
    if (dialog == "ZenOptionsDialog") {
        int panel = -1;
        if (kind == Kind::Checkbox) {
            if (id >= 0 && id <= 3) panel = id;
            else if (id == 6) panel = 0;
            else if (id == 5) panel = 2;
        } else if (kind == Kind::Slider) {
            if (id >= 0 && id <= 3) panel = id;
            else if (id == 7) panel = 0;
        } else if (kind == Kind::DropList) {
            if (id >= 1 && id <= 3) panel = id;
        }
        if (panel >= 0) return 527 + panel;  // 527..530 category titles
    }
    return -1;
}

bool IsRadioCheckbox(const std::string& dialog, int id) {
    return dialog == "OptionsDialog" && id >= 8 && id <= 10;
}

std::vector<MouseStep> PlanCheckboxToggle(uintptr_t w, int absX, int absY, int width,
                                          int height) {
    int cx = absX + width / 2, cy = absY + height / 2;
    return {Step(MouseStep::Move, cx, cy, w), Step(MouseStep::Down, cx, cy),
            Step(MouseStep::Up, cx, cy)};
}

int SliderThumbPos(double value, int trackLen, int thumbLen) {
    // Slider::MouseDown/Draw: _ftol(value * (len - thumb)), truncation.
    return (int)(value * (double)(trackLen - thumbLen));
}

int ListRowAt(int localY, int itemHeight, double position) {
    if (itemHeight <= 0) return -1;
    double v = (double)(localY - off::kListTopInset) / itemHeight + position;
    if (v < 0.0) return -1;  // the game's ftol maps (-1, 0) to row 0; treat as "above"
    return (int)v;
}

bool PlanSliderSet(uintptr_t w, const SliderState& s, int absX, int absY, double target,
                   std::vector<MouseStep>* steps, double* predicted) {
    steps->clear();
    target = Clamp01(target);
    int len = s.horizontal ? s.width : s.height;
    int cross = s.horizontal ? s.height : s.width;
    int thumb = s.horizontal ? s.thumbCelW : s.thumbCelH;
    if (len <= 0 || cross <= 0) return false;
    int crossMid = cross / 2;
    auto at = [&](int along) {
        return s.horizontal ? std::make_pair(absX + along, absY + crossMid)
                            : std::make_pair(absX + crossMid, absY + along);
    };
    int thumbPos = thumb > 0 ? SliderThumbPos(s.value, len, thumb) : 0;
    auto inThumb = [&](int p) { return thumb > 0 && p >= thumbPos && p < thumbPos + thumb; };

    // 1. Track click: value = local / len (FUN_005a2aa0). Exact 1.0 is
    //    unreachable (local < len), so 1.0 uses a drag.
    int local = (int)std::floor(target * len + 0.5);
    if (target < 1.0 && local < len && !inThumb(local)) {
        auto p = at(local);
        steps->push_back(Step(MouseStep::Move, p.first, p.second, w));
        steps->push_back(Step(MouseStep::Down, p.first, p.second));
        steps->push_back(Step(MouseStep::Up, p.first, p.second));
        *predicted = (double)local / len;
        return true;
    }

    // 2. Drag the thumb: press inside it, move, release. value =
    //    (local - rel) / (len - thumb), clamped (FUN_005a2c60).
    int span = len - thumb;
    if (thumb <= 0 || span <= 0) return false;
    int press = thumbPos + thumb / 2;
    int rel = press - thumbPos;
    int dest;
    if (target >= 1.0) dest = span + rel + 2;       // overshoot; the game clamps to 1
    else if (target <= 0.0) dest = rel - 2;         // overshoot; the game clamps to 0
    else dest = (int)std::floor(target * span + 0.5) + rel;
    auto p0 = at(press);
    auto p1 = at(dest);
    steps->push_back(Step(MouseStep::Move, p0.first, p0.second, w));
    steps->push_back(Step(MouseStep::Down, p0.first, p0.second));
    steps->push_back(Step(MouseStep::Move, p1.first, p1.second));  // drag (button held)
    steps->push_back(Step(MouseStep::Up, p1.first, p1.second));
    *predicted = Clamp01((double)(dest - rel) / span);
    return true;
}

bool PlanListClick(uintptr_t w, const ListState& s, int absX, int absY, int index,
                   std::vector<MouseStep>* steps) {
    steps->clear();
    if (index < 0 || index >= s.count || s.itemHeight <= 0 || s.width <= 0) return false;
    double rowTop = off::kListTopInset + (index - s.position) * s.itemHeight;
    int y = (int)std::floor(rowTop + s.itemHeight / 2.0);
    if (y < 0 || y >= s.height) return false;  // off the visible page: scroll first
    if (ListRowAt(y, s.itemHeight, s.position) != index) return false;
    int x = absX + s.width / 2;
    steps->push_back(Step(MouseStep::Move, x, absY + y, w));
    steps->push_back(Step(MouseStep::Down, x, absY + y));
    steps->push_back(Step(MouseStep::Up, x, absY + y));
    return true;
}

bool LogicalToClientCentre(uintptr_t wm, int lx, int ly, int* cx, int* cy) {
    int32_t l[4], c[4];
    for (int i = 0; i < 4; ++i) {
        if (!ReadI32(wm + sexy::kWmLogicalRect + 4 * i, &l[i]) ||
            !ReadI32(wm + sexy::kWmClientRect + 4 * i, &c[i]))
            return false;
    }
    if (l[2] <= 0 || l[3] <= 0 || c[2] <= 0 || c[3] <= 0) return false;
    // Centre of logical pixel lx: (lx - L.x + 0.5) * C.w / L.w + C.x.
    *cx = (int)(((int64_t)(lx - l[0]) * 2 + 1) * c[2] / (2 * (int64_t)l[2])) + c[0];
    *cy = (int)(((int64_t)(ly - l[1]) * 2 + 1) * c[3] / (2 * (int64_t)l[3])) + c[1];
    return true;
}

}  // namespace controls
