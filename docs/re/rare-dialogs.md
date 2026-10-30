# Rarely seen dialogs and screens

Pass: 2026-10-30, **static analysis only** (Ghidra 12.0.4 headless on the
copy `private/ghidra-dialogs`, exe SHA-256 d5516244...88c3, base 0x400000).
Nothing in this file is runtime-confirmed. Game text is not quoted; strings
are named by their id in `private/pak/properties/default.xml`. Code:
`src/mod/dialog_text.{h,cpp}` (compile-checked with `tools/compile-one.bat`,
not built, not run).

Scratch outputs (private, not committed): `private/ghidra-dialogs/*.c`
(decompilations), `*.txt` (disassembly, caller lists, instruction greps),
`vtables.txt`, helper scripts `vtdiff.py` (vtable slot diff against a base
class), `resname.py` (offline image-global -> resource id, same tables as
`GameImageResourceName`), `grep.sh` (ActionsGrep.java on this copy). One
image and five properties files were extracted from `main.pak` into
`private/ghidra-dialogs/img/` to check the Zen intro and badge/rank data.

## Shared facts

- Two globals hold the app object: `0x008E1730` (SexyAppBase ctor
  FUN_0046b440 stores `this`) and `0x008E170C` (FUN_00791c00 stores the
  result of `new Bej3App`, ctor FUN_0076d9e0). Same object; dialog code uses
  the second. The mod reads `sexy::kGAppPtr`.
- App vtable: `+0x110` GetDialog(id), `+0x114` AddDialog(dialog),
  `+0x118` AddDialog(id, dialog), `+0x10C` DoDialog(id, modal, header,
  lines, footer, buttonMode) (seen in Board::Update FUN_00767090,
  FUN_0076f6c0, FUN_0076cde0).
- Bej3Dialog vtable slots (diff of RTTI vtables): 22 Update, 26 Draw,
  27 DrawAll, 48 KeyDown; EndLevelDialog adds 87-94 (below).
- Board Update FUN_00767090 is where the in-game award dialogs are created:
  if neither dialog 0x2B nor 0x27 is open and (byte `board+0x306D` badge
  pending or byte `board+0x306C` rank-up pending) and (`board+0xD1C` == 0 or
  >= 300, i.e. playing or 300 ticks after game over): rank-up first
  (`new RankUpDialog(board)`, AddDialog, `board+0xC88 = 0`, clear 0x306C),
  else, when no dialog 0x22 is open and the board's award vector
  `+0x305C..+0x3060` is non-empty, `new BadgeAwardDialog(board)`, then
  clear 0x306D. While dialog 0x17 is open the board update stops there.

## Per dialog

### RankUpDialog (vtable 0x0086C664, dialog id 0x2B)

- Created only in Board::Update (above). Flag `+0x306C` is set only by
  ZenBoard's level-up handler FUN_007120e0 (Board vtable slot 0x274,
  HyperspaceEvent phase 5): it adds the level's rank points to the profile
  (`profile+0x1F8`, 64-bit), recomputes the rank (FUN_0071a330) and sets the
  flag when it differs from `profile+0x1F0`. Other writers of `+0x306C`:
  FUN_0074a880 (reset to 0) and Board::Update (clear). So **the dialog
  appears only in Zen, at a level-up that crosses a rank** (static; other
  modes add rank points at game end through the end screen's RankBarWidget
  ctor FUN_00682f90 and show no RankUpDialog as far as traced).
- Rank from points (FUN_0071a330): rank k needs the sum over j < k of
  150000 + 100000*j points (rank 1 at 150 000, rank 2 at 400 000, ...).
  Points per Zen level = ftol(`board+0xC88` x board vtable+0x1EC result);
  the factor was not decoded.
- Ctor FUN_0078ec50: header 429 (Dialog header field), button label 430,
  `+0x228` board, `+0x22C` RankBarWidget(id 1000, board, dialog, 0).
- Draw FUN_0078f0d0 draws 431, the rank name (twice: glow then text), 432,
  and `n` + 433 (n = points to next rank / 1000, FUN_006747a0). The
  RankBarWidget child draws 427/428.
- Rank name: index = `[rankBar+0x144]` (board) ? `[rankBar+0x138]`
  (display rank) : `profile+0x1F0`; FUN_00676470 clamps it to
  `size-1` of the vector<wstring> at `gApp+0xD14` (stride 0x1C) and copies
  that entry. The vector holds the names from `properties/ranks.txt`
  (131 lines; load site not traced).
- Generic reading: header + drawn text probably covers it, but the rank name
  is drawn twice at slightly different positions and with animated alpha.
  **Reader:** `ReadRankUp` returns the index and name.

### BadgeAwardDialog (vtable 0x0086F6EC, dialog id 0x17)

- Created in Board::Update when `+0x306D` is set. The flag is set by the
  Badge award function FUN_0077d790 (badge object: +4 id, +8 new level,
  +0xC old level, +0x10 board): if the id is not yet in the board's award
  vector and the new level is higher, it logs "BadgeEarned", stores the
  level in `profile+0x1A0+4*id`, sets `board+0x306D` and appends the id
  (FUN_007785a0). The check loop FUN_0077d970 runs over all 20 badges from
  Board method FUN_007560e0 with an "in game" flag (`board+0xD1C` == 0);
  its call sites were not traced. So an award can appear during play or
  after the game-over delay.
- Ctor FUN_0067f4f0: **no header** (empty Dialog fields), button label 69,
  disabled until done; `+0x228` board, `+0x22C` BadgeWidget in award mode
  (`+0xBC` = board, `+0xC0` = `[board+0x3068]` badge manager).
- Draw FUN_0067a030 draws 70 only. The BadgeWidget (badges.md) presents the
  awarded ids one by one: index `widget+0x328`; for the current id it draws
  72, then 71 formatted with the level name (string id = level, 1..4) and
  the badge name (`gApp+0xD2C` vector), then the badge's award line
  (Badge vtable +0x14). Update FUN_0068faf0 enables the button when
  `+0x328` == vector size.
- Generic reading: only what is drawn in the current frame, and the badge
  text animates (alpha); the dialog stays open while the badge changes, so
  a new badge would not be announced. **Reader:** `ReadBadgeAward` returns
  all awarded ids with name, level, level name, the shown index and done.
- Thresholds (`properties/badges.txt`, loaded to `gApp+0xD44`): the first
  level of most badges is low, e.g. badge 4 (Classic score) 50 000 points,
  badge 6 (Lightning score) 100 000, badge 18 (Classic level) level 10,
  badge 0 / 2 (Flame / Hypercube totals) 50 / 25 gems.

### UnlockDialog (vtable 0x0086EDBC, dialog id 0x27)

- Bej3App::UnlockEndlessMode FUN_0076f6c0(mode 0..3): if `gApp+0x974` == 0
  and `profile+0x180+mode` == 0, sets it to 1, builds names 584..587 and
  shows `new UnlockDialog(names[mode])` with AddDialog(0x27). Callers and
  conditions:

  | Mode | Unlocks | Caller | Condition (static) |
  |---|---|---|---|
  | 0 | 584 | ClassicBoard HyperspaceEvent FUN_0073b280, phase 5 | `board+0xE04` + 1 > 4 (reaching level 5) |
  | 1 | 585 | ZenBoard FUN_007120e0, phase 5 | same, in Zen |
  | 2 | 586 | SpeedBoard slot 0x3A4 FUN_00716ab0, after the end dialog | score `board+0xD24` >= 100 000 |
  | 3 | 587 | QuestMenu::Update FUN_00697d40 | a counter > 3 (hypothesis: first relic = 4 quests) |

- Ctor FUN_006803b0: header 501, button 502, the mode name copied to the
  wstring at `+0x228`. Draw FUN_006800d0: 503, `this+0x228`, 504.
- Generic reading covers it (header field + three drawn lines).
  **Reader:** `ReadUnlockedModeName` as a check/fallback.

### ZenIntroDialog (vtable 0x0086F86C, dialog id 0x2C)

- ZenBoard ctor FUN_007126f0 shows it when bit 23 of `profile+0x21C` is
  clear, then sets the bit: **once per profile, the first time Zen starts.**
  The Test profile has a `zen.sav`, so it has seen it; a new profile is
  needed.
- Ctor FUN_0078d6f0: no header/lines, button label 575. Draw FUN_0078d440
  draws only images: `[0x008D8670]` IMAGE_ZENUI_INTRO between
  `[0x008D7A9C]` IMAGE_ZENUI_HEADER and `[0x008D8978]` IMAGE_ZENUI_FOOTER
  (names via `resname.py`). The extracted intro image (768 set) contains the
  whole message as artwork (a title line, four feature lines with icons,
  and a pointer to the Zen button). **The game has no text for it.**
- Generic reading speaks only the button. **Reader:**
  `ReadZenIntroImageName` gives the resource id. Decision for the user/lead:
  speak the image words ("zenui intro") only, or also the four Zen options
  section titles 527-530 that the image describes (that pairing would be a
  documented lookup, not the game's own association).

### EndLevelDialog family (dialog id 0x26)

Classes (RTTI) and their creating board method (Board vtable slot 0x404
"end dialog", slot 0x3A4 for SpeedBoard):

| Class | Vtable | Ctor | Created by |
|---|---|---|---|
| EndLevelDialog | 0x0086C1BC | 0x006A7C80 | FUN_006cf310 (QuestBoard family slot 0x404: quests) |
| ClassicEndLevelDialog | 0x0086C01C | 0x007842B0 | FUN_0073b100 (ClassicBoard) |
| ButterflyEndLevelDialog | 0x0086BE7C | 0x00784C30 | FUN_006d0590 (ButterflyBoard) |
| PokerEndLevelDialog | 0x0086C4C4 | 0x00782CF0 | FUN_006d1540 (PokerBoard) |
| InfernoEndLevelDialog | 0x008662C4 | 0x006D3180 | InfernoBoard slot 0x404 |
| DigGoalEndLevelDialog | 0x00865C54 | 0x007338D0 | FUN_006b9780 (DigBoard) |
| SpeedEndLevelDialog | 0x0086D034 | 0x00716AB0 | SpeedBoard slot 0x3A4 (runtime-verified reading) |

- Base ctor FUN_006a7c80: no header; `+0x228` board; copies stats
  `board+0xC84..0xD1C` to `+0x2C8..`, score `board+0xD24` to `+0x2C4`,
  level `board+0xE04` to `+0x378`, `board+0xE0C` to `+0x37C`; RankBarWidget
  at `+0x2C0`. Buttons: if `gApp+0xBD8` (QuestMenu*) == 0: id 0 label 227,
  id 3 label 228, id 2 label 229; else id 0 224, id 1 225, id 2 226.
- ButtonDepress FUN_006a91e0: 0 leave game (board vtable +0x3A0), 1 new
  quest (only when `board+0x118C` == 1), 2 play again, 3 opens a plain
  Bej3Dialog id 0x2A (header 237, button 548) containing a
  **BadgeWidget(0, 0)** - the same grid as Records > Badges.
- Draw FUN_006a87a0: 236 + animated score, then slot 94. Slots: 87
  DrawSpecialGemDisplay FUN_006a70f0, 88 DrawHighScores (233), 89
  DrawStatsLabels, 90 DrawStatsText (base: empty), 91 DrawLabeledHighScores
  (234), 92 DrawStatsFrame (calls 89, 90), 93 DrawLabeledStatsFrame (235),
  94 DrawFrames.
- Per class: Classic and Butterflies draw 93, 91, **87**; labels 165-169 /
  159-163. Poker draws hand names 405-411 with counts; labels 401-404.
  Inferno labels 271-284 (special gems with text labels, ids 277-279). DigGoal labels 204-223. Speed 483-498 (verified). Quests (base):
  empty stats frame, final score, top scores.
- **DrawSpecialGemDisplay is ambiguous as drawn text**: three icons
  IMAGE_GAMEOVER_ICON_FLAME_LRG `[0x008D92E0]`, ..._STAR_LRG `[0x008D7278]`,
  ..._HYPERCUBE_LRG `[0x008D9644]` (plus a section image), each followed by
  only a count format (230-232) with counts `+0x30C`, `+0x310`, `+0x314` (stats
  17, 18, 19). **Reader:** `ReadEndLevelSpecialGems` (Classic, Butterfly
  only) returns icon resource ids and counts. Also `ReadEndLevel` (score,
  level, button set) for checks.
- Everything else is drawn text with labels; the generic table-aware
  reading should cover it (to verify per class).

### Quest completed / failed, quest mode complete (421)

- QuestBoard::Update queues announcement 417 in the board queue (HUD
  reader covers it).
- QuestBoard-family slot 0x3A4 FUN_006ded60 (after a countdown at
  `+0x307C`): if `+0x31AC` -> slot 0x404 end dialog (generic
  EndLevelDialog, quest button set); else if `+0x3184` -> SecretMenu
  (FUN_0076c2a0); else FUN_006ded00 -> back to the quest menu.
  Hypothesis: failure shows the end dialog, success returns to the quest
  menu.
- QuestMenu::Update FUN_00697d40: when `QuestMenu+0x600` == 1800 creates
  `new Announcement(421, board 0)` and stores it at **QuestMenu+0x178**
  (not in any board queue). Strings 422-424 go to the TooltipManager
  (FUN_00677d00), 420 and 570/571 are relic texts (not traced further).
  **Reader:** `ReadQuestMenuAnnouncement` (checks the Announcement vtable
  0x00863BD8, text at +0x10).

### Game over / out of moves (164)

- Board::GameOver 0x007602C0 -> GameOverAnnounce; ClassicBoard queues 164
  (hud.md). Then the board's end-dialog method creates the class above.
  Classic has no timer: the only end is a board with no moves.

### Saved-game prompt (Bej3App::DoSavedGameCheck FUN_0076cde0)

- Called when starting Classic (FUN_007737d0), Lightning (FUN_00773440,
  creates SpeedBoard) and the four secret modes (FUN_00774060); not for Zen
  or Quest. FUN_007432a0 asks the new board for its save file name
  (vtable +0x160) under the profile folder and returns whether it exists.
- Dialog: DoDialog(id 20, modal, header 75, lines 76, footer empty,
  buttonMode 2); yes button label 77, no button label 78, extra
  Bej3DialogButton id 0x3EA label 79. All in Dialog fields and button
  labels: **generic reading covers it**.

### Safety link of the binaural section (ZenOptionsDialog, label 579)

- The HyperlinkWidget (label 579) has id 1; ZenOptionsDialog ButtonDepress
  FUN_00683160: id 0 saves settings (log line "ZenOptions ..."), **id 1 calls
  ShowBinauralWarning FUN_006771c0(0)**. (menus.md says "1 closes"; that
  is wrong.) Enabling section 3 (title 530; FUN_0068b870) calls it with 1.
- FUN_006771c0 builds header, lines and button text from **literals compiled
  in the exe** (the xml ids 580-583 are unused copies, which is why they
  showed as unreferenced) and calls DoDialog(id = arg ? 0x2D : 0, ...,
  buttonMode arg ? 2 : 3); with arg 1 a second button gets a literal label.
- The text is in the Dialog header/lines fields at runtime, so **generic
  reading covers it** (still read from game memory, not from the xml).

### SecretMenu and the developer Menu

- SecretMenu (vtable 0x0086DA0C, ctor FUN_00678940, InitWidgets
  FUN_00678560): back button id 1000 (label 476) and one ButtonWidget per
  entry of the secret quest set `[gApp+0xBD0]` (`properties/secret.cfg`),
  labelled with each entry's name, ids 0..n-1. Draw heading 477. Shown by
  FUN_0076c2a0, called from the developer Menu (button 0x3FC, label 298) and
  from FUN_006ded60 (above). secret.cfg says these entries can be started
  with the `-quest` option by adding 1000 to the index (comment in the data
  file; the parser branch for `-quest` was not decoded).
- Menu (vtable 0x0086D8A4, ctor FUN_00679720): created by FUN_0076c500
  ("back to menu") when `gApp+0xBD8` (QuestMenu) is 0, then the "MainMenu"
  transition plays. Labels 293-300. Whether it is ever visible in the
  shipped flow is **unknown** (no runtime sighting); an F12 dump would show
  a widget of class "Menu".
- Command-line options (parser FUN_0076f9d0, strings at 0x008618B4..):
  `-600`/`-768`/`-1200` (art scale), `-wide`, `-zen` (`gApp+0x9DD`),
  `-classic` (`+0x9DE`), `-speed` (`+0x9DF`), `-quest`, `-webroot`,
  `-testbkg`. MainMenu setup (FUN_006a3530 at 0x006a45ae) starts Zen,
  Classic or Lightning directly when the matching flag is set. No cheat or
  debug key handler was found (searched strings for debug/cheat; not an
  exhaustive search of key handlers).

## How the lead can make each appear (disposable profile only)

Use a new throwaway profile (or the existing "Test" profile in the restored
setup), never the user's. Steam launch options such as `-classic` only skip
menus; they are optional.

| Dialog | Least invasive way |
|---|---|
| Saved-game prompt | Test profile already has `classic.sav`: play orb > Classic mode button. Choose the cancel button (79) to leave the save as it is. |
| ZenIntroDialog | New profile, play orb > Zen mode button (first Zen start only). |
| UnlockDialog | Classic: reach level 5 (unlocks 584); also appears in Zen at level 5 (585) or Lightning with 100 000 points (586, shows over the end screen). |
| BadgeAwardDialog | Classic: 50 000 points (badge 4 first level); Lightning 100 000 (badge 6) together with the unlock above. Whether it shows mid-game or after game over depends on the untraced badge-check timing. |
| RankUpDialog | Zen only: keep playing levels until total rank points pass 150 000 (rank 1) on a fresh profile. Not reachable in other modes (static). |
| ButterflyEndLevelDialog / Inferno / Poker / DigGoal | Need the mode unlocked. Butterflies: let a butterfly reach the top; Ice Storm: let columns rise; Diamond Mine: let the timer run out. |
| ClassicEndLevelDialog | Only by running out of moves; no shortcut found. |
| Generic EndLevelDialog | Fail a move-limited quest (e.g. the 20-move quest) by wasting moves. |
| End screen badges button (228; dialog 0x2A) | Button id 3 on any non-quest end screen (Lightning is quickest: 60 s). |
| Quest-complete announcement 421 | Only after every quest; not practical. |
| Safety dialog (binaural section) | Zen options > section 3 (title 530) > the 579 link (id 1), or tick its enable checkbox (caption 515; id 0x2D variant). |
| SecretMenu / Menu | Not reachable through normal menus (static). |

## Integration (lead)

1. Add `src/mod/dialog_text.cpp` to `tools/build.bat`.
2. F12 dump: for each visible dialog call `dialog_text::DumpRareDialog`.
3. In `DialogSpeech`, by `ClassifyRareDialog(d.ptr)`:
   - RankUp: if the cleaned rank name is not in the text, add it after the
     drawn text.
   - BadgeAward: speak the current badge from `ReadBadgeAward`
     (badges[shownIndex]: name, level name) when the drawn text lacks it;
     poll in NavigatorTick and announce when `shownIndex` changes and when
     `sequenceDone` turns true (the button becomes usable).
   - ZenIntro: decision needed (above); at least the image words.
   - EndLevel (Classic/Butterfly): replace the bare count fragments (230-232) with
     the icon words + count from `ReadEndLevelSpecialGems` (image-name
     fallback, same rule as image-only buttons; list it in status.md).
   - Inside dialog 0x2A, offer the BadgeWidget grid like Records > Badges.
4. NavigatorTick on the quest menu: speak `ReadQuestMenuAnnouncement` text
   when it first appears.

## Runtime checklist

- [ ] Saved-game prompt: header 75, lines 76, three buttons; class name and
      dialog id 20.
- [ ] ZenIntro: `rare: ZenIntroDialog ... image=IMAGE_ZENUI_INTRO`; header,
      lines empty; button label 575.
- [ ] Unlock: `rare:   mode:` equals the drawn mode name; profile byte
      `+0x180+mode` became 1.
- [ ] BadgeAward: ids match the badges earned; `shown` advances and `done`
      becomes 1 when the button enables; names equal the drawn names.
- [ ] RankUp: rank name equals the drawn one; `fromBoard` = 1.
- [ ] Classic/Butterfly end: gem counts equal the drawn counts (230-232); image names
      FLAME/STAR/HYPERCUBE in drawn order; score equals the final score.
- [ ] End screen badges button (228) opens dialog 0x2A with a BadgeWidget.
- [ ] The binaural safety link opens a dialog with header/lines read by the generic
      path.
- [ ] Confirm `gApp+0x974` is 0 on a normal install (else no unlocks).

## Open questions

- Rank points factor (board vtable +0x1EC) and whether non-Zen rank-ups
  show anything besides the end screen's rank bar.
- Call sites of the badge check FUN_007560e0 (timing of awards).
- Meaning of QuestBoard `+0x31AC` / `+0x3184` (end dialog vs. SecretMenu vs.
  quest menu) and the quest counter used for unlock 587.
- Whether the developer Menu is ever visible.
