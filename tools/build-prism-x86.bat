@echo off
rem Builds Prism (MPL-2.0) as a 32-bit DLL for the 32-bit game process.
rem Upstream publishes no x86 Windows binary, so we build the pinned tag
rem (v0.18.3, cloned to deps\prism-src). The GDK speech backend is off
rem because it needs the Microsoft GDK, which is not installed.
setlocal
set ROOT=%~dp0..
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" || exit /b 1
rem Prism requires C23, which CMake 3.31 cannot enable for MSVC. Use CMake 4
rem from the local build venv (python -m venv deps\buildenv; pip install cmake).
set PATH=%ROOT%\deps\buildenv\Scripts;%PATH%
if exist "%ROOT%\deps\prism-build-x86\CMakeCache.txt" del "%ROOT%\deps\prism-build-x86\CMakeCache.txt"
cmake -S "%ROOT%\deps\prism-src" -B "%ROOT%\deps\prism-build-x86" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ^
  -DPRISM_ENABLE_TESTS=OFF -DPRISM_ENABLE_DEMOS=OFF -DPRISM_ENABLE_SHIMS=OFF ^
  -DPRISM_ENABLE_LEGACY_BACKENDS=ON -DPRISM_ENABLE_GDK_SPEECH_BACKEND=OFF ^
  -DPRISM_ENABLE_GDEXTENSION=OFF || exit /b 1
cmake --build "%ROOT%\deps\prism-build-x86" || exit /b 1
cmake --install "%ROOT%\deps\prism-build-x86" --prefix "%ROOT%\deps\prism-x86" || exit /b 1
