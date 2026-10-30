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
