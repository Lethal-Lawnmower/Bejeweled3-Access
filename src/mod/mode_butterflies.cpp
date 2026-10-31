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

// Butterflies (ButterflyBoard): butterfly gems rise from the bottom row
// towards the spider at the top; matching one with its colour frees it, a
// butterfly that moves up out of the top row escapes and the spider ends the
// game when it reaches it. Fields and evidence are in docs/modes/
// butterflies.md; all are static reads of this exe, none checked live yet.
//
// Deliberately not spoken: the spawn countdown (+0x3258) and each
// butterfly's move countdown (+0x244). Neither is drawn: only
// ButterflyBoard logic reads +0x3258, and the piece drawing 0x0074F130 reads
// +0x244 only for COUNTER (0x200) pieces, which butterflies are not.
namespace {

// Piece record (docs/re/board-objects.md).
constexpr int kPieceId = 0x04;
constexpr int kPieceColour = 0x220;
constexpr int kPieceFlags = 0x228;
constexpr uint32_t kFlagButterfly = 0x80;
// -1 once the butterfly has left the top row towards the spider (static:
// ButterflyBoard UpdateGame 0x006E82A0 sets +0x20C = -1, +0x208 = column
// when a row-0 butterfly is due to move up; the spider then heads for it
// and GameOver follows when it arrives). Meaning of other values unknown.
constexpr int kPieceEscape = 0x20C;

// Spider's horizontal position in columns, float, 0 = left column (static:
// UpdateGame compares it with the target butterfly's column +8, moves it
// there, and calls GameOver when it equals an escaped butterfly's column).
constexpr int kSpiderX = 0x3288;

constexpr int kStrButterfly = 133;  // tooltip title (Board::GetTooltipText)
constexpr int kColourNameBase = 455;

struct Butterfly {
    int id = 0, col = 0, row = 0, colour = -1;
    bool escaping = false;
};

struct ButterflyState {
    bool ok = false;
    std::vector<Butterfly> list;  // top row first, then left to right
    bool spiderKnown = false;
    float spiderX = 0.0f;
};

uintptr_t g_board = 0;
ButterflyState g_prev;
int g_spiderSaid = -1;  // spider column last announced

bool ReadFloat(uintptr_t addr, float* out) {
    uint32_t raw;
    if (!ReadU32(addr, &raw)) return false;
    std::memcpy(out, &raw, sizeof raw);
    return std::isfinite(*out);
}

bool ReadButterfly(uintptr_t piece, int index, Butterfly* out, bool* isButterfly) {
    *isButterfly = false;
    uint32_t flags;
    if (!ReadU32(piece + kPieceFlags, &flags)) return false;
    if (!(flags & kFlagButterfly)) return true;
    int32_t id, colour, escape;
    if (!ReadI32(piece + kPieceId, &id) || !ReadI32(piece + kPieceColour, &colour) ||
        !ReadI32(piece + kPieceEscape, &escape))
        return false;
    out->id = id;
    out->colour = colour;
    out->col = index % 8;
    out->row = index / 8;
    out->escaping = escape == -1;
    *isButterfly = true;
    return true;
}

bool ReadState(uintptr_t board, ButterflyState* s) {
    *s = ButterflyState();
    for (int i = 0; i < 64; ++i) {
        uint32_t piece;
        if (!ReadU32(board + board::kGrid + 4 * i, &piece)) return false;
        if (!piece) continue;
        Butterfly b;
        bool is = false;
        if (!ReadButterfly(piece, i, &b, &is)) return false;
        if (is) s->list.push_back(b);
    }
    float x;
    if (ReadFloat(board + kSpiderX, &x) && x >= -0.5f && x <= 7.5f) {
        s->spiderKnown = true;
        s->spiderX = x;
    }
    s->ok = true;
    return true;
}

// Column the spider is over, or -1 while it is between columns.
int SpiderColumn(const ButterflyState& s) {
    if (!s.spiderKnown) return -1;
    float nearest = std::floor(s.spiderX + 0.5f);
    if (std::fabs(s.spiderX - nearest) > 0.05f) return -1;
    return (int)nearest;
}

// ---- words ------------------------------------------------------------------

std::wstring Str(int id) {
    std::wstring text;
    return GameString(id, &text) ? CleanGameText(text) : L"";
}

std::wstring ColourName(int colour) {
    if (colour < 0 || colour > 6) return L"";
    return Str(kColourNameBase + colour);
}

std::wstring ButterflyWord() {
    std::wstring word = Str(kStrButterfly);
    return word.empty() ? L"butterfly" : word;
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

std::wstring ColumnName(int col) {
    if (CurrentSettings().coords == CoordStyle::Chess) return std::wstring(1, (wchar_t)(L'A' + col));
    return L"column " + std::to_wstring(col + 1);
}

// Distance to the spider in rows, as a sighted player reads the column.
std::wstring RowsFromTop(int row) {
    if (row == 0) return L"in the top row";
    return std::to_wstring(row) + (row == 1 ? L" row" : L" rows") + L" below the top row";
}

// "1 butterfly", "3 butterflies" (the mod's own words for a count).
std::wstring Count(size_t n) {
    return std::to_wstring(n) + (n == 1 ? L" butterfly" : L" butterflies");
}

std::wstring Named(const Butterfly& b) {
    std::wstring colour = ColourName(b.colour);
    return (colour.empty() ? L"" : colour + L" ") + ButterflyWord();
}

std::wstring SpiderText(const ButterflyState& s) {
    int col = SpiderColumn(s);
    if (col >= 0) return L"spider above " + ColumnName(col);
    return s.spiderKnown ? L"spider moving" : L"";
}

// ---- hooks ------------------------------------------------------------------

std::wstring ButterflyCell(uintptr_t board, uintptr_t piece, int col, int row) {
    if (!piece) return L"";
    Butterfly b;
    bool is = false;
    if (!ReadButterfly(piece, col + 8 * row, &b, &is) || !is) return L"";
    if (b.escaping) return L"escaping to the spider";
    std::wstring out = RowsFromTop(row);
    float x;
    if (ReadFloat(board + kSpiderX, &x) && std::fabs(x - (float)col) <= 0.05f) out += L", under the spider";
    return out;
}

std::wstring ButterflyBrief(uintptr_t board) {
    ButterflyState s;
    if (!ReadState(board, &s)) return L"";
    size_t n = s.list.size();
    std::wstring out = Count(n);
    if (n > 0) {
        const Butterfly& top = s.list.front();  // list is in reading order: top row first
        out += L", nearest the top " + Position(top.col, top.row);
    }
    std::wstring spider = SpiderText(s);
    if (!spider.empty()) out += L", " + spider;
    return out;
}

std::wstring ButterflyDetails(uintptr_t board) {
    ButterflyState s;
    if (!ReadState(board, &s)) return L"";
    std::wstring out = SpiderText(s);
    std::wstring list;
    for (const Butterfly& b : s.list) {
        std::wstring colour = ColourName(b.colour);
        std::wstring item = (colour.empty() ? L"unknown colour" : colour) + L" " + Position(b.col, b.row);
        if (b.escaping) item += L", escaping";
        list += (list.empty() ? L"" : L"; ") + item;
    }
    std::wstring head = Count(s.list.size());
    if (!list.empty()) head += L", from the top: " + list;
    return out.empty() ? head : out + L". " + head;
}

void ButterflyTick(uintptr_t board, std::vector<std::wstring>* events) {
    ButterflyState now;
    if (!ReadState(board, &now)) return;
    // After game over the board is cleared away; nothing more to report.
    uint8_t over = 0;
    if (ReadU8(board + board::kGameOver, &over) && over) {
        g_prev = now;
        return;
    }
    if (board != g_board || !g_prev.ok) {
        g_board = board;
        g_prev = now;
        g_spiderSaid = SpiderColumn(now);
        return;
    }
    std::map<int, Butterfly> before, after;
    for (const Butterfly& b : g_prev.list) before[b.id] = b;
    for (const Butterfly& b : now.list) after[b.id] = b;

    // Freed: a butterfly gone from the board that was not escaping.
    std::vector<std::wstring> freed;
    for (const auto& p : before)
        if (!after.count(p.first) && !p.second.escaping) freed.push_back(ColourName(p.second.colour));
    if (freed.size() == 1) {
        events->push_back((freed[0].empty() ? L"" : freed[0] + L" ") + ButterflyWord() + L" freed");
    } else if (freed.size() > 1) {
        std::wstring colours;
        for (const std::wstring& c : freed)
            if (!c.empty()) colours += (colours.empty() ? L"" : L", ") + c;
        events->push_back(Count(freed.size()) + L" freed" +
                          (colours.empty() ? L"" : L": " + colours));
    }

    for (const auto& p : after) {
        const Butterfly& b = p.second;
        auto old = before.find(p.first);
        if (old == before.end()) {
            // New butterfly (they appear in the bottom row).
            events->push_back(L"new " + Named(b) + L", " + Position(b.col, b.row));
            continue;
        }
        if (b.escaping && !old->second.escaping) {
            events->push_back(Named(b) + L" escaping to the spider, " + ColumnName(b.col));
        } else if (!b.escaping && b.row == 0 && old->second.row != 0) {
            events->push_back(Named(b) + L" reached the top row, " + Position(b.col, b.row));
        }
    }

    // The spider follows the butterfly nearest the top; say where it stops.
    int spider = SpiderColumn(now);
    if (spider >= 0 && spider != g_spiderSaid) {
        events->push_back(L"spider moves above " + ColumnName(spider));
        g_spiderSaid = spider;
    }
    g_prev = now;
}

void ButterflyReset() {
    g_board = 0;
    g_prev = ButterflyState();
    g_spiderSaid = -1;
}

}  // namespace

const ModeHooks& ButterflyHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.cellText = ButterflyCell;
        h.briefStatus = ButterflyBrief;
        h.details = ButterflyDetails;
        h.tick = ButterflyTick;
        h.reset = ButterflyReset;
        return h;
    }();
    return hooks;
}
