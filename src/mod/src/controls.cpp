#include "controls.h"

#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "hook.h"
#include "log.h"
#include "patch.h"

namespace
{
// PanelHotkey (0x47AF28) walks buttons with eax = button index and edx = visible slot.
// Original: cmp ch, [ebx+eax+g_hotkeyTable]   ; per-command letter
// Grid:     cmp ch, [edx+g_gridKeys]           ; per-slot key
constexpr DWORD kPanelCompare = 0x47AF65;
const BYTE kPanelCompareOrig[] = { 0x3A, 0xAC, 0x03, 0x48, 0x9B, 0x4B, 0x00 };

// InGameKeyDispatch digit case: "if (!ctrl && !alt) RecallGroup(c - '0')".
constexpr DWORD kDigitCase = 0x47B4EE;
constexpr DWORD kDigitCaseDone = 0x47B50C;
const BYTE kDigitCaseOrig[] = { 0x80, 0x3D, 0x6D, 0x27, 0x4B, 0x00, 0x00, 0x75, 0x15 };

BYTE g_gridKeys[32];
bool g_gridHotkeys;

// Command-popup hover label: "mov edx,0xFFFF ; call SetStatusTextById(eax = text index)".
// Replaced so the label reads "Move  [Q]" with the key of the button's visible slot.
constexpr DWORD kPanelLabel = 0x44CD82;
constexpr DWORD kPanelLabelResume = 0x44CD8C;
const BYTE kPanelLabelOrig[] = { 0xBA, 0xFF, 0xFF, 0x00, 0x00, 0xE8, 0xB0, 0x33, 0xFF, 0xFF };
char g_labelBuffer[90];

int VisibleSlot(DWORD mask, int buttonIndex)
{
    int slot = 0;
    for (int i = 0; i < buttonIndex; ++i)
        slot += (mask >> i) & 1;
    return slot;
}

const char* __cdecl GridLabel(DWORD textIndex, DWORD buttonIndex)
{
    const char* text = (const char*)WatcomCall(game::fnResourceText, 0x80000000 | textIndex);
    char plain[80];
    size_t n = 0;
    for (; *text && n < sizeof(plain) - 1; ++text)
        if (*text != '<' && *text != '>')
            plain[n++] = *text;
    plain[n] = 0;

    int slot = VisibleSlot(WatcomCall(game::fnPanelEnabledMask), (int)buttonIndex);
    BYTE key = slot < (int)sizeof(g_gridKeys) ? g_gridKeys[slot] : 0;
    if (key)
        _snprintf_s(g_labelBuffer, sizeof(g_labelBuffer), _TRUNCATE, "%s  [%c]", plain, key);
    else
        _snprintf_s(g_labelBuffer, sizeof(g_labelBuffer), _TRUNCATE, "%s", plain);
    return g_labelBuffer;
}

// Entered with eax = text index and the button index still at [esp+0x74].
__declspec(naked) void PanelLabelThunk()
{
    __asm
    {
        movzx edx, byte ptr [esp + 0x74]
        push ecx
        push edx
        push eax
        call GridLabel
        add esp, 8
        pop ecx
        mov edx, 0xFFFF
        push ebx
        mov ebx, game::fnSetStatusText
        call ebx
        pop ebx
        push kPanelLabelResume
        ret
    }
}

void InstallGridHotkeys(const char* keys)
{
    memset(g_gridKeys, 0, sizeof(g_gridKeys));
    for (size_t i = 0; keys[i] && i < sizeof(g_gridKeys); ++i)
        g_gridKeys[i] = (BYTE)toupper((unsigned char)keys[i]);

    if (!HookVerify(kPanelCompare, kPanelCompareOrig, sizeof(kPanelCompareOrig), "grid hotkeys"))
        return;
    BYTE code[7] = { 0x3A, 0xAA, 0, 0, 0, 0, 0x90 };
    *(DWORD*)(code + 2) = (DWORD)g_gridKeys;
    PatchWrite(kPanelCompare, code, sizeof(code));
    if (HookVerify(kPanelLabel, kPanelLabelOrig, sizeof(kPanelLabelOrig), "grid labels"))
        HookJump(kPanelLabel, PanelLabelThunk, sizeof(kPanelLabelOrig));
    Log("grid hotkeys: %s", keys);
}

void __cdecl OnGroupDigit(DWORD digit)
{
    if (*game::altDown)
        return;
    WatcomCall(*game::ctrlDown ? game::fnAssignGroup : game::fnRecallGroup, digit);
}

// Entered with dl = digit character; resumes at the dispatcher's "handled" exit,
// which restores ebx/ecx/edx/esi/edi itself.
__declspec(naked) void DigitCaseThunk()
{
    __asm
    {
        movzx eax, dl
        sub eax, 0x30
        push eax
        call OnGroupDigit
        add esp, 4
        mov eax, kDigitCaseDone
        jmp eax
    }
}

void InstallCtrlGroups()
{
    if (!HookVerify(kDigitCase, kDigitCaseOrig, sizeof(kDigitCaseOrig), "ctrl groups"))
        return;
    HookJump(kDigitCase, DigitCaseThunk, sizeof(kDigitCaseOrig));
    Log("ctrl+digit assigns control groups");
}
}  // namespace

void ControlsInstall(const char* iniPath)
{
    char keys[64];
    GetPrivateProfileStringA("Controls", "GridKeys", "QWERTASDFGZXCVB", keys, sizeof(keys), iniPath);
    g_gridHotkeys = GetPrivateProfileIntA("Controls", "GridHotkeys", 1, iniPath) != 0;
    if (g_gridHotkeys)
        InstallGridHotkeys(keys);
    if (GetPrivateProfileIntA("Controls", "CtrlGroups", 1, iniPath))
        InstallCtrlGroups();
}

char ControlsGridKey(int slot)
{
    return g_gridHotkeys && slot >= 0 && slot < (int)sizeof(g_gridKeys) ? (char)g_gridKeys[slot] : 0;
}
