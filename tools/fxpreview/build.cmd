@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
call "%VCVARS%" amd64_x86 >nul || exit /b 1
cd /d "%~dp0"
set "CNC=%~dp0..\..\..\..\..\cnc-ddraw"
if not exist build mkdir build
cl /nologo /O2 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /I "%CNC%\inc" /Fo:build\ fxpreview.c "%CNC%\src\wwfx.c" "%CNC%\src\lodepng.c" /link /OUT:build\fxpreview.exe d3d9.lib user32.lib winmm.lib || exit /b 1
echo built build\fxpreview.exe
