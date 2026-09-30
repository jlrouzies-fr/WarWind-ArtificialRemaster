#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "controls.h"
#include "devcmd.h"
#include "fx.h"
#include "game.h"
#include "hires.h"
#include "hud.h"
#include "input.h"
#include "log.h"
#include "music.h"
#include "options.h"
#include "patch.h"
#include "proxy.h"
#include "saveload.h"
#include "timing.h"
#include "video.h"

namespace
{
char g_gameDir[MAX_PATH];
char g_iniPath[MAX_PATH];

// Records the first faults in the log: the game's runtime exits silently on a crash.
LONG CALLBACK LogFault(EXCEPTION_POINTERS* e)
{
    static LONG logged;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION) && InterlockedIncrement(&logged) <= 3)
    {
        const CONTEXT& c = *e->ContextRecord;
        Log("fault %08X at %08X (access %08X) eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X [esp]=%08X",
            code, c.Eip, (DWORD)e->ExceptionRecord->ExceptionInformation[1], c.Eax, c.Ebx, c.Ecx, c.Edx, c.Esi, c.Edi,
            c.Ebp, c.Esp, *(DWORD*)c.Esp);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool IsExpectedGameExe()
{
    return memcmp((void*)game::kVersionCheckAddr, game::kVersionCheckBytes, sizeof(game::kVersionCheckBytes)) == 0;
}

void InitMod()
{
    GetModuleFileNameA(nullptr, g_gameDir, MAX_PATH);
    *strrchr(g_gameDir, '\\') = 0;
    _snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%s\\WarWindHD.ini", g_gameDir);

    char logPath[MAX_PATH];
    _snprintf_s(logPath, sizeof(logPath), _TRUNCATE, "%s\\WarWindHD.log", g_gameDir);
    LogInit(logPath);
    Log("War Wind HD loaded into %s", g_gameDir);
    AddVectoredExceptionHandler(1, LogFault);

    if (!IsExpectedGameExe())
    {
        Log("WW.EXE is not the expected 1.2 DX3 build; mod disabled");
        return;
    }

    char patchDir[MAX_PATH];
    GetPrivateProfileStringA("Mod", "PatchDir", "WarWindHD", patchDir, MAX_PATH, g_iniPath);
    char fullPatchDir[MAX_PATH];
    _snprintf_s(fullPatchDir, sizeof(fullPatchDir), _TRUNCATE, "%s\\%s", g_gameDir, patchDir);
    Log("%d patch file(s) applied from %s", PatchApplyDirectory(fullPatchDir), fullPatchDir);

    MusicInstall();
    ControlsInstall(g_iniPath);
    VideoInstall(g_iniPath);
    TimingInstall(g_iniPath);
    HudInstall(g_iniPath);
    HiresInstall(g_iniPath, fullPatchDir);
    FxInstall(g_iniPath, g_gameDir);
    OptionsInstall();
    InputInstall(g_iniPath);
    SaveLoadInstall(g_iniPath);

    if (GetPrivateProfileIntA("Dev", "Commands", 0, g_iniPath))
        DevCmdStart(g_gameDir);
}
}  // namespace

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!ProxyLoadRealDll())
            return FALSE;
        // Only the game executable gets modded; any other process loading this DLL just gets the forwarded system DLL.
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        const char* base = strrchr(exe, '\\');
        if (base && !_stricmp(base + 1, "WW.EXE"))
            InitMod();
    }
    return TRUE;
}
