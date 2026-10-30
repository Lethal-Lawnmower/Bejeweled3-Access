#include "dialog_text.h"

#include "game_memory.h"
#include "game_strings.h"
#include "log.h"
#include "sexy.h"

// Every offset below is STATIC evidence (Ghidra on the Steam exe); the
// function that proves it is named next to it and in docs/re/rare-dialogs.md.
// None is runtime-confirmed yet.
namespace dialog_text {
namespace {

// The game reads its app object through two globals that hold the same
// pointer: 0x008E1730 (set by the SexyAppBase ctor FUN_0046b440) and
// 0x008E170C (set right after `new Bej3App` in FUN_00791c00). The dialog
// code below uses the second; sexy::kGAppPtr is the first.

// App fields (FUN_00676470, BadgeWidget::Draw, FUN_006a7c80).
constexpr int kAppProfile = 0xB80;      // Profile*
constexpr int kAppQuestMenu = 0xBD8;    // QuestMenu* or 0 (EndLevelDialog ctor button choice)
constexpr int kAppRankNames = 0xD14;    // vector<wstring> begin (+0xD18 end)
constexpr int kAppBadgeNames = 0xD2C;   // vector<wstring> begin (+0xD30 end)
constexpr int kWStringStride = 0x1C;    // sizeof(std::wstring), MSVC 2008

// Profile fields.
constexpr int kProfileRank = 0x1F0;     // int rank (RankBarWidget ctor FUN_00682f90)

// RankUpDialog (ctor FUN_0078ec50, Draw FUN_0078f0d0).
constexpr int kRankUpRankBar = 0x22C;   // RankBarWidget* (ctor `new 0x150`, FUN_00682f90)
// RankBarWidget (ctor FUN_00682f90).
constexpr int kRankBarDispRank = 0x138; // display rank (log "mDispRank = %i")
constexpr int kRankBarBoard = 0x144;    // Board* or 0

// BadgeAwardDialog (ctor FUN_0067f4f0, Update FUN_0068faf0).
constexpr int kAwardBoard = 0x228;      // Board* (ctor arg; Board::Update passes `this`)
constexpr int kAwardWidget = 0x22C;     // BadgeWidget* (`new 0x340`, FUN_0067c370)
// BadgeWidget in award mode (badges.md; Update FUN_0068fb50).
constexpr int kBwAwardSource = 0xBC;    // Board* (non-zero = award mode)
constexpr int kBwManager = 0xC0;        // [board+0x3068]: Badge* at +0x04 + 4*id
constexpr int kBwAwardIndex = 0x328;    // badge being presented
// Board award fields (Board::Update FUN_00767090, Badge award FUN_0077d790).
constexpr int kBoardAwardBegin = 0x305C;  // vector<int> of newly earned badge ids
constexpr int kBoardAwardEnd = 0x3060;
// Badge object (manager ctor FUN_007770d0).
constexpr int kBadgeId = 0x04;
constexpr int kBadgeLevel = 0x08;

// UnlockDialog (ctor FUN_006803b0 copies its wstring argument here; Draw
// FUN_006800d0 draws `this+0x228`).
constexpr int kUnlockModeName = 0x228;

// ZenIntroDialog Draw FUN_0078d440 draws three images: [0x008D8670] (the
// message), [0x008D7A9C] above and [0x008D8978] below it.
constexpr uintptr_t kZenIntroImageGlobal = 0x008D8670;

// EndLevelDialog (ctor FUN_006a7c80).
constexpr int kEndBoard = 0x228;        // Board*
constexpr int kEndScore = 0x2C4;        // copy of board+0xD24 (score)
constexpr int kEndLevel = 0x378;        // copy of board+0xE04 (0-based level)
// Stat copies: dialog+0x2C8+4*i = board+0xC84+4*i. DrawSpecialGemDisplay
// (vtable slot 87, FUN_006a70f0) prints +0x30C/+0x310/+0x314 with strings
// 230/231/232 next to the icons below (in that order).
struct GemSlot {
    uintptr_t imageGlobal;
    int countOffset;
};
constexpr GemSlot kGemSlots[] = {
    {0x008D92E0, 0x30C},
    {0x008D7278, 0x310},
    {0x008D9644, 0x314},
};

// QuestMenu::Update FUN_00697d40 stores `new Announcement` (string 421,
// board 0) at QuestMenu+0x178. Announcement: vtable 0x00863BD8, text
// std::wstring at +0x10 (hud.md, ctor FUN_0078C7F0).
constexpr int kQuestMenuAnnouncement = 0x178;
constexpr uintptr_t kVtAnnouncement = 0x00863BD8;
constexpr int kAnnouncementText = 0x10;

bool ReadPtr(uintptr_t addr, uintptr_t* out) {
    uint32_t v;
    if (!ReadU32(addr, &v) || v == 0) return false;
    *out = v;
    return true;
}

bool ReadApp(uintptr_t* app) { return ReadPtr(sexy::kGAppPtr, app); }

// Element `index` of an app-owned std::vector<std::wstring> (begin at
// `beginOff`, end right after). False if out of range or unreadable.
bool ReadAppWStringVector(int beginOff, int index, std::wstring* out) {
    uintptr_t app, begin, end;
    if (index < 0 || !ReadApp(&app) || !ReadPtr(app + beginOff, &begin) ||
        !ReadPtr(app + beginOff + 4, &end) || end < begin)
        return false;
    int count = static_cast<int>((end - begin) / kWStringStride);
    if (index >= count) return false;
    return ReadWString(begin + static_cast<uintptr_t>(index) * kWStringStride, out);
}

int AppWStringVectorSize(int beginOff) {
    uintptr_t app, begin, end;
    if (!ReadApp(&app) || !ReadPtr(app + beginOff, &begin) || !ReadPtr(app + beginOff + 4, &end) ||
        end < begin)
        return -1;
    return static_cast<int>((end - begin) / kWStringStride);
}

bool IsClass(uintptr_t object, const char* name) { return ClassNameOf(object) == name; }

}  // namespace

RareDialog ClassifyRareDialog(uintptr_t dialog) {
    std::string c = ClassNameOf(dialog);
    if (c == "RankUpDialog") return RareDialog::RankUp;
    if (c == "BadgeAwardDialog") return RareDialog::BadgeAward;
    if (c == "UnlockDialog") return RareDialog::Unlock;
    if (c == "ZenIntroDialog") return RareDialog::ZenIntro;
    if (!c.empty() && IsKindOf(dialog, "EndLevelDialog")) return RareDialog::EndLevel;
    return RareDialog::None;
}

bool ReadRankUp(uintptr_t dialog, RankUpInfo* out) {
    *out = RankUpInfo();
    if (!IsClass(dialog, "RankUpDialog")) return false;
    // Draw: EAX = rankBar->board ? rankBar+0x138 : profile+0x1F0 (0x0078f152..
    // 0x0078f182), then FUN_00676470 clamps to names.size()-1 and copies
    // names[EAX] from gApp+0xD14.
    uintptr_t bar, board = 0;
    if (!ReadPtr(dialog + kRankUpRankBar, &bar)) return false;
    uint32_t b;
    if (!ReadU32(bar + kRankBarBoard, &b)) return false;
    board = b;
    int32_t rank;
    if (board != 0) {
        if (!ReadI32(bar + kRankBarDispRank, &rank)) return false;
        out->fromBoard = true;
    } else {
        uintptr_t app, profile;
        if (!ReadApp(&app) || !ReadPtr(app + kAppProfile, &profile) ||
            !ReadI32(profile + kProfileRank, &rank))
            return false;
    }
    int names = AppWStringVectorSize(kAppRankNames);
    if (names > 0 && rank >= names - 1) rank = names - 1;
    out->rankKnown = true;
    out->rank = rank;
    ReadAppWStringVector(kAppRankNames, rank, &out->rankName);
    return true;
}

bool ReadBadgeAward(uintptr_t dialog, BadgeAwardInfo* out) {
    *out = BadgeAwardInfo();
    if (!IsClass(dialog, "BadgeAwardDialog")) return false;
    uintptr_t widget, board, manager = 0;
    if (!ReadPtr(dialog + kAwardWidget, &widget)) return false;
    // The widget's award source is the board; fall back to the dialog's copy.
    if (!ReadPtr(widget + kBwAwardSource, &board) && !ReadPtr(dialog + kAwardBoard, &board))
        return false;
    ReadPtr(widget + kBwManager, &manager);
    uintptr_t begin, end;
    if (!ReadPtr(board + kBoardAwardBegin, &begin) || !ReadPtr(board + kBoardAwardEnd, &end) ||
        end < begin || (end - begin) / 4 > 20)
        return false;
    int count = static_cast<int>((end - begin) / 4);
    for (int i = 0; i < count; ++i) {
        AwardedBadge a;
        int32_t id;
        if (!ReadI32(begin + 4 * i, &id)) continue;
        a.id = id;
        ReadAppWStringVector(kAppBadgeNames, id, &a.name);
        uintptr_t badge;
        int32_t badgeId, level;
        if (manager && id >= 0 && id < 20 && ReadPtr(manager + 4 + 4 * id, &badge) &&
            ReadI32(badge + kBadgeId, &badgeId) && badgeId == id &&
            ReadI32(badge + kBadgeLevel, &level)) {
            a.levelKnown = true;
            a.level = level;
            // FUN_00778130(badge, 0): level name = string id `level` (1..4).
            if (level >= 1 && level <= 4) GameString(level, &a.levelName);
        }
        out->badges.push_back(a);
    }
    int32_t index;
    if (ReadI32(widget + kBwAwardIndex, &index)) {
        out->indexKnown = true;
        out->shownIndex = index;
        out->sequenceDone = index >= count;
    }
    return true;
}

bool ReadUnlockedModeName(uintptr_t dialog, std::wstring* out) {
    out->clear();
    if (!IsClass(dialog, "UnlockDialog")) return false;
    return ReadWString(dialog + kUnlockModeName, out) && !out->empty();
}

bool ReadZenIntroImageName(std::string* out) {
    out->clear();
    uintptr_t image;
    if (!ReadPtr(kZenIntroImageGlobal, &image)) return false;
    return GameImageResourceName(image, out);
}

bool ReadEndLevel(uintptr_t dialog, EndLevelInfo* out) {
    *out = EndLevelInfo();
    if (!IsKindOf(dialog, "EndLevelDialog")) return false;
    out->className = ClassNameOf(dialog);
    uintptr_t board;
    if (ReadPtr(dialog + kEndBoard, &board)) out->board = board;
    int32_t v;
    if (ReadI32(dialog + kEndScore, &v)) {
        out->scoreKnown = true;
        out->finalScore = v;
    }
    if (ReadI32(dialog + kEndLevel, &v)) {
        out->levelKnown = true;
        out->level = v;
    }
    // The ctor picks the button set from gApp+0xBD8 at construction time;
    // reading it now is only right while the quest menu has not changed.
    uintptr_t app;
    uint32_t qm;
    if (ReadApp(&app) && ReadU32(app + kAppQuestMenu, &qm)) out->questButtons = qm != 0;
    return true;
}

bool ReadEndLevelSpecialGems(uintptr_t dialog, std::vector<SpecialGemCount>* out) {
    out->clear();
    // Only these two draw frames call DrawSpecialGemDisplay (vtable +0x15C):
    // ClassicEndLevelDialog FUN_007839f0, ButterflyEndLevelDialog FUN_00784370.
    std::string c = ClassNameOf(dialog);
    if (c != "ClassicEndLevelDialog" && c != "ButterflyEndLevelDialog") return false;
    for (const GemSlot& s : kGemSlots) {
        SpecialGemCount g;
        uintptr_t image;
        if (ReadPtr(s.imageGlobal, &image)) GameImageResourceName(image, &g.imageName);
        int32_t n;
        if (ReadI32(dialog + s.countOffset, &n)) {
            g.countKnown = true;
            g.count = n;
        }
        out->push_back(g);
    }
    return true;
}

bool ReadQuestMenuAnnouncement(uintptr_t questMenu, std::wstring* out) {
    out->clear();
    if (!IsClass(questMenu, "QuestMenu")) return false;
    uintptr_t ann;
    uint32_t vt;
    if (!ReadPtr(questMenu + kQuestMenuAnnouncement, &ann) || !ReadU32(ann, &vt) ||
        vt != kVtAnnouncement)
        return false;
    return ReadWString(ann + kAnnouncementText, out) && !out->empty();
}

void DumpRareDialog(uintptr_t dialog) {
    switch (ClassifyRareDialog(dialog)) {
    case RareDialog::RankUp: {
        RankUpInfo r;
        if (!ReadRankUp(dialog, &r)) {
            Log("rare: RankUpDialog %08X unreadable", (unsigned)dialog);
            break;
        }
        Log("rare: RankUpDialog %08X rank=%d fromBoard=%d", (unsigned)dialog, r.rank,
            r.fromBoard ? 1 : 0);
        LogWide("rare:   rank name: ", r.rankName);
        break;
    }
    case RareDialog::BadgeAward: {
        BadgeAwardInfo b;
        if (!ReadBadgeAward(dialog, &b)) {
            Log("rare: BadgeAwardDialog %08X unreadable", (unsigned)dialog);
            break;
        }
        Log("rare: BadgeAwardDialog %08X badges=%d shown=%d done=%d", (unsigned)dialog,
            (int)b.badges.size(), b.shownIndex, b.sequenceDone ? 1 : 0);
        for (const AwardedBadge& a : b.badges) {
            Log("rare:   id=%d level=%d%s", a.id, a.level, a.levelKnown ? "" : " (unknown)");
            LogWide("rare:     name: ", a.name);
            LogWide("rare:     level name: ", a.levelName);
        }
        break;
    }
    case RareDialog::Unlock: {
        std::wstring m;
        Log("rare: UnlockDialog %08X", (unsigned)dialog);
        if (ReadUnlockedModeName(dialog, &m)) LogWide("rare:   mode: ", m);
        break;
    }
    case RareDialog::ZenIntro: {
        std::string img;
        Log("rare: ZenIntroDialog %08X image=%s", (unsigned)dialog,
            ReadZenIntroImageName(&img) ? img.c_str() : "(unknown)");
        break;
    }
    case RareDialog::EndLevel: {
        EndLevelInfo e;
        if (!ReadEndLevel(dialog, &e)) break;
        Log("rare: %s %08X board=%08X score=%d%s level=%d%s questButtons=%d",
            e.className.c_str(), (unsigned)dialog, (unsigned)e.board, e.finalScore,
            e.scoreKnown ? "" : "?", e.level, e.levelKnown ? "" : "?", e.questButtons ? 1 : 0);
        std::vector<SpecialGemCount> gems;
        if (ReadEndLevelSpecialGems(dialog, &gems))
            for (const SpecialGemCount& g : gems)
                Log("rare:   gem icon %s count=%d%s", g.imageName.c_str(), g.count,
                    g.countKnown ? "" : "?");
        break;
    }
    case RareDialog::None:
        break;
    }
}

}  // namespace dialog_text
