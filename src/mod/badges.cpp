#include "badges.h"

#include <cstring>

#include "game_memory.h"
#include "game_strings.h"
#include "log.h"
#include "sexy.h"
#include "text.h"

// Offsets below are static evidence from Ghidra (docs/re/badges.md lists
// the function and line for each). None is runtime-confirmed yet.
namespace {

constexpr int kBadgeCount = 20;  // Update/Draw loop bound 0x14; manager holds ids 0..19

// gApp fields
constexpr int kAppArtScale = 0x9E8;     // S: every layout value is S * n / 1200
constexpr int kAppBadgeNames = 0xD2C;   // vector<wstring> begin (+0xD30 end), 0x1C per id
constexpr int kAppBadgeLevels = 0xD44;  // vector<0x18-byte entry> begin (+0xD48 end)
constexpr int kNameStride = 0x1C;
constexpr int kLevelEntryStride = 0x18;
constexpr int kLevelEntryBegin = 0x0C;  // int* thresholds begin inside an entry
constexpr int kLevelEntryEnd = 0x10;

// BadgeWidget fields (ctor FUN_0067c370, Update FUN_0068fb50, Draw FUN_00685b90)
constexpr int kBwAwardSource = 0xBC;  // 0 on the Records tab (BadgesTab ctor passes 0)
constexpr int kBwManager = 0xC0;      // badge manager; Badge* at +0x04 + 4*id
constexpr int kBwLevels = 0xC8;       // int[20] level as drawn (0 = dimmed / not earned)
constexpr int kBwHoverFade = 0x118;   // float[20] hover highlight 0..1

// Badge fields (manager ctor FUN_007770d0)
constexpr int kBadgeId = 0x04;

// WidgetManager last mouse position (Update reads [mWidgetManager+0x10C/0x110])
constexpr int kWmMouseX = 0x10C;
constexpr int kWmMouseY = 0x110;

// Image used for the hover rectangle: Update calls GetCelRect(0) on [0x008D8814].
// Image fields from FUN_0045b1d0 / FUN_0045b1e0.
constexpr uintptr_t kBadgeHitImagePtr = 0x008D8814;
constexpr int kImgWidth = 0x2C;
constexpr int kImgHeight = 0x30;
constexpr int kImgRows = 0x34;
constexpr int kImgCols = 0x38;

bool ReadApp(uintptr_t* app) {
    uint32_t v;
    if (!ReadU32(sexy::kGAppPtr, &v) || v == 0) return false;
    *app = v;
    return true;
}

// FUN_00673ba0: badge centre relative to the widget. Integer divisions are
// signed and truncate like the game's magic-number divisions; the y value
// goes through doubles and is truncated (CVTTSD2SI in FUN_00792bf0).
void BadgeCentre(int id, int scale, int width, int height, int* x, int* y) {
    int step = scale * 205 / 1200;
    *x = (id % 5) * step - scale * 410 / 1200 + width / 2;
    double fy = static_cast<double>(height / 2) - scale * 307.5 / 1200.0;
    fy += static_cast<double>((id / 5) * step);
    fy -= static_cast<double>(scale * 15 / 1200);
    *y = static_cast<int>(fy);
    if (id > 14) *y += scale * 30 / 1200;  // the fourth ("elite") row sits lower
}

// Cel 0 size of the hover image; false if unreadable.
bool HitCelSize(int* w, int* h) {
    uint32_t img;
    int32_t iw, ih, rows, cols;
    if (!ReadU32(kBadgeHitImagePtr, &img) || img == 0) return false;
    if (!ReadI32(img + kImgWidth, &iw) || !ReadI32(img + kImgHeight, &ih) ||
        !ReadI32(img + kImgRows, &rows) || !ReadI32(img + kImgCols, &cols))
        return false;
    if (rows <= 0 || cols <= 0 || iw <= 0 || ih <= 0) return false;
    *w = iw / cols;
    *h = ih / rows;
    return true;
}

bool ReadBadgeName(uintptr_t app, int id, std::wstring* out) {
    uint32_t begin, end;
    if (!ReadU32(app + kAppBadgeNames, &begin) || !ReadU32(app + kAppBadgeNames + 4, &end))
        return false;
    if (begin == 0 || end < begin || (end - begin) / kNameStride <= static_cast<uint32_t>(id))
        return false;
    return ReadWString(begin + id * kNameStride, out);
}

int ReadLevelCount(uintptr_t app, int id) {
    uint32_t begin, end;
    if (!ReadU32(app + kAppBadgeLevels, &begin) || !ReadU32(app + kAppBadgeLevels + 4, &end))
        return -1;
    if (begin == 0 || end < begin ||
        (end - begin) / kLevelEntryStride <= static_cast<uint32_t>(id))
        return -1;
    uint32_t entry = begin + id * kLevelEntryStride, tb, te;
    if (!ReadU32(entry + kLevelEntryBegin, &tb) || !ReadU32(entry + kLevelEntryEnd, &te) ||
        te < tb || (te - tb) / 4 > 16)
        return -1;
    return static_cast<int>((te - tb) / 4);
}

struct GridBasics {
    uintptr_t app = 0;
    int scale = 0, width = 0, height = 0;
};

bool ReadBasics(uintptr_t widget, GridBasics* g) {
    if (!IsBadgeWidget(widget) || !ReadApp(&g->app)) return false;
    int32_t s, w, h;
    if (!ReadI32(g->app + kAppArtScale, &s) || !ReadI32(widget + sexy::kWidth, &w) ||
        !ReadI32(widget + sexy::kHeight, &h) || s <= 0 || w <= 0 || h <= 0)
        return false;
    g->scale = s;
    g->width = w;
    g->height = h;
    return true;
}

}  // namespace

bool IsBadgeWidget(uintptr_t widget) {
    return widget != 0 && ClassNameOf(widget) == "BadgeWidget";
}

bool ReadBadgeGrid(uintptr_t badgeWidget, int absX, int absY, std::vector<BadgeCell>* out) {
    out->clear();
    GridBasics g;
    if (!ReadBasics(badgeWidget, &g)) return false;
    int celW = 0, celH = 0;
    bool celKnown = HitCelSize(&celW, &celH);
    uint32_t manager = 0;
    ReadU32(badgeWidget + kBwManager, &manager);

    for (int id = 0; id < kBadgeCount; ++id) {
        BadgeCell c;
        c.id = id;
        int rx, ry;
        BadgeCentre(id, g.scale, g.width, g.height, &rx, &ry);
        c.centerX = absX + rx;
        c.centerY = absY + ry;
        if (celKnown) {
            c.hitKnown = true;
            c.hitX = c.centerX - celW / 2;
            c.hitY = c.centerY - celH / 2;
            c.hitWidth = celW;
            c.hitHeight = celH;
        }
        uint32_t badge = 0, badgeId = 0;
        if (manager != 0 && ReadU32(manager + 4 + 4 * id, &badge) && badge != 0 &&
            ReadU32(badge + kBadgeId, &badgeId) && badgeId == static_cast<uint32_t>(id)) {
            c.badge = badge;
            c.badgeClass = ClassNameOf(badge);
        }
        int32_t level;
        if (ReadI32(badgeWidget + kBwLevels + 4 * id, &level) && level >= 0 && level <= 16) {
            c.levelKnown = true;
            c.level = level;
        }
        c.levelCount = ReadLevelCount(g.app, id);
        ReadBadgeName(g.app, id, &c.name);
        // FUN_00778130: level L (1..4) is named by string id L.
        if (c.levelKnown && c.level >= 1 && c.level <= 4) GameString(c.level, &c.levelName);
        uint32_t fadeBits;
        if (ReadU32(badgeWidget + kBwHoverFade + 4 * id, &fadeBits))
            std::memcpy(&c.hoverFade, &fadeBits, sizeof c.hoverFade);
        out->push_back(c);
    }
    return true;
}

int HoveredBadgeId(uintptr_t badgeWidget, int absX, int absY) {
    GridBasics g;
    if (!ReadBasics(badgeWidget, &g)) return -2;
    uint8_t over;
    if (!ReadU8(badgeWidget + sexy::kIsOver, &over)) return -2;
    if (!over) return -1;
    int celW, celH;
    if (!HitCelSize(&celW, &celH)) return -2;
    uint32_t wm;
    int32_t mx, my;
    if (!ReadU32(badgeWidget + sexy::kWidgetManager, &wm) || wm == 0 ||
        !ReadI32(wm + kWmMouseX, &mx) || !ReadI32(wm + kWmMouseY, &my))
        return -2;
    int lx = mx - absX, ly = my - absY;
    for (int id = 0; id < kBadgeCount; ++id) {
        int rx, ry;
        BadgeCentre(id, g.scale, g.width, g.height, &rx, &ry);
        int left = rx - celW / 2, top = ry - celH / 2;
        if (lx >= left && lx < left + celW && ly >= top && ly < top + celH) return id;
    }
    return -1;
}

void DumpBadgeGrid(uintptr_t badgeWidget, int absX, int absY) {
    std::vector<BadgeCell> cells;
    if (!ReadBadgeGrid(badgeWidget, absX, absY, &cells)) {
        Log("badges: grid unreadable (widget %08X)", static_cast<unsigned>(badgeWidget));
        return;
    }
    uint32_t awardSource = 0;
    ReadU32(badgeWidget + kBwAwardSource, &awardSource);
    Log("badges: widget %08X abs %d,%d award source %08X hovered %d", static_cast<unsigned>(badgeWidget),
        absX, absY, awardSource, HoveredBadgeId(badgeWidget, absX, absY));
    for (const BadgeCell& c : cells) {
        Log("badges: id %d centre %d,%d hit %s %d,%d %dx%d level %s%d/%d fade %.2f class %s",
            c.id, c.centerX, c.centerY, c.hitKnown ? "known" : "unknown", c.hitX, c.hitY,
            c.hitWidth, c.hitHeight, c.levelKnown ? "" : "unknown ", c.level, c.levelCount,
            static_cast<double>(c.hoverFade), c.badgeClass.c_str());
        LogWide("badges:   name: ", CleanGameText(c.name));
        if (!c.levelName.empty()) LogWide("badges:   level name: ", CleanGameText(c.levelName));
    }
}
