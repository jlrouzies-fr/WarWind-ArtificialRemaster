#pragma once
#include <windows.h>

// Verifies that `expected` is present at addr before any hook is written, so a
// different WW.EXE build is refused instead of corrupted.
bool HookVerify(DWORD addr, const BYTE* expected, size_t len, const char* what);

// Overwrites addr with `jmp target`, padding the rest of `len` bytes with NOPs.
bool HookJump(DWORD addr, const void* target, size_t len);

// Overwrites the 5-byte call instruction at addr with `call target`.
bool HookCall(DWORD addr, const void* target);

// Calls a Watcom register-convention function: eax = a0, edx = a1 (, ebx = a2).
DWORD WatcomCall(DWORD fn, DWORD a0 = 0, DWORD a1 = 0);
DWORD WatcomCall3(DWORD fn, DWORD a0, DWORD a1, DWORD a2);
// eax, edx, ebx, ecx and one stack argument, which the callee pops (DrawImage, DrawPanel9).
DWORD WatcomCall5(DWORD fn, DWORD a0, DWORD a1, DWORD a2, DWORD a3, DWORD stack0);

// Detour: verifies `expected` (len bytes of whole, position-independent instructions) at
// addr, copies them into a trampoline that continues at addr+len, and jumps addr to `hook`.
// Returns the trampoline (call or jump to it to run the original), or nullptr.
void* HookDetour(DWORD addr, const BYTE* expected, size_t len, const void* hook, const char* what);

// Defines Wrap_<name>, a detour body that runs `before`, the original through the trampoline
// pointer t_<name>, then `after`, preserving every register the Watcom caller expects (only eax,
// the original's result, changes). `before` and `after` are cdecl functions without arguments.
#define WRAP(name, before, after)                        \
    __declspec(naked) void Wrap_##name()                 \
    {                                                    \
        __asm { pushad }                                 \
        __asm { call before }                            \
        __asm { popad }                                  \
        __asm { call dword ptr [t_##name] }              \
        __asm { push eax }                               \
        __asm { pushad }                                 \
        __asm { call after }                             \
        __asm { popad }                                  \
        __asm { pop eax }                                \
        __asm { ret }                                    \
    }
