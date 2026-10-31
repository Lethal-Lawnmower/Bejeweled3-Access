@echo off
rem Builds the release package dist\Bejeweled3-Access-<version>.zip:
rem   - the mod (winmm.dll), Release, no debug info (no local paths inside)
rem   - Prism (prism.dll), Release, source paths trimmed to relative ones
rem   - licences, Prism notices, the player readme
rem Requires the same tools as tools\build.bat and deps\prism-src (v0.18.3).
setlocal
for %%I in ("%~dp0..") do set ROOT=%%~fI
set VERSION=0.2
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
set PATH=%ROOT%\deps\buildenv\Scripts;%PATH%

rem Prism: /d1trimfile removes the local folder from __FILE__ strings.
cmake -S "%ROOT%\deps\prism-src" -B "%ROOT%\deps\prism-build-release" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ^
  -DPRISM_ENABLE_TESTS=OFF -DPRISM_ENABLE_DEMOS=OFF -DPRISM_ENABLE_SHIMS=OFF ^
  -DPRISM_ENABLE_LEGACY_BACKENDS=ON -DPRISM_ENABLE_GDEXTENSION=OFF ^
  "-DCMAKE_C_FLAGS=/d1trimfile:%ROOT%\deps\prism-src\\" ^
  "-DCMAKE_CXX_FLAGS=/d1trimfile:%ROOT%\deps\prism-src\\" >nul || exit /b 1
cmake --build "%ROOT%\deps\prism-build-release" >nul || exit /b 1

rem The mod, Release (no /DEBUG, so no .pdb path in the DLL).
cmake -S "%ROOT%" -B "%ROOT%\build-release" -G Ninja -DCMAKE_BUILD_TYPE=Release >nul || exit /b 1
cmake --build "%ROOT%\build-release" >nul || exit /b 1

set OUT=%ROOT%\dist\Bejeweled3-Access-%VERSION%
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%\licences\prism" || exit /b 1
copy /y "%ROOT%\build-release\winmm.dll" "%OUT%\" >nul || exit /b 1
copy /y "%ROOT%\deps\prism-build-release\prism.dll" "%OUT%\" >nul || exit /b 1
copy /y "%ROOT%\packaging\README.txt" "%OUT%\" >nul || exit /b 1
copy /y "%ROOT%\LICENSE" "%OUT%\licences\LICENSE-Bejeweled3-Access.txt" >nul || exit /b 1
copy /y "%ROOT%\deps\prism-src\LICENSE" "%OUT%\licences\prism\LICENSE.txt" >nul || exit /b 1
copy /y "%ROOT%\deps\prism-src\NOTICE" "%OUT%\licences\prism\NOTICE.txt" >nul || exit /b 1
xcopy /e /i /q /y "%ROOT%\deps\prism-src\LICENSES" "%OUT%\licences\prism\LICENSES" >nul || exit /b 1
powershell -NoProfile -Command "Compress-Archive -Force -Path '%OUT%\*' -DestinationPath '%ROOT%\dist\Bejeweled3-Access-%VERSION%.zip'" || exit /b 1
echo packaged %ROOT%\dist\Bejeweled3-Access-%VERSION%.zip
