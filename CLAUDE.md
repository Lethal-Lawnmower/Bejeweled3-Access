# Bejeweled 3 Access - agent instructions

Read `docs/status.md` and `docs/decisions.md` first; `docs/index.md` lists the rest.

- Game install: `C:\Program Files (x86)\Steam\steamapps\common\Bejeweled 3`
  (Steam app 78000). Only `winmm.dll` and `prism.dll` from this project go there.
- Speech: Prism only (32-bit build in `deps/prism-x86`). No other speech
  library or fallback.
- Keep game files, extracted pak content and Ghidra databases in `private/`
  (git-ignored). Never commit or upload them.
- Every memory offset in `src/mod/sexy.h` needs evidence recorded in
  `docs/reverse-engineering.md` (static / runtime / community).
- Build with `tools/build.bat` (32-bit MSVC). Close the game before
  `tools/install.sh`. Log: `%LOCALAPPDATA%\Bejeweled3Access\mod.log`.
- Update `docs/status.md` at every milestone.
