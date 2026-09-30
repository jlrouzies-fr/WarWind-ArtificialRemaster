#include "endscreen.h"

#include <windows.h>
#include <ddraw.h>
#include <string.h>

#include <optional>
#include <vector>

#include "game.h"
#include "hook.h"
#include "hudart.h"
#include "log.h"
#include "menuart.h"
#include "offscreen.h"
#include "widemenu.h"

namespace
{
constexpr int kW = 960, kH = 540, kArtW = 640, kArtH = 480;
constexpr int kBoxPad = 12;

// DrawEndTally: the result title and lines, in the race's big font, over the picture.
constexpr DWORD kDrawEndTally = 0x46CC4C;
const BYTE kDrawEndTallyOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00 };   // pushes ; sub esp, 100h
void* t_DrawEndTally;

Offscreen g_scratch;
std::optional<DrawTarget> g_target;
std::vector<BYTE> g_block;   // the tally's pixels, 0 = transparent
int g_blockW, g_blockH;
Shades g_shades;
PALETTEENTRY g_shadesPalette[256];

bool __cdecl Active()
{
    return WideMenuShown() && *game::currentScreenHandler == game::fnInGameScreenHandler && *game::endScreen;
}

void __cdecl BeforeTally()
{
    if (!Active() || (!g_scratch.Valid() && !g_scratch.Create(kArtW, kArtH)))
        return;
    Pixels p;
    if (!g_scratch.Lock(&p))
        return;
    for (int y = 0; y < kArtH; ++y)
        memset(p.bits + y * p.pitch, 0, kArtW);
    g_scratch.Unlock();
    g_target.emplace(g_scratch.Surface(), kArtW, kArtH);
}

// Keeps the bounding box of what the tally printed.
void __cdecl AfterTally()
{
    if (!g_target)
        return;
    g_target.reset();
    Pixels p;
    if (!g_scratch.Lock(&p, true))
        return;
    int x0 = kArtW, y0 = kArtH, x1 = -1, y1 = -1;
    for (int y = 0; y < kArtH; ++y)
        for (int x = 0; x < kArtW; ++x)
            if (p.bits[y * p.pitch + x])
            {
                x0 = min(x0, x);
                x1 = max(x1, x);
                y0 = min(y0, y);
                y1 = max(y1, y);
            }
    g_block.clear();
    if (x1 >= 0)
    {
        g_blockW = x1 - x0 + 1;
        g_blockH = y1 - y0 + 1;
        g_block.resize((size_t)g_blockW * g_blockH);
        for (int y = 0; y < g_blockH; ++y)
            memcpy(&g_block[(size_t)y * g_blockW], p.bits + (y0 + y) * p.pitch + x0, g_blockW);
    }
    g_scratch.Unlock();
}

WRAP(DrawEndTally, BeforeTally, AfterTally)

// The tally centred in the column right of the picture, on a darkened, bevelled box.
void __cdecl Decorate(IDirectDrawSurface* screen)
{
    PALETTEENTRY pal[256];
    if (g_block.empty() || !PaletteRead(pal))
        return;
    if (memcmp(pal, g_shadesPalette, sizeof(pal)) && g_shades.Build())
        memcpy(g_shadesPalette, pal, sizeof(pal));
    int ox, oy;
    WideMenuOrigin(&ox, &oy);
    int column = kArtW + ox;
    int bx = column + (kW - column - g_blockW) / 2, by = (kH - g_blockH) / 2;
    int boxX = bx - kBoxPad, boxY = by - kBoxPad, boxW = g_blockW + 2 * kBoxPad, boxH = g_blockH + 2 * kBoxPad;

    Pixels p;
    if (!LockSurface(screen, &p))
        return;
    for (int y = boxY; y < boxY + boxH; ++y)
        for (int x = boxX; x < boxX + boxW; ++x)
            p.bits[y * p.pitch + x] = g_shades.Apply(p.bits[y * p.pitch + x], 1.0f, x, y);
    PlateBevel(p, boxX, boxY, boxW, boxH, StoneTones(pal));
    for (int y = 0; y < g_blockH; ++y)
        for (int x = 0; x < g_blockW; ++x)
            if (BYTE c = g_block[(size_t)y * g_blockW + x])
                p.bits[(by + y) * p.pitch + bx + x] = c;
    screen->Unlock(nullptr);
}
}  // namespace

void EndScreenInstall()
{
    if (!HookVerify(kDrawEndTally, kDrawEndTallyOrig, sizeof(kDrawEndTallyOrig), "wide end screen"))
        return;
    t_DrawEndTally = HookDetour(kDrawEndTally, kDrawEndTallyOrig, sizeof(kDrawEndTallyOrig), Wrap_DrawEndTally,
                                "wide end screen");
    if (!t_DrawEndTally)
        return;
    WideMenuRegister({ Active, Decorate, nullptr });
    Log("endscreen: victory / defeat tally beside the picture");
}
