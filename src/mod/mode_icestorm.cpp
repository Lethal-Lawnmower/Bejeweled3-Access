#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "board_layout.h"
#include "game_memory.h"
#include "game_strings.h"
#include "mode_info.h"
#include "text.h"

// Ice Storm (InfernoBoard): ice columns rising from the bottom of the board.
// What a sighted player sees: which columns have ice, how high each one is,
// a column cracking once it is over the top, a column crushed by a vertical
// match, the combo and multiplier texts. Fields and evidence are in
// docs/modes/ice-storm.md; all of them are static reads of this exe, none
// has been checked on a live board yet.
namespace {

// Eight column records, stride 0x348, from Board+0x3D28 (static: the board
// ctor 0x006E2D90 builds them with _eh_vector_constructor_iterator_
// (board+0x3D28, 0x348, 8, 0x006CBE90, ...); the dtor frees the same range).
constexpr int kColumns = 0x3D28;
constexpr int kColumnStride = 0x348;
constexpr int kColumnCount = 8;
// Ice height in rows, float, counted from the bottom of the board: the
// column draw 0x006D5F10 / 0x006D63B0 places the ice top at
// (8.0 - height) * 128 px from the board top (disassembly 0x006D66EC..
// 0x006D671A: fld 8.0, fsub height, fmul 128.0). Written by vtable+0x42C
// 0x006C4F20 from the rise logic in 0x006E0CC0.
constexpr int kColHeight = 0x0C;
// Record index that owns (draws) this column slot: a slot is drawn only when
// it equals its own index (draw loops in 0x006D5F10 / 0x006D63B0 test
// [rec+0x1C] == i). A two-wide column sets both slots to the left one
// (activation 0x006E0650).
constexpr int kColLead = 0x1C;
// Width in grid columns, 1 or 2 (static: 0x006E0650 stores the width it is
// given here and loops over that many records; the draw uses a different
// offset and animation when it is 2).
constexpr int kColWidth = 0x24;
// Updates spent over the top, -1 while the ice is at or below the top
// (static: 0x006E0CC0 resets it to -1 when height <= 8.0, sets 0 and then
// counts up while height > 8.0).
constexpr int kColOverTop = 0x34;
constexpr int kColActive = 0x45;   // byte: column has ice (set by 0x006E0650)
constexpr int kColCrushed = 0x46;  // byte: crushed by a vertical match (0x006DB1A0)
// Per-column danger rate, double (static: copied from Board+0x3688 when the
// column starts, 0x006E0650; used by the crack ratio 0x006CE540).
constexpr int kColDangerRate = 0x48;
// Seconds over the top before the board is lost, int (static: divisor in
// the crack ratio 0x006CE540). Crack ratio = min(1, overTop / 100 /
// seconds * rate); the draw passes 0 for the extra term (EBX = 0 at
// 0x006D6725), the loss check in 0x006E0CC0 ends the game once it is > 1.
constexpr int kSecondsToLose = 0x3CBC;
// Top of the board in rows (constant 8.0 at 0x00880F88, used by the danger
// test vtable+0x368 0x006C5C20 and 0x006E0CC0).
constexpr float kTopRows = 8.0f;
// Mod default, not the game's: say "near the top" once a column has this
// many rows of ice, again after it fell below kNearTopReset.
constexpr float kNearTopRows = 6.0f;
constexpr float kNearTopReset = 5.5f;

// Consecutive crushes in the current combo (static: IncrementColCleared
// 0x006D6D60 increments [+0x36C8], resets it when the combo window has
// passed, and from 3 on shows the combo texts, endless boards only).
constexpr int kComboCount = 0x36C8;
constexpr int kComboFirstNamed = 4;    // count 4 -> string 549 ... 12 -> 557
constexpr int kStrComboNameBase = 549;
constexpr int kStrComboNameMax = 558;  // count 13 and above
constexpr int kStrCombo = 559;         // "%s..." with the name (empty for 3)
constexpr int kStrComboTimes = 560;    // "%d..." with the count
constexpr int kStrMultiplier = 561;    // RefreshMultiplierText 0x006CC010

struct Column {
    bool ok = false;      // record readable
    float height = 0.0f;
    int lead = -1, width = 0, overTop = -1;
    bool active = false, crushed = false;
    double rate = 0.0;
};

struct IceState {
    bool ok = false;
    Column cols[kColumnCount];
    int secondsToLose = 0;
    bool perpetual = false;
    int combo = 0;
    int multiplier = 0;
};

uintptr_t g_board = 0;
IceState g_prev;
int g_zone[kColumnCount] = {};  // per lead column: 0 low, 1 near the top, 2 over the top

bool ReadFloat(uintptr_t addr, float* out) {
    uint32_t raw;
    if (!ReadU32(addr, &raw)) return false;
    std::memcpy(out, &raw, sizeof raw);
    return std::isfinite(*out);
}

bool ReadDouble(uintptr_t addr, double* out) {
    uint32_t lo, hi;
    if (!ReadU32(addr, &lo) || !ReadU32(addr + 4, &hi)) return false;
    uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
    std::memcpy(out, &bits, sizeof bits);
    return std::isfinite(*out);
}

bool ReadColumn(uintptr_t board, int i, Column* c) {
    *c = Column();
    uintptr_t rec = board + kColumns + i * kColumnStride;
    uint8_t active, crushed;
    if (!ReadFloat(rec + kColHeight, &c->height) || !ReadI32(rec + kColLead, &c->lead) ||
        !ReadI32(rec + kColWidth, &c->width) || !ReadI32(rec + kColOverTop, &c->overTop) ||
        !ReadU8(rec + kColActive, &active) || !ReadU8(rec + kColCrushed, &crushed))
        return false;
    if (!ReadDouble(rec + kColDangerRate, &c->rate)) c->rate = 0.0;  // crack ratio then unknown
    c->active = active != 0;
    c->crushed = crushed != 0;
    c->ok = true;
    return true;
}

bool ReadState(uintptr_t board, IceState* s) {
    *s = IceState();
    uint8_t perpetual;
    if (!ReadU8(board + board::kPerpetual, &perpetual)) return false;
    s->perpetual = perpetual != 0;
    for (int i = 0; i < kColumnCount; ++i)
        if (!ReadColumn(board, i, &s->cols[i])) return false;
    if (!ReadI32(board + kSecondsToLose, &s->secondsToLose)) s->secondsToLose = 0;
    if (!ReadI32(board + kComboCount, &s->combo)) s->combo = 0;
    if (!ReadI32(board + board::kMultiplier, &s->multiplier)) s->multiplier = 0;
    s->ok = true;
    return true;
}

// A record that draws itself (lead == own index) with a valid width.
bool IsLead(const IceState& s, int i) {
    const Column& c = s.cols[i];
    return c.ok && c.lead == i && (c.width == 1 || (c.width == 2 && i + 1 < kColumnCount));
}

// Ice a sighted player sees: a drawn column that is not breaking apart.
bool HasIce(const IceState& s, int i) {
    return IsLead(s, i) && s.cols[i].active && !s.cols[i].crushed && s.cols[i].height > 0.0f;
}

// The lead record whose ice covers grid column col, or -1.
int LeadFor(const IceState& s, int col) {
    if (col < 0 || col >= kColumnCount || !s.cols[col].ok) return -1;
    int lead = s.cols[col].lead;
    if (lead < 0 || lead >= kColumnCount || !IsLead(s, lead)) return -1;
    if (col < lead || col >= lead + s.cols[lead].width) return -1;
    return lead;
}

std::wstring ColumnName(const IceState& s, int lead) {
    if (s.cols[lead].width == 2)
        return L"columns " + std::to_wstring(lead + 1) + L" and " + std::to_wstring(lead + 2);
    return L"column " + std::to_wstring(lead + 1);
}

// Height to the nearest half row, e.g. "3 and a half rows".
std::wstring RowsText(float height) {
    int halves = static_cast<int>(std::floor(height * 2.0f + 0.5f));
    if (halves <= 0) return L"less than half a row";
    int whole = halves / 2;
    bool half = (halves % 2) != 0;
    if (whole == 0) return L"half a row";
    std::wstring out = std::to_wstring(whole);
    if (half) return out + L" and a half rows";
    return out + (whole == 1 ? L" row" : L" rows");
}

// How far the crack has spread once a column is over the top, 0..100, or
// -1 when it cannot be computed (replicates 0x006CE540 with the draw's
// extra term 0).
int CrackPercent(const IceState& s, int lead) {
    const Column& c = s.cols[lead];
    if (c.overTop < 0 || s.secondsToLose <= 0 || c.rate <= 0.0) return -1;
    double ratio = (c.overTop / 100.0) / s.secondsToLose * c.rate;
    if (ratio > 1.0) ratio = 1.0;
    return static_cast<int>(ratio * 100.0 + 0.5);
}

std::wstring ColumnText(const IceState& s, int lead) {
    std::wstring out = ColumnName(s, lead) + L", " + RowsText(s.cols[lead].height);
    if (s.cols[lead].height > kTopRows) {
        out += L", over the top";
        int crack = CrackPercent(s, lead);
        if (crack >= 0) out += L", cracked " + std::to_wstring(crack) + L" percent";
    }
    return out;
}

int HighestLead(const IceState& s) {
    int best = -1;
    for (int i = 0; i < kColumnCount; ++i)
        if (HasIce(s, i) && (best < 0 || s.cols[i].height > s.cols[best].height)) best = i;
    return best;
}

// Game text with its first %d / %s / %i replaced by value, then cleaned.
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

// The two combo texts IncrementColCleared shows for a combo of count, e.g.
// the combo name and the count text; empty below 3.
std::wstring ComboText(int count) {
    if (count < 3) return L"";
    std::wstring name;
    if (count >= kComboFirstNamed) {
        int id = count >= kComboFirstNamed + (kStrComboNameMax - kStrComboNameBase)
                     ? kStrComboNameMax
                     : kStrComboNameBase + (count - kComboFirstNamed);
        if (!GameString(id, &name)) return L"";  // raw: may carry the game's spacing entity
    }
    std::wstring combo = GameFormat(kStrCombo, name);
    std::wstring times = GameFormat(kStrComboTimes, std::to_wstring(count));
    if (combo.empty()) return times;
    return times.empty() ? combo : combo + L", " + times;
}

// ---- hooks -----------------------------------------------------------------

std::wstring IceCell(uintptr_t board, uintptr_t piece, int col, int row) {
    if (!piece || row < 0 || row > 7) return L"";
    IceState s;
    if (!ReadState(board, &s)) return L"";
    int lead = LeadFor(s, col);
    if (lead < 0 || !HasIce(s, lead)) return L"";
    // The ice top is at row coordinate 8 - height from the top; the cell
    // spans row .. row + 1. Say "in ice" when it is fully covered, "partly
    // in ice" when at least half of it is.
    float top = kTopRows - s.cols[lead].height;
    float covered = (row + 1) - top;
    if (covered >= 1.0f) return L"in ice";
    if (covered >= 0.5f) return L"partly in ice";
    return L"";
}

std::wstring IceBrief(uintptr_t board) {
    IceState s;
    if (!ReadState(board, &s)) return L"";
    int best = HighestLead(s);
    if (best < 0) return L"no ice";
    return L"highest ice " + ColumnText(s, best);
}

std::wstring IceDetails(uintptr_t board) {
    IceState s;
    if (!ReadState(board, &s)) return L"ice columns not readable";
    std::wstring out = L"Ice";
    std::wstring none;  // grid columns without ice, e.g. "2, 5"
    int noneCount = 0;
    int count = 0;
    for (int i = 0; i < kColumnCount; ++i) {
        if (HasIce(s, i)) {
            out += L". " + ColumnText(s, i);
            ++count;
        } else if (IsLead(s, i) && s.cols[i].crushed) {
            out += L". " + ColumnName(s, i) + L", breaking";
        }
    }
    for (int c = 0; c < kColumnCount; ++c) {
        int lead = LeadFor(s, c);
        if (lead < 0 || !HasIce(s, lead)) {
            none += (none.empty() ? L"" : L", ") + std::to_wstring(c + 1);
            ++noneCount;
        }
    }
    if (count == 0) return L"no ice on the board";
    out += L". Highest " + ColumnName(s, HighestLead(s));
    if (noneCount > 0) out += (noneCount == 1 ? L". No ice in column " : L". No ice in columns ") + none;
    return out;
}

void IceTick(uintptr_t board, std::vector<std::wstring>* events) {
    if (board != g_board) {
        g_board = board;
        g_prev = IceState();
        for (int& z : g_zone) z = 0;
    }
    IceState now;
    if (!ReadState(board, &now)) return;
    if (!g_prev.ok) {
        g_prev = now;
        for (int i = 0; i < kColumnCount; ++i) {
            float h = HasIce(now, i) ? now.cols[i].height : 0.0f;
            g_zone[i] = h > kTopRows ? 2 : h >= kNearTopRows ? 1 : 0;
        }
        return;
    }
    const IceState& was = g_prev;

    for (int i = 0; i < kColumnCount; ++i) {
        bool iceNow = HasIce(now, i), iceWas = HasIce(was, i);
        // A column crushed by a vertical match (with the combo the game shows).
        if (IsLead(now, i) && now.cols[i].crushed && !(IsLead(was, i) && was.cols[i].crushed)) {
            std::wstring text = ColumnName(now, i) + L" crushed";
            if (now.perpetual && now.combo != was.combo) {
                std::wstring combo = ComboText(now.combo);
                if (!combo.empty()) text += L", " + combo;
            }
            events->push_back(text);
        }
        // New ice starting in a column.
        if (iceNow && !iceWas && !now.cols[i].crushed)
            events->push_back(L"new ice, " + ColumnName(now, i));
        // Zone changes as the ice rises; each said once until it falls back.
        float h = iceNow ? now.cols[i].height : 0.0f;
        int zone = g_zone[i];
        if (h > kTopRows) {
            if (zone < 2) events->push_back(ColumnName(now, i) + L" over the top");
            zone = 2;
        } else if (h >= kNearTopRows) {
            if (zone < 1) events->push_back(ColumnName(now, i) + L" near the top, " + RowsText(h));
            zone = 1;
        } else if (h < kNearTopReset) {
            zone = 0;
        } else if (zone == 2) {
            zone = 1;  // back below the top but still high
        }
        g_zone[i] = zone;
    }

    // Multiplier raised (the game shows its multiplier text).
    if (now.multiplier > was.multiplier && now.multiplier > 1) {
        std::wstring text = GameFormat(kStrMultiplier, std::to_wstring(now.multiplier));
        events->push_back(text.empty() ? L"multiplier " + std::to_wstring(now.multiplier) : text);
    }
    g_prev = now;
}

void IceReset() {
    g_board = 0;
    g_prev = IceState();
    for (int& z : g_zone) z = 0;
}

}  // namespace

const ModeHooks& IceStormHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.cellText = IceCell;
        h.briefStatus = IceBrief;
        h.details = IceDetails;
        h.tick = IceTick;
        h.reset = IceReset;
        return h;
    }();
    return hooks;
}
