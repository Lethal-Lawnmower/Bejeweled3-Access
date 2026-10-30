#include "screen_names.h"

#include <cstring>

#include "game_memory.h"
#include "game_strings.h"
#include "sexy.h"

// Evidence for every constant: docs/re/screen-names-and-keys.md. All
// static (Ghidra, 2026-10-30) unless marked otherwise.

namespace {

// --- Board parameters --------------------------------------------------
//
// Every Board keeps a std::map<std::string, std::wstring> of its mode
// parameters at +0xC8 (head node at +0xE0). Board constructors put the mode
// name under "Title" (ClassicBoard 0x0073B390, ZenBoard 0x007126F0,
// SpeedBoard 0x00718090: params["Title"] = a compiled wide literal); quest
// and secret-mode boards get the parameters of their entry in quest.cfg /
// secret.cfg, whose `Quest "<name>"` line the parser FUN_00791C80 stores as
// "Title" and, unless the entry sets its own, also as "TitleText". The game
// reads "Title" back through Board vtable+0x27C (0x00741660) for its
// GameStart/GameOver/GameLoaded event log, draws "TitleText" on quest boards
// (0x006D3FB0) and uses it as the QuestHelpDialog header (0x006CCFD0).
constexpr int kBoardParams = 0xC8;

// Same std::map<std::string, std::wstring> instantiation as the app's named
// string table (both use find FUN_00409830), so the same node layout as
// game_strings.cpp: head pointer at map+0x18, root = head->_Parent (+4),
// key std::string at node+0xC, value std::wstring at node+0x28, _Isnil
// byte at node+0x45.
constexpr int kMapHead = 0x18;
constexpr int kNodeLeft = 0x0;
constexpr int kNodeParent = 0x4;
constexpr int kNodeRight = 0x8;
constexpr int kNodeKey = 0xC;
constexpr int kNodeValue = 0x28;
constexpr int kNodeIsNil = 0x45;
constexpr int kMaxDepth = 64;

// MSVC 2008 std::string: +4 inline buffer (16 chars) or heap pointer when
// capacity > 15, +0x14 size, +0x18 capacity.
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

// Value of `key` in a std::map<std::string, std::wstring> located at map.
bool MapStringValue(uintptr_t map, const char* key, std::wstring* out) {
    size_t keyLen = strlen(key);
    uint32_t head, node;
    if (!ReadU32(map + kMapHead, &head) || head == 0) return false;
    if (!ReadU32(head + kNodeParent, &node)) return false;
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        uint8_t nil;
        if (node == 0 || node == head || !ReadU8(node + kNodeIsNil, &nil) || nil) return false;
        std::string nodeKey;
        if (!ReadNarrow(node + kNodeKey, &nodeKey)) return false;
        size_t n = nodeKey.size() < keyLen ? nodeKey.size() : keyLen;
        int c = n ? memcmp(nodeKey.data(), key, n) : 0;
        if (c == 0) c = nodeKey.size() < keyLen ? -1 : (nodeKey.size() > keyLen ? 1 : 0);
        if (c == 0) return ReadWString(node + kNodeValue, out);
        uint32_t next;
        if (!ReadU32(node + (c > 0 ? kNodeLeft : kNodeRight), &next)) return false;
        node = next;
    }
    return false;
}

bool NonEmpty(const std::wstring& s) {
    for (wchar_t c : s)
        if (c != L' ' && c != L'\t' && c != L'\r' && c != L'\n') return true;
    return false;
}

// --- Screens named by string ids the game's own code uses -----------------
//
// The string ids are those the game's constructors / Draw functions pass to
// its string lookup (docs/re/menus.md, default.xml comments); the text is
// read from the running game's table.
//   PlayMenu       287: label of MainMenu orb 2, whose ButtonDepress
//                       (0x006A2DE0) opens the play menu.
//   RecordsScreen  286: label of MainMenu orb 1 (opens the records screen).
//   QuestMenu      347: label of PlayMenu orb 3, whose start function
//                       FUN_00773B70 creates the QuestMenu (ctor 0x00693760).
//   SecretMenu     477: heading drawn by SecretMenu::Draw (0x00677580).
//   ZenOptionsDialog 513 (hypothesis): header of the hint tooltip that
//                       ZenBoard::Update (0x007116C0) shows on the Zen board;
//                       that it points at the options button is not proven.
struct ScreenStringRule {
    const char* className;
    int stringId;
    ScreenNameSource source;
};
constexpr ScreenStringRule kScreenStrings[] = {
    {"PlayMenu", 287, ScreenNameSource::OpeningButtonLabel},
    {"RecordsScreen", 286, ScreenNameSource::OpeningButtonLabel},
    {"QuestMenu", 347, ScreenNameSource::OpeningButtonLabel},
    {"SecretMenu", 477, ScreenNameSource::DrawnHeading},
    {"ZenOptionsDialog", 513, ScreenNameSource::HintTooltip},
};

// RecordsScreen tab switch FUN_0069C760: for i in 0..2, page [this+0xC4+4i]
// gets Widget::SetVisible (vtable+0x7C = 0x0046A780, writes +0x64) = selected,
// and tab button [this+0xD0+4i] gets Widget::SetDisabled (vtable+0x94 =
// 0x0046AAC0, writes +0x66) = selected. Tab buttons are Bej3DialogButtons
// created by FUN_0069CD80 with labels 472-474 (mLabel +0xC0).
constexpr int kRecordsPages = 0xC4;
constexpr int kRecordsTabButtons = 0xD0;
constexpr int kRecordsTabCount = 3;

// --- Key handling -----------------------------------------------------------
//
// WidgetManager::KeyDown (0x00585EB0) records mKeyDown[key] at +0x120 and
// calls only the focus widget (+0xA0) through vtable+0xC0 (KeyDown);
// KeyChar (0x00585E60) likewise through vtable+0xBC.
constexpr int kVtKeyDown = 0xC0;

// KeyDown implementations found in the vtables (slot 48):
constexpr uintptr_t kKeyDownWidget = 0x0046AB70;     // Tab -> mTabNext/mTabPrev
constexpr uintptr_t kKeyDownBoard = 0x007472B0;      // every Board subclass
constexpr uintptr_t kKeyDownBej3Dialog = 0x00769F00; // Escape if +0x201
constexpr uintptr_t kKeyDownOptions = 0x00673A10;    // Escape -> button 0
constexpr uintptr_t kKeyDownEndLevel = 0x006A5F20;   // Escape -> button 0
constexpr uintptr_t kKeyDownEdit = 0x0059F9A0;       // editing keys

constexpr int kWidgetTabPrev = 0x98;
constexpr int kWidgetTabNext = 0x9C;
constexpr int kBej3DialogEscapeEnabled = 0x201;  // set only by DoSavedGameCheck
constexpr int kBoardKeysBlocked = 0x1410;        // KeyDown returns at once if set
constexpr int kBoardEscapeBlocked = 0x2978;      // Escape ignored if set

bool KeyDownOf(uintptr_t widget, uintptr_t* fn) {
    uint32_t vtable, f;
    if (!widget || !ReadU32(widget, &vtable) || !ReadU32(vtable + kVtKeyDown, &f)) return false;
    *fn = f;
    return true;
}

bool ByteSet(uintptr_t addr, bool* set) {
    uint8_t b;
    if (!ReadU8(addr, &b)) return false;
    *set = b != 0;
    return true;
}

bool Answer(const char** what, const char* text, bool result) {
    if (what) *what = text;
    return result;
}

}  // namespace

const char* ScreenNameSourceName(ScreenNameSource source) {
    switch (source) {
        case ScreenNameSource::None: return "none";
        case ScreenNameSource::BoardTitleText: return "board TitleText";
        case ScreenNameSource::BoardTitle: return "board Title";
        case ScreenNameSource::DialogHeader: return "dialog header";
        case ScreenNameSource::DrawnHeading: return "drawn heading";
        case ScreenNameSource::OpeningButtonLabel: return "opening button label";
        case ScreenNameSource::HintTooltip: return "hint tooltip (hypothesis)";
    }
    return "?";
}

bool ScreenNameWithSource(uintptr_t screen, bool allowHypotheses, std::wstring* out,
                          ScreenNameSource* source) {
    if (source) *source = ScreenNameSource::None;
    if (!screen || !out) return false;
    std::wstring text;

    if (IsKindOf(screen, "Board")) {
        if (MapStringValue(screen + kBoardParams, "TitleText", &text) && NonEmpty(text)) {
            if (source) *source = ScreenNameSource::BoardTitleText;
            *out = text;
            return true;
        }
        if (MapStringValue(screen + kBoardParams, "Title", &text) && NonEmpty(text)) {
            if (source) *source = ScreenNameSource::BoardTitle;
            *out = text;
            return true;
        }
        return false;
    }

    // Framework Dialog and every Bej3Dialog subclass (OptionsDialog,
    // HelpDialog, UserSelectionDialog, ...): the header passed to the
    // Dialog constructor FUN_00583870. Empty for header-less dialogs
    // (HintDialog, EndLevelDialog, ...).
    if (IsKindOf(screen, "Dialog")) {
        if (ReadWString(screen + sexy::kDialogHeader, &text) && NonEmpty(text)) {
            if (source) *source = ScreenNameSource::DialogHeader;
            *out = text;
            return true;
        }
        return false;
    }

    std::string cls = ClassNameOf(screen);
    for (const ScreenStringRule& rule : kScreenStrings) {
        if (cls != rule.className) continue;
        if (rule.source == ScreenNameSource::HintTooltip && !allowHypotheses) return false;
        if (!GameString(rule.stringId, &text) || !NonEmpty(text)) return false;
        if (source) *source = rule.source;
        *out = text;
        return true;
    }
    return false;
}

bool ScreenName(uintptr_t screen, std::wstring* out) {
    return ScreenNameWithSource(screen, false, out, nullptr);
}

bool ScreenSection(uintptr_t screen, std::wstring* out) {
    if (!screen || !out || ClassNameOf(screen) != "RecordsScreen") return false;
    for (int i = 0; i < kRecordsTabCount; ++i) {
        uint32_t page, tab;
        bool visible;
        if (!ReadU32(screen + kRecordsPages + 4 * i, &page) || page == 0) return false;
        if (!ByteSet(page + sexy::kVisible, &visible)) return false;
        if (!visible) continue;
        if (!ReadU32(screen + kRecordsTabButtons + 4 * i, &tab) || tab == 0) return false;
        if (!IsKindOf(tab, "ButtonWidget")) return false;
        std::wstring label;
        if (!ReadWString(tab + sexy::kButtonLabel, &label) || !NonEmpty(label)) return false;
        *out = label;
        return true;
    }
    return false;
}

bool GameHandlesKey(uintptr_t focus, int vk, const char** what) {
    uintptr_t fn;
    if (!KeyDownOf(focus, &fn)) return Answer(what, "no focus widget", false);

    if (fn == kKeyDownBoard) {
        bool blocked;
        if (!ByteSet(focus + kBoardKeysBlocked, &blocked)) return Answer(what, "unreadable", false);
        if (blocked) return Answer(what, "board keys blocked (+0x1410)", false);
        switch (vk) {
            case 0x1B: {
                bool escBlocked;
                if (ByteSet(focus + kBoardEscapeBlocked, &escBlocked) && escBlocked)
                    return Answer(what, "board Escape blocked (+0x2978)", false);
                return Answer(what, "board Escape: presses the menu button (id 1)", true);
            }
            case 0x20: return Answer(what, "board Space: select or deselect the gem at the cursor", true);
            case 0x25: case 0x26: case 0x27: case 0x28:
                return Answer(what, "board arrow: move the cursor, or swap the selected gem", true);
            case 'W': case 'A': case 'S': case 'D':
                return Answer(what, "board WASD: swap the gem at the cursor", true);
        }
        return Answer(what, "board ignores this key", false);
    }
    if (fn == kKeyDownOptions)
        return vk == 0x1B ? Answer(what, "Options Escape: BACK (button 0)", true)
                          : Answer(what, "Options ignores this key", false);
    if (fn == kKeyDownEndLevel)
        return vk == 0x1B ? Answer(what, "end-level Escape: button 0 (main menu)", true)
                          : Answer(what, "end-level dialog ignores this key", false);
    if (fn == kKeyDownBej3Dialog) {
        if (vk != 0x1B) return Answer(what, "dialog ignores this key", false);
        bool enabled;
        if (!ByteSet(focus + kBej3DialogEscapeEnabled, &enabled)) return Answer(what, "unreadable", false);
        return enabled ? Answer(what, "dialog Escape: presses its No (else Yes) button", true)
                       : Answer(what, "dialog Escape disabled", false);
    }
    if (fn == kKeyDownEdit) {
        // EditWidget::KeyDown passes keys outside A..Y (letters arrive as
        // characters) to ProcessKey 0x0059EB20, which acts on Left, Right,
        // Home, End, Backspace, Delete (Shift extends the selection) and
        // Enter (EditListener::EditWidgetText; WelcomeDialog/NewUserDialog
        // turn that into ButtonDepress(1000)). Copy/cut/paste/undo come in as
        // control characters through KeyChar, not here.
        switch (vk) {
            case 0x08: case 0x0D: case 0x23: case 0x24: case 0x25: case 0x27: case 0x2E:
                return Answer(what, vk == 0x0D ? "edit Enter: submits (EditWidgetText)" : "edit caret/delete key", true);
        }
        return Answer(what, "edit field ignores this key", false);
    }
    if (fn == kKeyDownWidget) {
        if (vk != 0x09) return Answer(what, "widget ignores this key", false);
        uint32_t next = 0, prev = 0;
        ReadU32(focus + kWidgetTabNext, &next);
        ReadU32(focus + kWidgetTabPrev, &prev);
        return (next || prev) ? Answer(what, "Tab moves game focus via tab links", true)
                              : Answer(what, "Tab without tab links", false);
    }
    return Answer(what, "unknown KeyDown", false);
}

bool GameAppHandlesKey(int vk, bool alt, bool ctrl, const char** what) {
    // F1 is consumed by the driver's DebugKeyDown before any widget.
    if (vk == 0x70) return Answer(what, "F1: consumed by the framework", true);
    if (vk == 0x0D && alt) return Answer(what, "Alt+Enter: toggles fullscreen/windowed when allowed", true);
    if (vk == 'D' && alt && ctrl) return Answer(what, "Ctrl+Alt+D: toggles framework debug keys", true);
    // Further framework keys (F2, F3, F8, F11, Ctrl+Alt+W/B/P) only act when
    // debug keys are on: app+0x6DC (hypothesis: driver->mApp == gApp).
    uint32_t app;
    uint8_t debugKeys = 0;
    if (ReadU32(sexy::kGAppPtr, &app) && app && ReadU8(app + 0x6DC, &debugKeys) && debugKeys &&
        (vk == 0x71 || vk == 0x72 || vk == 0x77 || vk == 0x7A || (alt && ctrl && vk == 'W')))
        return Answer(what, "framework debug key (debug keys are on)", true);
    return Answer(what, "not a framework key", false);
}
