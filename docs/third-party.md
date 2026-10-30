# Third-party material inventory

Reviewed 2026-10-30. "In repo" means the repository contains it.

| Item | Use | In repo | Licence / basis | Notes |
|---|---|---|---|---|
| Prism v0.18.3 (github.com/ethindp/prism, commit 94329ebe) | speech; built locally as 32-bit `prism.dll` | No (source and build in `deps/`, git-ignored) | MPL-2.0; bundles simdutf (Apache-2.0), fmt (MIT), moderncom (MIT), dr_wav, concurrentqueue, NVDA controller IDL (MPL-2.0 by permission) | Any binary release must ship Prism's `LICENSE`, `LICENSES/` and `NOTICE`, and say where its source is |
| PopCap SexyAppFramework 1.34 (github.com/PeggleCommunity/PopCapFramework) | reference for class layouts and field names only | No code copied | PopCap framework licence | Field names in comments (mLabel, mDialogHeader, ...) come from its headers |
| FatBrother1/bejeweled3-deck-bot | board memory offsets as leads | No code copied | none stated | Facts only, verified against our build where used |
| bognarit80/Bejeweled3PlusExtender | piece-flag meanings, struct offset leads | No code copied | MIT | Facts only |
| redstone59/Bejeweled3WorldChampionshipSet | stat offset leads | No | not reviewed | Not used in code |
| Ghidra 12.0.4 | analysis tool | No | Apache-2.0 | Our scripts in `tools/ghidra/` are our own |
| MSVC, CMake 4 (pip), Python | build tools | No | their own | Not distributed |

Game material (executable, `main.pak`, extracted files, saves, Ghidra
databases, decompiler output) lives in `private/` and is never committed.
The docs refer to the game's string-table ids instead of quoting game
text, and describe decompiled code in prose (decision in `decisions.md`).

Project licence: GPL-3.0-only (user decision 2026-10-30, `LICENSE`).
Compatibility: Prism is MPL-2.0 without the "Incompatible With Secondary
Licenses" notice, so it may be combined with GPL-3.0; it ships as a
separate DLL with its own licence and notices. The statically linked MSVC
runtime falls under the GPL's System Libraries exception (our reading, not
legal advice).
