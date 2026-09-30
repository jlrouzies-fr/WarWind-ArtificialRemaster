#include "hook.h"

#include <string.h>

#include "log.h"
#include "patch.h"

bool HookVerify(DWORD addr, const BYTE* expected, size_t len, const char* what)
{
    if (memcmp((void*)addr, expected, len) == 0)
        return true;
    Log("%s: unexpected bytes at %08X, feature disabled", what, addr);
    return false;
}

bool HookJump(DWORD addr, const void* target, size_t len)
{
    BYTE code[16];
    if (len < 5 || len > sizeof(code))
        return false;
    memset(code, 0x90, len);
    code[0] = 0xE9;
    *(DWORD*)(code + 1) = (DWORD)target - (addr + 5);
    return PatchWrite(addr, code, len);
}

bool HookCall(DWORD addr, const void* target)
{
    BYTE call[5] = { 0xE8 };
    *(DWORD*)(call + 1) = (DWORD)target - (addr + 5);
    return PatchWrite(addr, call, sizeof(call));
}

namespace
{
BYTE* g_codeCave;
size_t g_codeCaveUsed;

BYTE* AllocCode(size_t n)
{
    if (!g_codeCave)
        g_codeCave = (BYTE*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_codeCave || g_codeCaveUsed + n > 4096)
        return nullptr;
    BYTE* p = g_codeCave + g_codeCaveUsed;
    g_codeCaveUsed += (n + 15) & ~15;
    return p;
}
}  // namespace

void* HookDetour(DWORD addr, const BYTE* expected, size_t len, const void* hook, const char* what)
{
    if (!HookVerify(addr, expected, len, what))
        return nullptr;
    BYTE* tramp = AllocCode(len + 5);
    if (!tramp)
    {
        Log("%s: out of trampoline space", what);
        return nullptr;
    }
    memcpy(tramp, (void*)addr, len);
    tramp[len] = 0xE9;
    *(DWORD*)(tramp + len + 1) = (addr + len) - ((DWORD)tramp + len + 5);
    if (!HookJump(addr, hook, len))
        return nullptr;
    return tramp;
}

DWORD WatcomCall3(DWORD fn, DWORD a0, DWORD a1, DWORD a2)
{
    DWORD result;
    __asm
    {
        push ebx
        mov eax, a0
        mov edx, a1
        mov ebx, a2
        call fn
        mov result, eax
        pop ebx
    }
    return result;
}

DWORD WatcomCall5(DWORD fn, DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD stack0)
{
    DWORD result;
    __asm
    {
        push ebx
        push stack0
        mov eax, a0
        mov edx, a1
        mov ebx, a2
        mov ecx, a3
        call fn
        mov result, eax
        pop ebx
    }
    return result;
}

DWORD WatcomCall(DWORD fn, DWORD a0, DWORD a1)
{
    DWORD result;
    __asm
    {
        mov eax, a0
        mov edx, a1
        call fn
        mov result, eax
    }
    return result;
}
