#include "hover_text.h"

#include <windows.h>

#include <cstring>

#include "game_memory.h"
#include "log.h"
#include "sexy.h"

// Every constant below is static evidence from docs/re/hover-text.md
// (Ghidra copy private/ghidra-hover). Section names in that document are
// given next to each group.

namespace hover_text {
namespace {

// ---- shared ---------------------------------------------------------------

constexpr uintptr_t kTextBegin = 0x00401000;  // .text (PE section table)
constexpr uintptr_t kTextEnd = 0x007BE575;
constexpr uintptr_t kRdataBegin = 0x007BF000;  // .rdata
constexpr uintptr_t kRdataEnd = 0x008BC40A;

constexpr int kWStringSize = 0x1C;  // MSVC 2008 std::wstring (sexy.h)

bool App(uintptr_t* app) {
    uint32_t p;
    if (!ReadU32(sexy::kGAppPtr, &p) || p == 0) return false;
    *app = p;
    return true;
}

// Element `index` of an MSVC vector whose begin/end pointers are at
// vec+0 / vec+4, with elements of `elementSize` bytes.
bool VectorElement(uintptr_t beginField, int elementSize, int index, uintptr_t* element,
                   int* count) {
    uint32_t begin, end;
    if (!ReadU32(beginField, &begin) || !ReadU32(beginField + 4, &end)) return false;
    // An empty MSVC vector has begin == end == 0.
    if ((begin == 0 && end != 0) || end < begin || (end - begin) % elementSize != 0) return false;
    int n = static_cast<int>((end - begin) / elementSize);
    if (n > 100000) return false;
    if (count) *count = n;
    if (index < 0) return true;  // count only
    if (index >= n) return false;
    *element = begin + static_cast<uintptr_t>(index) * elementSize;
    return true;
}

// ---- Play menu (hover-text.md "Play menu") ---------------------------------

constexpr int kPlayMenuDescription = 0x1C8;  // std::wstring, ButtonMouseEnter 0068ea89
constexpr int kPlayMenuLockedFlag = 0x1E4;   // byte, 0068ea9b; Draw draws 352 when set
constexpr int kAppProfile = 0xB80;           // [gApp+0xB80] profile
constexpr int kProfileModeUnlocked = 0x180;  // +0x180..0x183 for buttons 4..7

// ---- Quest menu (hover-text.md "Quest orbs") -------------------------------

constexpr int kAppQuestData = 0xBCC;       // [gApp+0xBCC] quest set
constexpr int kQuestVectorBegin = 0x94;    // begin +0x94, end +0x98
constexpr int kQuestEntrySize = 0x3C;
constexpr int kQuestName = 0x00;           // std::wstring
constexpr int kQuestProperties = 0x1C;     // std::map<std::string, std::wstring>
constexpr int kQuestMenuPage = 0x14C;      // int page 0..4
constexpr int kQuestMenuOrbs = 0x4CC;      // QuestMenuBtn*[8], not completed
constexpr int kQuestMenuOrbsDone = 0x4EC;  // QuestMenuBtn*[8], completed
constexpr int kOrbsPerPage = 8;

// std::map<std::string, std::wstring> layout (same type as the named string
// table, docs/re/menus.md; FUN_00403180 returns node+0x28 for the quest map).
constexpr int kMapHead = 0x18;
constexpr int kNodeLeft = 0x0;
constexpr int kNodeParent = 0x4;
constexpr int kNodeRight = 0x8;
constexpr int kNodeKey = 0xC;      // std::string
constexpr int kNodeValue = 0x28;   // std::wstring
constexpr int kNodeIsNil = 0x45;
constexpr int kMaxDepth = 64;

// ---- Badges (hover-text.md "Badges") ---------------------------------------

constexpr int kBadgeId = 0x04;
constexpr int kBadgeLevel = 0x08;
constexpr int kBadgeSlotTitleSuffix = 0x10;  // vtable slot 4
constexpr int kBadgeSlotNextLevel = 0x18;    // slot 6
constexpr int kBadgeSlotMaxLevel = 0x1C;     // slot 7
constexpr int kAppBadgeNames = 0xD2C;        // vector<std::wstring> begin/end
constexpr int kAppBadgeLevels = 0xD44;       // vector of 0x18-byte entries
constexpr int kBadgeLevelEntrySize = 0x18;
constexpr int kBadgeLevelThresholds = 0x0C;  // int vector begin/end in the entry
constexpr uintptr_t kGameFree = 0x0064DBCF;  // operator delete -> _free (cdecl)

// ---- Zen (hover-text.md "Zen section descriptions") ------------------------

struct LiteralSite {
    uintptr_t lengthPush;   // PUSH imm8 (6A) or PUSH imm32 (68): length
    uintptr_t addressPush;  // PUSH imm32 (68): wchar_t literal
};
// FUN_0042ab20 builds wstring desc[4] in section-id order and indexes it by
// [container+0xC4] at 0042b1ad..0042b1c7.
constexpr LiteralSite kZenDescriptionSites[4] = {
    {0x0042B06E, 0x0042B075},
    {0x0042B0A8, 0x0042B0AC},
    {0x0042B0E1, 0x0042B0EE},
    {0x0042B119, 0x0042B11D},
};

// ---- Tips (hover-text.md "Main menu tip") ----------------------------------

constexpr int kAppTipsBegin = 0xCFC;  // vector<std::wstring> begin, end +0xD00
constexpr int kAppTipIndex = 0xD78;   // int TipIdx

// ---- Dialog buttons / TooltipManager ---------------------------------------

constexpr int kDialogButtonTipTitle = 0x184;  // std::wstring
constexpr int kDialogButtonTipBody = 0x1A0;   // std::wstring, shown when non-empty
constexpr int kAppTooltipManager = 0xBF0;
constexpr int kTooltipVectorBegin = 0xD4;     // begin +0xD4, end +0xD8
constexpr int kTooltipEntrySize = 0x78;
constexpr int kTooltipTitle = 0x1C;
constexpr int kTooltipBody = 0x38;
constexpr int kTooltipRefreshed = 0x5C;

// ---- std::map<std::string, std::wstring> lookup ----------------------------

bool ReadNarrow(uintptr_t addr, std::string* out) {
    uint32_t size, cap;
    if (!ReadU32(addr + 0x14, &size) || !ReadU32(addr + 0x18, &cap)) return false;
    if (size > 256 || cap < size || cap < 15) return false;
    uintptr_t data = addr + 4;
    if (cap > 15) {
        uint32_t heap;
        if (!ReadU32(data, &heap)) return false;
        data = heap;
    }
    out->resize(size);
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t c;
        if (!ReadU8(data + i, &c)) return false;
        (*out)[i] = static_cast<char>(c);
    }
    return true;
}

// Read-only find (the game's own FUN_00403180 is operator[], which would
// insert a missing key).
bool FindStringKey(uintptr_t map, const char* key, std::wstring* out) {
    uint32_t head, node;
    if (!ReadU32(map + kMapHead, &head) || head == 0) return false;
    if (!ReadU32(head + kNodeParent, &node)) return false;
    const std::string wanted(key);
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        uint8_t nil;
        if (node == 0 || !ReadU8(node + kNodeIsNil, &nil)) return false;
        if (nil || node == head) return false;  // not present
        std::string nodeKey;
        if (!ReadNarrow(node + kNodeKey, &nodeKey)) return false;
        int c = nodeKey.compare(wanted);
        if (c == 0) return ReadWString(node + kNodeValue, out);
        uint32_t next;
        if (!ReadU32(node + (c > 0 ? kNodeLeft : kNodeRight), &next)) return false;
        node = next;
    }
    return false;
}

// ---- calling a Badge text slot ---------------------------------------------

// A std::wstring the game constructs in place (hidden return pointer).
struct GameWString {
    uint32_t proxy;     // +0x00 (not touched by the slot functions)
    uint32_t buffer[4]; // +0x04 inline wchar_t[8] or heap pointer
    uint32_t size;      // +0x14
    uint32_t capacity;  // +0x18
};
static_assert(sizeof(GameWString) == kWStringSize, "MSVC 2008 wstring layout");

// The slots are __thiscall(Badge* this, std::wstring* out) with RET 4. A
// __fastcall pointer with an unused EDX argument has the same register and
// stack contract (ECX = this, one stack argument popped by the callee).
using WStringSlot = void*(__fastcall*)(uintptr_t self, uintptr_t unusedEdx, GameWString* out);
using GameFreeFn = void(__cdecl*)(void* block);

// No C++ objects with destructors in here (SEH restriction).
bool GuardedSlotCall(uintptr_t fn, uintptr_t self, GameWString* out, DWORD* code) {
    __try {
        reinterpret_cast<WStringSlot>(fn)(self, 0, out);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GuardedFree(void* block, DWORD* code) {
    __try {
        reinterpret_cast<GameFreeFn>(kGameFree)(block);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Calls vtable slot `slotOffset` of `object`, copies the returned text and
// releases the game's heap buffer the way the game's callers do
// (`CMP cap,8; JC skip; PUSH buffer; CALL 0x0064DBCF`, e.g. 007784c2).
bool CallWStringSlot(uintptr_t object, int slotOffset, std::wstring* out) {
    uint32_t vtable, fn;
    if (!ReadU32(object, &vtable) || !ReadU32(vtable + slotOffset, &fn)) return false;
    if (fn < kTextBegin || fn >= kTextEnd) {
        Log("hover_text: slot +0x%X of %08X points outside .text (%08X)", slotOffset,
            static_cast<unsigned>(object), fn);
        return false;
    }
    GameWString s = {};
    s.capacity = 7;  // an empty string, should the callee read before writing
    DWORD code = 0;
    if (!GuardedSlotCall(fn, object, &s, &code)) {
        Log("hover_text: game function %08X faulted (0x%08lX) for %08X", fn, code,
            static_cast<unsigned>(object));
        return false;  // a partly built heap buffer is leaked, not freed blindly
    }
    bool ok = ReadWString(reinterpret_cast<uintptr_t>(&s), out);
    if (s.capacity >= 8 && s.buffer[0] != 0) {
        if (!GuardedFree(reinterpret_cast<void*>(static_cast<uintptr_t>(s.buffer[0])), &code))
            Log("hover_text: game free faulted (0x%08lX)", code);
    }
    if (!ok) Log("hover_text: unreadable string from %08X", fn);
    return ok;
}

}  // namespace

// ---- Play menu ---------------------------------------------------------------

bool PlayMenuDescription(uintptr_t playMenu, std::wstring* text, bool* locked) {
    if (ClassNameOf(playMenu) != "PlayMenu") return false;
    if (!ReadWString(playMenu + kPlayMenuDescription, text) || text->empty()) return false;
    if (locked) {
        uint8_t flag;
        if (!ReadU8(playMenu + kPlayMenuLockedFlag, &flag)) return false;
        *locked = flag != 0;
    }
    return true;
}

bool PlayModeUnlocked(int modeButtonId, bool* unlocked) {
    if (modeButtonId >= 0 && modeButtonId <= 3) {
        *unlocked = true;
        return true;
    }
    if (modeButtonId < 4 || modeButtonId > 7) return false;
    uintptr_t app;
    uint32_t profile;
    uint8_t flag;
    if (!App(&app) || !ReadU32(app + kAppProfile, &profile) || profile == 0) return false;
    if (!ReadU8(profile + kProfileModeUnlocked + (modeButtonId - 4), &flag)) return false;
    *unlocked = flag != 0;
    return true;
}

// ---- Quests ------------------------------------------------------------------

int QuestCount() {
    uintptr_t app, element;
    uint32_t quests;
    int count = -1;
    if (!App(&app) || !ReadU32(app + kAppQuestData, &quests) || quests == 0) return -1;
    if (!VectorElement(quests + kQuestVectorBegin, kQuestEntrySize, -1, &element, &count))
        return -1;
    return count;
}

bool QuestText(int questIndex, std::wstring* name, std::wstring* description) {
    uintptr_t app, entry;
    uint32_t quests;
    if (!App(&app) || !ReadU32(app + kAppQuestData, &quests) || quests == 0) return false;
    if (!VectorElement(quests + kQuestVectorBegin, kQuestEntrySize, questIndex, &entry, nullptr))
        return false;
    if (name && !ReadWString(entry + kQuestName, name)) return false;
    if (description && !FindStringKey(entry + kQuestProperties, "Description", description))
        return false;
    return true;
}

bool QuestIndexForOrb(uintptr_t questMenu, uintptr_t orbButton, int* questIndex) {
    if (ClassNameOf(questMenu) != "QuestMenu" || orbButton == 0) return false;
    int32_t id, page;
    if (!ReadI32(orbButton + sexy::kButtonId, &id) || !ReadI32(questMenu + kQuestMenuPage, &page))
        return false;
    int slot = id >= 2000 ? id - 2000 : id - 1000;  // as QuestMenu::ButtonDepress
    if (slot < 0 || slot >= kOrbsPerPage || page < 0) return false;
    uint32_t a, b;
    if (!ReadU32(questMenu + kQuestMenuOrbs + 4 * slot, &a) ||
        !ReadU32(questMenu + kQuestMenuOrbsDone + 4 * slot, &b))
        return false;
    if (a != orbButton && b != orbButton) return false;  // not this menu's orb
    *questIndex = slot + kOrbsPerPage * page;
    return true;
}

bool QuestOrbText(uintptr_t questMenu, uintptr_t orbButton, std::wstring* name,
                  std::wstring* description) {
    int index;
    return QuestIndexForOrb(questMenu, orbButton, &index) && QuestText(index, name, description);
}

// ---- Badges ------------------------------------------------------------------

bool BadgeTooltip(uintptr_t badge, std::wstring* title, std::wstring* body) {
    if (!IsKindOf(badge, "Badge")) return false;
    uintptr_t app, nameAddr, levelEntry;
    int32_t id, level;
    if (!App(&app) || !ReadI32(badge + kBadgeId, &id) || !ReadI32(badge + kBadgeLevel, &level))
        return false;
    if (!VectorElement(app + kAppBadgeNames, kWStringSize, id, &nameAddr, nullptr)) return false;
    if (!VectorElement(app + kAppBadgeLevels, kBadgeLevelEntrySize, id, &levelEntry, nullptr))
        return false;
    int levelCount = 0;
    uintptr_t unused;
    if (!VectorElement(levelEntry + kBadgeLevelThresholds, 4, -1, &unused, &levelCount))
        return false;

    if (title) {
        std::wstring name, suffix;
        if (!ReadWString(nameAddr, &name)) return false;
        if (!CallWStringSlot(badge, kBadgeSlotTitleSuffix, &suffix)) return false;
        *title = name + suffix;
    }
    if (body) {
        int slot = level < levelCount ? kBadgeSlotNextLevel : kBadgeSlotMaxLevel;
        if (!CallWStringSlot(badge, slot, body)) return false;
    }
    return true;
}

// ---- Zen ---------------------------------------------------------------------

bool ZenSectionDescription(int sectionId, std::wstring* out) {
    if (sectionId < 0 || sectionId > 3) return false;
    const LiteralSite& site = kZenDescriptionSites[sectionId];
    uint8_t lenOp, addrOp;
    uint32_t length = 0, literal;
    if (!ReadU8(site.lengthPush, &lenOp) || !ReadU8(site.addressPush, &addrOp)) return false;
    if (lenOp == 0x6A) {
        uint8_t n;
        if (!ReadU8(site.lengthPush + 1, &n)) return false;
        length = n;
    } else if (lenOp != 0x68 || !ReadU32(site.lengthPush + 1, &length)) {
        lenOp = 0;
    }
    if (lenOp == 0 || addrOp != 0x68 || !ReadU32(site.addressPush + 1, &literal)) {
        Log("hover_text: Zen description site %d does not match this executable", sectionId);
        return false;
    }
    if (length == 0 || length > 1024 || literal < kRdataBegin ||
        literal + 2 * (length + 1) > kRdataEnd)
        return false;
    out->resize(length);
    for (uint32_t i = 0; i <= length; ++i) {
        uint8_t lo, hi;
        if (!ReadU8(literal + 2 * i, &lo) || !ReadU8(literal + 2 * i + 1, &hi)) return false;
        wchar_t c = static_cast<wchar_t>(lo | (hi << 8));
        if (i == length) return c == L'\0';  // the length must end at the terminator
        if (c == L'\0') return false;
        (*out)[i] = c;
    }
    return false;
}

// ---- Tips --------------------------------------------------------------------

bool CurrentTip(std::wstring* out, int* index, int* count) {
    uintptr_t app, unused, element;
    int32_t tipIdx;
    int n = 0;
    if (!App(&app) || !ReadI32(app + kAppTipIndex, &tipIdx)) return false;
    if (!VectorElement(app + kAppTipsBegin, kWStringSize, -1, &unused, &n)) return false;
    if (count) *count = n;
    if (n == 0 || tipIdx <= 0) return false;  // the game draws no tip then
    int i = static_cast<int>(static_cast<uint32_t>(tipIdx - 1) % static_cast<uint32_t>(n));
    if (!VectorElement(app + kAppTipsBegin, kWStringSize, i, &element, nullptr)) return false;
    if (index) *index = i;
    return ReadWString(element, out) && !out->empty();
}

// ---- Bej3DialogButton --------------------------------------------------------

bool DialogButtonTooltip(uintptr_t button, std::wstring* title, std::wstring* body) {
    if (ClassNameOf(button) != "Bej3DialogButton") return false;
    std::wstring b;
    if (!ReadWString(button + kDialogButtonTipBody, &b) || b.empty()) return false;
    if (title && !ReadWString(button + kDialogButtonTipTitle, title)) return false;
    if (body) *body = b;
    return true;
}

// ---- TooltipManager ----------------------------------------------------------

bool ReadActiveTooltips(std::vector<ActiveTooltip>* out) {
    out->clear();
    uintptr_t app, unused;
    uint32_t manager;
    int n = 0;
    if (!App(&app) || !ReadU32(app + kAppTooltipManager, &manager) || manager == 0) return false;
    if (ClassNameOf(manager) != "TooltipManager") return false;
    if (!VectorElement(manager + kTooltipVectorBegin, kTooltipEntrySize, -1, &unused, &n))
        return false;
    for (int i = 0; i < n && i < 32; ++i) {
        uintptr_t e;
        ActiveTooltip t;
        int32_t x, y;
        uint8_t refreshed;
        if (!VectorElement(manager + kTooltipVectorBegin, kTooltipEntrySize, i, &e, nullptr) ||
            !ReadI32(e, &x) || !ReadI32(e + 4, &y) || !ReadWString(e + kTooltipTitle, &t.title) ||
            !ReadWString(e + kTooltipBody, &t.body) || !ReadU8(e + kTooltipRefreshed, &refreshed))
            return false;
        t.x = x;
        t.y = y;
        t.refreshed = refreshed != 0;
        out->push_back(t);
    }
    return true;
}

}  // namespace hover_text
