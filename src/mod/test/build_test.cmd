@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64_x86 >nul 2>nul
cd /d "%~dp0"
cl /nologo proxytest.c /Fe:proxytest.exe dsound.lib
copy /y ..\build\dsound.dll . >nul
del /q winmm.dll 2>nul
