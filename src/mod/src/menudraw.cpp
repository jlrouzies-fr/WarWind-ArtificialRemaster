#include "menudraw.h"

#include <windows.h>

#include "hook.h"
#include "log.h"

namespace
{
constexpr DWORD kDrawMenu = 0x46D014;
const BYTE kDrawMenuOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57 };   // push ebx/ecx/edx/esi/edi

constexpr int kMaxHooks = 4;
MenuDrawFn g_before[kMaxHooks], g_after[kMaxHooks];
int g_beforeCount, g_afterCount;
void* t_DrawMenu;

void __cdecl RunBefore()
{
    for (int i = 0; i < g_beforeCount; ++i)
        g_before[i]();
}

void __cdecl RunAfter()
{
    for (int i = 0; i < g_afterCount; ++i)
        g_after[i]();
}

WRAP(DrawMenu, RunBefore, RunAfter)
}  // namespace

bool MenuDrawHook(MenuDrawFn before, MenuDrawFn after)
{
    if (!t_DrawMenu)
    {
        if (!HookVerify(kDrawMenu, kDrawMenuOrig, sizeof(kDrawMenuOrig), "menu draw"))
            return false;
        t_DrawMenu = HookDetour(kDrawMenu, kDrawMenuOrig, sizeof(kDrawMenuOrig), Wrap_DrawMenu, "menu draw");
        if (!t_DrawMenu)
            return false;
    }
    if ((before && g_beforeCount == kMaxHooks) || (after && g_afterCount == kMaxHooks))
    {
        Log("menu draw: too many hooks");
        return false;
    }
    if (before)
        g_before[g_beforeCount++] = before;
    if (after)
        g_after[g_afterCount++] = after;
    return true;
}
