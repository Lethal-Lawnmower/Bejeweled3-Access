# Bejeweled 3 Access

This is Bejeweled 3 Access. This repository is public more for
transparency's sake and to see the active development, and for you to add
and/or fix changes that you specifically want. This is more a tech
experiment for me, since we've never, to my knowledge, had any good
mainstream hits in this genre before.

**Do not purchase the game if you are hesitant or unsure about this mod. It
only works with the Steam version on its latest version.**

Timed modes are excluded for the time being till we get the gameplay ironed
out to the way we like.

---

A screen-reader accessibility mod for the Steam release of Bejeweled 3
(PopCap, 2010). It speaks menus, dialogs, buttons and the game board through
[Prism](https://github.com/ethindp/prism), adds keyboard navigation to the
mouse-only menus, and describes the board while you play with the game's own
keyboard controls.

Status: version 0.1. Menus and dialogs are complete; the board works in
Classic and Zen (Butterflies, Poker and move-based quests untested;
Lightning, Ice Storm, Diamond Mine and timed quests not supported yet). See
[docs/status.md](docs/status.md).

## How it works

The mod is a `winmm.dll` proxy. The game imports WINMM, so Windows loads the
mod from the game folder; every winmm call is passed to the real system DLL.
The mod reads the game's own widget tree (labels, positions, visibility) and
clicks buttons by sending mouse messages, so the game behaves exactly as if
the button had been clicked. Nothing in the game files is modified.

Details: [docs/architecture.md](docs/architecture.md).

## Keys (provisional)

Menus and dialogs:

| Key | Action |
|---|---|
| Up / Down, Tab / Shift+Tab | Move between buttons, checkboxes, sliders, tabs and fields |
| Home / End | First / last item |
| Enter or Space | Press the focused button, toggle a checkbox, select a tab |
| Left / Right | Change the focused slider by 10 % |
| F5 | Repeat the current screen or dialog and the focused item |
| F1 | List the keys for menus or the board (0.2) |
| F6 | Game status during play (score, level, progress, time, moves) |
| F12 | Developer: write the widget tree and drawn text to the log |

On the game board (stage 1, 2026-10-30):

| Key | Action |
|---|---|
| Arrows | The game's own cursor; the mod says the gem under it (colour and special type, in the game's words) |
| E | Where am I: gem and position |
| Q | Neighbours: the gems above, below, left and right |
| Space or Enter | Select the gem under the cursor (the game's Space; Enter added by the mod, setting `enterselects`) |
| H | Press the game's hint button; the mod says which gem the hint points to (also for automatic hints) |
| X | Next special gem; again for the one after (on by default from 0.2) |
| T | Time left in timed modes, as the game's timer shows it (0.2) |
| M | Mode details: poker hand, butterflies, ice columns, ... (0.2, not live-tested) |
| F6 | Score, level, progress, moves, plus the mode's status |
| F1 | List the keys (0.2) |
| R / C | Read the row / column (off by default, see settings) |

After each move the mod plays the game's own soft button sound when the
board has settled and says a short summary (points, gems cleared, cascades,
special gems made or set off). All of this is configurable in
`%LOCALAPPDATA%\Bejeweled3Access\settings.ini` (created on first start, with
explanations). Not supported for now: Lightning, Ice Storm, Diamond Mine and
the timed quests.

During a game the game's own keys work as normal (arrows move the gem
reticle, W/A/S/D swap, Space selects a gem (it pauses timed modes only
before the keyboard cursor has been used), Escape opens the game menu). Tab moves between the
board's buttons (hint, menu) and Enter presses the one Tab just reached.

In a text field, letters, Space, Backspace and Enter go to the game, and the
mod speaks what you type. Moving to an item tells the game it is hovered
(through the game's own code, not the mouse), and descriptions such as mode,
quest, badge and Zen section texts are read from the game's data. The mod
works the same in fullscreen or windowed mode and at any art resolution.

## Install (local build)

1. Build: see [docs/building.md](docs/building.md).
2. With the game closed, run `sh tools/install.sh`. It copies `winmm.dll` and
   `prism.dll` into the game folder and overwrites none of the game's files.
3. Start Bejeweled 3 from Steam as usual. You should hear
   "Bejeweled 3 access loaded".

Uninstall: `sh tools/install.sh --remove`.
Log: `%LOCALAPPDATA%\Bejeweled3Access\mod.log`.

## Repository

Public GitHub repository: `Lethal-Lawnmower/Bejeweled3-Access`. Game files,
extracted game data, saves and analysis databases are never committed (see
`.gitignore` and `docs/third-party.md`); the docs refer to the game's
string-table ids instead of quoting its text.

## Repository contents

- `src/proxy` - winmm export forwarding (generated tables + stubs)
- `src/mod` - speech, game memory access, widget tree, navigation
- `tools` - build/install scripts, pak lister, RTTI scanner, Ghidra scripts
- `docs` - research, reverse-engineering notes, status, decisions

Not in the repository (git-ignored): game files, extracted game data,
Ghidra databases (`private/`), third-party sources and builds (`deps/`).

## Licence

Copyright (C) 2026 Lethal-Lawnmower.

Bejeweled 3 Access is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free
Software Foundation, version 3 of the License. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; see `LICENSE` for the full text.

Prism, the speech library shipped as `prism.dll`, is licensed under MPL-2.0
(compatible with GPL-3.0); its licence, notices and source location must be
included with any binary release (see `docs/third-party.md`).

This project contains no game files or game content. Bejeweled 3 is a
trademark of its owners; this is an unofficial fan accessibility mod.
