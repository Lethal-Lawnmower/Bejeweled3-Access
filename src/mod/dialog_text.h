#pragma once

// Structured readers for rarely seen dialogs whose drawn text alone is
// ambiguous or incomplete: rank up, badge award, mode unlock, the Zen intro
// (text baked into an image), the icon-only special-gem counts on the
// Classic / Butterflies end screens, and the quest-mode-complete
// announcement (string 421) that the quest menu keeps outside any board
// queue.
//
// Evidence, triggers and the runtime checklist: docs/re/rare-dialogs.md.
// Everything here is STATIC analysis (Ghidra, Steam exe SHA-256
// d5516244...88c3); nothing is runtime-confirmed yet.
//
// Rules kept: no game text in this source. Every string returned is read
// from the game's memory at call time (dialog members, the app's name
// tables, the numeric string table) or is a resource id the game itself
// uses for the image it draws. Values are raw: pass text through
// CleanGameText before speaking. No game code is called; all reads are
// guarded, so a failed read leaves the field unknown (never a default).
// Call from the game's window thread (NavigatorTick), like the rest of the
// navigator.
//
// All dialogs here are Bej3Dialog -> Dialog, so the navigator's generic
// DialogSpeech (header/lines/footer fields + drawn text) already runs for
// them; these readers add what that cannot get right on its own.

#include <cstdint>
#include <string>
#include <vector>

namespace dialog_text {

enum class RareDialog {
    None,          // not one of the classes below (or unreadable)
    RankUp,        // RankUpDialog, dialog id 0x2B
    BadgeAward,    // BadgeAwardDialog, dialog id 0x17
    Unlock,        // UnlockDialog, dialog id 0x27
    ZenIntro,      // ZenIntroDialog, dialog id 0x2C
    EndLevel,      // EndLevelDialog or a subclass, dialog id 0x26
};

// Classifies a dialog by its RTTI class name.
RareDialog ClassifyRareDialog(uintptr_t dialog);

// --- RankUpDialog -----------------------------------------------------------
// The dialog draws string 431, the new rank's name (twice, glow + text),
// string 432 and "<n>" + string 433 (points to the next rank, in thousands).
// The rank name is built from data at draw time, so it is read here the way
// Draw FUN_0078f0d0 / FUN_00676470 do it.
struct RankUpInfo {
    bool rankKnown = false;
    int rank = -1;              // index the game draws (clamped like the game)
    bool fromBoard = false;     // true: RankBarWidget display rank; false: profile rank
    std::wstring rankName;      // raw, from the app's rank-name vector; empty if unreadable
};
bool ReadRankUp(uintptr_t dialog, RankUpInfo* out);

// --- BadgeAwardDialog -------------------------------------------------------
// The dialog has no header; it draws string 70 and hosts a BadgeWidget in
// award mode that presents the newly earned badges one after another
// (string 72, then string 71 formatted with level name and badge name, then
// the badge's own award line). Its button (label 69) stays disabled
// until the sequence has shown every badge.
struct AwardedBadge {
    int id = -1;                // badge id 0..19 (same ids as the Records grid)
    bool levelKnown = false;
    int level = 0;              // level now earned (1 = first level)
    std::wstring name;          // raw, from the app's badge-name vector
    std::wstring levelName;     // raw, numeric string id == level (1..4)
};
struct BadgeAwardInfo {
    std::vector<AwardedBadge> badges;  // in award order
    bool indexKnown = false;
    int shownIndex = -1;        // badge the widget is presenting (== size when done)
    bool sequenceDone = false;  // shownIndex >= badges.size()
};
bool ReadBadgeAward(uintptr_t dialog, BadgeAwardInfo* out);

// --- UnlockDialog -----------------------------------------------------------
// Header 501 is in the Dialog header field; the unlocked mode's name
// (one of strings 584..587, chosen by Bej3App::UnlockEndlessMode) is stored
// in the dialog and drawn between strings 503 and 504.
bool ReadUnlockedModeName(uintptr_t dialog, std::wstring* out);

// --- ZenIntroDialog ---------------------------------------------------------
// Has no header, lines or drawn text: its whole message is one image
// (resource IMAGE_ZENUI_INTRO, drawn between a header and a footer image),
// plus an OK button (label 575). The game has no text for it. This returns
// the resource id of the image the dialog's Draw uses, for the lead to
// decide how to present it (see rare-dialogs.md, "Zen intro").
bool ReadZenIntroImageName(std::string* out);

// --- EndLevelDialog family --------------------------------------------------
struct EndLevelInfo {
    std::string className;      // RTTI, e.g. "ClassicEndLevelDialog" (diagnostics only)
    uintptr_t board = 0;        // Board* the dialog was built from
    bool scoreKnown = false;
    int finalScore = 0;         // copy of board score taken in the ctor
    bool levelKnown = false;
    int level = 0;              // 0-based board level copy (drawn +1 where shown)
    bool questButtons = false;  // button set 224/225/226 (quest menu exists), else 227/228/229
};
bool ReadEndLevel(uintptr_t dialog, EndLevelInfo* out);

// Classic and Butterflies end screens draw three special-gem counts as an
// icon followed by a count (strings 230-232), so the drawn text has no gem
// names. The game's own names for the icons are their image resource ids
// (e.g. the flame / star / hypercube game-over icons). Returns false for
// other end dialogs (Lightning and Ice Storm draw text labels instead).
struct SpecialGemCount {
    std::string imageName;      // resource id of the icon drawn; empty if unreadable
    bool countKnown = false;
    int count = 0;
};
bool ReadEndLevelSpecialGems(uintptr_t dialog, std::vector<SpecialGemCount>* out);

// --- QuestMenu completion announcement ---------------------------------------
// QuestMenu::Update creates an Announcement (string 421) with no board and
// keeps it at QuestMenu+0x178, so HudNewAnnouncements never sees it. Returns
// the text while that announcement object exists. questMenu: the QuestMenu
// widget (RTTI "QuestMenu").
bool ReadQuestMenuAnnouncement(uintptr_t questMenu, std::wstring* out);

// Logs everything the readers above find for a dialog (F12 dump). Prefix
// "rare:".
void DumpRareDialog(uintptr_t dialog);

}  // namespace dialog_text
