#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "board_layout.h"
#include "game_memory.h"
#include "game_strings.h"
#include "mode_info.h"
#include "settings.h"
#include "text.h"

// Quests on the QuestBoard family (QuestBoard, MoveLimit, TimeLimit,
// NoLose, BadMoveLimit, Balance, Filler, TimeBomb, RealTimeBomb boards).
// Two things decide what the screen shows beyond the gems:
// - the goal object at Board+0x3180 (RTTI class): gold squares (Alchemy),
//   wall pieces (Wall Blast), bombs (Clear Bombs, which are pieces);
// - the board class (the quest's Challenge): bomb counters, the Balance
//   scale, how full the Avalanche board is, moves left.
// Goal progress and target, moves left and time are already in F6/T
// (hud.cpp), so they are not repeated. Fields and evidence:
// docs/modes/quests.md; all are static reads of this exe, none has been
// checked on a live board yet. Hidden Objects (Buried Treasure, Sandstorm)
// is deliberately not covered: its object map would tell where the treasure
// lies under the sand, which the player cannot see (see the doc).
namespace {

// Piece record (no vtable; docs/re/board-objects.md "Piece record").
constexpr int kPieceId = 0x04;
constexpr int kPieceFlags = 0x228;
constexpr int kPieceCounter = 0x244;
constexpr uint32_t kFlagMoveBomb = 0x20;   // counts moves (setter 0x0073CCC0, bool false)
constexpr uint32_t kFlagTimeBomb = 0x40;   // counts time (same setter, bool true; 0x006C6200)

// QuestGoal: Board* back pointer (static: every goal getter reads its board
// through +0x104; board_layout.h kGoalBoard).
// GoldLinesGoal / GoldLinesFlipGoal: one 0x20-byte record per square, gold
// state int at +0x134 + 0x20*(col + 8*row). Static: the gild function
// (vt+0x120, 0x006B78F0, both classes) is called as (row, col) and only
// raises the state while it is below 1, adding 1 to the level points when it
// passes 0; init 0x006B04F0 sets 0, the flip init 0x006B0F20 sets -1; the
// draw 0x006ABFC0 has three looks: below 0, 0, above 0.
constexpr int kGoldLineState = 0x134;
constexpr int kGoldLineStride = 0x20;
// GoldGoal / GoldYellowGoal: int per square at +4 + 4*(col + 8*row), 1 =
// gold. Static: GoldYellowGoal 0x006AA140 indexes it with piece col +
// 8*row; draw 0x006AC200 draws a square where it is non-zero.
constexpr int kGoldSquares = 0x4;
// WallBlastGoal: 4 bytes per square from +0x118, byte 0 = wall present.
// Static: init 0x006B0FB0 fills it from the quest grid in reading order and
// counts walls into +0x110 (the target); the hit 0x006AD740 (EAX = col,
// row argument) clears the byte and adds a level point.
constexpr int kWallCells = 0x118;
constexpr int kWallStride = 4;

// BalanceBoard (static: piece tallied 0x006D06A0 counts colour 0 into
// +0x3260 and colour 6 into +0x325C, each up to Gems +0x3248; update
// 0x006E3C90 moves the scale +0x3250 (float) by clamp((blue - red) /
// Balance, -1, 1) / RollSpeedDivisor and calls GameOver when |scale| > 1.0).
constexpr int kBalanceGems = 0x3248;
constexpr int kBalanceBlue = 0x325C;
constexpr int kBalanceRed = 0x3260;
constexpr int kBalanceScale = 0x3250;
constexpr int kColourRed = 0, kColourBlue = 6;
// FillerBoard (Avalanche): gems that drop after every move (DropPerMove,
// static: per-move 0x006C52A0 adds +0x3248 to the waiting count) and its
// danger rule, vt+0x368 = 0x006C5240: gems on the board + DropPerMove - 3 >
// 63 while the game is not over (+0xD1C == 0).
constexpr int kFillerDropPerMove = 0x3248;
constexpr uintptr_t kFillerDanger = 0x006C5240;
// MoveLimitBoard danger rule, vt+0x368 = 0x006C4DA0: moves left < 4.
constexpr uintptr_t kMoveLimitDanger = 0x006C4DA0;
constexpr int kVfDanger = 0x368;

// Game strings (default.xml ids; text not copied here).
constexpr int kStrTimeBomb = 137;   // tooltip title, Board::GetTooltipText
constexpr int kStrMovesLeft = 301;  // "Moves Left: %i", MoveLimitBoard::DrawBottomFrame
constexpr int kColourNameBase = 455;

enum class GoalKind { None, GoldLines, GoldLinesFlip, Gold, Wall, Butterfly, Dig, Other };
enum class BoardKind { Plain, MoveBombs, TimeBombs, Balance, Filler, MoveLimit };

bool ReadFloat(uintptr_t addr, float* out) {
    uint32_t bits;
    if (!ReadU32(addr, &bits)) return false;
    memcpy(out, &bits, sizeof bits);
    return true;
}

std::wstring Str(int id) {
    std::wstring text;
    return GameString(id, &text) ? CleanGameText(text) : L"";
}

// Fills %d / %i placeholders of a game string with values read from the
// game; empty if the string is missing or has another placeholder.
std::wstring Formatted(int id, std::initializer_list<int> values) {
    std::wstring format;
    if (!GameString(id, &format)) return L"";
    std::wstring out;
    auto next = values.begin();
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] != L'%') {
            out += format[i];
            continue;
        }
        if (i + 1 < format.size() && format[i + 1] == L'%') {
            out += L'%';
            ++i;
            continue;
        }
        if (i + 1 >= format.size() || (format[i + 1] != L'd' && format[i + 1] != L'i') ||
            next == values.end())
            return L"";
        out += std::to_wstring(*next++);
        ++i;
    }
    return CleanGameText(out);
}

std::wstring ColourName(int colour) {
    if (colour < 0 || colour > 6) return L"";
    return Str(kColourNameBase + colour);
}

// Same wording as board_access's Position (settings coords).
std::wstring Position(int col, int row) {
    const Settings& s = CurrentSettings();
    if (s.coords == CoordStyle::Chess)
        return std::wstring(1, (wchar_t)(L'A' + col)) + std::to_wstring(row + 1);
    if (s.coords == CoordStyle::RowColumn)
        return L"row " + std::to_wstring(row + 1) + L", column " + std::to_wstring(col + 1);
    return L"column " + std::to_wstring(col + 1) + L", row " + std::to_wstring(row + 1);
}

std::wstring BombWord() {
    std::wstring word = Str(kStrTimeBomb);
    return word.empty() ? L"bomb" : word;
}

// ---- which quest is this ---------------------------------------------------

struct QuestKind {
    uintptr_t goal = 0;  // 0 when the board has no (valid) goal
    GoalKind goalKind = GoalKind::None;
    BoardKind boardKind = BoardKind::Plain;
};

uintptr_t g_kindBoard = 0, g_kindGoal = 0;
QuestKind g_kind;

GoalKind GoalKindOf(const std::string& name) {
    if (name == "GoldLinesGoal") return GoalKind::GoldLines;
    if (name == "GoldLinesFlipGoal") return GoalKind::GoldLinesFlip;
    if (name == "GoldGoal" || name == "GoldYellowGoal") return GoalKind::Gold;
    if (name == "WallBlastGoal") return GoalKind::Wall;
    if (name == "ButterflyGoal") return GoalKind::Butterfly;
    if (name == "DigGoal") return GoalKind::Dig;
    return GoalKind::Other;  // Clear, ClearBombs, Score, HiddenObject, ...: nothing on the cells
}

BoardKind BoardKindOf(uintptr_t board) {
    std::string name = ClassNameOf(board);
    if (name == "TimeBombBoard") return BoardKind::MoveBombs;
    if (name == "RealTimeBombBoard") return BoardKind::TimeBombs;
    if (name == "BalanceBoard") return BoardKind::Balance;
    if (name == "FillerBoard") return BoardKind::Filler;
    if (IsKindOf(board, "MoveLimitBoard")) return BoardKind::MoveLimit;
    return BoardKind::Plain;
}

// RTTI lookups are cached per board and goal pointer (the goal is created
// with the board; a new quest makes a new board).
const QuestKind& KindOf(uintptr_t board) {
    uint32_t goal = 0, back = 0;
    if (!ReadU32(board + board::kQuestGoal, &goal) ||
        (goal && (!ReadU32(goal + board::kGoalBoard, &back) || back != board)))
        goal = 0;  // unreadable or not this board's goal: treat as none
    if (board != g_kindBoard || goal != g_kindGoal) {
        g_kindBoard = board;
        g_kindGoal = goal;
        g_kind = QuestKind();
        g_kind.goal = goal;
        g_kind.goalKind = goal ? GoalKindOf(ClassNameOf(goal)) : GoalKind::None;
        g_kind.boardKind = BoardKindOf(board);
    }
    return g_kind;
}

// Another mode's hooks for a goal it owns (a Butterflies or Diamond Mine
// goal on a board of this family), or null.
const ModeHooks* Delegate(const QuestKind& k) {
    if (k.goalKind == GoalKind::Butterfly) return &ButterflyHooks();
    if (k.goalKind == GoalKind::Dig) return &DiamondMineHooks();
    return nullptr;
}

bool VirtualTarget(uintptr_t object, int slot, uint32_t* target) {
    uint32_t vtable;
    return ReadU32(object, &vtable) && ReadU32(vtable + slot, target);
}

// ---- goal squares -----------------------------------------------------------

enum class Square { Unknown, Plain, Lead, Gold, Wall, NoWall };

Square SquareOf(const QuestKind& k, int col, int row) {
    int index = col + 8 * row;
    switch (k.goalKind) {
    case GoalKind::GoldLines:
    case GoalKind::GoldLinesFlip: {
        int32_t v;
        if (!ReadI32(k.goal + kGoldLineState + kGoldLineStride * index, &v)) return Square::Unknown;
        if (v > 0) return Square::Gold;
        if (k.goalKind == GoalKind::GoldLinesFlip && v == 0) return Square::Lead;
        return Square::Plain;
    }
    case GoalKind::Gold: {
        int32_t v;
        if (!ReadI32(k.goal + kGoldSquares + 4 * index, &v)) return Square::Unknown;
        return v ? Square::Gold : Square::Plain;
    }
    case GoalKind::Wall: {
        uint8_t v;
        if (!ReadU8(k.goal + kWallCells + kWallStride * index, &v)) return Square::Unknown;
        return v ? Square::Wall : Square::NoWall;
    }
    default:
        return Square::Unknown;
    }
}

const wchar_t* SquareWord(const QuestKind& k, Square s) {
    switch (s) {
    case Square::Gold: return L"gold";
    case Square::Lead: return L"lead";
    case Square::Plain: return k.goalKind == GoalKind::GoldLinesFlip ? L"not flipped" : L"not gold";
    case Square::Wall: return L"wall";
    default: return L"";
    }
}

// The squares still to change: not gold yet, or still walled.
bool Remaining(Square s) { return s == Square::Plain || s == Square::Lead || s == Square::Wall; }

struct SquareCounts {
    bool ok = false;
    int remaining = 0, lead = 0, plain = 0;
};

SquareCounts CountSquares(const QuestKind& k) {
    SquareCounts c;
    for (int i = 0; i < 64; ++i) {
        Square s = SquareOf(k, i % 8, i / 8);
        if (s == Square::Unknown) return SquareCounts();
        if (Remaining(s)) ++c.remaining;
        if (s == Square::Lead) ++c.lead;
        if (s == Square::Plain) ++c.plain;
    }
    c.ok = true;
    return c;
}

std::wstring SquaresLeftText(const QuestKind& k, const SquareCounts& c) {
    if (!c.ok) return L"";
    if (k.goalKind == GoalKind::Wall)
        return std::to_wstring(c.remaining) + (c.remaining == 1 ? L" wall piece left" : L" wall pieces left");
    if (k.goalKind == GoalKind::GoldLinesFlip && c.remaining > 0)
        return std::to_wstring(c.remaining) + L" squares not gold, " + std::to_wstring(c.lead) + L" of them lead";
    return std::to_wstring(c.remaining) + (c.remaining == 1 ? L" square not gold" : L" squares not gold");
}

// The remaining squares row by row ("row 3: columns 3, 4, 5"), or each
// square's name in chess coordinates.
std::wstring RemainingList(const QuestKind& k) {
    bool chess = CurrentSettings().coords == CoordStyle::Chess;
    std::wstring out;
    for (int row = 0; row < 8; ++row) {
        std::wstring cols;
        for (int col = 0; col < 8; ++col) {
            Square s = SquareOf(k, col, row);
            if (s == Square::Unknown) return L"";
            if (!Remaining(s)) continue;
            std::wstring item = chess ? Position(col, row) : std::to_wstring(col + 1);
            if (k.goalKind == GoalKind::GoldLinesFlip) item += std::wstring(L" ") + SquareWord(k, s);
            cols += (cols.empty() ? L"" : L", ") + item;
        }
        if (cols.empty()) continue;
        out += (out.empty() ? L"" : L"; ") +
               (chess ? cols : L"row " + std::to_wstring(row + 1) + L": column" +
                                   (cols.find(L',') == std::wstring::npos ? L" " : L"s ") + cols);
    }
    return out;
}

// ---- bombs ------------------------------------------------------------------

struct Bomb {
    int id = 0, col = 0, row = 0, counter = 0;
    bool moves = false;  // counts moves (else time)
};

bool ReadBombs(uintptr_t board, std::vector<Bomb>* out) {
    out->clear();
    for (int i = 0; i < 64; ++i) {
        uint32_t piece, flags;
        if (!ReadU32(board + board::kGrid + 4 * i, &piece)) return false;
        if (!piece) continue;
        if (!ReadU32(piece + kPieceFlags, &flags)) return false;
        if (!(flags & (kFlagMoveBomb | kFlagTimeBomb))) continue;
        Bomb b;
        if (!ReadI32(piece + kPieceId, &b.id) || !ReadI32(piece + kPieceCounter, &b.counter)) return false;
        b.col = i % 8;
        b.row = i / 8;
        b.moves = (flags & kFlagMoveBomb) != 0;
        out->push_back(b);
    }
    return true;
}

// "5 moves left" / "5 seconds left". Time bombs count down once per 100
// game updates (piece +0x250 = 100, set in the piece init 0x00720120; tick
// in UpdateGame 0x00767E20), which is a second if the game updates 100
// times a second (hypothesis, docs/re/hud.md).
std::wstring BombCount(int counter, bool moves) {
    return std::to_wstring(counter) +
           (moves ? (counter == 1 ? L" move left" : L" moves left")
                  : (counter == 1 ? L" second left" : L" seconds left"));
}

const Bomb* Lowest(const std::vector<Bomb>& bombs) {
    const Bomb* low = nullptr;
    for (const Bomb& b : bombs)
        if (!low || b.counter < low->counter) low = &b;
    return low;
}

// Counter values worth a warning, as the number turns low on screen.
bool BombWarning(const Bomb& b) {
    if (b.moves) return b.counter >= 1 && b.counter <= 3;
    return b.counter == 10 || b.counter == 5 || (b.counter >= 1 && b.counter <= 3);
}

// ---- balance, avalanche, moves ----------------------------------------------

struct BalanceState {
    bool ok = false;
    int red = 0, blue = 0, gems = 0;
    float scale = 0.0f;  // > 0 blue heavier, < 0 red heavier; +-1 loses
};

bool ReadBalance(uintptr_t board, BalanceState* s) {
    *s = BalanceState();
    if (!ReadI32(board + kBalanceRed, &s->red) || !ReadI32(board + kBalanceBlue, &s->blue) ||
        !ReadI32(board + kBalanceGems, &s->gems) || !ReadFloat(board + kBalanceScale, &s->scale) ||
        !std::isfinite(s->scale))
        return false;
    s->ok = true;
    return true;
}

int TiltPercent(const BalanceState& s) {
    float a = std::fabs(s.scale);
    if (a > 1.0f) a = 1.0f;
    return (int)(a * 100.0f + 0.5f);
}

std::wstring TiltText(const BalanceState& s) {
    int percent = TiltPercent(s);
    if (percent == 0) return L"level";
    // Which way the drawn scale leans for each sign is not yet checked on
    // screen; the sign follows the heavier colour (update 0x006E3C90).
    std::wstring side = ColourName(s.scale > 0 ? kColourBlue : kColourRed);
    if (side.empty()) return L"";
    return side + L" side down " + std::to_wstring(percent) + L" percent";
}

std::wstring BalanceText(const BalanceState& s) {
    std::wstring red = ColourName(kColourRed), blue = ColourName(kColourBlue);
    if (red.empty() || blue.empty()) return L"";
    std::wstring out = red + L" " + std::to_wstring(s.red) + L" of " + std::to_wstring(s.gems) + L", " + blue +
                       L" " + std::to_wstring(s.blue) + L" of " + std::to_wstring(s.gems);
    std::wstring tilt = TiltText(s);
    if (!tilt.empty()) out += L", " + tilt;
    return out;
}

struct FillState {
    bool ok = false;
    int gems = 0;
    int perColumn[8] = {};
    bool danger = false, dangerKnown = false;
};

bool ReadFill(uintptr_t board, FillState* s) {
    *s = FillState();
    for (int i = 0; i < 64; ++i) {
        uint32_t piece;
        if (!ReadU32(board + board::kGrid + 4 * i, &piece)) return false;
        if (piece) {
            ++s->gems;
            ++s->perColumn[i % 8];
        }
    }
    // The game's own "almost full" rule, only when the vtable slot is the
    // reviewed FillerBoard function.
    uint32_t target;
    int32_t drop, gameOver;
    if (VirtualTarget(board, kVfDanger, &target) && target == kFillerDanger &&
        ReadI32(board + kFillerDropPerMove, &drop) && ReadI32(board + board::kGameOverCount, &gameOver)) {
        s->dangerKnown = true;
        s->danger = s->gems + drop - 3 > 63 && gameOver == 0;
    }
    s->ok = true;
    return true;
}

// Moves left while the game's own danger rule (< 4) holds, as its bottom
// frame shows them; empty otherwise.
std::wstring LowMovesText(uintptr_t board) {
    uint32_t target;
    int32_t moves;
    if (!VirtualTarget(board, kVfDanger, &target) || target != kMoveLimitDanger ||
        !ReadI32(board + board::kMovesLeft, &moves) || moves >= 4 || moves < 0)
        return L"";
    std::wstring text = Formatted(kStrMovesLeft, {moves});
    return text.empty() ? std::to_wstring(moves) + L" moves left" : text;
}

// ---- hooks ------------------------------------------------------------------

void Add(std::wstring* out, const std::wstring& text, const wchar_t* sep = L", ") {
    if (!text.empty()) *out += (out->empty() ? L"" : sep) + text;
}

std::wstring QuestCell(uintptr_t board, uintptr_t piece, int col, int row) {
    const QuestKind& k = KindOf(board);
    std::wstring out;
    if (piece) {
        uint32_t flags;
        int32_t counter;
        if (ReadU32(piece + kPieceFlags, &flags) && (flags & (kFlagMoveBomb | kFlagTimeBomb)) &&
            ReadI32(piece + kPieceCounter, &counter))
            Add(&out, BombCount(counter, (flags & kFlagMoveBomb) != 0));
    }
    Add(&out, SquareWord(k, SquareOf(k, col, row)));
    const ModeHooks* other = Delegate(k);
    if (other && other->cellText) Add(&out, other->cellText(board, piece, col, row));
    return out;
}

std::wstring QuestBrief(uintptr_t board) {
    const QuestKind& k = KindOf(board);
    std::wstring out;
    if (k.boardKind == BoardKind::MoveBombs || k.boardKind == BoardKind::TimeBombs) {
        std::vector<Bomb> bombs;
        if (ReadBombs(board, &bombs)) {
            const Bomb* low = Lowest(bombs);
            if (!low)
                Add(&out, L"no " + BombWord());
            else
                Add(&out, std::to_wstring(bombs.size()) + L" " + BombWord() + L", lowest " +
                              BombCount(low->counter, low->moves));
        }
    }
    if (k.boardKind == BoardKind::Balance) {
        BalanceState s;
        if (ReadBalance(board, &s)) Add(&out, BalanceText(s));
    }
    if (k.boardKind == BoardKind::Filler) {
        FillState s;
        if (ReadFill(board, &s)) {
            Add(&out, std::to_wstring(s.gems) + L" of 64 squares filled");
            if (s.dangerKnown && s.danger) Add(&out, L"almost full");
        }
    }
    if (k.goalKind == GoalKind::GoldLines || k.goalKind == GoalKind::GoldLinesFlip ||
        k.goalKind == GoalKind::Gold || k.goalKind == GoalKind::Wall)
        Add(&out, SquaresLeftText(k, CountSquares(k)));
    const ModeHooks* other = Delegate(k);
    if (other && other->briefStatus) Add(&out, other->briefStatus(board));
    return out;
}

std::wstring QuestDetails(uintptr_t board) {
    const QuestKind& k = KindOf(board);
    std::wstring out;
    if (k.boardKind == BoardKind::MoveBombs || k.boardKind == BoardKind::TimeBombs) {
        std::vector<Bomb> bombs;
        if (ReadBombs(board, &bombs)) {
            if (bombs.empty()) Add(&out, L"no " + BombWord(), L". ");
            // Lowest counter first, as the urgent ones.
            std::vector<Bomb> left = bombs;
            while (!left.empty()) {
                size_t low = 0;
                for (size_t i = 1; i < left.size(); ++i)
                    if (left[i].counter < left[low].counter) low = i;
                const Bomb& b = left[low];
                Add(&out, BombWord() + L" " + BombCount(b.counter, b.moves) + L", " + Position(b.col, b.row),
                    L". ");
                left.erase(left.begin() + low);
            }
        }
    }
    if (k.boardKind == BoardKind::Balance) {
        BalanceState s;
        if (ReadBalance(board, &s)) Add(&out, BalanceText(s), L". ");
    }
    if (k.boardKind == BoardKind::Filler) {
        FillState s;
        if (ReadFill(board, &s)) {
            std::wstring cols;
            for (int c = 0; c < 8; ++c) cols += (c ? L", " : L"") + std::to_wstring(s.perColumn[c]);
            Add(&out, std::to_wstring(s.gems) + L" of 64 squares filled. Gems per column, left to right: " + cols,
                L". ");
            if (s.dangerKnown && s.danger) Add(&out, L"almost full", L". ");
        }
    }
    if (k.goalKind == GoalKind::GoldLines || k.goalKind == GoalKind::GoldLinesFlip ||
        k.goalKind == GoalKind::Gold || k.goalKind == GoalKind::Wall) {
        SquareCounts c = CountSquares(k);
        Add(&out, SquaresLeftText(k, c), L". ");
        if (c.ok && c.remaining > 0) Add(&out, RemainingList(k), L": ");
    }
    std::wstring moves = LowMovesText(board);
    Add(&out, moves, L". ");
    const ModeHooks* other = Delegate(k);
    if (other && other->details) Add(&out, other->details(board), L". ");
    return out;
}

// Remembered state for the tick.
struct QuestTrack {
    bool ok = false;
    std::map<int, int> bombs;  // bomb piece id -> last counter seen
    SquareCounts squares;
    bool balanceOk = false;
    int tiltStep = 0;          // highest warning step passed (0, 50, 75, 90)
    bool fillDanger = false;
    std::wstring lowMoves;
};

uintptr_t g_board = 0;
QuestTrack g_prev;

int TiltStep(int percent, int previous) {
    // Re-arm a step only when the scale went back 10 percent below it.
    const int steps[] = {90, 75, 50};
    for (int s : steps)
        if (percent >= s || (previous >= s && percent > s - 10)) return s;
    return 0;
}

void QuestTick(uintptr_t board, std::vector<std::wstring>* events) {
    if (board != g_board) {
        g_board = board;
        g_prev = QuestTrack();
    }
    const QuestKind& k = KindOf(board);
    QuestTrack now;

    std::vector<Bomb> bombs;
    bool bombsOk = (k.boardKind == BoardKind::MoveBombs || k.boardKind == BoardKind::TimeBombs) &&
                   ReadBombs(board, &bombs);
    for (const Bomb& b : bombs) now.bombs[b.id] = b.counter;

    bool squaresWanted = k.goalKind == GoalKind::GoldLines || k.goalKind == GoalKind::GoldLinesFlip ||
                         k.goalKind == GoalKind::Gold || k.goalKind == GoalKind::Wall;
    if (squaresWanted) now.squares = CountSquares(k);

    BalanceState balance;
    if (k.boardKind == BoardKind::Balance && ReadBalance(board, &balance)) {
        now.balanceOk = true;
        now.tiltStep = TiltStep(TiltPercent(balance), g_prev.tiltStep);
    }
    FillState fill;
    bool fillOk = k.boardKind == BoardKind::Filler && ReadFill(board, &fill) && fill.dangerKnown;
    if (fillOk) now.fillDanger = fill.danger;
    now.lowMoves = LowMovesText(board);
    now.ok = true;

    if (g_prev.ok) {
        const QuestTrack& was = g_prev;
        if (bombsOk) {
            for (const Bomb& b : bombs) {
                auto old = was.bombs.find(b.id);
                if (old == was.bombs.end())
                    events->push_back(L"new " + BombWord() + L", " + BombCount(b.counter, b.moves) + L", " +
                                      Position(b.col, b.row));
                else if (old->second != b.counter && BombWarning(b))
                    events->push_back(BombWord() + L" " + BombCount(b.counter, b.moves) + L", " +
                                      Position(b.col, b.row));
            }
        }
        if (squaresWanted && now.squares.ok && was.squares.ok && now.squares.remaining < was.squares.remaining)
            events->push_back(SquaresLeftText(k, now.squares));
        if (now.balanceOk && was.balanceOk && now.tiltStep > was.tiltStep) {
            std::wstring tilt = TiltText(balance);
            if (!tilt.empty()) events->push_back(tilt);
        }
        if (fillOk && now.fillDanger && !was.fillDanger) events->push_back(L"board almost full");
        if (!now.lowMoves.empty() && now.lowMoves != was.lowMoves) events->push_back(now.lowMoves);
    }
    g_prev = now;

    const ModeHooks* other = Delegate(k);
    if (other && other->tick) other->tick(board, events);
}

void QuestReset() {
    g_board = 0;
    g_prev = QuestTrack();
    g_kindBoard = g_kindGoal = 0;
    g_kind = QuestKind();
    if (ButterflyHooks().reset) ButterflyHooks().reset();
    if (DiamondMineHooks().reset) DiamondMineHooks().reset();
}

}  // namespace

const ModeHooks& QuestHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.cellText = QuestCell;
        h.briefStatus = QuestBrief;
        h.details = QuestDetails;
        h.tick = QuestTick;
        h.reset = QuestReset;
        return h;
    }();
    return hooks;
}
