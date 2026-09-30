#include "briefing.h"

#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "gametext.h"
#include "hook.h"
#include "log.h"
#include "menuart.h"
#include "offscreen.h"
#include "patch.h"
#include "widemenu.h"

namespace
{
// ---- game side (kb.h "briefing")

auto* const briefingState = (const BYTE*)0x4B6F1C;    // 0/1 map page (Hall of Heroes), 2 text, 3 start
auto* const textFirstLine = (const int*)0x4B74C4;
auto* const textLines = (const int*)0x4B74C0;
auto* const gameKind = (const int*)0x4B66EC;           // 0 campaign, 2 built-in scenario (no map)
constexpr int kBuiltIn = 2, kCampaignMissions = 7;
constexpr DWORD fnDrawMissionMap = 0x46A5DC;           // map frame + its three labels
constexpr DWORD fnPrintTextWrapped = 0x4401A8;         // (eax first, edx window, ebx mode, ecx x; y, lh, width, text)
constexpr DWORD kBriefingText = 0x8000049B;            // + 20 * race + 2 * mission; its goal is the next text
constexpr DWORD kBigFont = 0x800100E7, kLabelFont = 0x800100F9;
constexpr int kTextPage = 2;                           // states below draw the map page

// Text page: after its background the mod clears the panel and draws the map; the block that fills
// the map box with stone and prints the text there (up to the free of the text buffer) is the mod's.
constexpr DWORD kTextBackgroundCall = 0x46AF9F;
const BYTE kTextBackgroundCallOrig[] = { 0xE8, 0xD8, 0x8A, 0xFA, 0xFF };
constexpr DWORD kTextBlock = 0x46B099, kTextBlockEnd = 0x46B177;
const BYTE kTextBlockOrig[] = { 0x89, 0xE0, 0xB9, 0x6B, 0x02, 0x00 };   // mov eax, esp ; mov ecx, 26Bh
constexpr DWORD kMapPage = 0x46B23C;
const BYTE kMapPageOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };
void* t_MapPage;

// ---- layout (the game's coordinates; the screen is 30 px lower)

constexpr int kWellX = 646, kWellY = 20, kWellW = 288, kWellH = 440;   // the right panel's text well
constexpr int kTextX = 654, kTextY = 28, kTextW = 272, kLineH = 18;
constexpr int kTextWindow = 20;                        // rows shown = window + 1, as the game counts
constexpr int kOrbX = kWellX + kWellW - 46, kOrbY = kWellY + kWellH - 46, kOrbSize = 41;
constexpr int kMapBoxX0 = 20, kMapBoxX1 = 0x26B, kMapBoxY0 = 20, kMapBoxY1 = 0x133;
constexpr int kGoalX = 30, kGoalY = 344, kGoalW = 500, kGoalLineH = 17, kGoalLines = 5;
// The scroll buttons' own rectangles (ids 0 / 1; also the Hall's previous / next unit).
constexpr int kScrollUpX = 463, kScrollUpY = 350, kScrollDownX = 463, kScrollDownY = 367;

struct CodePatch
{
    DWORD addr;
    BYTE original[5], wide[5];
    int len;
};
constexpr BYTE kThreshold = (BYTE)kTextWindow;
const CodePatch kPatches[] = {
    // The line count leaves out the goal (the well shows it) and uses the panel's width.
    { 0x46AC4F, { 0x8D, 0x83 }, { 0xEB, 0x2D }, 2 },                                        // jmp over the goal
    { 0x46AC9E, { 0x4B, 0x02, 0x00, 0x00 }, { (BYTE)kTextW, (BYTE)(kTextW >> 8), 0, 0 }, 4 },
    // Scroll limits and the orb's "more text" test: 13 rows -> the panel's.
    { 0x46A1B9, { 0x0D }, { kThreshold }, 1 }, { 0x46A2AB, { 0x0D }, { kThreshold }, 1 },
    { 0x46AE16, { 0x0D }, { kThreshold }, 1 }, { 0x46AFE2, { 0x0D }, { kThreshold }, 1 },
    { 0x46B00D, { 0x0D }, { kThreshold }, 1 }, { 0x46BADE, { 0x0D }, { kThreshold }, 1 },
    { 0x46BAF2, { 0x0D }, { kThreshold }, 1 },
    // The scroll orb (frames 34..36): (444, 340) -> the panel's lower right corner.
    { 0x46AE24, { 0x54, 0x01, 0, 0 }, { (BYTE)kOrbY, (BYTE)(kOrbY >> 8), 0, 0 }, 4 },
    { 0x46AE29, { 0xBC, 0x01, 0, 0 }, { (BYTE)kOrbX, (BYTE)(kOrbX >> 8), 0, 0 }, 4 },
    { 0x46AE44, { 0x54, 0x01, 0, 0 }, { (BYTE)kOrbY, (BYTE)(kOrbY >> 8), 0, 0 }, 4 },
    { 0x46AE4B, { 0xBC, 0x01, 0, 0 }, { (BYTE)kOrbX, (BYTE)(kOrbX >> 8), 0, 0 }, 4 },
    { 0x46AFE8, { 0x54, 0x01, 0, 0 }, { (BYTE)kOrbY, (BYTE)(kOrbY >> 8), 0, 0 }, 4 },
    { 0x46AFEF, { 0xBC, 0x01, 0, 0 }, { (BYTE)kOrbX, (BYTE)(kOrbX >> 8), 0, 0 }, 4 },
    { 0x46B01B, { 0x54, 0x01, 0, 0 }, { (BYTE)kOrbY, (BYTE)(kOrbY >> 8), 0, 0 }, 4 },
    { 0x46B020, { 0xBC, 0x01, 0, 0 }, { (BYTE)kOrbX, (BYTE)(kOrbX >> 8), 0, 0 }, 4 },
    { 0x46B03B, { 0x54, 0x01, 0, 0 }, { (BYTE)kOrbY, (BYTE)(kOrbY >> 8), 0, 0 }, 4 },
    { 0x46B042, { 0xBC, 0x01, 0, 0 }, { (BYTE)kOrbX, (BYTE)(kOrbX >> 8), 0, 0 }, 4 },
};

DWORD g_drawImage = game::fnDrawImage;

int Race()
{
    return *game::playerRace & 3;
}

bool HasMap()
{
    return *gameKind != kBuiltIn && *game::missionIndex < kCampaignMissions;
}

DWORD BriefingTextId()
{
    return kBriefingText + 20 * Race() + 2 * *game::missionIndex;
}

// The game's briefing text without its goal: the text with the clan's name, then an empty line.
void BuildText(char* out, size_t size)
{
    int clan = *game::playerClan;
    const char* clanName = (const char*)(0x5440F8 + (clan - 1) * 0x101 + 0x15);
    _snprintf_s(out, size, _TRUNCATE, GameText(BriefingTextId()), clanName);
    strcat_s(out, size, "\x01\x01");
}

int PrintTextWrapped(int first, int window, int x, int y, int lineH, int width, const char* text)
{
    DWORD fn = fnPrintTextWrapped;
    int lines;
    __asm
    {
        push text
        push width
        push lineH
        push y
        mov eax, first
        mov edx, window
        xor ebx, ebx
        mov ecx, x
        call fn
        mov lines, eax
    }
    return lines;
}

void SetClip(int x0, int y0, int x1, int y1)
{
    const int clip[4] = { x0, x1, y0, y1 };
    memcpy(game::clipRect, clip, sizeof(clip));
}

// What was printed in the panel goes (the panel's stone is the wide menu's surround). The panel
// lies beside the 640x480 layout: only the wide menus' canvas has it.
void ClearPanel()
{
    Pixels p;
    auto* canvas = (IDirectDrawSurface*)*game::lpBackBuffer;
    if (!WideMenuShown() || !LockSurface(canvas, &p))
        return;
    for (int y = kWellY; y < kWellY + kWellH; ++y)
        memset(p.bits + y * p.pitch + kWellX, 0, kWellW);
    canvas->Unlock(nullptr);
}

void PrintPanel(int firstLine)
{
    char text[0x1400];
    BuildText(text, sizeof(text));
    int saved[4];
    memcpy(saved, game::clipRect, sizeof(saved));
    SetClip(kWellX, kWellY, kWellX + kWellW - 1, kWellY + kWellH - 1);
    DWORD oldFont = WatcomCall(game::fnSelectFont, kBigFont + 4 * Race());
    PrintTextWrapped(firstLine, kTextWindow, kTextX, kTextY, kLineH, kTextW, text);
    WatcomCall(game::fnSelectFont, oldFont);
    memcpy(game::clipRect, saved, sizeof(saved));
}

void PrintGoal()
{
    DWORD oldFont = WatcomCall(game::fnSelectFont, kLabelFont);
    PrintAt(kGoalX, kGoalY, "SCENARIO GOAL");
    WatcomCall(game::fnSelectFont, kBigFont + 4 * Race());
    PrintWrapped(kGoalX, kGoalY + 20, kGoalW, kGoalLineH, kGoalLines, GameText(BriefingTextId() + 1));
    WatcomCall(game::fnSelectFont, oldFont);
}

// ---- text page

void __cdecl AfterTextBackground()
{
    if (!WideMenuShown())
        return;
    ClearPanel();
    if (HasMap())
        WatcomCall(fnDrawMissionMap);
}

// Replaces "call DrawImage(background)": the flags argument is still on the stack.
__declspec(naked) void TextBackgroundThunk()
{
    __asm
    {
        push dword ptr [esp + 4]
        call dword ptr [g_drawImage]
        pushad
        call AfterTextBackground
        popad
        ret 4
    }
}

void __cdecl TextBlock()
{
    if (!WideMenuShown())
        return;
    if (!HasMap())
        DrawStoneFill(Race(), kMapBoxX0, kMapBoxY0, kMapBoxX1, kMapBoxY1);
    PrintPanel(*textFirstLine);
    PrintGoal();
}

__declspec(naked) void TextBlockThunk()
{
    __asm
    {
        pushad
        call TextBlock
        popad
        push kTextBlockEnd
        ret
    }
}

// ---- map page (Hall of Heroes)

void __cdecl BeforeMapPage()
{
    ClearPanel();
}

void __cdecl AfterMapPage()
{
    if (!WideMenuShown())
        return;
    PrintPanel(0);
    if (*game::missionIndex == 0 && *briefingState == 0)   // the well is empty stone
        PrintGoal();
}

WRAP(MapPage, BeforeMapPage, AfterMapPage)

// ---- the wide menu's screen: the moved scroll orb takes the scroll buttons' clicks

bool __cdecl Active()
{
    return WideMenuShown() && *game::currentScreenHandler == game::fnBriefingHandler;
}

bool __cdecl Alias(int screenX, int screenY, int* gameX, int* gameY)
{
    int ox, oy;
    WideMenuOrigin(&ox, &oy);
    int x = screenX - ox - kOrbX, y = screenY - oy - kOrbY;
    if (*briefingState < kTextPage || *textLines <= kTextWindow || x < 0 || y < 0 || x >= kOrbSize || y >= kOrbSize)
        return false;
    bool up = y < kOrbSize / 2;
    *gameX = up ? kScrollUpX : kScrollDownX;
    *gameY = up ? kScrollUpY : kScrollDownY;
    return true;
}

}  // namespace

void BriefingInstall()
{
    if (!HookVerify(kTextBackgroundCall, kTextBackgroundCallOrig, sizeof(kTextBackgroundCallOrig), "wide briefing") ||
        !HookVerify(kTextBlock, kTextBlockOrig, sizeof(kTextBlockOrig), "wide briefing") ||
        !HookVerify(kMapPage, kMapPageOrig, sizeof(kMapPageOrig), "wide briefing"))
        return;
    for (const CodePatch& p : kPatches)
        if (!HookVerify(p.addr, p.original, p.len, "wide briefing"))
            return;
    t_MapPage = HookDetour(kMapPage, kMapPageOrig, sizeof(kMapPageOrig), Wrap_MapPage, "wide briefing");
    if (!t_MapPage)
        return;
    for (const CodePatch& p : kPatches)
        PatchWrite(p.addr, p.wide, p.len);
    HookCall(kTextBackgroundCall, TextBackgroundThunk);
    HookJump(kTextBlock, TextBlockThunk, sizeof(kTextBlockOrig));
    WideMenuRegister({ Active, nullptr, Alias });
    Log("briefing: map and text side by side, goal in the well");
}
