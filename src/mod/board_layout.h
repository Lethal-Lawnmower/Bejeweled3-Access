#pragma once

// Memory layout of the game board (Sexy::Board and subclasses) and the HUD
// state around it, for the Steam Bejeweled3.exe (SHA-256 d5516244...88c3,
// image base 0x00400000). Evidence for every value is recorded in
// docs/re/hud.md. Labels:
//   static     - read from Ghidra/capstone disassembly of this exe
//   community  - third-party claim, not yet checked by us
//   hypothesis - inferred (meaning or exact semantics not proven)
// "runtime" is only used once the lead has confirmed a value in a live dump.
//
// Function addresses in comments are this exe's (FUN_xxxxxxxx in Ghidra).

#include <cstdint>

namespace board {

// ---------------------------------------------------------------------------
// gApp -> Board
// ---------------------------------------------------------------------------

// Bej3App::mBoard (static). Bej3App code stores each newly constructed board
// here, then calls board->Resize and WidgetManager(gApp+0x360)->AddWidget
// (e.g. 0x007733ac, 0x0077353e, 0x00774564); it is zeroed after the board is
// removed (0x007596cc) and in the Bej3App ctor (0x0076dd30).
// Community: FatBrother1 deck-bot uses 0xBE8 for this build. Other community
// projects use 0xBE0: they target a different exe build (their gApp global
// and vtable addresses differ), so 0xBE0 does not apply here.
constexpr int kAppBoard = 0xBE8;

// ---------------------------------------------------------------------------
// Board (base class, vtable 0x00864CB4; ctor 0x0074AE30)
// ---------------------------------------------------------------------------

// Gem grid: Piece* at +0xF8 + 4*(col + 8*row) (static: ShowHint 0x0075AEB0
// indexes [board + (col + row*8)*4 + 0xF8]). Owned by the grid agent.
constexpr int kGrid = 0xF8;

// Secondary vtable (ButtonListener interface) at +0xBC (static: buttons
// are created with listener = board+0xBC, 0x007452A9).
constexpr int kButtonListener = 0xBC;

// Per-game stats, int[0x26] at +0xC84, indexed by stat id (static: AddStat
// 0x0073D170 "add [esi+ecx*4+0xC84], edi", MaxStat 0x0073CFD0).
constexpr int kStats = 0xC84;
constexpr int kStatCount = 0x26;  // static: save/load loop 0x00765337
// Stat 0x1C = butterflies released (static: ButterflyGoal 0x006AB2F0 adds
// stat 0x1C when a piece with flag 0x80 (butterfly) is cleared).
constexpr int kStatButterfliesFreed = 0x1C;

// Game-over countdown: set to 1 in Board::GameOver (0x007602C0), counts up
// afterwards; 0 while playing (static; meaning of later values hypothesis).
constexpr int kGameOverCount = 0xD1C;
// Quest/level-complete countdown (hypothesis: QuestBoard code near the
// quest-completed announcement decrements/adds it, 0x006DD99F/0x006DDA0F).
// vtable+0x33C (0x0073B640) allows the HUD buttons only while both this and
// kGameOverCount are <= 0.
constexpr int kCompleteCount = 0xD20;

// Score (static: Board::AddPoints 0x0074A311 "add [esi+0xD24], edi" capped
// at 0x7FFFFFFF; reset to 0 in 0x0074AAE5). Community: deck-bot agrees.
constexpr int kScore = 0xD24;
// Score as currently displayed (animated towards kScore). Static:
// Board::DrawScore 0x00746F20 formats [this+0xD64].
constexpr int kDisplayedScore = 0xD64;
// Game over flag, byte (static: set to 1 in Board::GameOver 0x007602C0).
constexpr int kGameOver = 0xD8C;

// Points toward the current level/goal (static: AddPoints 0x0074A2E6 adds
// to it; Board::GetLevelPoints vtable+0x26C = 0x0073BF60 returns it).
// Diamond Mine shows this value as its "$" gold total (static: DigGoal
// draw 0x006BDEDF reads goal->board->+0xE00).
constexpr int kLevelPoints = 0xE00;
// Level index, 0-based (static: HyperspaceEvent 0x00760F8A formats
// the level heading with [this+0xE04]+1; GetLevelPointsTotal uses it).
// Community: deck-bot's "level" at 0xE0C is wrong for this build (see
// kMultiplier).
constexpr int kLevel = 0xE04;
// Score multiplier, drawn as a multiplier count (static: DrawPointMultiplier 0x00752600
// formats [this+0xE0C]; InfernoBoard multiplier text 0x006CC2E7; reset to
// 1 in 0x0074AADB).
constexpr int kMultiplier = 0xE0C;
// Game update counter used by the base timer (static: GetTicksLeft
// 0x0073BB80 uses max(0, [+0xE38] - 250)). Units: 1/100 s (hypothesis from
// the "(ticks + 99) / 100" seconds conversion in DrawTimer 0x007516A0).
constexpr int kUpdateCount = 0xE38;

// Timer override: if byte +0x12F8 is set, GetTicksLeft returns int +0x1308
// (static: 0x0073BB80 and SpeedBoard 0x00713FA0). Meaning hypothesis.
constexpr int kTicksOverrideFlag = 0x12F8;
constexpr int kTicksOverride = 0x1308;
// User pause (space bar) toggle, byte (static: Board::KeyChar 0x0073B670
// toggles it on ' ' when the board is timed; IsGamePaused 0x0073C260).
constexpr int kUserPaused = 0x12C4;
// Hint button cooldown, float; the hint button only works while it is 0
// (static: Board::ButtonDepress 0x0074BC40 case 0 tests [listener+0x120C]).
constexpr int kHintCooldown = 0x12C8;
// Pause overlay alpha, float (static: DrawOverlay 0x00756CB0 draws the pause text
// when vtable+0x1AC is true and this > 0).
constexpr int kPauseAlpha = 0x12CC;
// Board suspended / transitioning, byte (hypothesis: set by 0x00742E60 from
// button id 3; ShowHint, KeyChar, IsGamePaused and ButtonDepress all bail
// out while it is set).
constexpr int kSuspended = 0x1410;

// Quest description text, std::wstring (static: QuestBoard init 0x006DEBC8
// assigns the quest's "Description" parameter; QuestBoard::DrawBottomWidget
// 0x006C9D40 draws it).
constexpr int kQuestDescription = 0x2680;
// Byte: DrawScore shows "<score> of <target>" when set (static 0x0074702D).
constexpr int kShowScoreTarget = 0x269C;
// Hint timing ints (static: ShowHint 0x0075AEB0 sets +0x26A0 = 250 when the
// hint comes from the button and clears +0x26A4). Meaning hypothesis.
constexpr int kHintDelay = 0x26A0;
constexpr int kHintIdle = 0x26A4;

// Board-owned HUD buttons (Bej3Button, vtable 0x0086498C, children of the
// board widget). Static: created in 0x00745240 with ids 0..3.
constexpr int kButtonHint = 0x286C;    // id 0: ShowHint (vtable+0x328)
constexpr int kButtonMenu = 0x2870;    // id 1: opens OptionsDialog, dialog id 21
constexpr int kButtonExtra = 0x2874;   // id 3: calls 0x00742E60 (purpose unknown)
constexpr int kButtonReset = 0x2878;   // id 2: reset confirm dialog, id 22
constexpr int kButtonIdHint = 0;
constexpr int kButtonIdMenu = 1;
constexpr int kButtonIdReset = 2;
constexpr int kButtonIdExtra = 3;
// Bottom-widget/HUD layout kind (static: 0 = Board default (0x0074B4BF),
// 1 = SpeedBoard (0x0071823E) and endless Dig (0x007372A1), 3 = QuestBoard
// (0x006D9086)). Which buttons exist depends on it (0x00745240).
constexpr int kHudKind = 0xC1C;

// Byte: level complete pending; Board::UpdateGame 0x007696xx creates the
// level-complete announcement when it is set and the board is still
// (static; community autosplitter names it toBeComplete).
constexpr int kLevelCompletePending = 0x288C;

// std::vector<Announcement*> mAnnouncements (MSVC 2008: begin +0x3040,
// end +0x3044, capacity end +0x3048). Static: Announcement ctor 0x0078C7F0
// push_back's into board+0x3034 (helper 0x0041B090 uses +0xC/+0x10/+0x14);
// Announcement::Update 0x0078CD80 erases begin() when finished; DrawOverlay
// draws only the front element. So the front is the one on screen.
constexpr int kAnnouncementsBegin = 0x3040;
constexpr int kAnnouncementsEnd = 0x3044;

// ---------------------------------------------------------------------------
// Board virtual functions (byte offset in the primary vtable). Values are
// read by replicating the reviewed implementations in hud.cpp, never by
// calling game code. hud.cpp only trusts the target addresses listed there.
// ---------------------------------------------------------------------------
constexpr int kVfTimeLimitSeconds = 0x1A0;  // 0 = untimed
constexpr int kVfLevelPointsTotal = 0x268;  // level/goal target
constexpr int kVfLevelPoints = 0x26C;       // progress toward the target
constexpr int kVfGameOverAnnounce = 0x278;  // time-up / game-over / no-more-moves announcement
constexpr int kVfTicksLeft = 0x2EC;         // centiseconds, -1 = untimed
constexpr int kVfShowHint = 0x328;

// ---------------------------------------------------------------------------
// QuestBoard (vtable 0x00866FBC; ctor 0x006D9000) and its subclasses
// (Dig, Poker, Butterfly, Inferno, Balance, MoveLimit, TimeLimit, ...)
// ---------------------------------------------------------------------------

// QuestGoal* (static: QuestBoard ctor zeroes it; GetLevelPointsTotal
// 0x006C4BD0 forwards to goal->vtable+0x58).
constexpr int kQuestGoal = 0x3180;
// Byte: endless ("perpetual") mode, i.e. the main-menu Poker, Butterflies,
// Ice Storm and Diamond Mine modes rather than a quest (hypothesis from the
// getters: Poker/Butterfly targets become 0 when set, 0x006C4D70/0x006AA420;
// Dig switches to HUD kind 1, 0x00737290). Community: PlusExtender
// IsPerpetual (at a different offset in their build).
constexpr int kPerpetual = 0x3184;
// Quest id, -1 until set (hypothesis: ctor sets -1 at 0x006D907C;
// PlusExtender reads quest ids as "mode = quest id >= 1000").
constexpr int kQuestId = 0x3234;
// Mode parameter used as the time limit by DigBoard/TimeLimitBoard
// (GetTimeLimit 0x006C4A30) and as half the target by BalanceBoard
// (0x006C53A0). Static.
constexpr int kQuestParam = 0x3248;
// MoveLimitBoard moves left (static: DrawBottomFrame 0x006CBA10 formats
// the moves-left line with [this+0x324C]).
constexpr int kMovesLeft = 0x324C;
// DigBoard timer cap: GetTicksLeft 0x00736B50 = min(base, [+0x3274]) when
// nonzero (static).
constexpr int kDigTicksCap = 0x3274;

// InfernoBoard (Ice Storm) endless-mode level values, float, scaled by
// 10000 (static: 0x006C4F70 / 0x006C5D00).
constexpr int kInfernoTargetF = 0x3270;
constexpr int kInfernoPointsF = 0x326C;

// PokerBoard quest target (static: 0x006C4D70 returns [+0x3968] unless
// perpetual).
constexpr int kPokerTarget = 0x3968;

// SpeedBoard (Lightning) values used by its getters (static):
//   GetLevelPointsTotal 0x007140D0 = [+0x30FC] * [+0x3084] + [+0x30F8]
//   GetLevelPoints      0x007140B0 = [+0xE00] - (int)double[+0x30D0]
//   GetTicksLeft        0x00713FA0 = min(clamp(limit*100 - max(0,
//                         float[+0x3284] - 250), 0, limit*100), [+0x30F4])
constexpr int kSpeedTargetMul = 0x30FC;
constexpr int kSpeedTargetLevel = 0x3084;
constexpr int kSpeedTargetBase = 0x30F8;
constexpr int kSpeedPointsBaseD = 0x30D0;
constexpr int kSpeedElapsedF = 0x3284;
constexpr int kSpeedTicksCap = 0x30F4;

// ---------------------------------------------------------------------------
// QuestGoal (vtable 0x0086A6BC) and subclasses
// ---------------------------------------------------------------------------
constexpr int kGoalBoard = 0x104;        // static: goal getters read board via +0x104
constexpr int kGoalRequirement = 0x110;  // static: most goal vtable+0x58 getters
constexpr int kGoalPerLevel = 0x114;     // static: ClearGoal 0x006AA320
constexpr int kGoalAltRequirement = 0x118;  // static: Clear/Hidden/HiddenGrid
constexpr int kGoalVfLevelPointsTotal = 0x58;
// DigGoal: target 0x3BC (static 0x006A9810); depth base, double at +0x4F0
// (static: DigGoal::DrawOverlay 0x006BD55D adds an animated term
// curve(+0x518) * step(+0x4FC, doubled when byte +0x647) before truncating).
constexpr int kDigGoalTarget = 0x3BC;
constexpr int kDigGoalDepthD = 0x4F0;
// HiddenObjectGoal target (static 0x006A9950).
constexpr int kHiddenObjectTarget = 0x1F4C;

// ---------------------------------------------------------------------------
// Announcement (vtable 0x00863BD8, size 0x190, ctor 0x0078C7F0,
// Update 0x0078CD80, Draw 0x0078C9D0). Not a widget.
// ---------------------------------------------------------------------------
constexpr int kAnnText = 0x10;     // std::wstring; '\n' separates lines
constexpr int kAnnBlocksPlay = 0x181;  // byte (hypothesis)
constexpr int kAnnBoard = 0x184;   // Board*, 0 for QuestMenu announcements
constexpr uintptr_t kVtAnnouncement = 0x00863BD8;

}  // namespace board
