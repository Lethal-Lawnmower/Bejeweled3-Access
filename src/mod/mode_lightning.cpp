#include <cstring>
#include <string>
#include <vector>

#include "board_layout.h"
#include "game_memory.h"
#include "game_strings.h"
#include "mode_info.h"
#include "settings.h"
#include "text.h"

// Lightning (SpeedBoard): time gems and their seconds, the bonus time the
// collected time gems have saved up for the next round, the speed chain
// ("N MATCH CHAIN"), Blazing Speed and the end-of-game Last Hurrah. The time
// left itself is the T key (hud.cpp) and score/multiplier are F6, so they
// are not repeated here. Fields and evidence: docs/modes/lightning.md; all
// are static reads of this exe, none has been checked on a live board yet.
namespace {

// Piece record (no vtable; docs/re/board-objects.md "Piece record").
constexpr int kPieceColour = 0x220;   // 0..6, -1 none
constexpr int kPieceFlags = 0x228;
constexpr int kPieceCounter = 0x244;  // time gem: seconds (static: tooltip 0x007476E0)
constexpr uint32_t kFlagMultiplier = 0x10;
constexpr uint32_t kFlagTimeGem = 0x20000;   // static: SpeedBoard vt+0x2E4 0x00716C60 sets it

// Bonus time saved up, whole seconds. Static: SpeedBoard::PieceTallied
// 0x00715360 adds a matched time gem's +0x244 to [0xC2B] (+0x30AC) and to
// [0xC2C] (+0x30B0, the round total for the end dialog); SpeedBoard::GameOver
// 0x00719350, when the clock runs out with this above 0, turns it into the
// next round's time ((60 - pool) * 100 ticks), zeroes it and raises the
// multiplier [899] (+0xE0C) by one; the init 0x00718500 zeroes it.
constexpr int kBonusPool = 0x30AC;
// Byte set once the clock ran out with no bonus left: GameOver queues the
// TIME UP announcement (string 480) and sets it before setting off the
// remaining special gems (the Last Hurrah phase). Static: 0x00719350 tests
// and sets [+0x3096]; 0x00718500 clears it.
constexpr int kLastHurrah = 0x3096;
// Speed chain: successful swaps in quick succession (static: 0x0074C070
// increments +0x1080 when the previous swap was less than 301 ticks ago;
// Board::DrawGameElements 0x00750320 draws string 149/598 "%d MATCH CHAIN"
// with it, and SPEED +%d below while it is above 0).
constexpr int kSpeedChain = 0x1080;
// Blazing Speed, float: 1.0 when it starts (BlazingSpeed 0x0074A590), falls
// to 0 in UpdateGame 0x00767E20 (docs/re/board-events.md).
constexpr int kBlazingSpeed = 0x1188;

// Game strings (default.xml ids; text not copied here).
constexpr int kStrTimeGem = 135;        // tooltip title, Board::GetTooltipText
constexpr int kStrPlusSeconds = 479;    // "+%d sec", SpeedBoard::PieceTallied
constexpr int kStrBonusTime = 481;      // "+%d:%02d", SpeedBoard::DrawFrame2
constexpr int kStrLastHurrah = 482;     // SpeedBoard::DrawFrame2
constexpr int kStrChain = 149;          // "%d MATCH CHAIN", Board::DrawGameElements
constexpr int kStrMultiplierGem = 125;  // tooltip title
constexpr int kColourNameBase = 455;

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

// Fills the printf placeholders of a game string (%d, %i, %02d, %%) with
// values read from the game, in order. Empty if the string is missing or
// has a placeholder this does not know, so callers fall back to their own
// words instead of speaking a half-filled text.
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
        size_t j = i + 1;
        bool zero = j < format.size() && format[j] == L'0';
        if (zero) ++j;
        int width = 0;
        while (j < format.size() && format[j] >= L'0' && format[j] <= L'9') width = width * 10 + (format[j++] - L'0');
        if (j >= format.size() || (format[j] != L'd' && format[j] != L'i') || next == values.end()) return L"";
        std::wstring number = std::to_wstring(*next++);
        while ((int)number.size() < width) number = (zero ? L"0" : L" ") + number;
        out += number;
        i = j;
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

// "+5 sec" as the game shows it when a time gem is matched.
std::wstring PlusSeconds(int seconds) {
    std::wstring text = Formatted(kStrPlusSeconds, {seconds});
    return text.empty() ? L"plus " + std::to_wstring(seconds) + L" seconds" : text;
}

struct SpeedState {
    bool ok = false;
    int pool = 0;        // bonus seconds saved up
    bool lastHurrah = false;
    int chain = 0;
    bool blazing = false;
    int multiplier = 0;
    bool multiplierKnown = false;
};

bool ReadState(uintptr_t board, SpeedState* s) {
    *s = SpeedState();
    int32_t pool, chain;
    uint8_t hurrah;
    float blazing;
    if (!ReadI32(board + kBonusPool, &pool) || !ReadU8(board + kLastHurrah, &hurrah) ||
        !ReadI32(board + kSpeedChain, &chain) || !ReadFloat(board + kBlazingSpeed, &blazing))
        return false;
    s->pool = pool;
    s->lastHurrah = hurrah != 0;
    s->chain = chain;
    s->blazing = blazing > 0.0f;
    int32_t mult = 0;
    s->multiplierKnown = ReadI32(board + board::kMultiplier, &mult);
    s->multiplier = mult;
    s->ok = true;
    return true;
}

std::wstring BonusTimeText(int pool) {
    if (pool <= 0) return L"";
    std::wstring shown = Formatted(kStrBonusTime, {pool / 60, pool % 60});
    if (shown.empty()) shown = std::to_wstring(pool) + L" seconds";
    return L"bonus time " + shown;
}

std::wstring ChainText(int chain) {
    if (chain < 2) return L"";  // one swap is not a chain yet
    std::wstring text = Formatted(kStrChain, {chain});
    return text.empty() ? std::to_wstring(chain) + L" match chain" : text;
}

// ---- hooks ------------------------------------------------------------------

std::wstring LightningCell(uintptr_t, uintptr_t piece, int, int) {
    if (!piece) return L"";
    uint32_t flags;
    int32_t seconds;
    if (!ReadU32(piece + kPieceFlags, &flags) || !(flags & kFlagTimeGem)) return L"";
    if (!ReadI32(piece + kPieceCounter, &seconds) || seconds <= 0) return L"";
    return PlusSeconds(seconds);
}

std::wstring LightningBrief(uintptr_t board) {
    SpeedState s;
    if (!ReadState(board, &s)) return L"";
    std::wstring out;
    auto add = [&](const std::wstring& t) {
        if (!t.empty()) out += (out.empty() ? L"" : L", ") + t;
    };
    add(BonusTimeText(s.pool));
    add(ChainText(s.chain));
    if (s.blazing) add(L"Blazing Speed");
    if (s.lastHurrah) add(Str(kStrLastHurrah));
    return out;
}

// Every time gem and multiplier gem on the board, as a sighted player
// sees them at a glance, then the brief status.
std::wstring LightningDetails(uintptr_t board) {
    std::wstring timeGems, multipliers;
    int timeCount = 0, multCount = 0;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            uint32_t piece, flags;
            int32_t colour, seconds;
            if (!ReadU32(board + board::kGrid + 4 * (col + 8 * row), &piece) || !piece) continue;
            if (!ReadU32(piece + kPieceFlags, &flags) || !ReadI32(piece + kPieceColour, &colour)) continue;
            std::wstring name = ColourName(colour);
            if ((flags & kFlagTimeGem) && ReadI32(piece + kPieceCounter, &seconds)) {
                timeGems += (timeGems.empty() ? L"" : L"; ") + name + (name.empty() ? L"" : L" ") +
                            PlusSeconds(seconds) + L", " + Position(col, row);
                ++timeCount;
            }
            if (flags & kFlagMultiplier) {
                multipliers += (multipliers.empty() ? L"" : L"; ") + name + (name.empty() ? L"" : L", ") +
                               Position(col, row);
                ++multCount;
            }
        }
    }
    std::wstring timeName = Str(kStrTimeGem), multName = Str(kStrMultiplierGem);
    if (timeName.empty()) timeName = L"time gem";
    if (multName.empty()) multName = L"multiplier gem";
    std::wstring out = timeCount ? std::to_wstring(timeCount) + L" " + timeName + L": " + timeGems
                                 : L"no " + timeName;
    if (multCount) out += L". " + std::to_wstring(multCount) + L" " + multName + L": " + multipliers;
    std::wstring brief = LightningBrief(board);
    if (!brief.empty()) out += L". " + brief;
    return out;
}

uintptr_t g_board = 0;
SpeedState g_prev;

void LightningTick(uintptr_t board, std::vector<std::wstring>* events) {
    if (board != g_board) {
        g_board = board;
        g_prev = SpeedState();
    }
    SpeedState now;
    if (!ReadState(board, &now)) return;
    if (!g_prev.ok) {
        g_prev = now;
        return;
    }
    const SpeedState& was = g_prev;

    // Time gems matched: the game shows "+N sec" where the gem was.
    if (now.pool > was.pool) events->push_back(PlusSeconds(now.pool - was.pool));
    // The clock ran out with bonus time saved: it becomes the next round's
    // time and the multiplier goes up by one (GameOver 0x00719350). A new
    // game also zeroes the pool but resets the multiplier, hence both tests.
    if (was.pool > 0 && now.pool == 0 && now.multiplierKnown && was.multiplierKnown &&
        now.multiplier == was.multiplier + 1)
        events->push_back(L"bonus time added, multiplier " + std::to_wstring(now.multiplier));
    if (now.chain > was.chain) {
        std::wstring chain = ChainText(now.chain);
        if (!chain.empty()) events->push_back(chain);
    }
    if (now.blazing && !was.blazing) events->push_back(L"Blazing Speed");
    if (!now.blazing && was.blazing) events->push_back(L"Blazing Speed over");
    if (now.lastHurrah && !was.lastHurrah) {
        std::wstring text = Str(kStrLastHurrah);
        if (!text.empty()) events->push_back(text);
    }
    g_prev = now;
}

void LightningReset() {
    g_board = 0;
    g_prev = SpeedState();
}

}  // namespace

const ModeHooks& LightningHooks() {
    static ModeHooks hooks = [] {
        ModeHooks h;
        h.cellText = LightningCell;
        h.briefStatus = LightningBrief;
        h.details = LightningDetails;
        h.tick = LightningTick;
        h.reset = LightningReset;
        return h;
    }();
    return hooks;
}
