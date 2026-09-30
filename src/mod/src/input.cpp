#include "input.h"

#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>

#include "camera.h"
#include "canvas.h"
#include "escmenu.h"
#include "fx.h"
#include "game.h"
#include "hires.h"
#include "hook.h"
#include "hud.h"
#include "layout.h"
#include "log.h"
#include "saveload.h"
#include "selection.h"
#include "widemenu.h"

namespace
{
using WndProcFn = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
WndProcFn g_original;
bool g_doubleClick, g_modernClicks;
bool g_swallowNextUp;

bool InMission()
{
    return *game::currentScreenHandler == game::fnInGameScreenHandler && *game::modalDialog == 0 &&
           *game::endScreen == 0;
}

bool InMapView(int x, int y)
{
    int tx, ty;
    HiresViewTiles(&tx, &ty);
    return x >= 0 && y >= 0 && x < tx * 24 && y < ty * 24;
}

bool Selectable(const game::Thing& t, const game::Thing& like)
{
    return t.type == like.type && t.category == like.category && t.owner == like.owner &&
           !(t.flags & 0x21);
}

void SelectSameType(WORD id)
{
    const game::Thing& like = game::things[id];
    if ((like.category != 2 && like.category != 3) || like.owner != *game::playerClan)
        return;
    int x0 = *game::smallStartX, y0 = *game::smallStartY, tilesX, tilesY;
    HiresViewTiles(&tilesX, &tilesY);
    if (CanvasActive())  // only what the screen shows of the canvas
    {
        int ox, oy;
        CanvasShownOffset(&ox, &oy);
        x0 += ox / layout::kTile;
        y0 += oy / layout::kTile;
        tilesX = layout::kScreenViewW / layout::kTile + 2;
        tilesY = layout::kScreenViewH / layout::kTile + 2;
    }
    SelectionClear();
    int count = 0;
    for (int ty = y0; ty < y0 + tilesY && ty < game::kMapTiles; ++ty)
        for (int tx = x0; tx < x0 + tilesX && tx < game::kMapTiles; ++tx)
            for (WORD u = game::tileThings[ty * game::kMapTiles + tx] & 0x7FF; u; u = game::things[u].nextOnTile & 0x7FF)
                if (Selectable(game::things[u], like) && SelectionAdd(u, false))
                    ++count;
    SelectionUpdateCurrent();
    Log("double-click: selected %d of type %u", count, like.type);
}

// ---- modern clicks: right button orders, left button selects / deselects

bool IsOwnCommandable(const game::Thing& t)
{
    return t.owner == *game::playerClan && (t.category == 2 || t.category == 3 || t.category == 8);
}

// The same condition the left-click handler applies before it turns a click into an order.
bool SelectionTakesOrders()
{
    WORD cur = *game::curSelected;
    return cur == 0x801 || (cur && cur < 0x800 && IsOwnCommandable(game::things[cur]));
}

// Thing under the cursor as the click handler sees it: enemies hidden by fog count as ground.
WORD ThingUnderCursor()
{
    WORD id = (WORD)WatcomCall3(game::fnFindThingAtScreenPos, *game::mouseX, *game::mouseY, 0);
    if (id && game::things[id].owner != *game::playerClan &&
        WatcomCall(game::fnGetConcealment, (DWORD)&game::things[id], 1) == 4)
        return 0;
    return id;
}

// Leaves the command popup or a targeting mode the way the game's own cancel does.
void CancelPendingCommand()
{
    WatcomCall(game::fnClearPopupFlags);
    *game::pendingCmd = 0;
    *game::panelMode = 1;
    WatcomCall(game::fnResetPanel);
    WatcomCall(game::fnClosePopupLevel);
    if (*game::cursorHidden == 1)
    {
        *game::cursorHidden = 0;
        WatcomCall(game::fnShowCursor);
        WatcomCall(game::fnRestoreCursor);
    }
    *game::waypointCount = 0;
}

bool ModernClickApplies()
{
    return g_modernClicks && *game::altPanelOpen == 0 && InMapView(*game::mouseX, *game::mouseY);
}

// FindThingAtScreenPos (eax = x, edx = y, ebx = clan whose things are skipped). While an order is
// synthesised from a right click, the player's own things are skipped, so the order path takes a
// click on a friendly unit as a click on the ground under it.
const BYTE kFindThingOrig[] = { 0x51, 0x56, 0x57, 0x83, 0xEC, 0x04 };   // push ecx/esi/edi ; sub esp, 4
void* t_FindThing;
DWORD g_skipClan;

__declspec(naked) void FindThingThunk()
{
    __asm
    {
        cmp dword ptr [g_skipClan], 0
        je original
        push ebx
        mov ebx, dword ptr [g_skipClan]
        call dword ptr [t_FindThing]
        pop ebx
        ret
    original:
        jmp dword ptr [t_FindThing]
    }
}

// The game's left-click order path for the selection at lParam, friendly units counting as ground.
LRESULT OrderAt(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    *game::dragAnchorX = *game::dragAnchorY = -1;
    g_skipClan = *game::playerClan;
    LRESULT r = g_original(hwnd, WM_LBUTTONUP, wParam, lParam);
    g_skipClan = 0;
    return r;
}

// ---- minimap: left button moves the camera (press or drag), right button orders the selection

// Minimap in canvas coordinates: 96x96 pixels, one per two map tiles.
constexpr int kMinimapLeft = 0x212 + layout::kDX, kMinimapTop = 0x88, kMinimapSize = 0x60;
bool g_minimapDrag;

bool InMinimap(LPARAM canvasPos)
{
    int x = GET_X_LPARAM(canvasPos) - kMinimapLeft, y = GET_Y_LPARAM(canvasPos) - kMinimapTop;
    return x >= 0 && y >= 0 && x <= kMinimapSize && y <= kMinimapSize;
}

void GlideToMinimap(LPARAM canvasPos)
{
    int x = GET_X_LPARAM(canvasPos) - kMinimapLeft, y = GET_Y_LPARAM(canvasPos) - kMinimapTop;
    x = max(0, min(kMinimapSize, x));
    y = max(0, min(kMinimapSize, y));
    CameraJumpTo(x * 2 * layout::kTile + layout::kTile, y * 2 * layout::kTile + layout::kTile);
}

// Outside targeting (a pending command picks its target with the left button as in the original).
bool MinimapSwapApplies()
{
    return g_modernClicks && CanvasActive() && *game::altPanelOpen == 0 && *game::panelMode < 5;
}

// Returns true when the message was handled.
bool OnMinimapMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    *result = 0;
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        if (!MinimapSwapApplies() || !InMinimap(lParam))
            return false;
        g_minimapDrag = true;
        SetCapture(hwnd);
        GlideToMinimap(lParam);
        return true;
    case WM_MOUSEMOVE:
        if (g_minimapDrag)
            GlideToMinimap(lParam);
        return false;
    case WM_LBUTTONUP:
        if (!g_minimapDrag)
            return false;
        g_minimapDrag = false;
        ReleaseCapture();
        return true;
    case WM_RBUTTONDOWN:
        return MinimapSwapApplies() && InMinimap(lParam);
    case WM_RBUTTONUP:
        if (!MinimapSwapApplies() || !InMinimap(lParam))
            return false;
        *result = OrderAt(hwnd, wParam, lParam);  // the game's minimap order
        *game::rButtonHeld = 0;
        return true;
    }
    return false;
}

// Right button: order the selection to the clicked spot or target through the game's own
// left-click order path (a friendly unit is moved to). With nothing to order, a click on an own
// unit keeps the original behaviour (select it with its command popup).
LRESULT OnRightButtonUp(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    if (*game::panelMode >= 5)
    {
        CancelPendingCommand();
        *game::rButtonHeld = 0;
        return 0;
    }
    WORD id = ThingUnderCursor();
    bool onOwnUnit = id && IsOwnCommandable(game::things[id]);
    if (onOwnUnit && !SelectionTakesOrders())
        return g_original(hwnd, WM_RBUTTONUP, wParam, lParam);
    if (*game::panelMode == 2)
        CancelPendingCommand();
    *game::rButtonHeld = 0;
    if (!SelectionTakesOrders() || *game::lButtonHeld)
        return 0;
    return OrderAt(hwnd, wParam, lParam);
}

// Left button: select what is clicked (never order it), deselect on empty ground.
LRESULT OnLeftButtonUp(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    // With the command grid, the floating command popup is not shown: a click on the map closes it.
    if (HudGridEnabled() && *game::panelMode == 2)
        CancelPendingCommand();
    short ax = *game::dragAnchorX, ay = *game::dragAnchorY;
    bool boxSelect = ax >= 0 && (abs(*game::mouseX - ax) > 16 || abs(*game::mouseY - ay) > 16);
    if (boxSelect || *game::panelMode >= 2 || *game::waypointCount || *game::altDown)
        return g_original(hwnd, WM_LBUTTONUP, wParam, lParam);

    WORD id = ThingUnderCursor();
    if (id)
    {
        if (!IsOwnCommandable(game::things[id]) && SelectionTakesOrders())
        {
            WatcomCall(game::fnClearPopupFlags);
            SelectionClear();
            *game::curSelected = 0;
        }
        return g_original(hwnd, WM_LBUTTONUP, wParam, lParam);
    }

    *game::dragAnchorX = *game::dragAnchorY = -1;
    *game::lButtonHeld = 0;
    if (*game::shiftDown || *game::ctrlDown)
        return 0;
    LRESULT r = g_original(hwnd, WM_RBUTTONUP, wParam, lParam);  // the game's deselect-all
    *game::rButtonHeld = 0;
    return r;
}

// A box selection started on the map keeps its corner on the map while the mouse is over the HUD.
LPARAM ClampToMapWhileBoxSelecting(LPARAM screen)
{
    if (!InputBoxSelecting() || GET_Y_LPARAM(screen) < layout::kScreenViewH)
        return screen;
    return MAKELPARAM(GET_X_LPARAM(screen), layout::kScreenViewH - 1);
}

LRESULT WINAPI HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Mouse messages arrive in screen coordinates; missions draw into the larger canvas, whose
    // coordinates the game's own handlers get.
    bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && msg != WM_MOUSEWHEEL;
    // The save / load screen runs its own loop and takes the input; nothing reaches the game.
    if (SaveLoadActive() && ((msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)))
        return 0;
    if (mouse)
        CanvasSetScreenMouse(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    bool mission = *game::currentScreenHandler == game::fnInGameScreenHandler;
    if ((mission || msg == WM_KEYUP || msg == WM_KILLFOCUS || msg == WM_ACTIVATEAPP) &&
        CameraOnMessage(msg, wParam, lParam))
        return 0;
    if (mouse && mission && CanvasActive() && InMission() &&
        HudOnMouse(msg, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
    int menuDX, menuDY;
    LPARAM menuPos;
    if (mouse && EscMenuMouse(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &menuPos))
        lParam = menuPos;
    else if (mouse && HiresMenuOffset(&menuDX, &menuDY))
        lParam = MAKELPARAM(GET_X_LPARAM(lParam) - menuDX, GET_Y_LPARAM(lParam) - menuDY);
    else if (mouse && mission && CanvasActive())
        lParam = CanvasTranslateMouse(ClampToMapWhileBoxSelecting(lParam));
    else if (mouse)
        WideMenuMouse(&lParam);
    if (!InMission())
        return g_original(hwnd, msg, wParam, lParam);
    if (msg == WM_KEYDOWN && !(lParam & 0x40000000) && FxOnKeyDown(wParam))
        return 0;

    LRESULT minimapResult;
    if (OnMinimapMessage(hwnd, msg, wParam, lParam, &minimapResult))
        return minimapResult;

    switch (msg)
    {
    case WM_LBUTTONDBLCLK:
        if (g_doubleClick && InMapView(*game::mouseX, *game::mouseY))
        {
            WORD id = (WORD)WatcomCall3(game::fnFindThingAtScreenPos, *game::mouseX, *game::mouseY, 0);
            if (id)
            {
                SelectSameType(id);
                g_swallowNextUp = true;  // the release that follows would re-select the single unit
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_swallowNextUp)
        {
            g_swallowNextUp = false;
            return 0;
        }
        if (ModernClickApplies())
            return OnLeftButtonUp(hwnd, wParam, lParam);
        break;
    case WM_RBUTTONUP:
        if (ModernClickApplies())
            return OnRightButtonUp(hwnd, wParam, lParam);
        break;
    }
    return g_original(hwnd, msg, wParam, lParam);
}

const BYTE kWndProcPrologue[] = { 0x53, 0x56, 0x57, 0x55, 0x83, 0xEC, 0x40 };
}  // namespace

bool InputBoxSelecting()
{
    return *game::lButtonHeld && *game::dragAnchorX >= 0 && *game::dragAnchorX < layout::kViewW &&
           *game::dragAnchorY >= 0 && *game::dragAnchorY < layout::kViewH;
}

void InputInstall(const char* iniPath)
{
    g_doubleClick = GetPrivateProfileIntA("Controls", "DoubleClickType", 1, iniPath) != 0;
    g_modernClicks = GetPrivateProfileIntA("Controls", "ModernClicks", 1, iniPath) != 0;
    g_original = (WndProcFn)HookDetour(game::fnWndProc, kWndProcPrologue, sizeof(kWndProcPrologue), HookedWndProc, "mouse input");
    if (g_modernClicks)
        t_FindThing = HookDetour(game::fnFindThingAtScreenPos, kFindThingOrig, sizeof(kFindThingOrig), FindThingThunk, "right-click orders");
    if (g_original)
        Log("mouse: double-click=%d modern-clicks=%d", g_doubleClick, g_modernClicks);
}
