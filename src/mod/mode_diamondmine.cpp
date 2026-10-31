#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "board_layout.h"
#include "game_memory.h"
#include "game_strings.h"
#include "mode_info.h"
#include "text.h"

// Diamond Mine (DigBoard with a DigGoal): ground tiles in the grid (dirt,
// rocks, boulders, gold, diamonds, artifacts, buried hypercubes), the depth
// and the gold total. Tiles are named exactly as the game's own tooltip
// names them (DigGoal::GetTooltipText 0x006AC360, strings 184-203). Fields
// and evidence are in docs/modes/diamond-mine.md; all of them are static
// reads of this exe, none has been checked on a live board yet.
namespace {

constexpr int kGrid = 0xF8;               // Piece*[64], col + 8*row (board_access.cpp)
constexpr int kPieceId = 0x04;            // key of the tile map (docs/re/board-objects.md)
constexpr int kPieceFlags = 0x228;
constexpr uint32_t kFlagGround = 0x10000; // ground tile (DigGoal parser 0x006BBB10)

// std::map<int, int[8]> of tile records keyed by piece id at DigGoal+0x228
// (static: GetTooltipText 0x006AC360 calls operator[] 0x004224C0 on
// goal+0x228 with piece+4; find 0x004228C0 is compared with the head at
// goal+0x240). MSVC 2008 layout as in game_strings.cpp: head (sentinel)
// pointer at map+0x18, head->_Parent (+4) = root; node +0 left, +4 parent,
// +8 right, +0xC key, +0x10 value; _Isnil byte at +0x31 (0x004224C0 tests
// [node+0x31]).
constexpr int kGoalTiles = 0x228;
constexpr int kMapHead = 0x18;
constexpr int kNodeLeft = 0x0, kNodeParent = 0x4, kNodeRight = 0x8, kNodeKey = 0xC;
constexpr int kNodeValue = 0x10;
constexpr int kNodeIsNil = 0x31;
// Tile record (value, 8 ints; setter 0x006B3700):
//   [0] strength / treasure size, [1] its starting value, [2] kind,
//   [6] byte 1 set when a block's strength reached 0 (0x006B59F0).
constexpr int kRecStrength = 0x0;
constexpr int kRecKind = 0x8;
constexpr int kRecBrokenByte = 0x19;      // value + 0x19 = node + 0x29
constexpr int kKindArtifact = 0, kKindBlock = 1, kKindTreasure = 2;
constexpr int kDarkRock = 4;              // only special gems break it (string 199)

// Buried hypercubes: std::vector<int> of piece ids, first at goal+0x45C,
// last at goal+0x460 (static: GetTooltipText searches it with 0x00421240
// before naming a block; 0x006BB6A0 turns such a piece into a hypercube).
constexpr int kGoalCubesFirst = 0x45C;
constexpr int kGoalCubesLast = 0x460;
// Pieces that left the grid at the top during a scroll: std::vector<Piece*>,
// first goal+0x444, last goal+0x448 (static: board-flow row shift
// 0x006BE3E0; 0x006BB6A0 searches it by piece pointer).
constexpr int kGoalScrolledFirst = 0x444;
constexpr int kGoalScrolledLast = 0x448;

// Depth as drawn (static, DigGoal::DrawOverlay 0x006BD535..0x006BD575):
// trunc(depth + curve(goal+0x518) * step * (mega ? 2 : 1)) * 10, shown as
// three digits, or the letters 545-547 when it reaches 1000. The curve's
// last evaluated value is cached as a double at curve+0x38 by GetOutVal
// 0x00437C60, which the draw calls every frame. Drawn in endless mode only.
constexpr int kGoalDepth = 0x4F0;                 // double (board::kDigGoalDepthD)
constexpr int kGoalScrollCurveValue = 0x518 + 0x38;
constexpr int kGoalDigStep = 0x4FC;               // int, rows per scroll
constexpr int kGoalMega = 0x647;                  // byte, double scroll
constexpr int kDepthMax = 1000;
constexpr int kStrDepthLetters = 545;             // 545, 546, 547 left to right

// Time bonus per scroll (static: DoTimeBonus 0x00736F20, called from
// DigGoal::Update 0x006C2AD0 when a scroll starts: string 171 with
// [board+0x324C], or 170 with [board+0x3278] for the mega scroll).
constexpr int kBoardTimeBonus = 0x324C;
constexpr int kBoardMegaTimeBonus = 0x3278;
constexpr int kStrTimeBonus = 171, kStrMegaTimeBonus = 170;
constexpr int kStrGold = 173;  // "$%s" with the gold total (DrawOverlay 0x006BDEDF)

// Tooltip titles (GetTooltipText): artifact 184, gold 186, diamonds 188,
// hypercube 190, dirt 192, rocks 194, stones 196, dark rock 198, boulder
// 200 (strength 5), boulder 202 (more).
constexpr int kStrArtifact = 184, kStrGoldTile = 186, kStrDiamonds = 188, kStrHypercube = 190;
constexpr int kStrBlockByStrength[] = {0, 192, 194, 196, 198, 200};
constexpr int kStrBigBoulder = 202;
constexpr int kDiamondFrom = 4;  // treasure size < 4 is gold, else diamonds

constexpr int kSettleTicks = 10;  // depth must hold this many ticks before it is spoken

struct Tile {
    bool known = false;  // record found
    int kind = -1, strength = 0;
    bool cube = false, broken = false;
};

struct DigState {
    bool ok = false;
    uintptr_t goal = 0;
    bool perpetual = false;
    bool depthKnown = false;
    int depth = 0;       // as drawn
    bool goldKnown = false;
    int gold = 0;
    bool mega = false;
    std::map<int, std::wstring> treasures;  // piece id -> tile name, treasure/artifact tiles in the grid
};

uintptr_t g_board = 0;
uintptr_t g_goalChecked = 0;
bool g_goalIsDig = false;
DigState g_prev;
int g_spokenDepth = -1;
int g_depthStable = 0;

bool ReadDouble(uintptr_t addr, double* out) {
    uint32_t lo, hi;
    if (!ReadU32(addr, &lo) || !ReadU32(addr + 4, &hi)) return false;
    uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
    std::memcpy(out, &bits, sizeof bits);
    return std::isfinite(*out);
}

// The board's DigGoal, or 0 (RTTI checked once per goal pointer).
uintptr_t DigGoalOf(uintptr_t board) {
    uint32_t goal;
    if (!ReadU32(board + board::kQuestGoal, &goal) || goal == 0) return 0;
    if (goal != g_goalChecked) {
        g_goalChecked = goal;
        g_goalIsDig = ClassNameOf(goal) == "DigGoal";
    }
    return g_goalIsDig ? goal : 0;
}

// Read-only lookup in the tile map (never the game's operator[], which
// inserts a default record for a missing key).
bool FindTile(uintptr_t goal, int id, Tile* t) {
    *t = Tile();
    uint32_t head, node;
    if (!ReadU32(goal + kGoalTiles + kMapHead, &head) || head == 0 ||
        !ReadU32(head + kNodeParent, &node))
        return false;
    for (int depth = 0; depth < 64; ++depth) {
        uint8_t nil;
        if (node == 0 || node == head || !ReadU8(node + kNodeIsNil, &nil) || nil) return false;
        int32_t key;
        if (!ReadI32(node + kNodeKey, &key)) return false;
        if (key == id) {
            uintptr_t v = node + kNodeValue;
            uint8_t broken;
            if (!ReadI32(v + kRecStrength, &t->strength) || !ReadI32(v + kRecKind, &t->kind) ||
                !ReadU8(v + kRecBrokenByte, &broken))
                return false;
            t->broken = broken != 0;
            t->known = true;
            return true;
        }
        uint32_t next;
        if (!ReadU32(node + (id < key ? kNodeLeft : kNodeRight), &next)) return false;
        node = next;
    }
    return false;
}

bool IsBuriedCube(uintptr_t goal, int id, bool* cube) {
    uint32_t first, last;
    if (!ReadU32(goal + kGoalCubesFirst, &first) || !ReadU32(goal + kGoalCubesLast, &last) ||
        last < first || (last - first) % 4 != 0 || (last - first) / 4 > 64)
        return false;
    *cube = false;
    for (uint32_t p = first; p < last; p += 4) {
        int32_t v;
        if (!ReadI32(p, &v)) return false;
        if (v == id) *cube = true;
    }
    return true;
}

// Tile name the game's tooltip shows for this record (0x006AC360 order).
std::wstring TileName(const Tile& t) {
    int id = 0;
    if (t.kind == kKindArtifact) id = kStrArtifact;
    else if (t.kind == kKindTreasure) id = t.strength < kDiamondFrom ? kStrGoldTile : kStrDiamonds;
    else if (t.kind == kKindBlock && t.cube) id = kStrHypercube;
    else if (t.kind == kKindBlock && t.strength >= 1 && t.strength <= 5) id = kStrBlockByStrength[t.strength];
    else if (t.kind == kKindBlock) id = kStrBigBoulder;  // the tooltip's default case
    std::wstring name;
    if (id == 0 || !GameString(id, &name)) return L"";
    return CleanGameText(name);
}

// Matches still needed next to a block, from the damage rule in 0x006B59F0:
// each hit takes 1, or 2 while the strength is above 4; dark rock (4) is not
// hit by plain matches. Left out where the count is not certain.
std::wstring HitsText(const Tile& t) {
    if (t.kind != kKindBlock || t.cube || t.broken) return L"";
    int hits = 0;
    if (t.strength >= 1 && t.strength <= 3) hits = t.strength;
    else if (t.strength == 5) hits = 4;  // 5 -> 3 -> 2 -> 1 -> 0 (string 201 agrees)
    else if (t.strength == kDarkRock) return L"special gems only";
    if (hits <= 0) return L"";
    return std::to_wstring(hits) + (hits == 1 ? L" hit" : L" hits");
}

std::wstring TileText(uintptr_t goal, uintptr_t piece) {
    uint32_t flags;
    int32_t id;
    if (!piece || !ReadU32(piece + kPieceFlags, &flags) || !(flags & kFlagGround) ||
        !ReadI32(piece + kPieceId, &id))
        return L"";
    Tile t;
    if (!FindTile(goal, id, &t)) return L"";
    if (t.kind == kKindBlock && !IsBuriedCube(goal, id, &t.cube)) return L"";
    std::wstring name = TileName(t);
    if (name.empty()) return L"";
    std::wstring hits = HitsText(t);
    return hits.empty() ? name : name + L", " + hits;
}

std::wstring GameFormat(int id, const std::wstring& value) {
    std::wstring raw;
    if (!GameString(id, &raw)) return L"";
    for (size_t i = 0; i + 1 < raw.size(); ++i) {
        if (raw[i] == L'%' && (raw[i + 1] == L'd' || raw[i + 1] == L's' || raw[i + 1] == L'i')) {
            raw.replace(i, 2, value);
            break;
        }
    }
    return CleanGameText(raw);
}

std::wstring DepthText(int depth) {
    if (depth < kDepthMax) return L"depth " + std::to_wstring(depth);
    std::wstring letters;
    for (int k = 0; k < 3; ++k) {
        std::wstring s;
        if (!GameString(kStrDepthLetters + k, &s)) return L"";
        letters += CleanGameText(s);
    }
    return L"depth " + letters;
}

std::wstring GoldText(int gold) {
    std::wstring text = GameFormat(kStrGold, std::to_wstring(gold));
    return text.empty() ? L"gold " + std::to_wstring(gold) : text;
}

bool ReadState(uintptr_t board, DigState* s) {
    *s = DigState();
    uintptr_t goal = DigGoalOf(board);
    uint8_t perpetual;
    if (!goal || !ReadU8(board + board::kPerpetual, &perpetual)) return false;
    s->goal = goal;
    s->perpetual = perpetual != 0;
    s->goldKnown = ReadI32(board + board::kLevelPoints, &s->gold);

    double depth, curve;
    int32_t step;
    uint8_t mega;
    if (ReadDouble(goal + kGoalDepth, &depth) && ReadDouble(goal + kGoalScrollCurveValue, &curve) &&
        ReadI32(goal + kGoalDigStep, &step) && ReadU8(goal + kGoalMega, &mega)) {
        s->mega = mega != 0;
        double shown = depth + curve * step * (s->mega ? 2 : 1);
        if (shown >= 0.0 && shown < 1.0e6) {
            s->depth = static_cast<int>(shown) * 10;  // _ftol2 truncation, then * 10
            s->depthKnown = true;
        }
    }

    for (int i = 0; i < 64; ++i) {
        uint32_t piece, flags;
        int32_t id;
        if (!ReadU32(board + kGrid + 4 * i, &piece) || !piece) continue;
        if (!ReadU32(piece + kPieceFlags, &flags) || !(flags & kFlagGround) ||
            !ReadI32(piece + kPieceId, &id))
            continue;
        Tile t;
        if (FindTile(goal, id, &t) && (t.kind == kKindTreasure || t.kind == kKindArtifact)) {
            std::wstring name = TileName(t);
            if (!name.empty()) s->treasures[id] = name;
        }
    }
    s->ok = true;
    return true;
}

// Ids of the pieces that left the grid at the top during scrolls.
std::set<int> ScrolledOffIds(uintptr_t goal) {
    std::set<int> ids;
    uint32_t first, last;
    if (!ReadU32(goal + kGoalScrolledFirst, &first) || !ReadU32(goal + kGoalScrolledLast, &last) ||
        last < first || (last - first) % 4 != 0 || (last - first) / 4 > 4096)
        return ids;
    for (uint32_t p = first; p < last; p += 4) {
        uint32_t piece;
        int32_t id;
        if (ReadU32(p, &piece) && piece && ReadI32(piece + kPieceId, &id)) ids.insert(id);
    }
    return ids;
}

// ---- hooks -----------------------------------------------------------------

std::wstring DigCell(uintptr_t board, uintptr_t piece, int, int) {
    uintptr_t goal = DigGoalOf(board);
    return goal ? TileText(goal, piece) : L"";
}

std::wstring DigBrief(uintptr_t board) {
    DigState s;
    if (!ReadState(board, &s)) return L"";
    // Depth and gold are drawn in endless mode; a quest shows its goal,
    // which F6 already reads as progress of target.
    if (!s.perpetual) return L"";
    std::wstring out;
    if (s.depthKnown) out = DepthText(s.depth);
    if (s.goldKnown) out += (out.empty() ? L"" : L", ") + GoldText(s.gold);
    return out;
}

std::wstring DigDetails(uintptr_t board) {
    uintptr_t goal = DigGoalOf(board);
    if (!goal) return L"mine not readable";
    DigState s;
    if (!ReadState(board, &s)) return L"mine not readable";
    std::wstring out;
    if (s.perpetual && s.depthKnown) out = DepthText(s.depth);
    if (s.perpetual && s.goldKnown) out += (out.empty() ? L"" : L", ") + GoldText(s.gold);
    int tiles = 0;
    for (int row = 0; row < 8; ++row) {
        // Runs of equal tiles in a row: "columns 1 to 3 DIRT, 1 hit".
        std::wstring rowText, runText;
        int runStart = -1;
        auto flush = [&](int endCol) {
            if (runStart < 0) return;
            std::wstring where = runStart == endCol
                                     ? L"column " + std::to_wstring(runStart + 1)
                                     : L"columns " + std::to_wstring(runStart + 1) + L" to " +
                                           std::to_wstring(endCol + 1);
            rowText += (rowText.empty() ? L"" : L"; ") + where + L" " + runText;
            runStart = -1;
        };
        for (int col = 0; col < 8; ++col) {
            uint32_t piece = 0;
            std::wstring text;
            if (ReadU32(board + kGrid + 4 * (col + 8 * row), &piece) && piece) text = TileText(goal, piece);
            if (!text.empty()) ++tiles;
            if (runStart >= 0 && text != runText) flush(col - 1);
            if (!text.empty() && runStart < 0) {
                runStart = col;
                runText = text;
            }
        }
        flush(7);
        if (!rowText.empty())
            out += std::wstring(out.empty() ? L"" : L". ") + L"row " + std::to_wstring(row + 1) + L": " + rowText;
    }
    if (tiles == 0) out += (out.empty() ? L"" : L". ") + std::wstring(L"no tiles on the board");
    return out;
}

void DigTick(uintptr_t board, std::vector<std::wstring>* events) {
    if (board != g_board) {
        g_board = board;
        g_prev = DigState();
        g_spokenDepth = -1;
        g_depthStable = 0;
    }
    DigState now;
    if (!ReadState(board, &now)) return;
    if (!g_prev.ok) {
        g_prev = now;
        g_spokenDepth = now.depthKnown ? now.depth : -1;
        return;
    }
    const DigState& was = g_prev;

    // Treasure and artifact tiles that left the grid other than by scrolling
    // off the top were dug up.
    std::set<int> scrolled;
    bool scrolledRead = false;
    for (const auto& t : was.treasures) {
        if (now.treasures.count(t.first)) continue;
        if (!scrolledRead) {
            scrolled = ScrolledOffIds(now.goal);
            scrolledRead = true;
        }
        if (!scrolled.count(t.first)) events->push_back(t.second + L" dug up");
    }

    // Gold total rising.
    if (now.goldKnown && was.goldKnown && now.gold > was.gold)
        events->push_back(L"plus " + std::to_wstring(now.gold - was.gold) + L", " + GoldText(now.gold));

    // Depth, once the scroll has finished rolling the display (endless only).
    if (now.perpetual && now.depthKnown) {
        if (was.depthKnown && now.depth == was.depth) ++g_depthStable;
        else g_depthStable = 0;
        if (g_depthStable == kSettleTicks && g_spokenDepth >= 0 && now.depth > g_spokenDepth) {
            std::wstring text;
            int32_t bonus;
            int id = now.mega ? kStrMegaTimeBonus : kStrTimeBonus;
            if (ReadI32(board + (now.mega ? kBoardMegaTimeBonus : kBoardTimeBonus), &bonus) && bonus > 0)
                text = GameFormat(id, std::to_wstring(bonus));
            std::wstring depth = DepthText(now.depth);
            events->push_back(text.empty() ? depth : text + L", " + depth);
        }
        if (g_depthStable >= kSettleTicks) g_spokenDepth = now.depth;
    }
    g_prev = now;
}

void DigReset() {
    g_board = 0;
    g_goalChecked = 0;
    g_goalIsDig = false;
    g_prev = DigState();
    g_spokenDepth = -1;
    g_depthStable = 0;
}

}  // namespace

const ModeHooks& DiamondMineHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.cellText = DigCell;
        h.briefStatus = DigBrief;
        h.details = DigDetails;
        h.tick = DigTick;
        h.reset = DigReset;
        return h;
    }();
    return hooks;
}
