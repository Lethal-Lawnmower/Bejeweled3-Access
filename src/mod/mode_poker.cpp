#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "board_layout.h"
#include "game_memory.h"
#include "game_strings.h"
#include "mode_info.h"
#include "text.h"

// Poker (PokerBoard): the five-card hand above the board, the score table
// of seven hand types with their values or skulls, the skull eliminator bar
// (endless) or the hands left (quest). Fields and evidence are in
// docs/modes/poker.md; all of them are static reads of this exe, none has
// been checked on a live board yet.
namespace {

// Hand record: 5 cards, stride 0xF8, from +0x3248 (static: AddCard
// 0x006CF460 writes [0xC92 + n*0x3E] colour, [0xC93 + ...] second colour,
// [0xC94 + ...] kind, then increments [0xDC8]; reset in 0x006CDCE0 sets
// colours to -2 and kind 0).
constexpr int kCards = 0x3248;
constexpr int kCardStride = 0xF8;
constexpr int kCardColour = 0x0;   // 0..6 (wild cards: rewritten by the evaluator)
constexpr int kCardSecond = 0x4;   // second colour when two gems made one card, -2 none
constexpr int kCardKind = 0x8;     // 0 plain, 1 hypercube, 2 flame gem, 3 star gem
constexpr int kCardCount = 0x3720; // cards in the hand, 0..5
constexpr int kMaxCards = 5;

// Hand type the cards make now, -1 none; the score table highlights that row
// (static: AddCard stores evaluator 0x006C80A0 result - 1; DrawTopWidget
// 0x006D4300 draws at that row when >= 0; reset to -1 in 0x006CDCE0).
constexpr int kHandType = 0x3A30;
constexpr int kHandTypes = 7;      // order of strings 588-594 (PokerBoard ctor 0x006DA1C0)
constexpr int kHandNameBase = 588;

// Highest skulled hand type, -1 none: rows 0..this show a skull instead of
// the hand value (static: DrawTopWidget row loop; 0x006CDCE0 adds a skull
// up to SkullMax +0x3990 and the eliminator removes one).
constexpr int kSkullTop = 0x398C;
// Hand values shown on the score table, std::vector<int> begin/end
// (static: DrawTopWidget prints [begin + 4*row] with "%i"; 0x006CB520
// passes [begin + 4*type] to TallyScore as the hand's points).
constexpr int kValuesBegin = 0x39A8;
constexpr int kValuesEnd = 0x39AC;
// Skull eliminator fill, float 0..1 (static: TallyScore 0x006C7560 adds the
// hand's eliminator value while skulls exist, capped at 1; 0x006CDCE0 removes
// a skull when it is full). Drawn by 0x006D1660 in endless mode only.
constexpr int kEliminator = 0x3984;
// Quest hands: remaining (decremented per finished hand in 0x006CDCE0, game
// over below 0) and the starting "Hands" value (init 0x006DFA40). The top
// widget shows min of the two with "Left", or "Last Hand" when the
// remaining count is below 1 (DrawTopWidget, non-endless branch).
constexpr int kHandsLeft = 0x396C;
constexpr int kHandsStart = 0x3978;
// Skull coin flips so far, then per hand type made (static: 0x006CB430
// increments +0x39E4 when a flip starts; 0x006CB520 increments
// +0x39E4 + 4*(type+1) after TallyScore). The flip's outcome byte
// (+0x3B90) is decided at the start but hidden until the coin lands, so it
// is deliberately not read; the game's own announcement says SAFE.
constexpr int kCoinFlips = 0x39E4;
constexpr int kTypeCounts = 0x39E8;
// Points of the hand just scored, 0 = no score (static: TallyScore).
constexpr int kLastPoints = 0x3A2C;

// Game strings (docs/re: default.xml ids; not copied here).
constexpr int kStrNeedBetter = 377;   // "... %s ..." over the board (DrawGameElements 0x006CB6D0)
constexpr int kStrLeftOne = 380;      // DrawTopWidget, fewer than 2 hands
constexpr int kStrLastHand = 381;
constexpr int kStrNoScore = 385;      // TallyScore
constexpr int kStrLeftMany = 567;
constexpr int kStrEliminator = 568;   // tooltip title of the eliminator bar
constexpr int kStrSkull = 597;        // DrawOverlay
constexpr int kStrFlame = 119, kStrStar = 121, kStrHypercube = 123;  // gem tooltip titles
constexpr int kColourNameBase = 455;

struct Card {
    int colour = -2, second = -2, kind = 0;
};

struct PokerState {
    bool ok = false;
    int count = 0;
    Card cards[kMaxCards];
    int handType = -1;
    int skullTop = -1;
    bool endless = false;
    bool handsKnown = false;
    int handsLeft = 0, handsStart = 0;
    int coinFlips = 0;
    int typeCounts[kHandTypes] = {};
    int lastPoints = 0;
    bool eliminatorKnown = false;
    float eliminator = 0.0f;
    std::vector<int> values;  // empty if unreadable
};

uintptr_t g_board = 0;
PokerState g_prev;

bool ReadFloat(uintptr_t addr, float* out) {
    uint32_t raw;
    if (!ReadU32(addr, &raw)) return false;
    std::memcpy(out, &raw, sizeof raw);
    return std::isfinite(*out);
}

bool ReadState(uintptr_t board, PokerState* s) {
    *s = PokerState();
    int32_t count, type, skull;
    uint8_t perpetual;
    if (!ReadI32(board + kCardCount, &count) || count < 0 || count > kMaxCards ||
        !ReadI32(board + kHandType, &type) || !ReadI32(board + kSkullTop, &skull) ||
        !ReadU8(board + board::kPerpetual, &perpetual) ||
        !ReadI32(board + kCoinFlips, &s->coinFlips) || !ReadI32(board + kLastPoints, &s->lastPoints))
        return false;
    s->count = count;
    s->handType = (type >= 0 && type < kHandTypes) ? type : -1;
    s->skullTop = (skull >= 0 && skull < kHandTypes) ? skull : -1;
    s->endless = perpetual != 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t c = board + kCards + i * kCardStride;
        if (!ReadI32(c + kCardColour, &s->cards[i].colour) || !ReadI32(c + kCardSecond, &s->cards[i].second) ||
            !ReadI32(c + kCardKind, &s->cards[i].kind))
            return false;
    }
    for (int t = 0; t < kHandTypes; ++t)
        if (!ReadI32(board + kTypeCounts + 4 * t, &s->typeCounts[t])) return false;
    s->handsKnown = ReadI32(board + kHandsLeft, &s->handsLeft) && ReadI32(board + kHandsStart, &s->handsStart);
    s->eliminatorKnown = ReadFloat(board + kEliminator, &s->eliminator);
    uint32_t vb, ve;
    if (ReadU32(board + kValuesBegin, &vb) && ReadU32(board + kValuesEnd, &ve) && vb && ve > vb &&
        (ve - vb) / 4 == kHandTypes && (ve - vb) % 4 == 0) {
        for (int t = 0; t < kHandTypes; ++t) {
            int32_t v;
            if (!ReadI32(vb + 4 * t, &v)) {
                s->values.clear();
                break;
            }
            s->values.push_back(v);
        }
    }
    s->ok = true;
    return true;
}

// ---- names, from the game ---------------------------------------------------

std::wstring Str(int id) {
    std::wstring text;
    return GameString(id, &text) ? CleanGameText(text) : L"";
}

std::wstring ColourName(int colour) {
    if (colour < 0 || colour > 6) return L"";
    return Str(kColourNameBase + colour);
}

std::wstring HandName(int type) {
    if (type < 0 || type >= kHandTypes) return L"";
    std::wstring name = Str(kHandNameBase + type);
    return name.empty() ? L"hand " + std::to_wstring(type + 1) : name;
}

std::wstring CardText(const Card& c) {
    if (c.kind == 1) {
        std::wstring cube = Str(kStrHypercube);
        return cube.empty() ? L"hypercube" : cube;
    }
    std::wstring out = ColourName(c.colour);
    if (out.empty()) out = L"unknown card";
    std::wstring second = ColourName(c.second);
    if (!second.empty() && c.second != c.colour) out += L" or " + second;
    std::wstring special = c.kind == 2 ? Str(kStrFlame) : c.kind == 3 ? Str(kStrStar) : L"";
    if (!special.empty()) out += L" " + special;
    return out;
}

// "Need <next hand> or better", the warning the game draws over the board
// while the current hand is skulled (DrawGameElements 0x006CB6D0 fills %s
// with the hand above the highest skull).
std::wstring NeedBetterText(const PokerState& s) {
    if (s.skullTop < 0 || s.handType < 0 || s.handType > s.skullTop || s.skullTop + 1 >= kHandTypes)
        return L"";
    std::wstring format = Str(kStrNeedBetter);
    size_t at = format.find(L"%s");
    if (at == std::wstring::npos) return L"";
    return format.replace(at, 2, HandName(s.skullTop + 1));
}

// Hands left as the quest's top widget shows it; empty in endless mode.
std::wstring HandsLeftText(const PokerState& s) {
    if (s.endless || !s.handsKnown) return L"";
    if (s.handsLeft < 1) return Str(kStrLastHand);
    int shown = s.handsLeft < s.handsStart ? s.handsLeft : s.handsStart;
    std::wstring left = Str(shown < 2 ? kStrLeftOne : kStrLeftMany);
    if (left.empty()) return L"";
    return std::to_wstring(shown) + (shown == 1 ? L" hand " : L" hands ") + left;
}

std::wstring EliminatorText(const PokerState& s) {
    if (!s.endless || !s.eliminatorKnown) return L"";
    std::wstring name = Str(kStrEliminator);
    if (name.empty()) return L"";
    int percent = (int)(s.eliminator * 100.0f + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return name + L" " + std::to_wstring(percent) + L" percent";
}

std::wstring SkullWord() {
    std::wstring skull = Str(kStrSkull);
    return skull.empty() ? L"skull" : skull;
}

void Add(std::wstring* out, const std::wstring& part, const wchar_t* sep = L", ") {
    if (part.empty()) return;
    if (!out->empty()) *out += sep;
    *out += part;
}

// ---- hooks ------------------------------------------------------------------

std::wstring PokerBrief(uintptr_t board) {
    PokerState s;
    if (!ReadState(board, &s)) return L"";
    std::wstring out = L"hand " + std::to_wstring(s.count) + L" of " + std::to_wstring(kMaxCards);
    if (s.count > 0 && s.handType >= 0) Add(&out, HandName(s.handType));
    if (s.skullTop >= 0) {
        int n = s.skullTop + 1;
        Add(&out, std::to_wstring(n) + L" " + SkullWord());
    }
    Add(&out, HandsLeftText(s));
    return out;
}

std::wstring PokerDetails(uintptr_t board) {
    PokerState s;
    if (!ReadState(board, &s)) return L"";
    std::wstring out = L"hand " + std::to_wstring(s.count) + L" of " + std::to_wstring(kMaxCards);
    std::wstring cards;
    for (int i = 0; i < s.count; ++i) Add(&cards, CardText(s.cards[i]));
    if (!cards.empty()) out += L": " + cards;
    if (s.count > 0 && s.handType >= 0) Add(&out, L"so far " + HandName(s.handType), L". ");
    Add(&out, NeedBetterText(s), L". ");
    // The score table, lowest hand type first: value, or a skull.
    std::wstring table;
    for (int t = 0; t < kHandTypes; ++t) {
        std::wstring row = HandName(t);
        if (t <= s.skullTop)
            row += L" " + SkullWord();
        else if ((int)s.values.size() == kHandTypes)
            row += L" " + std::to_wstring(s.values[t]);
        Add(&table, row);
    }
    Add(&out, table, L". ");
    Add(&out, EliminatorText(s), L". ");
    Add(&out, HandsLeftText(s), L". ");
    return out;
}

void PokerTick(uintptr_t board, std::vector<std::wstring>* events) {
    PokerState now;
    if (!ReadState(board, &now)) return;
    if (board != g_board || !g_prev.ok) {
        g_board = board;
        g_prev = now;
        return;
    }
    const PokerState& was = g_prev;

    // Cards added (one per move, sometimes two). The fifth card is followed
    // by the scored hand, so its hand name is not repeated here.
    if (now.count > was.count) {
        for (int i = was.count; i < now.count; ++i) {
            std::wstring text = L"card " + std::to_wstring(i + 1) + L", " + CardText(now.cards[i]);
            if (i + 1 == now.count && now.count < kMaxCards) {
                if (now.handType >= 0) text += L", " + HandName(now.handType);
                Add(&text, NeedBetterText(now));
            }
            events->push_back(text);
        }
    }

    // A hand was scored: the count of its type went up (after TallyScore).
    for (int t = 0; t < kHandTypes; ++t) {
        if (now.typeCounts[t] > was.typeCounts[t]) {
            std::wstring text = HandName(t);
            if (now.lastPoints > 0)
                text += L", plus " + std::to_wstring(now.lastPoints);
            else
                Add(&text, Str(kStrNoScore));
            events->push_back(text);
        }
    }

    if (now.coinFlips > was.coinFlips) events->push_back(L"skull coin flip");

    // Skulls added to or removed from the score table.
    if (now.skullTop > was.skullTop) {
        for (int t = was.skullTop + 1; t <= now.skullTop; ++t)
            events->push_back(L"new " + SkullWord() + L", " + HandName(t));
    } else if (now.skullTop < was.skullTop) {
        for (int t = was.skullTop; t > now.skullTop; --t)
            events->push_back(SkullWord() + L" removed, " + HandName(t));
    }

    std::wstring handsNow = HandsLeftText(now), handsWas = HandsLeftText(was);
    if (!handsNow.empty() && handsNow != handsWas) events->push_back(handsNow);

    g_prev = now;
}

void PokerReset() {
    g_board = 0;
    g_prev = PokerState();
}

}  // namespace

const ModeHooks& PokerHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.briefStatus = PokerBrief;
        h.details = PokerDetails;
        h.tick = PokerTick;
        h.reset = PokerReset;
        return h;
    }();
    return hooks;
}
