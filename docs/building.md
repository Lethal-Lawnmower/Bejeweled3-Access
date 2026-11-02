# Building

Verified 2026-10-30 on Windows 11 with Visual Studio 2022 Build Tools
(MSVC 19.44, x86 target) and Python 3.12.

## 1. CMake 4 (needed by Prism)

Prism requires C23, which CMake 3.31 cannot enable for MSVC.

```bash
py -3.12 -m venv deps/buildenv
deps/buildenv/Scripts/python -m pip install "cmake>=4.1,<5"
```

## 2. Prism, 32-bit

Upstream releases no 32-bit Windows binary, so build tag v0.18.3
(commit `94329ebeccbaee7f70b3494a290c9dfaac0184ee`):

```bash
git clone --depth 1 --branch v0.18.3 --recurse-submodules --shallow-submodules https://github.com/ethindp/prism.git deps/prism-src
```

```bash
cmd //c 'tools\build-prism-x86.bat'
```

Output: `deps/prism-x86/bin/prism.dll`, `lib/prism.lib`, `include/`. The
build disables tests, demos, shims and the GDK backend; all screen-reader
backends (NVDA, JAWS, SAPI, OneCore, UIA, ...) are enabled. The DLL links the
CRT statically, so no Visual C++ redistributable is required.

## 3. The mod

```bash
cmd //c 'tools\build.bat'
```

Output: `build/winmm.dll` (32-bit, static CRT, `prism.dll` delay-loaded).

If the system winmm.dll changes, regenerate the export tables:

```bash
python -I tools/gen_winmm_proxy.py 'C:\Windows\SysWOW64\winmm.dll' src/proxy
```

## 4. Install

Close the game first (a running game locks `winmm.dll`), then
`sh tools/install.sh`.

## Analysis tools

- `python -I tools/rtti_vtables.py Bejeweled3.exe [filter]` - vtables by class
- `python -I tools/pak_list.py main.pak [OUTDIR regex]` - list/extract pak
  (extract only into `private/`)
- `sh tools/ghidra/decompile.sh OUT.c ADDR...` - decompile functions that
  reference an address, using the read-only Ghidra project in `private/ghidra`
  (created with `analyzeHeadless ... -import` on a copy of the exe; Ghidra
  12.0.4; set `GHIDRA_HOME` to its install folder). Commands assume the
  project root as the current folder.
- `python -I tools/send_keys.py tab return` - post keys to the game window
  for testing

## 5. Release package

```bash
cmd //c tools\package.bat
```

Builds a Release mod (no debug info) and a Release Prism with source paths
trimmed (`/d1trimfile`), so neither DLL contains local folder names, then
writes `dist/Bejeweled3-Access-<version>/` and the zip: both DLLs, the
player readme (`packaging/README.txt`), the GPL-3.0 licence and Prism's
licence, notice and bundled licences. `dist/` is not committed.

## 6. Accessibility Mod Manager release

The mod is listed in the Accessibility Mod Manager through the public
catalog repository Lethal-Lawnmower/accessibility-mods (plugin ID
`lethal-lawnmower`, game ID `bejeweled-3`), maintained with the
manager's author CLI (`amm-author`, 0.30.0 from the manager's GitHub
release v2.0.10; not part of this repository). The catalog checkout and
the package staging folder live outside this repository.

For a new version, after `tools/package.bat` and the GitHub release:

1. Make a source folder laid out as in the game folder: `winmm.dll`,
   `prism.dll`, `Bejeweled3-Access-README.txt` (the player readme) and
   `Bejeweled3-Access-licences/` (the zip's `licences` folder). The longer
   names keep the game folder free of a generic README.
2. In the catalog checkout:

```bash
amm-author package build --source <staging folder> --game bejeweled-3 --version <version> --output <new zip path> --project .
amm-author package validate --zip <zip> --plugin lethal-lawnmower --game bejeweled-3 --version <version>
amm-author release publish --game bejeweled-3 --version <version> --channel stable --zip <zip> --notes "<notes>" --project . --dry-run
amm-author release publish --game bejeweled-3 --version <version> --channel stable --zip <zip> --notes "<notes>" --project . --yes
```

`release publish` adds `bejeweled-3-<version>.zip` to this repository's
release `v<version>` (beside the hand-install zip), verifies the public
hash, pushes the catalog and submits it to the manager's directory.
Publishing needs the user's approval each time.
