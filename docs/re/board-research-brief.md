# Board research: shared brief for the research agents

Phase: research only (user decision 2026-10-30). Six agents reverse engineer
the game board of Bejeweled 3 (Steam, 32-bit, PopCap SexyAppFramework,
MSVC 2008) so a later design can let a blind player navigate and play it.
The user has never played the game; part of the job is finding out what is
crucial on the board.

## Rules for every agent

- Static analysis only. Do NOT launch, close or send input to the game, do
  not modify saves, do not edit `src/`, `tools/` scripts of others, or any
  doc you do not own. Write findings only to your own doc
  (`docs/re/board-<topic>.md`) and scratch files in your own Ghidra copy
  folder `private/ghidra-<id>/`. You may add Ghidra scripts under
  `tools/ghidra/` with your own prefix.
- Evidence for every claim: function address and what the decompilation or
  disassembly shows (call sites, offsets, conventions). Label each claim
  static, community, or hypothesis. Never invent values.
- No game text in your doc: refer to string-table ids
  (`private/pak/properties/default.xml`, read-only) or describe text. Image
  resource ids (IMAGE_...) and class names are fine.
- Keep it useful for design: for each thing, say how a mod could read it
  (field to poll, function to hook, event to watch), whether it is stable
  while animations play, and what is uncertain.
- End your doc with a runtime checklist: what the lead should read in live
  memory on a throwaway profile to confirm your claims.

## Existing knowledge (read these first)

- `docs/status.md`, `docs/decisions.md` (user rules), `docs/re/hud.md`
  (board pointer `[gApp+0xBE8]`, score, level, multiplier, announcements,
  board buttons, mode classes, quest goals), `docs/reverse-engineering.md`
  (community board leads: piece grid `[Board+0xF8+4*col+32*row]`, colour
  `+0x220`, flags `+0x228`, counter `+0x244`), `docs/research.md`
  (community projects: FatBrother1/bejeweled3-deck-bot, bognarit80/
  Bejeweled3PlusExtender (MIT; piece flag enum), redstone59 offsets,
  HyperNervie autosplitters), `docs/re/screen-names-and-keys.md` (Board
  KeyDown 0x007472B0: arrows, WASD, Space; cursor at +0x2C44/+0x2C48),
  `docs/re/hover-text.md` (Board::GetTooltipText 0x007476E0, TooltipManager),
  `docs/re/game-actions.md` (calling conventions, thunks), `src/mod/sexy.h`,
  `src/mod/board_layout.h`, `src/mod/hud.*`.
- Tools: Ghidra project copies in `private/ghidra-<id>/` (headless; see
  `tools/ghidra/decompile.sh`, set `GHIDRA_PROJECT` to your copy and
  `GHIDRA_HOME` (your Ghidra 12.0.4 install folder)), disassembly/xref
  scripts in `tools/ghidra/` (MenusDisasm, TextDisasm, ControlsDisasm,
  ActionsGrep, HoverScan), `python -I tools/rtti_vtables.py
  private/ghidra/input/Bejeweled3.exe Board` for vtables. Exe base
  0x400000, no ASLR. gApp `[0x008E1730]`.

## Topics (one agent each)

1. `board-state.md`: the board model and settling.
2. `board-input.md`: cursor, selection, swaps, legality, hints, move finder.
3. `board-events.md`: matches, cascades, specials created/detonated, points.
4. `board-objects.md`: special gems and mode/quest objects.
5. `board-flow.md`: levels, refills, scrolling, game over, timers, pause, speed.
6. `board-descriptions.md`: the game's own gem descriptions, cross-check of
   community offsets, save format of the board.

Agents may read each other's docs as they appear, but must not edit them.
