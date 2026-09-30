#include "escmenu.h"

#include <ddraw.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "canvas.h"
#include "game.h"
#include "gametext.h"
#include "hires.h"
#include "hook.h"
#include "hudart.h"
#include "layout.h"
#include "log.h"
#include "menudraw.h"

using namespace layout;

namespace
{
// DrawMenu, in-game menu: DrawPanel9(art, [0x4B7728], 0x64, 12 rows, 18 cols) at 0x46DD6F, then
// one label per button; 0x46DDFE restores the font and returns.
constexpr DWORD kPanelDraw = 0x46DD6F, kPanelDrawResume = 0x46DD76, kLabelsDone = 0x46DDFE;
const BYTE kPanelDrawOrig[] = { 0x6A, 0x12, 0xB9, 0x0C, 0x00, 0x00, 0x00 };   // push 12h ; mov ecx, 0Ch
constexpr int kInGameMenu = 6, kBaseDialog = 0;

// Layout on the screen, the panel centred on the map area (ui_menus/mockups.py esc_menu).
constexpr int kPanelTile = 16;
constexpr int kCols = 50, kRows = 19;
constexpr int kPanelW = (kCols + 2) * kPanelTile, kPanelH = (kRows + 2) * kPanelTile;
constexpr int kPanelX = (kScreenViewW - kPanelW) / 2, kPanelY = (kScreenViewH - kPanelH) / 2;
constexpr int kButtonW = 196, kButtonH = 24, kPitch = 34, kLabelDY = 8;
constexpr int kLeftX = kPanelX + 40, kRightX = kPanelX + kPanelW - 40 - kButtonW, kTop = kPanelY + 82;
constexpr int kGoalX = kLeftX + kButtonW + 28, kGoalW = kRightX - 28 - kGoalX, kGoalH = 3 * kPitch - 10;
constexpr int kGoalLineH = 18, kGoalLines = 4;
constexpr int kHeaderDY = -20;
constexpr int kTitleY = kPanelY + 26, kRuleY = kPanelY + 46;
// The race's OK / back orb bar: RES.004 #10 + race, frame 6 (256x64).
constexpr DWORD kBarArt = 0x8004000A;
constexpr int kBarFrame = 6, kBarW = 256, kBarH = 64;
constexpr int kBarX = kPanelX + (kPanelW - kBarW) / 2, kBarY = kPanelY + kPanelH - 16 - 12 - kBarH - 14;
constexpr int kOrbLabelW = 150, kOrbLabelY = kBarY + kBarH + 4;

// Race panel fonts: big 0x800100E7 + 4 * race, small 0x800100E9 + 4 * race; +1 = highlighted.
constexpr DWORD kBigFont = 0x800100E7, kSmallFont = 0x800100E9;

enum MenuId : WORD
{
    kLoad = 0x01, kMainMenu = 0x07, kSoundOptions = 0x0D, kGameOptions = 0x0E, kQuit = 0x0F,
    kRestart = 0x10, kReviewGoal = 0x11, kSave = 0x13, kReturnToGame = 0x14,
};

struct Button
{
    WORD id;
    int x, y, w, h;
    bool orb;        // an orb of the bar (its label is under the bar), else a raised plate
};
const Button kButtons[] = {
    { kSave, kLeftX, kTop, kButtonW, kButtonH, false },
    { kLoad, kLeftX, kTop + kPitch, kButtonW, kButtonH, false },
    { kRestart, kLeftX, kTop + 2 * kPitch, kButtonW, kButtonH, false },
    { kGameOptions, kRightX, kTop, kButtonW, kButtonH, false },
    { kSoundOptions, kRightX, kTop + kPitch, kButtonW, kButtonH, false },
    { kQuit, kRightX, kTop + 2 * kPitch, kButtonW, kButtonH, false },
    { kReviewGoal, kGoalX + (kGoalW - kButtonW) / 2, kTop + 3 * kPitch + 2, kButtonW, kButtonH, false },
    { kReturnToGame, kBarX + 6, kBarY + 2, 64, 60, true },
    { kMainMenu, kBarX + kBarW - 70, kBarY + 2, 64, 60, true },
};
constexpr int kOrbLabelX[] = { kBarX - 40, kBarX + kBarW - 110 };   // Return to Game, Return to Main Menu

bool g_enabled;
std::vector<BYTE> g_scene;   // the dimmed mission behind the menu (map view)
bool g_sceneKept;
bool g_panelShown;           // the wide panel covers the kept scene

const game::MenuItem* FindItem(WORD id)
{
    for (const game::MenuItem* item = *game::menuItems; item; item = item->next)
        if (item->id == id)
            return item;
    return nullptr;
}

bool OverMission()
{
    return g_enabled && CanvasActive() && *game::currentScreenHandler == game::fnMenuScreenHandler;
}

bool Shown()
{
    return OverMission() && *game::menuScreen == kInGameMenu && *game::menuDialog == kBaseDialog &&
           !*game::menuConfirm;
}

bool LockCanvas(Pixels* out)
{
    return LockSurface((IDirectDrawSurface*)CanvasSurface(), out);
}

void UnlockCanvas()
{
    ((IDirectDrawSurface*)CanvasSurface())->Unlock(nullptr);
}

// Plates in the canvas at the screen layout shifted by (ox, oy).
void PaintPlates(int ox, int oy)
{
    Pixels p;
    if (!HudArtPrepare() || !LockCanvas(&p))
        return;
    PlateTones t = HudPlateTones();
    HudArtFill(p, ox + kPanelX + 40, oy + kRuleY, kPanelW - 80, 1, t.dark);
    HudArtFill(p, ox + kPanelX + 40, oy + kRuleY + 1, kPanelW - 80, 1, t.pale);
    for (const Button& b : kButtons)
        if (!b.orb && FindItem(b.id))
            PlateRaised(p, ox + b.x, oy + b.y, b.w, b.h, t, *game::menuHover == b.id ? t.dark : t.mid, false);
    if (FindItem(kReviewGoal))
        PlateSunken(p, ox + kGoalX, oy + kTop, kGoalW, kGoalH, t, t.well);
    UnlockCanvas();
}

void PaintText(int ox, int oy)
{
    DWORD race = 4 * (*game::playerRace & 3);
    DWORD oldFont = WatcomCall(game::fnSelectFont, kBigFont + race);
    char title[96];
    _snprintf_s(title, sizeof(title), _TRUNCATE, "%s  -  %s", game::leaderName, game::clanName);
    PrintCentred(ox + kPanelX, oy + kTitleY, kPanelW, title);
    bool goal = FindItem(kReviewGoal) != nullptr;
    if (goal)
    {
        DWORD text = game::kScenarioGoalBase + 20 * (*game::playerRace & 3) + 2 * *game::missionIndex;
        PrintWrapped(ox + kGoalX + 10, oy + kTop + 8, kGoalW - 20, kGoalLineH, kGoalLines, GameText(text));
    }

    WatcomCall(game::fnSelectFont, kSmallFont + race + 1);
    PrintCentred(ox + kLeftX, oy + kTop + kHeaderDY, kButtonW, "GAME");
    PrintCentred(ox + kRightX, oy + kTop + kHeaderDY, kButtonW, "OPTIONS");
    if (goal)
        PrintCentred(ox + kGoalX, oy + kTop + kHeaderDY, kGoalW, "SCENARIO GOAL");

    int orb = 0;
    for (const Button& b : kButtons)
    {
        bool present = FindItem(b.id) != nullptr;
        bool hot = *game::menuHover == b.id;
        const char* label = GameText(game::kMenuLabelBase + b.id);
        if (present)
            WatcomCall(game::fnSelectFont, kSmallFont + race + (hot ? 1 : 0));
        if (b.orb)
        {
            if (present)
                PrintCentred(ox + kOrbLabelX[orb], oy + kOrbLabelY, kOrbLabelW, label);
            ++orb;
        }
        else if (present)
            PrintCentred(ox + b.x, oy + b.y + kLabelDY, b.w, label);
    }
    WatcomCall(game::fnSelectFont, oldFont);
}

// Replaces the original panel and labels while the wide menu is shown.
bool __cdecl DrawPanel()
{
    if (!Shown())
        return false;
    int ox, oy;
    CanvasShownOffset(&ox, &oy);   // canvas = screen + the map offset shown
    HiresSuspendShift(true);
    DrawTarget target((IDirectDrawSurface*)CanvasSurface(), kCanvasW, kCanvasH);

    WatcomCall5(game::fnDrawPanel9, *game::menuPanelArt, ox + kPanelX, oy + kPanelY, kRows, kCols);
    PaintPlates(ox, oy);
    if (FindItem(kReturnToGame) || FindItem(kMainMenu))
        WatcomCall5(game::fnDrawImage, kBarArt + (*game::playerRace & 3), kBarFrame, ox + kBarX, oy + kBarY, 0);
    PaintText(ox, oy);
    HiresSuspendShift(false);
    g_panelShown = true;
    return true;
}

// Map view rows between the canvas and g_scene.
void CopyScene(bool keep)
{
    Pixels p;
    if (!LockCanvas(&p))
        return;
    for (int y = 0; y < kViewH; ++y)
    {
        BYTE* row = p.bits + y * p.pitch;
        BYTE* kept = &g_scene[(size_t)y * kViewW];
        if (keep)
            memcpy(kept, row, kViewW);
        else
            memcpy(row, kept, kViewW);
    }
    UnlockCanvas();
}

// A dialog other than the wide panel (options, sound, goal, a Yes / No question) is drawn over the
// mission, as in the original, not over the panel.
void __cdecl BeforeDrawMenu()
{
    if (g_panelShown && g_sceneKept && OverMission() && !Shown())
    {
        CopyScene(false);
        g_panelShown = false;
    }
}


__declspec(naked) void PanelThunk()
{
    __asm
    {
        pushad
        call DrawPanel
        test al, al
        popad
        jnz drawn
        push 12h
        mov ecx, 0Ch
        push kPanelDrawResume
        ret
    drawn:
        push kLabelsDone
        ret
    }
}
}  // namespace

void EscMenuInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("UI", "WideMenus", 1, iniPath))
        return;
    if (!HookVerify(kPanelDraw, kPanelDrawOrig, sizeof(kPanelDrawOrig), "wide in-game menu"))
        return;
    if (!MenuDrawHook(BeforeDrawMenu, nullptr))
        return;
    HookJump(kPanelDraw, PanelThunk, sizeof(kPanelDrawOrig));
    g_scene.resize((size_t)kViewW * kViewH);
    g_enabled = true;
    Log("escmenu: wide in-game menu");
}

void EscMenuSceneDrawn()
{
    if (!OverMission())
        return;
    PALETTEENTRY pal[256];
    Pixels p;
    if (!PaletteRead(pal) || !LockCanvas(&p))
        return;
    BYTE dim[256];
    for (int i = 0; i < 256; ++i)
        dim[i] = PaletteNearest(pal, pal[i].peRed * 2 / 5, pal[i].peGreen * 2 / 5, pal[i].peBlue * 2 / 5);
    for (int y = 0; y < kViewH; ++y)
    {
        BYTE* row = p.bits + y * p.pitch;
        for (int x = 0; x < kViewW; ++x)
            row[x] = dim[row[x]];
    }
    UnlockCanvas();
    CopyScene(true);
    g_sceneKept = true;
    g_panelShown = false;
}

bool EscMenuMouse(int screenX, int screenY, LPARAM* menuPos)
{
    if (!Shown())
        return false;
    *menuPos = MAKELPARAM(0, 0);   // left of every original button
    for (const Button& b : kButtons)
    {
        if (screenX < b.x || screenX >= b.x + b.w || screenY < b.y || screenY >= b.y + b.h)
            continue;
        if (const game::MenuItem* item = FindItem(b.id))
            *menuPos = MAKELPARAM((item->left + item->right) / 2, (item->top + item->bottom) / 2);
    }
    return true;
}
