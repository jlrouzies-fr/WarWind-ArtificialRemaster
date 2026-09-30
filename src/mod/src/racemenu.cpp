#include "racemenu.h"

#include <windows.h>

#include "game.h"
#include "hook.h"
#include "log.h"
#include "patch.h"
#include "widemenu.h"

namespace
{
constexpr DWORD kBuildRaceMenu = 0x470DF0;
const BYTE kBuildRaceMenuOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57 };   // push ebx/ecx/edx/esi/edi
void* t_BuildRaceMenu;

// New rectangles in the menu's coordinates (screen = + the wide menu origin, 16,30): the column
// right of the art, in the original order; the name fields side by side (ui_menus/campaign_rects.txt).
struct ItemRect
{
    WORD id, left, top, right, bottom;
};
const ItemRect kRects[] = {
    { 0x08, 688, 0, 815, 64 },    { 0x09, 816, 0, 943, 64 },     // leader's / clan's name
    { 0x01, 672, 66, 928, 108 },  { 0x02, 672, 108, 928, 150 },  // load saved game / built-in scenario
    { 0x0E, 672, 150, 928, 192 }, { 0x0A, 672, 192, 928, 234 },  // game / sound options
    { 0x04, 672, 234, 928, 276 }, { 0x05, 672, 276, 928, 318 },  // ideology / earned cinematics
    { 0x15, 672, 318, 928, 360 }, { 0x0F, 672, 360, 928, 402 },  // credits / quit
    { 0x00, 709, 404, 759, 476 }, { 0x12, 837, 404, 887, 476 },  // begin campaign / previous screen
};

// The name boxes, DrawPanel9(left - 0x14, 1 row, 7 columns), become one tile narrower
// (left - 0x10, 6 columns) so both fit side by side.
struct CodePatch
{
    DWORD addr;
    BYTE original[3], wide[3];
    int len;
};
const CodePatch kNameBoxes[] = {
    { 0x46DE7C, { 0x6A, 0x07 }, { 0x6A, 0x06 }, 2 },                // push 7 (columns)
    { 0x46DE96, { 0x83, 0xEA, 0x14 }, { 0x83, 0xEA, 0x10 }, 3 },    // sub edx, 14h
    { 0x46DF15, { 0x6A, 0x07 }, { 0x6A, 0x06 }, 2 },
    { 0x46DF2F, { 0x83, 0xEA, 0x14 }, { 0x83, 0xEA, 0x10 }, 3 },
};

// MenuInit puts the dialogs opened from this menu (options, sound, ideology) at x 0x148, over the
// original button column; in the wide layout they are centred on the stone (they are at most 320
// wide).
constexpr int kWideDialogX = (640 - 320) / 2;

void SetNameBoxes(bool wide)
{
    for (const CodePatch& p : kNameBoxes)
        PatchWrite(p.addr, wide ? p.wide : p.original, p.len);
}

void __cdecl NoOp() {}

void __cdecl AfterBuild()
{
    bool wide = WideMenuShown();
    SetNameBoxes(wide);
    if (!wide)
        return;
    *game::menuPanelX = kWideDialogX;
    for (game::MenuItem* item = *game::menuItems; item; item = item->next)
        for (const ItemRect& r : kRects)
            if (r.id == item->id)
            {
                item->left = r.left;
                item->top = r.top;
                item->right = r.right;
                item->bottom = r.bottom;
            }
}

WRAP(BuildRaceMenu, NoOp, AfterBuild)
}  // namespace

void RaceMenuInstall()
{
    if (!HookVerify(kBuildRaceMenu, kBuildRaceMenuOrig, sizeof(kBuildRaceMenuOrig), "wide race menu"))
        return;
    for (const CodePatch& p : kNameBoxes)
        if (!HookVerify(p.addr, p.original, p.len, "wide race menu name box"))
            return;
    t_BuildRaceMenu = HookDetour(kBuildRaceMenu, kBuildRaceMenuOrig, sizeof(kBuildRaceMenuOrig), Wrap_BuildRaceMenu,
                                 "wide race menu");
    if (t_BuildRaceMenu)
        Log("racemenu: buttons beside the race stone");
}
