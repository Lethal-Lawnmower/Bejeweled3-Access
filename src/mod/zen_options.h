#pragma once

// Zen options panel (ZenOptionsDialog, vtable 0x0086F55C) and its four
// sections (ZenCategoryContainer, vtable 0x0086F274).
//
// How the game works (static, Ghidra; details and addresses in
// docs/re/zen-options.md):
// - The dialog owns four containers at dialog+0xD8 + 4*id (ids 0-3, titles
//   are strings 527-530; id stored at
//   container+0xC4). dialog+0xE8 is the selected (expanding) section, -1 at
//   construction. There is no scroller: the vertical slider at dialog+0xD4
//   is the music volume (value = 1 - volume).
// - Selection is by HOVER only. Every Update, a container calls the dialog's
//   select function FUN_00683410(id) when the WidgetManager's last mouse
//   position lies in [mX, mX + bgImageWidth) x [mY + container+0x164,
//   mY + mHeight) and the container itself has mIsOver (so the mouse must be
//   on the container, not on one of its child controls). Leaving does not
//   collapse anything; only selecting another section does. Selecting is
//   refused while the selected section's drop-down list is open.
// - Each container has an expansion CurvedVal at container+0xC8. Every
//   Update the container sets, for every child except the first (the
//   Enable checkbox): visible = expansion > 0, disabled = !Enable checked.
// - Dialog Update lays the containers out top to bottom, each
//   170*S/1200 + expansion * container+0x138 high (S = [gApp+0x9E8]),
//   vertically centred on 600*S/1200, and puts BACK 40*S/1200 below.
//
// This module only reads memory and plans mouse steps; the navigator runs
// the steps with its normal hover path. Nothing here is runtime-verified.

#include <cstdint>
#include <string>
#include <vector>

#include "controls.h"
#include "widgets.h"

namespace zen {

namespace off {
// ZenOptionsDialog (ctor FUN_0068bb50, Update FUN_006876f0,
// select FUN_00683410, listeners FUN_0068b870 / FUN_0068b6c0 / FUN_0068b420).
constexpr int kDlgBoard = 0xCC;          // ZenBoard* (ctor argument)
constexpr int kDlgMusicSlider = 0xD4;    // vertical Slider, id 4
constexpr int kDlgSections = 0xD8;       // 4 x ZenCategoryContainer*, index = id
constexpr int kDlgSelected = 0xE8;       // int, -1 = none
constexpr int kDlgBackButton = 0xEC;     // Bej3DialogButton, id 0
// ZenCategoryContainer (ctor FUN_004298c0, Update FUN_0042a960,
// Draw FUN_00429bd0, DrawAll FUN_0042ab20).
constexpr int kSecDialog = 0xC0;         // owning ZenOptionsDialog*
constexpr int kSecId = 0xC4;             // int 0..3
constexpr int kSecCurve = 0xC8;          // CurvedVal (expansion)
constexpr int kCurveOutVal = 0x38;       // double, written by every GetOutVal (FUN_00437c60)
constexpr int kSecExtraHeight = 0x138;   // int, 150*S/1200
constexpr int kSecEnable = 0x13C;        // Checkbox* (first child)
constexpr int kSecCheckbox2 = 0x140;     // Checkbox* (section 0: id 6; section 2: id 5)
constexpr int kSecDropList = 0x144;      // DropListWidget* or 0
constexpr int kSecSlider = 0x148;        // Slider* or 0 (ids 0..3)
constexpr int kSecHoverTopInset = 0x164; // int, hover band starts this far below mY
constexpr int kSecHyperlink = 0x168;     // HyperlinkWidget* or 0 (section 3)
// Globals.
constexpr int kAppArtScale = 0x9E8;      // gApp: S, layout values are S*n/1200
constexpr int kAppProfile = 0xB80;       // gApp: current profile
constexpr int kProfileSubliminal = 0x2A0;  // bool, section 2 Draw picks the slider caption by it
constexpr uintptr_t kSectionBgImage = 0x008D7298;  // Image*, width +0x2C bounds the hover band
constexpr int kImageWidth = 0x2C;
}  // namespace off

constexpr int kSectionCount = 4;
constexpr int kTitleStringBase = 527;    // titles: GetString(0x20F + id), DrawAll FUN_0042ab20

struct ZenControl {
    uintptr_t widget = 0;
    controls::Kind kind = controls::Kind::None;
    bool isButton = false;     // HyperlinkWidget (a ButtonWidget)
    int controlId = -1;        // checkbox / slider / drop-list id, -1 otherwise
    bool visible = false;      // the widget's own mVisible (+0x64)
    bool disabled = false;     // the widget's own mDisabled (+0x66)
    // String id the container's Draw (FUN_00429bd0) draws as this control's
    // caption, or -1 when the game draws none or a literal without an id.
    int captionStringId = -1;
};

struct ZenSection {
    int id = -1;
    uintptr_t container = 0;
    int x = 0, y = 0, width = 0, height = 0;  // relative to the dialog
    int titleStringId = -1;
    std::wstring title;        // raw game string (clean before speaking); empty if unread
    uintptr_t enableCheckbox = 0;
    bool enabled = false;      // Enable checkbox checked
    std::vector<ZenControl> controls;  // children after the Enable checkbox, in list order
    bool controlsShown = false;  // any sub-control visible (game rule: expansion > 0)
    double expansion = -1.0;     // cached CurvedVal output, -1 if unreadable
    int extraHeight = 0;         // container+0x138
    int hoverTopInset = 0;       // container+0x164
};

struct ZenOptionsState {
    uintptr_t dialog = 0;
    int selected = -1;           // dialog+0xE8
    int artScale = 0;            // S
    int bgImageWidth = 0;        // 0 if unreadable
    uintptr_t musicSlider = 0;
    uintptr_t backButton = 0;
    ZenSection sections[kSectionCount];
    // True when the layout has stopped moving: the selected section is at
    // its full height (within 1 px) and every other section's controls are
    // hidden (their expansion reached 0). Positions taken before this may
    // still move.
    bool settled = false;
    // True when a drop-down list in the selected section is open; the game
    // then refuses to select another section.
    bool selectedListOpen = false;
};

// True if `widget` is a ZenOptionsDialog (RTTI).
bool IsZenOptionsDialog(uintptr_t widget);

// Reads the whole panel. False if any structural read fails (dialog fields,
// a container pointer, its id or geometry); fields that fail individually
// (title text, captions) are left empty / -1.
bool ReadZenOptions(uintptr_t dialog, ZenOptionsState* out);

// Index of `container` in the dialog's section array (= its id), or -1.
int SectionIndexOf(const ZenOptionsState& s, uintptr_t container);

enum class ExpandPlan {
    Ready,        // already selected; steps empty (may still be animating: check settled)
    Planned,      // steps filled: hover the container
    ListOpen,     // the selected section's drop-down is open; the game refuses to switch
    NoPoint,      // no point of the hover band reaches the container itself
    Unreadable,   // bad section index or container missing from the snapshot
};

// Plans the mouse path that makes the game itself select section `id`:
// one Move to a point of the container's hover band that the snapshot's
// HitTest gives to the container (not a child), waiting for its mIsOver.
// The container Update then selects it on the next frame. The chosen point
// stays inside the container through the resulting relayout.
ExpandPlan PlanExpandSection(const ZenOptionsState& s, int id, const std::vector<WidgetInfo>& all,
                             std::vector<controls::MouseStep>* steps);

// Pure helpers (exposed for checking the maths).
int ScaleArt(int artScale, int n);  // artScale * n / 1200, integer division as the game does
int SectionBaseHeight(int artScale);  // 170*S/1200
// Caption id the container Draw uses for a control, or -1. `subliminal` is
// the profile flag the section 2 Draw tests.
int CaptionStringId(int sectionId, controls::Kind kind, bool isButton, int controlId,
                    bool subliminal);

}  // namespace zen
