#include "mode_info.h"

#include "game_memory.h"

// Board class -> mode hooks. Class names are the RTTI names the game uses
// (docs/re/hud.md, "gApp -> Board, mode detection"). Exact names come first:
// PokerBoard, ButterflyBoard, InfernoBoard and DigBoard derive from the
// QuestBoard family but have their own hooks.

namespace {

const ModeHooks kNone;

}  // namespace

const ModeHooks& ModeHooksFor(uintptr_t board) {
    std::string boardClass = ClassNameOf(board);
    if (boardClass == "PokerBoard") return PokerHooks();
    if (boardClass == "ButterflyBoard") return ButterflyHooks();
    if (boardClass == "InfernoBoard") return IceStormHooks();
    if (boardClass == "DigBoard") return DiamondMineHooks();
    if (boardClass == "SpeedBoard") return LightningHooks();
    if (IsKindOf(board, "QuestBoard")) return QuestHooks();
    return kNone;  // Classic, Zen, unknown
}
