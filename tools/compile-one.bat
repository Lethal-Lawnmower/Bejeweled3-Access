@echo off
rem Syntax/type-checks one mod source file without linking (safe to run in
rem parallel). Usage: tools\compile-one.bat src\mod\foo.cpp
setlocal
set ROOT=%~dp0..
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cl /nologo /c /Zs /std:c++17 /EHsc /W4 /utf-8 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE ^
  /I"%ROOT%\src\proxy" /I"%ROOT%\deps\prism-x86\include" "%~1"
