@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
call "%VCVARS%" amd64_x86 >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
:: fx.cpp includes wwfx.h from the patched cnc-ddraw tree (third_party/cnc-ddraw). Set CNC_DDRAW to
:: that clone; by default <War Wind>\cnc-ddraw next to this repository or the development tree.
if not defined CNC_DDRAW if exist "%~dp0..\..\..\cnc-ddraw\inc\wwfx.h" set "CNC_DDRAW=%~dp0..\..\..\cnc-ddraw"
if not defined CNC_DDRAW set "CNC_DDRAW=%~dp0..\..\..\..\cnc-ddraw"
cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS /I "%CNC_DDRAW%\inc" /Fo:build\ src\*.cpp /link /DLL /DEF:src\dsound.def /OUT:build\dsound.dll /SAFESEH:NO user32.lib kernel32.lib gdi32.lib ole32.lib oleaut32.lib mfplat.lib mfuuid.lib dxguid.lib || exit /b 1
echo built build\dsound.dll
