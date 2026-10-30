#include "zen_options.h"

#include <cmath>
#include <cstring>

#include "game_memory.h"
#include "game_strings.h"
#include "sexy.h"

namespace zen {

namespace {

bool ReadPtr(uintptr_t addr, uintptr_t* out) {
    uint32_t p;
    if (!ReadU32(addr, &p)) return false;
    *out = p;
    return true;
}

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
    memcpy(&v, &bits, sizeof(v));
    if (!std::isfinite(v)) return false;
    *out = v;
    return true;
}

// Children of a WidgetContainer in list order (std::list at +0x04, head
// node pointer +0x18, nodes {next, prev, Widget*}). Bounded walk.
bool ReadChildren(uintptr_t container, std::vector<uintptr_t>* out) {
    out->clear();
    uintptr_t head;
    if (!ReadPtr(container + sexy::kWidgetsHead, &head) || head == 0) return false;
    uintptr_t node;
    if (!ReadPtr(head, &node)) return false;
    for (int guard = 0; node != head; ++guard) {
        if (guard > 64 || node == 0) return false;
        uintptr_t w;
        if (!ReadPtr(node + 8, &w)) return false;
        out->push_back(w);
        if (!ReadPtr(node, &node)) return false;
    }
    return true;
}

bool ReadAppInt(int offset, int32_t* out) {
    uint32_t app;
    if (!ReadU32(sexy::kGAppPtr, &app) || app == 0) return false;
    return ReadI32(app + offset, out);
}

bool ReadSubliminal(bool* out) {
    uint32_t app, profile;
    if (!ReadU32(sexy::kGAppPtr, &app) || app == 0) return false;
    if (!ReadU32(app + off::kAppProfile, &profile) || profile == 0) return false;
    return ReadBool(profile + off::kProfileSubliminal, out);
}

int ControlIdOf(uintptr_t w, controls::Kind kind) {
    switch (kind) {
    case controls::Kind::Checkbox: {
        controls::CheckboxState s;
        return controls::ReadCheckbox(w, &s) ? s.id : -1;
    }
    case controls::Kind::Slider: {
        controls::SliderState s;
        return controls::ReadSlider(w, &s) ? s.id : -1;
    }
    case controls::Kind::DropList: {
        controls::DropListState s;
        return controls::ReadDropList(w, &s) ? s.id : -1;
    }
    default:
        return -1;
    }
}

bool ReadSection(uintptr_t c, int expectedId, bool subliminal, bool haveSubliminal,
                 ZenSection* out) {
    ZenSection s;
    s.container = c;
    int32_t id, x, y, w, h, extra, inset;
    if (!ReadI32(c + off::kSecId, &id) || id != expectedId) return false;
    if (!ReadI32(c + sexy::kX, &x) || !ReadI32(c + sexy::kY, &y) || !ReadI32(c + sexy::kWidth, &w) ||
        !ReadI32(c + sexy::kHeight, &h))
        return false;
    s.id = id;
    s.x = x;
    s.y = y;
    s.width = w;
    s.height = h;
    if (ReadI32(c + off::kSecExtraHeight, &extra)) s.extraHeight = extra;
    if (ReadI32(c + off::kSecHoverTopInset, &inset)) s.hoverTopInset = inset;
    double e;
    if (ReadDouble(c + off::kSecCurve + off::kCurveOutVal, &e)) s.expansion = e;

    s.titleStringId = kTitleStringBase + id;
    if (!GameString(s.titleStringId, &s.title)) s.title.clear();

    if (!ReadPtr(c + off::kSecEnable, &s.enableCheckbox) || s.enableCheckbox == 0) return false;
    controls::CheckboxState en;
    if (!controls::ReadCheckbox(s.enableCheckbox, &en)) return false;
    s.enabled = en.checked;

    std::vector<uintptr_t> kids;
    if (!ReadChildren(c, &kids)) return false;
    // Container Update treats the first child as the Enable checkbox and
    // applies visibility / disabled state to every later child.
    for (size_t i = 1; i < kids.size(); ++i) {
        ZenControl z;
        z.widget = kids[i];
        z.kind = controls::Classify(z.widget);
        z.isButton = IsKindOf(z.widget, "ButtonWidget");
        z.controlId = ControlIdOf(z.widget, z.kind);
        ReadBool(z.widget + sexy::kVisible, &z.visible);
        ReadBool(z.widget + sexy::kDisabled, &z.disabled);
        z.captionStringId = haveSubliminal || !(id == 2 && z.kind == controls::Kind::Slider)
                                ? CaptionStringId(id, z.kind, z.isButton, z.controlId, subliminal)
                                : -1;
        if (z.visible) s.controlsShown = true;
        s.controls.push_back(z);
    }
    *out = s;
    return true;
}

}  // namespace

int ScaleArt(int artScale, int n) { return artScale * n / 1200; }

int SectionBaseHeight(int artScale) { return ScaleArt(artScale, 170); }

int CaptionStringId(int sectionId, controls::Kind kind, bool isButton, int controlId,
                    bool subliminal) {
    // From ZenCategoryContainer::Draw FUN_00429bd0 (caption drawn on the row
    // where the dialog ctor FUN_0068bb50 places each control) and the
    // listener ids in FUN_0068b870 / FUN_0068b6c0 / FUN_0068b420.
    using controls::Kind;
    if (isButton) return -1;  // Binaural safety link: literal text, no id
    switch (sectionId) {
    case 0:
        if (kind == Kind::Checkbox) return controlId == 0 ? 515 : (controlId == 6 ? 516 : -1);
        if (kind == Kind::Slider) return controlId == 0 ? 517 : (controlId == 7 ? 518 : -1);
        return -1;
    case 1:
        if (kind == Kind::Checkbox && controlId == 1) return 515;
        if (kind == Kind::DropList && controlId == 1) return 519;
        if (kind == Kind::Slider && controlId == 1) return 520;
        return -1;
    case 2:
        if (kind == Kind::Checkbox) return controlId == 2 ? 515 : (controlId == 5 ? 521 : -1);
        if (kind == Kind::DropList && controlId == 2) return 522;
        if (kind == Kind::Slider && controlId == 2) return subliminal ? 523 : 524;
        return -1;
    case 3:
        if (kind == Kind::Checkbox && controlId == 3) return 515;
        if (kind == Kind::DropList && controlId == 3) return 525;
        if (kind == Kind::Slider && controlId == 3) return 526;
        return -1;
    default:
        return -1;
    }
}

bool IsZenOptionsDialog(uintptr_t widget) {
    return widget != 0 && ClassNameOf(widget) == "ZenOptionsDialog";
}

bool ReadZenOptions(uintptr_t dialog, ZenOptionsState* out) {
    if (!IsZenOptionsDialog(dialog)) return false;
    ZenOptionsState s;
    s.dialog = dialog;
    int32_t sel, scale;
    if (!ReadI32(dialog + off::kDlgSelected, &sel) || sel < -1 || sel >= kSectionCount) return false;
    if (!ReadAppInt(off::kAppArtScale, &scale) || scale <= 0) return false;
    s.selected = sel;
    s.artScale = scale;
    ReadPtr(dialog + off::kDlgMusicSlider, &s.musicSlider);
    ReadPtr(dialog + off::kDlgBackButton, &s.backButton);
    uintptr_t bg;
    int32_t bgW;
    if (ReadPtr(off::kSectionBgImage, &bg) && bg != 0 && ReadI32(bg + off::kImageWidth, &bgW) &&
        bgW > 0)
        s.bgImageWidth = bgW;

    bool subliminal = false;
    bool haveSubliminal = ReadSubliminal(&subliminal);
    for (int i = 0; i < kSectionCount; ++i) {
        uintptr_t c;
        if (!ReadPtr(dialog + off::kDlgSections + 4 * i, &c) || c == 0) return false;
        if (!ReadSection(c, i, subliminal, haveSubliminal, &s.sections[i])) return false;
    }

    if (s.selected >= 0) {
        const ZenSection& cur = s.sections[s.selected];
        uintptr_t drop;
        controls::DropListState d;
        if (ReadPtr(cur.container + off::kSecDropList, &drop) && drop != 0 &&
            controls::ReadDropList(drop, &d))
            s.selectedListOpen = d.open;
    }

    // Settled: the layout the dialog Update produces once every curve has
    // finished (selected = base + extra, others' controls hidden).
    bool settled = true;
    int base = SectionBaseHeight(scale);
    for (int i = 0; i < kSectionCount; ++i) {
        const ZenSection& z = s.sections[i];
        if (i == s.selected) {
            if (std::abs(z.height - (base + z.extraHeight)) > 1) settled = false;
        } else if (z.controlsShown || std::abs(z.height - base) > 1) {
            settled = false;
        }
    }
    s.settled = settled;
    *out = s;
    return true;
}

int SectionIndexOf(const ZenOptionsState& s, uintptr_t container) {
    for (int i = 0; i < kSectionCount; ++i)
        if (container != 0 && s.sections[i].container == container) return i;
    return -1;
}

ExpandPlan PlanExpandSection(const ZenOptionsState& s, int id, const std::vector<WidgetInfo>& all,
                             std::vector<controls::MouseStep>* steps) {
    steps->clear();
    if (id < 0 || id >= kSectionCount) return ExpandPlan::Unreadable;
    if (s.selected == id) return ExpandPlan::Ready;
    if (s.selectedListOpen) return ExpandPlan::ListOpen;
    const ZenSection& z = s.sections[id];
    int index = -1;
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].ptr == z.container) index = (int)i;
    if (index < 0) return ExpandPlan::Unreadable;
    const WidgetInfo& c = all[index];

    // Hover band (container Update FUN_0042a960), in absolute coordinates.
    // The game compares the WidgetManager mouse position with the
    // container's parent-relative mX/mY; the dialog sits at the logical
    // origin, so absX/absY equal mX/mY there (runtime 2026-10-30).
    int bandW = s.bgImageWidth > 0 && s.bgImageWidth < c.width ? s.bgImageWidth : c.width;
    int top = c.absY + (z.hoverTopInset > 0 ? z.hoverTopInset : 0);
    int bottom = c.absY + c.height;  // exclusive
    // Prefer the title row (DrawAll draws the title centred at bandW/2,
    // baseline 63*S/1200) and points near the top: a point within the first
    // 108 rows of a collapsed section stays inside it after the relayout
    // (see docs/re/zen-options.md).
    int titleY = c.absY + ScaleArt(s.artScale, 63) - ScaleArt(s.artScale, 10);
    std::vector<int> ys;
    if (titleY >= top && titleY < bottom) ys.push_back(titleY);
    for (int y = top + 2; y < bottom - 1 && y < c.absY + SectionBaseHeight(s.artScale); y += 4)
        ys.push_back(y);
    const int xs[] = {c.absX + bandW / 2, c.absX + bandW / 4, c.absX + bandW * 3 / 4,
                      c.absX + 4, c.absX + bandW - 5};
    for (int y : ys)
        for (int x : xs) {
            if (x < c.absX || x >= c.absX + bandW) continue;
            if (HitTest(all, x, y) != index) continue;
            steps->push_back({controls::MouseStep::Move, x, y, z.container});
            return ExpandPlan::Planned;
        }
    return ExpandPlan::NoPoint;
}

}  // namespace zen
