#include "hud.h"

#include <cstdio>
#include <cstring>

#include "board_layout.h"
#include "game_memory.h"
#include "sexy.h"

// Virtual getters (level target, progress, time left) are evaluated by
// replicating the game's own implementation for each vtable target address
// we reviewed (docs/re/hud.md lists them). An unreviewed target means a
// subclass we have not analysed, so the value stays unknown instead of
// being guessed.

namespace {

bool ReadF32(uintptr_t addr, float* out) {
    uint32_t bits;
    if (!ReadU32(addr, &bits)) return false;
    memcpy(out, &bits, sizeof bits);
    return true;
}

bool ReadF64(uintptr_t addr, double* out) {
    uint32_t lo, hi;
    if (!ReadU32(addr, &lo) || !ReadU32(addr + 4, &hi)) return false;
    uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
    memcpy(out, &bits, sizeof bits);
    return true;
}

bool ReadBool(uintptr_t addr, bool* out) {
    uint8_t b;
    if (!ReadU8(addr, &b)) return false;
    *out = b != 0;
    return true;
}

void SetInt(HudInt* field, bool ok, int value) {
    field->known = ok;
    field->value = ok ? value : 0;
}

void ReadIntField(uintptr_t addr, HudInt* field) {
    int32_t v = 0;
    bool ok = ReadI32(addr, &v);
    SetInt(field, ok, v);
}

void ReadByteField(uintptr_t addr, HudInt* field) {
    bool b = false;
    bool ok = ReadBool(addr, &b);
    SetInt(field, ok, b ? 1 : 0);
}

// Address of the function in vtable slot `slotOffset` of `object`.
bool VirtualTarget(uintptr_t object, int slotOffset, uint32_t* target) {
    uint32_t vtable;
    return ReadU32(object, &vtable) && ReadU32(vtable + slotOffset, target);
}

// --- Reviewed getter implementations (addresses from this exe) -----------

// Board vtable+0x1A0 GetTimeLimit (seconds, 0 = untimed).
bool TimeLimitSeconds(uintptr_t b, int* out) {
    uint32_t t;
    if (!VirtualTarget(b, board::kVfTimeLimitSeconds, &t)) return false;
    switch (t) {
    case 0x0073C250: *out = 0; return true;           // Board: untimed
    case 0x007140F0: *out = 60; return true;          // SpeedBoard: 60 s rounds
    case 0x006C4A30:                                  // DigBoard, TimeLimitBoard
        return ReadI32(b + board::kQuestParam, reinterpret_cast<int32_t*>(out));
    default: return false;
    }
}

// Shared head of the timer getters: the override flag and the time limit.
// Returns false if unreadable; sets *done when the result is already final.
bool TicksPrologue(uintptr_t b, int* limit, int* out, bool* done) {
    *done = false;
    bool overridden;
    if (!ReadBool(b + board::kTicksOverrideFlag, &overridden)) return false;
    if (overridden) {
        *done = true;
        return ReadI32(b + board::kTicksOverride, reinterpret_cast<int32_t*>(out));
    }
    if (!TimeLimitSeconds(b, limit)) return false;
    if (*limit == 0) {
        *out = -1;  // untimed
        *done = true;
    }
    return true;
}

// Board::GetTicksLeft 0x0073BB80:
// clamp(limit*100 - max(0, updateCount - 250), 0, limit*100).
bool BaseTicksLeft(uintptr_t b, int* out) {
    int limit = 0;
    bool done;
    if (!TicksPrologue(b, &limit, out, &done)) return false;
    if (done) return true;
    int32_t updates;
    if (!ReadI32(b + board::kUpdateCount, &updates)) return false;
    int total = limit * 100;
    int elapsed = updates - 250;
    if (elapsed < 0) elapsed = 0;
    int left = total - elapsed;
    if (left < 0) left = 0;
    if (left > total) left = total;
    *out = left;
    return true;
}

// SpeedBoard::GetTicksLeft 0x00713FA0 (x87 version of the same clamp on a
// float elapsed counter, then capped by +0x30F4). Exact rounding of the x87
// sequence is replicated as truncation (_ftol2); verify against the drawn
// timer at runtime.
bool SpeedTicksLeft(uintptr_t b, int* out) {
    int limit = 0;
    bool done;
    if (!TicksPrologue(b, &limit, out, &done)) return false;
    if (done) return true;
    float elapsedRaw;
    int32_t cap;
    if (!ReadF32(b + board::kSpeedElapsedF, &elapsedRaw) ||
        !ReadI32(b + board::kSpeedTicksCap, &cap))
        return false;
    float total = static_cast<float>(limit * 100);
    double elapsed = static_cast<double>(elapsedRaw) - 250.0;
    if (elapsed < 0.0) elapsed = 0.0;
    double left = total - elapsed;
    if (left < 0.0) left = 0.0;
    if (left > total) left = total;
    int result = static_cast<int>(left);
    if (cap < result) result = cap;
    *out = result;
    return true;
}

// Board vtable+0x2EC GetTicksLeft (1/100 s, -1 = untimed).
bool TicksLeft(uintptr_t b, int* out) {
    uint32_t t;
    if (!VirtualTarget(b, board::kVfTicksLeft, &t)) return false;
    switch (t) {
    case 0x0073BB80: return BaseTicksLeft(b, out);
    case 0x00713FA0: return SpeedTicksLeft(b, out);
    case 0x00736B50: {  // DigBoard: min(base, cap) when cap != 0
        int base;
        int32_t cap;
        if (!BaseTicksLeft(b, &base) || !ReadI32(b + board::kDigTicksCap, &cap))
            return false;
        *out = (cap != 0 && cap < base) ? cap : base;
        return true;
    }
    default: return false;
    }
}

bool Perpetual(uintptr_t b, bool* out) { return ReadBool(b + board::kPerpetual, out); }

// QuestGoal vtable+0x58 (goal's level target).
bool GoalTarget(uintptr_t goal, int* out) {
    uint32_t t;
    if (!VirtualTarget(goal, board::kGoalVfLevelPointsTotal, &t)) return false;
    int32_t v = 0;
    switch (t) {
    case 0x006A9B10:  // QuestGoal (base): no target
        *out = 0;
        return true;
    case 0x006AA470: case 0x006AA3B0: case 0x006AA2A0: case 0x006AA250:
    case 0x006AA200: case 0x006AA130: case 0x006A9D10: case 0x006A99E0:
    case 0x006A99D0: case 0x006A9970:  // Arsenal, ClearBombs, Firestorm, Gold,
        // GoldLines(+Flip), GoldYellow, Preserver, Score, Survival, WallBlast
        if (!ReadI32(goal + board::kGoalRequirement, &v)) return false;
        *out = v;
        return true;
    case 0x006AA420: {  // ButterflyGoal: 0 in endless mode
        uint32_t gb;
        bool perpetual;
        if (!ReadU32(goal + board::kGoalBoard, &gb) || !Perpetual(gb, &perpetual))
            return false;
        if (perpetual) { *out = 0; return true; }
        if (!ReadI32(goal + board::kGoalRequirement, &v)) return false;
        *out = v;
        return true;
    }
    case 0x006AA320: {  // ClearGoal: base + level*perLevel, capped by +0x118 if > 0
        uint32_t gb;
        int32_t req, per, cap, level;
        if (!ReadU32(goal + board::kGoalBoard, &gb) || !ReadI32(gb + board::kLevel, &level) ||
            !ReadI32(goal + board::kGoalRequirement, &req) ||
            !ReadI32(goal + board::kGoalPerLevel, &per) ||
            !ReadI32(goal + board::kGoalAltRequirement, &cap))
            return false;
        int base = req + level * per;
        *out = (cap > 0 && cap < base) ? cap : base;
        return true;
    }
    case 0x006AA2E0:  // DetonatorGoal: constant 20
        *out = 20;
        return true;
    case 0x006AA100: case 0x006A9F60:  // HiddenGoal, HiddenGridGoal
        if (!ReadI32(goal + board::kGoalAltRequirement, &v)) return false;
        *out = v;
        return true;
    case 0x006A9950:  // HiddenObjectGoal
        if (!ReadI32(goal + board::kHiddenObjectTarget, &v)) return false;
        *out = v;
        return true;
    case 0x006A9810:  // DigGoal
        if (!ReadI32(goal + board::kDigGoalTarget, &v)) return false;
        *out = v;
        return true;
    default: return false;
    }
}

bool QuestGoalTarget(uintptr_t b, int* out) {
    uint32_t goal;
    if (!ReadU32(b + board::kQuestGoal, &goal)) return false;
    if (goal == 0) { *out = 0; return true; }
    return GoalTarget(goal, out);
}

// Board vtable+0x268 GetLevelPointsTotal.
bool LevelTarget(uintptr_t b, int* out) {
    uint32_t t;
    if (!VirtualTarget(b, board::kVfLevelPointsTotal, &t)) return false;
    int32_t a, c, d;
    switch (t) {
    case 0x0073BF70:  // Board/Classic: 2500 + 750 * level
        if (!ReadI32(b + board::kLevel, &a)) return false;
        *out = a * 750 + 2500;
        return true;
    case 0x0070E450:  // ZenBoard: level capped at 30
        if (!ReadI32(b + board::kLevel, &a)) return false;
        if (a >= 30) a = 30;
        *out = a * 750 + 2500;
        return true;
    case 0x007140D0:  // SpeedBoard
        if (!ReadI32(b + board::kSpeedTargetMul, &a) ||
            !ReadI32(b + board::kSpeedTargetLevel, &c) ||
            !ReadI32(b + board::kSpeedTargetBase, &d))
            return false;
        *out = a * c + d;
        return true;
    case 0x006C53A0:  // BalanceBoard
        if (!ReadI32(b + board::kQuestParam, &a)) return false;
        *out = a * 2;
        return true;
    case 0x006C4D70: {  // PokerBoard
        bool perpetual;
        if (!Perpetual(b, &perpetual)) return false;
        if (perpetual) { *out = 0; return true; }
        if (!ReadI32(b + board::kPokerTarget, &a)) return false;
        *out = a;
        return true;
    }
    case 0x006C4F70: {  // InfernoBoard
        bool perpetual;
        if (!Perpetual(b, &perpetual)) return false;
        if (!perpetual) return QuestGoalTarget(b, out);
        float f;
        if (!ReadF32(b + board::kInfernoTargetF, &f)) return false;
        *out = static_cast<int>(f * 10000.0);
        return true;
    }
    case 0x006C4BD0: return QuestGoalTarget(b, out);  // QuestBoard family
    default: return false;
    }
}

// Board vtable+0x26C GetLevelPoints.
bool LevelPoints(uintptr_t b, int* out) {
    uint32_t t;
    if (!VirtualTarget(b, board::kVfLevelPoints, &t)) return false;
    int32_t p;
    switch (t) {
    case 0x0073BF60:
        return ReadI32(b + board::kLevelPoints, reinterpret_cast<int32_t*>(out));
    case 0x007140B0: {  // SpeedBoard: points minus the round's base
        double base;
        if (!ReadI32(b + board::kLevelPoints, &p) || !ReadF64(b + board::kSpeedPointsBaseD, &base))
            return false;
        *out = p - static_cast<int>(base);
        return true;
    }
    case 0x006C5D00: {  // InfernoBoard
        bool perpetual;
        if (!Perpetual(b, &perpetual)) return false;
        if (!perpetual)
            return ReadI32(b + board::kLevelPoints, reinterpret_cast<int32_t*>(out));
        float f;
        if (!ReadF32(b + board::kInfernoPointsF, &f)) return false;
        *out = static_cast<int>(f * 10000.0);
        return true;
    }
    default: return false;
    }
}

HudMode ModeFromClass(const std::string& cls) {
    if (cls == "ClassicBoard") return HudMode::Classic;
    if (cls == "ZenBoard") return HudMode::Zen;
    if (cls == "SpeedBoard") return HudMode::Lightning;
    if (cls == "PokerBoard") return HudMode::Poker;
    if (cls == "ButterflyBoard") return HudMode::Butterflies;
    if (cls == "InfernoBoard") return HudMode::IceStorm;
    if (cls == "DigBoard") return HudMode::DiamondMine;
    return HudMode::Unknown;
}

// Removes the game's "^RRGGBB^" colour codes and turns line breaks into
// spaces, collapsing repeated spaces.
std::wstring CleanForSpeech(const std::wstring& text) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'^') {
            size_t close = text.find(L'^', i + 1);
            if (close != std::wstring::npos && close - i <= 9) {
                i = close;
                continue;
            }
        }
        if (c == L'\n' || c == L'\r' || c == L'\t') c = L' ';
        if (c == L' ' && (out.empty() || out.back() == L' ')) continue;
        out.push_back(c);
    }
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}

// Announcement tracking for HudNewAnnouncements.
uintptr_t gLastBoard = 0;
uintptr_t gLastFront = 0;
std::wstring gLastFrontText;

}  // namespace

const char* HudModeName(HudMode mode) {
    switch (mode) {
    case HudMode::Classic: return "Classic";
    case HudMode::Zen: return "Zen";
    case HudMode::Lightning: return "Lightning";
    case HudMode::Poker: return "Poker";
    case HudMode::Butterflies: return "Butterflies";
    case HudMode::IceStorm: return "Ice Storm";
    case HudMode::DiamondMine: return "Diamond Mine";
    case HudMode::Quest: return "Quest";
    default: return "Unknown";
    }
}

uintptr_t HudCurrentBoard() {
    uint32_t app, b;
    if (!ReadU32(sexy::kGAppPtr, &app) || app == 0) return 0;
    if (!ReadU32(app + board::kAppBoard, &b)) return 0;
    if (b == 0 || !IsKindOf(b, "Board")) return 0;
    return b;
}

bool ReadHud(HudState* out) {
    *out = HudState();
    uintptr_t b = HudCurrentBoard();
    if (b == 0) return false;
    out->board = b;
    out->boardClass = ClassNameOf(b);
    out->mode = ModeFromClass(out->boardClass);
    bool questFamily = IsKindOf(b, "QuestBoard");
    if (out->mode == HudMode::Unknown && questFamily) out->mode = HudMode::Quest;

    ReadByteField(b + sexy::kVisible, &out->visible);
    ReadIntField(b + board::kScore, &out->score);
    ReadIntField(b + board::kDisplayedScore, &out->displayedScore);
    ReadIntField(b + board::kMultiplier, &out->multiplier);
    ReadIntField(b + board::kLevel, &out->level);
    if (out->level.known) out->level.value += 1;  // stored 0-based

    int v = 0;
    bool ok = LevelPoints(b, &v);
    SetInt(&out->levelPoints, ok, v);
    ok = LevelTarget(b, &v);
    SetInt(&out->levelTarget, ok, v);
    ok = TimeLimitSeconds(b, &v);
    SetInt(&out->timeLimitSeconds, ok, v);
    ok = TicksLeft(b, &v) && v >= 0;  // -1 means untimed
    SetInt(&out->ticksLeft, ok, v);

    ReadByteField(b + board::kGameOver, &out->gameOver);
    ReadIntField(b + board::kGameOverCount, &out->gameOverCount);
    ReadByteField(b + board::kLevelCompletePending, &out->levelCompletePending);
    ReadByteField(b + board::kUserPaused, &out->userPaused);

    float alpha = 0;
    ok = ReadF32(b + board::kPauseAlpha, &alpha);
    // The overlay is only drawn on timed boards (vtable+0x1AC = time limit != 0).
    bool timed = out->timeLimitSeconds.known && out->timeLimitSeconds.value != 0;
    SetInt(&out->pausedOverlay, ok && out->timeLimitSeconds.known, (timed && alpha > 0.0f) ? 1 : 0);
    float cooldown = 0;
    ok = ReadF32(b + board::kHintCooldown, &cooldown);
    SetInt(&out->hintCooldown, ok, cooldown != 0.0f ? 1 : 0);

    int32_t stat = 0;
    if (out->mode == HudMode::Butterflies) {
        ok = ReadI32(b + board::kStats + 4 * board::kStatButterfliesFreed, &stat);
        SetInt(&out->butterfliesFreed, ok, stat);
    }
    if (out->boardClass == "MoveLimitBoard") ReadIntField(b + board::kMovesLeft, &out->movesLeft);

    if (questFamily) {
        bool perpetual = false;
        ok = Perpetual(b, &perpetual);
        SetInt(&out->endless, ok, perpetual ? 1 : 0);
        uint32_t goal = 0;
        if (ReadU32(b + board::kQuestGoal, &goal) && goal != 0) {
            out->questGoalClass = ClassNameOf(goal);
            double depth;
            if (out->questGoalClass == "DigGoal" && ReadF64(goal + board::kDigGoalDepthD, &depth) &&
                depth >= 0.0 && depth < 1.0e7)
                SetInt(&out->digDepth, true, static_cast<int>(depth));
        }
        out->questDescriptionKnown = ReadWString(b + board::kQuestDescription, &out->questDescription);
    }
    return true;
}

std::vector<std::wstring> HudNewAnnouncements() {
    std::vector<std::wstring> result;
    uintptr_t b = HudCurrentBoard();
    if (b != gLastBoard) {
        gLastBoard = b;
        gLastFront = 0;
        gLastFrontText.clear();
    }
    if (b == 0) return result;

    uint32_t begin, end;
    if (!ReadU32(b + board::kAnnouncementsBegin, &begin) ||
        !ReadU32(b + board::kAnnouncementsEnd, &end))
        return result;
    if (begin == 0 || end <= begin || (end - begin) / 4 > 64) {
        gLastFront = 0;  // queue empty (or implausible): next front is new
        gLastFrontText.clear();
        return result;
    }
    uint32_t front, vtable;
    if (!ReadU32(begin, &front) || front == 0) return result;
    if (!ReadU32(front, &vtable) || vtable != board::kVtAnnouncement) return result;

    std::wstring text;
    if (!ReadWString(front + board::kAnnText, &text)) return result;
    // A new front object, or the same address reused for a different text.
    if (front != gLastFront || text != gLastFrontText) {
        gLastFront = front;
        gLastFrontText = text;
        std::wstring clean = CleanForSpeech(text);
        if (!clean.empty()) result.push_back(clean);
    }
    return result;
}

BoardButtonRole HudBoardButtonRole(uintptr_t widget) {
    uintptr_t b = HudCurrentBoard();
    if (b == 0 || widget == 0) return BoardButtonRole::None;
    struct { int field; BoardButtonRole role; } const kButtons[] = {
        {board::kButtonHint, BoardButtonRole::Hint},
        {board::kButtonMenu, BoardButtonRole::Menu},
        {board::kButtonReset, BoardButtonRole::Reset},
        {board::kButtonExtra, BoardButtonRole::Extra},
    };
    for (const auto& entry : kButtons) {
        uint32_t p;
        if (ReadU32(b + entry.field, &p) && p == widget) return entry.role;
    }
    return BoardButtonRole::None;
}

std::string HudDescribe(const HudState& s) {
    if (s.board == 0) return "no board";
    auto num = [](const HudInt& v) {
        return v.known ? std::to_string(v.value) : std::string("?");
    };
    char head[64];
    snprintf(head, sizeof head, "board %08X ", static_cast<unsigned>(s.board));
    std::string line = head + s.boardClass + " (" + HudModeName(s.mode) + ")";
    line += " score " + num(s.score) + " shown " + num(s.displayedScore);
    line += " level " + num(s.level) + " points " + num(s.levelPoints) + "/" + num(s.levelTarget);
    line += " mult " + num(s.multiplier) + " limit " + num(s.timeLimitSeconds) +
            "s ticksLeft " + num(s.ticksLeft);
    line += " gameOver " + num(s.gameOver) + "/" + num(s.gameOverCount) +
            " lvlDone " + num(s.levelCompletePending) + " paused " + num(s.userPaused) +
            "/" + num(s.pausedOverlay) + " hintCd " + num(s.hintCooldown);
    if (s.endless.known) line += " endless " + num(s.endless);
    if (s.movesLeft.known) line += " moves " + num(s.movesLeft);
    if (s.butterfliesFreed.known) line += " butterflies " + num(s.butterfliesFreed);
    if (s.digDepth.known) line += " depth " + num(s.digDepth);
    if (!s.questGoalClass.empty()) line += " goal " + s.questGoalClass;
    return line;
}
