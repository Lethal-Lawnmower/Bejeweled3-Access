@echo off
rem Builds the mod (build\winmm.dll) with the 32-bit MSVC toolchain.
setlocal
set ROOT=%~dp0..
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
set PATH=%ROOT%\deps\buildenv\Scripts;%PATH%
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo || exit /b 1
cmake --build "%ROOT%\build" || exit /b 1
