# Research: game binary and community work

Date: 2026-10-30. Game: Steam Bejeweled 3, `Bejeweled3.exe`
SHA-256 `d55162445511fcbf711ab7922e57d9bdc27ba2fa72b3bde6ab553e0756e688c3`
(PE timestamp 2011-10-25).

## Binary facts (verified by inspection)

- 32-bit x86 PE, image base `0x00400000`, **no ASLR** (DllCharacteristics
  `0x8000`), so addresses are fixed between runs.
- Not packed: plain `.text/.rdata/.data/.rsrc` sections, no Steam DRM stub.
- Built on PopCap's SexyAppFramework (namespace `Sexy`). MSVC RTTI is present:
  `tools/rtti_vtables.py` finds 369 class names and their vtables, e.g.
  `Bej3App` 0x008647B4, `MainMenu` 0x0086C35C, `Board` 0x00864CB4,
  `ButtonWidget` 0x00872724, `Dialog` 0x00875024, `WidgetManager` 0x008751A4.
- Imports WINMM.dll, used for the proxy-DLL injection point.
- `main.pak` is a standard PopCap pak (XOR 0xF7). `tools/pak_list.py` lists it
  (10,117 entries). `properties\default.xml` is the full UI string table
  (UTF-8, `<String id=N>`), and every string carries a comment naming the
  source function that uses it (e.g. id 287 is tagged
  `MainMenu::MainMenu:72`, the play orb's label; id 365 is tagged
  `PlayMenu::ButtonMouseEnter:624`, the Classic mode description).
  Strings embed colour codes `^RRGGBB^` and `&cr;` line breaks that must be
  stripped before speech.

## Community work found

| Project | What it gives us | Licence |
|---|---|---|
| [FatBrother1/bejeweled3-deck-bot](https://github.com/FatBrother1/bejeweled3-deck-bot) `bot/reader_mem.py` | Board memory map verified on the Steam build (Sep 2026): gApp `0x008E1730`, Board `[gApp+0xBE8]`, piece pointers `[Board+0xF8+4*col+32*row]`, colour `[piece+0x220]` (0 red,1 white,2 green,3 yellow,4 purple,5 orange,6 blue,-1 none), flags `[piece+0x228]`, score `[Board+0xD24]`, level `[Board+0xE0C]` | none stated: use facts only, copy no code |
| [bognarit80/Bejeweled3PlusExtender](https://github.com/bognarit80/Bejeweled3PlusExtender) | Plugin loader for the "Bejeweled 3 Plus" mod. Piece flag enum (flame 1, hypercube 2, star 4, multiplier 16, bombs 32/64, butterfly 128, …), field offsets. Its vtable addresses are for a **different build** (ClassicBoard 0x829FB4 there vs 0x0086511C here), so its addresses do not apply directly | MIT |
| [redstone59/Bejeweled3WorldChampionshipSet](https://github.com/redstone59/Bejeweled3WorldChampionshipSet) | Per-mode stat offsets (deprecated, uses `gApp+0xBE0`, possibly another build) | check before reuse |
| [HyperNervie/Bejeweled-Autosplitters](https://github.com/HyperNervie/Bejeweled-Autosplitters) | LiveSplit memory paths | not reviewed yet |
| [PeggleCommunity/PopCapFramework](https://github.com/PeggleCommunity/PopCapFramework) | SexyAppFramework source for class layouts (Widget, ButtonWidget, Dialog, WidgetManager). Reference only | PopCap framework licence |
| houseofwaffles.net Bejeweled 3 asset guide | Pak format, assets are JPEG2000 / OGG / MO3 | n/a |

No existing accessibility mod or screen-reader work for Bejeweled 3 was found.
No source decompilation exists. Leaked-source repositories were deliberately not used.

The community board offsets are leads until verified in our own runtime
dumps (see `status.md`).
