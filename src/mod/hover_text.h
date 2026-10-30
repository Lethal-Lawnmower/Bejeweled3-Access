#pragma once

// Text the game only shows while the mouse is over something (mode
// descriptions, quest orb and badge tooltips, Zen section descriptions, the
// main-menu tip line), obtained without a mouse, coordinates, window or
// screen mode: by reading the game's own data or calling the game's own
// text-building functions.
//
// Evidence for every address, offset and calling convention:
// docs/re/hover-text.md (static analysis of the Steam Bejeweled3.exe,
// SHA-256 d5516244...88c3). Not yet runtime-verified.
//
// Rules for callers:
// - Call on the game's window thread, outside the game's drawing (the mod's
//   WM_TIMER / WM_KEYDOWN handlers are fine). BadgeTooltip runs game code.
// - Text is returned raw: colour codes such as ^FFAACC^ and ^oldclr^, "\n"
//   and doubled spaces are left in place; clean it (CleanGameText) before
//   speaking. A false return means unknown, never "no text": do not
//   substitute a guess.
// - Nothing here copies game text into the mod; every string is read from
//   the running game (memory or its own code's literals).

#include <cstdint>
#include <string>
#include <vector>

namespace hover_text {

// ---- Play menu (PlayMenu, vtable 0x0086E38C) -------------------------------
// The game builds a mode's description only inside its ButtonMouseEnter
// listener (FUN_0068e0e0) and stores it in PlayMenu+0x1C8; PlayMenu::Draw
// draws that field, preceded by string 352 when the flag at +0x1E4 is set
// (a locked mode). To get the description of mode button `id` (0..7) without
// a mouse, first call game_actions::HoverWidget(modeButton, true), then read
// it here. Raw text, already formatted by the game (strings 365-376).
// `locked` (optional) receives the +0x1E4 flag. False if the object is not a
// PlayMenu, a read failed, or the field is empty.
bool PlayMenuDescription(uintptr_t playMenu, std::wstring* text, bool* locked);

// Unlock state the game tests for mode buttons 4..7 (profile [gApp+0xB80]
// +0x180+(id-4)); modes 0..3 are always unlocked (no test in the code).
bool PlayModeUnlocked(int modeButtonId, bool* unlocked);

// ---- Quest menu (QuestMenu 0x0086DF4C, QuestMenuBtn 0x0086E4F4) ------------
// Quest data loaded from the quest config: vector of 0x3C-byte entries at
// [gApp+0xBCC]+0x94; entry +0 = quest name (std::wstring), entry +0x1C =
// std::map<std::string, std::wstring> of the quest's keys. The orb tooltip
// (QuestMenu::Update) is title = name, body = "^FFAACC^" + map["Description"].
int QuestCount();  // -1 if unreadable
bool QuestText(int questIndex, std::wstring* name, std::wstring* description);

// Quest index of an orb button: slot = mId - 1000 (or - 2000 for the
// "completed" variant), index = slot + 8 * page ([QuestMenu+0x14C]). The
// button must be one of the eight pairs the QuestMenu keeps at +0x4CC/+0x4EC.
bool QuestIndexForOrb(uintptr_t questMenu, uintptr_t orbButton, int* questIndex);

// Both of the above: the exact title and body the game puts in the orb's
// tooltip (body without the ^FFAACC^ colour prefix the game adds).
bool QuestOrbText(uintptr_t questMenu, uintptr_t orbButton, std::wstring* name,
                  std::wstring* description);

// ---- Badges (Badge objects, BadgeWidget manager+4+4*id) --------------------
// Tooltip title and body exactly as BadgeWidget::Update builds them:
// title = gApp badge-name table [id] + Badge vtable slot 4 (progress such as
// "  (0/25)", empty at max level); body = slot 6 (next-level text) when
// level < level count, else slot 7 (max-level text). Calls the game's own
// virtual functions (thiscall, std::wstring returned through a hidden
// pointer) inside an SEH guard and frees the game's string with the game's
// own free (0x0064DBCF). `badge` is a Badge* (e.g. BadgeCell::badge from
// badges.h). Body has no ^FFAACC^ prefix (the game adds it when drawing).
bool BadgeTooltip(uintptr_t badge, std::wstring* title, std::wstring* body);

// ---- Zen options sections --------------------------------------------------
// Description of Zen section 0..3 (the container's id at +0xC4). The game
// draws an English literal compiled into its DrawAll (FUN_0042ab20), not a
// string-table entry; this reads that literal from the running executable,
// locating it through the operands of the game's own PUSH instructions (so a
// different executable build fails the opcode check instead of returning
// wrong text). Needs no section to be open.
bool ZenSectionDescription(int sectionId, std::wstring* out);

// ---- Main menu tip line ----------------------------------------------------
// The tip MainMenu::Draw (FUN_006a0ca0) shows at the bottom: tips vector
// (properties\tips.txt) at gApp+0xCFC/+0xD00, std::wstring elements, shown
// tip = tips[(TipIdx - 1) % count] with TipIdx at gApp+0xD78; no tip while
// TipIdx <= 0 or the list is empty. Whether it is currently faded in is not
// read (MainMenu+0x708 alpha, not decoded). index/count are optional.
bool CurrentTip(std::wstring* out, int* index, int* count);

// ---- Bej3DialogButton tooltips ---------------------------------------------
// Bej3DialogButton::Update (FUN_00404260) requests a tooltip while hovered
// when its wstring at +0x1A0 is non-empty: title +0x184, body "^FFAACC^" +
// +0x1A0. Returns false when the button has no tooltip text.
bool DialogButtonTooltip(uintptr_t button, std::wstring* title, std::wstring* body);

// ---- TooltipManager [gApp+0xBF0] (diagnostics) -----------------------------
// Tooltips currently requested by any widget (badges, quest orbs, dialog
// buttons, board gems...). Entries exist only while something is hovered,
// so this is not a no-hover source; it lets a log or fallback see the exact
// text without the draw hook. Layout is a hypothesis (docs/re/hover-text.md).
struct ActiveTooltip {
    int x = 0, y = 0;   // anchor, logical pixels (entry +0, +4)
    std::wstring title;  // entry +0x1C
    std::wstring body;   // entry +0x38
    bool refreshed = false;  // entry +0x5C, set when re-requested this frame
};
bool ReadActiveTooltips(std::vector<ActiveTooltip>* out);

}  // namespace hover_text
