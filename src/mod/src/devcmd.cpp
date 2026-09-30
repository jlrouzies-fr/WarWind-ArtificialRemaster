#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "canvas.h"
#include "capture.h"
#include "devcmd.h"
#include "fx.h"
#include "game.h"
#include "log.h"
#include "patch.h"

namespace
{
char g_cmdPath[MAX_PATH];
char g_gameDir[MAX_PATH];

// Modifier keys the harness holds down; the game polls them through GetKeyState.
bool g_virtualKeys[256];
decltype(&GetKeyState) g_nextGetKeyState;

SHORT WINAPI DevGetKeyState(int vk)
{
    if (vk >= 0 && vk < 256 && g_virtualKeys[vk])
        return (SHORT)0x8000;
    return g_nextGetKeyState(vk);
}

// cnc-ddraw patches the same import slot during its own init, so the hook is
// (re)applied on every "mod" command and chains to whatever is in the slot then.
void EnsureGetKeyStateHook()
{
    void* current = *game::iatGetKeyState;
    if (current == (void*)DevGetKeyState)
        return;
    g_nextGetKeyState = (decltype(&GetKeyState))current;
    void* hook = (void*)DevGetKeyState;
    PatchWrite((DWORD)game::iatGetKeyState, &hook, sizeof(hook));
}

void KeyDown(HWND hwnd, UINT vk)
{
    PostMessageA(hwnd, WM_KEYDOWN, vk, 1 | (MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16));
}

void KeyUp(HWND hwnd, UINT vk)
{
    PostMessageA(hwnd, WM_KEYUP, vk, 1 | (MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16) | 0xC0000000);
}

void SendKey(HWND hwnd, UINT vk)
{
    KeyDown(hwnd, vk);
    Sleep(30);
    KeyUp(hwnd, vk);
}

void Click(HWND hwnd, int x, int y, bool right)
{
    LPARAM pos = MAKELPARAM(x, y);
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, pos);
    Sleep(30);
    PostMessageA(hwnd, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, right ? MK_RBUTTON : MK_LBUTTON, pos);
    Sleep(60);
    PostMessageA(hwnd, right ? WM_RBUTTONUP : WM_LBUTTONUP, 0, pos);
}

void DoubleClick(HWND hwnd, int x, int y)
{
    LPARAM pos = MAKELPARAM(x, y);
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, pos);
    Sleep(30);
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pos);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, pos);
    PostMessageA(hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, pos);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, pos);
}

void LeftDrag(HWND hwnd, int x0, int y0, int x1, int y1)
{
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x0, y0));
    Sleep(30);
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x0, y0));
    for (int i = 1; i <= 8; ++i)
    {
        Sleep(40);
        PostMessageA(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8));
    }
    Sleep(40);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x1, y1));
}

void MiddleDrag(HWND hwnd, int x0, int y0, int x1, int y1)
{
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x0, y0));
    Sleep(30);
    PostMessageA(hwnd, WM_MBUTTONDOWN, MK_MBUTTON, MAKELPARAM(x0, y0));
    for (int i = 1; i <= 8; ++i)
    {
        Sleep(30);
        PostMessageA(hwnd, WM_MOUSEMOVE, MK_MBUTTON, MAKELPARAM(x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8));
    }
    Sleep(30);
    PostMessageA(hwnd, WM_MBUTTONUP, 0, MAKELPARAM(x1, y1));
}

// Screen centre of a thing's sprite from the game's per-frame sprite rectangle lists
// (0x4C2DE8 + 0xA00*k, counts at 0x4B33B0[k]; records {u16 id, i16 y1, i16 x1, i16 y2, i16 x2}).
bool ThingScreenPos(WORD id, int* x, int* y)
{
    int current = *(BYTE*)0x4B33AC & 1;  // the list FindThingAtScreenPos reads (last completed frame)
    for (int pass = 0; pass < 2; ++pass)
    {
        int k = pass == 0 ? current : current ^ 1;
        const BYTE* list = (const BYTE*)0x4C2DE8 + 0xA00 * k;
        int count = ((int*)0x4B33B0)[k];
        for (int i = 0; i < count && i < 0xA00 / 10; ++i)
        {
            const short* r = (const short*)(list + i * 10);
            if ((WORD)r[0] == id)
            {
                // The lists hold canvas coordinates; the screen shows the canvas map at an offset.
                int ox, oy;
                CanvasShownOffset(&ox, &oy);
                *x = (r[2] + r[4]) / 2 - ox;
                *y = (r[1] + r[3]) / 2 - oy;
                return true;
            }
        }
    }
    return false;
}

void Peek(DWORD addr, int count)
{
    char hex[3 * 64 + 1] = {};
    count = count > 64 ? 64 : count;
    for (int i = 0; i < count; ++i)
        sprintf_s(hex + i * 3, 4, "%02X ", ((BYTE*)addr)[i]);
    Log("peek %08X: %s", addr, hex);
}

void Execute(char* line)
{
    char* ctx = nullptr;
    char* verb = strtok_s(line, " \t\r\n", &ctx);
    if (!verb)
        return;
    char* a = strtok_s(nullptr, " \t\r\n", &ctx);
    char* b = strtok_s(nullptr, " \t\r\n", &ctx);
    char* c = strtok_s(nullptr, " \t\r\n", &ctx);
    HWND hwnd = *game::hwndMain;

    if ((!_stricmp(verb, "capture") || !_stricmp(verb, "capcanvas")) && a)
    {
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", g_gameDir, a);
        if (!_stricmp(verb, "capcanvas"))
            CaptureSurface(CanvasSurface(), path);
        else
            CaptureFrame(path);
    }
    else if (!_stricmp(verb, "key") && a && hwnd)
        SendKey(hwnd, strtoul(a, nullptr, 16));
    else if (!_stricmp(verb, "keydown") && a && hwnd)
        KeyDown(hwnd, strtoul(a, nullptr, 16));
    else if (!_stricmp(verb, "keyup") && a && hwnd)
        KeyUp(hwnd, strtoul(a, nullptr, 16));
    else if (!_stricmp(verb, "mod") && a && b)
    {
        EnsureGetKeyStateHook();
        int vk = strtoul(a, nullptr, 16) & 0xFF;
        g_virtualKeys[vk] = atoi(b) != 0;
        auto viaSlot = (decltype(&GetKeyState))*game::iatGetKeyState;
        Log("mod: vk %02X=%d, slot %p (hook %p), GetKeyState via slot = %04X, ctrl byte %d",
            vk, g_virtualKeys[vk], *game::iatGetKeyState, (void*)DevGetKeyState, (WORD)viaSlot(vk), *game::ctrlDown);
    }
    else if (!_stricmp(verb, "click") && a && b && hwnd)
        Click(hwnd, atoi(a), atoi(b), c && (c[0] == 'r' || c[0] == 'R'));
    else if ((!_stricmp(verb, "clickid") || !_stricmp(verb, "rclickid")) && a && hwnd)
    {
        int x, y;
        WORD id = (WORD)strtoul(a, nullptr, 16);
        if (ThingScreenPos(id, &x, &y))
        {
            Log("devcmd: thing %03X at %d,%d", id, x, y);
            Click(hwnd, x, y, verb[0] == 'r' || verb[0] == 'R');
        }
        else
            Log("devcmd: thing %03X not on screen", id);
    }
    else if (!_stricmp(verb, "dblclick") && a && b && hwnd)
        DoubleClick(hwnd, atoi(a), atoi(b));
    else if (!_stricmp(verb, "wheel") && a && hwnd)
        PostMessageA(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (short)atoi(a)), 0);
    else if ((!_stricmp(verb, "mdrag") || !_stricmp(verb, "ldrag")) && a && b && c && hwnd)
    {
        char* d = strtok_s(nullptr, " \t\r\n", &ctx);
        if (d && verb[0] == 'm')
            MiddleDrag(hwnd, atoi(a), atoi(b), atoi(c), atoi(d));
        else if (d)
            LeftDrag(hwnd, atoi(a), atoi(b), atoi(c), atoi(d));
    }
    else if (!_stricmp(verb, "move") && a && b && hwnd)
        PostMessageA(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(atoi(a), atoi(b)));
    else if (!_stricmp(verb, "peek") && a && b)
        Peek(strtoul(a, nullptr, 16), atoi(b));
    else if (!_stricmp(verb, "poke") && a && b)
    {
        BYTE bytes[64];
        int n = 0;
        bytes[n++] = (BYTE)strtoul(b, nullptr, 16);
        for (char* hex = c; hex && n < 64; hex = strtok_s(nullptr, " \t\r\n", &ctx))
            bytes[n++] = (BYTE)strtoul(hex, nullptr, 16);
        PatchWrite(strtoul(a, nullptr, 16), bytes, n);
        Log("poke %s: %d bytes", a, n);
    }
    else if ((!_stricmp(verb, "fxdump") || !_stricmp(verb, "fxshot")) && a)
    {
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", g_gameDir, a);
        if (verb[2] == 'd' || verb[2] == 'D')
            FxRequestDump(path);
        else if (!FxRequestScreenshot(path))
            Log("devcmd: fxshot unavailable (renderer without Modern Graphics)");
    }
    else if (!_stricmp(verb, "memdump") && a && b && c)
    {
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", g_gameDir, c);
        FILE* f = nullptr;
        if (fopen_s(&f, path, "wb") == 0 && f)
        {
            fwrite((const void*)strtoul(a, nullptr, 16), 1, strtoul(b, nullptr, 16), f);
            fclose(f);
        }
    }
    else if (!_stricmp(verb, "camera") && a && b)
        CameraJumpTo(atoi(a), atoi(b));
    else if (!_stricmp(verb, "fxtoggle"))
        FxToggle();
    else if (!_stricmp(verb, "sleep") && a)
        Sleep(atoi(a));
    else
        Log("devcmd: ignored '%s'", verb);
}

DWORD WINAPI Worker(void*)
{
    char claimed[MAX_PATH];
    _snprintf_s(claimed, sizeof(claimed), _TRUNCATE, "%s.run", g_cmdPath);
    for (;;)
    {
        Sleep(100);
        if (!MoveFileExA(g_cmdPath, claimed, MOVEFILE_REPLACE_EXISTING))
            continue;
        FILE* f = nullptr;
        if (fopen_s(&f, claimed, "r") != 0 || !f)
            continue;
        char line[512];
        while (fgets(line, sizeof(line), f))
        {
            Log("devcmd> %s", line);
            Execute(line);
        }
        fclose(f);
        DeleteFileA(claimed);
    }
}
}  // namespace

void DevCmdStart(const char* gameDir)
{
    lstrcpynA(g_gameDir, gameDir, MAX_PATH);
    _snprintf_s(g_cmdPath, sizeof(g_cmdPath), _TRUNCATE, "%s\\WarWindHD.cmd", gameDir);
    DeleteFileA(g_cmdPath);
    CloseHandle(CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr));
}
