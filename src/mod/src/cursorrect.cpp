#include "cursorrect.h"

#include <string.h>

#include "canvas.h"
#include "fx.h"
#include "game.h"
#include "hook.h"
#include "input.h"
#include "layout.h"
#include "log.h"

using namespace layout;

namespace
{
// DrawGameFrame: DrawRectOutline(left, top, right, bottom, thickness, colour) of the rectangle
// InGameMouseTick left at 0x4C2DB0 (canvas coordinates), while dragging or placing.
constexpr DWORD kOutlineCall = 0x41ABD3;
const BYTE kOutlineCallOrig[] = { 0xE8, 0xB0, 0x9C, 0xFF, 0xFF };   // call 414888h
DWORD g_drawRectOutline = 0x414888;

struct Recorded
{
    bool valid;
    bool placement;          // footprint snapped to tiles, else the box-selection drag
    int left, top, right, bottom;
    BYTE color;
    int mouseX, mouseY;      // canvas mouse position the game computed the rectangle from
} g_rect;

bool __cdecl Record(const int* args)
{
    if (!CanvasActive())
        return false;
    g_rect.valid = true;
    g_rect.placement = *game::cursorHidden != 0;
    g_rect.left = args[0];
    g_rect.top = args[1];
    g_rect.right = args[2];
    g_rect.bottom = args[3];
    g_rect.color = (BYTE)args[5];
    g_rect.mouseX = *game::mouseX;
    g_rect.mouseY = *game::mouseY;
    return true;
}

// Replaces the call: records the six stack arguments (ret 18h like the original), or draws when
// the canvas is not attached.
__declspec(naked) void OutlineThunk()
{
    __asm
    {
        pushad
        lea eax, [esp + 36]
        push eax
        call Record
        add esp, 4
        test al, al
        popad
        jz draw
        ret 18h
    draw:
        jmp dword ptr [g_drawRectOutline]
    }
}

int TileFloor(int v)
{
    return (v >= 0 ? v : v - (kTile - 1)) / kTile * kTile;
}

void Span(BYTE* bits, int pitch, int y, int x0, int x1, BYTE color)
{
    x0 = max(x0, 0);
    x1 = min(x1, kScreenViewW - 1);
    if (y < 0 || y >= kScreenViewH || x1 < x0)
        return;
    memset(bits + y * pitch + x0, color, x1 - x0 + 1);
    FxMarkScreenUi(x0, y, x1 - x0 + 1, 1);
}

void Column(BYTE* bits, int pitch, int x, int y0, int y1, BYTE color)
{
    y0 = max(y0, 0);
    y1 = min(y1, kScreenViewH - 1);
    if (x < 0 || x >= kScreenViewW || y1 < y0)
        return;
    for (int y = y0; y <= y1; ++y)
        bits[y * pitch + x] = color;
    FxMarkScreenUi(x, y0, 1, y1 - y0 + 1);
}
}  // namespace

void CursorRectInstall()
{
    if (!HookVerify(kOutlineCall, kOutlineCallOrig, sizeof(kOutlineCallOrig), "cursor rect"))
        return;
    HookCall(kOutlineCall, OutlineThunk);
    Log("cursor rect: drag box and placement footprint drawn at present rate");
}

void CursorRectFrameStart()
{
    g_rect.valid = false;
}

void CursorRectDraw(BYTE* bits, int pitch, int shownX, int shownY)
{
    if (!g_rect.valid)
        return;
    int mx, my;
    CanvasScreenMouse(&mx, &my);
    bool onMap = mx >= 0 && mx < kScreenViewW && my >= 0 && my < kScreenViewH;
    int left = g_rect.left - shownX, top = g_rect.top - shownY;
    int right = g_rect.right - shownX, bottom = g_rect.bottom - shownY;
    if (g_rect.placement)
    {
        if (!*game::cursorHidden)
            return;
        if (onMap)
        {
            int dx = TileFloor(mx + shownX) - TileFloor(g_rect.mouseX);
            int dy = TileFloor(my + shownY) - TileFloor(g_rect.mouseY);
            left += dx;
            right += dx;
            top += dy;
            bottom += dy;
        }
    }
    else
    {
        if (!InputBoxSelecting())
            return;
        left = *game::dragAnchorX - shownX;
        top = *game::dragAnchorY - shownY;
        right = min(max(mx, 0), kScreenViewW - 1);
        bottom = min(max(my, 0), kScreenViewH - 1);
    }
    if (left == right || top == bottom)
        return;
    int x0 = min(left, right), x1 = max(left, right);
    int y0 = min(top, bottom), y1 = max(top, bottom);
    Span(bits, pitch, y0, x0, x1, g_rect.color);
    Span(bits, pitch, y1, x0, x1, g_rect.color);
    Column(bits, pitch, x0, y0, y1, g_rect.color);
    Column(bits, pitch, x1, y0, y1, g_rect.color);
}
