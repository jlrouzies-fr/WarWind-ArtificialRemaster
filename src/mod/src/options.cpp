#include "options.h"

#include <windows.h>

#include "fx.h"
#include "game.h"
#include "hook.h"
#include "log.h"
#include "menudraw.h"

// "Change Game Options" gets an eighth row, "Modern Graphics  OFF / ON", below "Full screen
// cinematics". The dialog's buttons are a list built by BuildGameOptions (AddMenuItem per button,
// ids 0x2E..0x3E); DrawMenu draws the known ids and the release handler dispatches ids up to 0x3E
// through a jump table. The row uses ids above that range: added at the end of BuildGameOptions,
// drawn after DrawMenu, handled before the jump table.
namespace
{
constexpr DWORD kIdOn = 0x3F, kIdOff = 0x40;

inline DWORD* const optionButtonArt = (DWORD*)0x5E1D7C;  // frames 2 = selected, 3 = not selected
constexpr int kDialogGameOptions = 6;

DWORD g_addMenuItem = 0x46CBA0;   // (eax id, edx left, ebx top, ecx right, [stack] bottom, sound), ret 8
constexpr DWORD fnSetTextPos = 0x43FD2C;
constexpr DWORD fnPrintString = 0x43FE50;

// Row geometry: the existing rows are 0x20 apart from Y+0x20; ON/OFF columns as theirs.
constexpr int kRowTop = 0x100, kRowBottom = 0x110, kLabelX = 0x1F, kLabelY = 0x102;
constexpr int kOffLeft = 0xDF, kOffRight = 0xEF, kOnLeft = 0x101, kOnRight = 0x111;

void AddMenuItem(DWORD id, int left, int top, int right, int bottom)
{
    __asm
    {
        push 0x800201EB          // the other option buttons' click sound
        push bottom
        mov eax, id
        mov edx, left
        mov ebx, top
        mov ecx, right
        call dword ptr [g_addMenuItem]
    }
}

void DrawImage(DWORD res, int frame, int x, int y)
{
    WatcomCall5(game::fnDrawImage, res, frame, x, y, 0);
}

void __cdecl AddModernRow()
{
    int x = *game::menuPanelX & 0xFFFF, y = *game::menuPanelY & 0xFFFF;
    AddMenuItem(kIdOff, x + kOffLeft, y + kRowTop, x + kOffRight, y + kRowBottom);
    AddMenuItem(kIdOn, x + kOnLeft, y + kRowTop, x + kOnRight, y + kRowBottom);
}

void __cdecl OnRowClick(DWORD id)
{
    if (*game::menuDialog != kDialogGameOptions || (id != kIdOn && id != kIdOff))
        return;
    FxSetEnabled(id == kIdOn);
}

void __cdecl AfterDrawMenu()
{
    if (*game::menuDialog != kDialogGameOptions)
        return;
    int x = *game::menuPanelX & 0xFFFF, y = *game::menuPanelY & 0xFFFF;
    WatcomCall(fnSetTextPos, x + kLabelX, y + kLabelY);
    WatcomCall(fnPrintString, (DWORD)"Modern Graphics");
    bool on = FxEnabled();
    DrawImage(*optionButtonArt, on ? 3 : 2, x + kOffLeft, y + kRowTop);
    DrawImage(*optionButtonArt, on ? 2 : 3, x + kOnLeft, y + kRowTop);
}

// Replaces "mov edx, 6" at the end of BuildGameOptions (the dialog id it then stores).
__declspec(naked) void BuildOptionsThunk()
{
    __asm
    {
        pushad
        call AddModernRow
        popad
        mov edx, 6
        ret
    }
}

// Replaces "cmp ebx, 0x3E ; ja exit" in the release handler: ebx = the released button's id.
DWORD g_releaseDispatch = 0x46EFF7;
DWORD g_releaseExit = 0x46FA81;
__declspec(naked) void ReleaseThunk()
{
    __asm
    {
        cmp ebx, 0x3E
        ja ours
        jmp dword ptr [g_releaseDispatch]
    ours:
        pushad
        push ebx
        call OnRowClick
        add esp, 4
        popad
        jmp dword ptr [g_releaseExit]
    }
}

constexpr DWORD kBuildEnd = 0x47059C;
const BYTE kBuildEndOrig[] = { 0xBA, 0x06, 0x00, 0x00, 0x00 };                        // mov edx, 6
constexpr DWORD kReleaseRange = 0x46EFEE;
const BYTE kReleaseRangeOrig[] = { 0x83, 0xFB, 0x3E, 0x0F, 0x87, 0x8A, 0x0A, 0x00, 0x00 };   // cmp ebx, 3Eh ; ja 46FA81
}  // namespace

void OptionsInstall()
{
    if (!HookVerify(kBuildEnd, kBuildEndOrig, sizeof(kBuildEndOrig), "options build") ||
        !HookVerify(kReleaseRange, kReleaseRangeOrig, sizeof(kReleaseRangeOrig), "options release") ||
        !MenuDrawHook(nullptr, AfterDrawMenu))
        return;
    HookCall(kBuildEnd, BuildOptionsThunk);
    HookJump(kReleaseRange, ReleaseThunk, sizeof(kReleaseRangeOrig));
    Log("options: \"Modern Graphics\" row added to Change Game Options");
}
