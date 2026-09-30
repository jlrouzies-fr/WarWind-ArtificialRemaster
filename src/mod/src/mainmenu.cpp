#include "mainmenu.h"

#include <windows.h>
#include <ddraw.h>
#include <string.h>

#include <vector>

#include "canvas.h"
#include "game.h"
#include "gametext.h"
#include "hook.h"
#include "hudart.h"
#include "log.h"
#include "menuart.h"
#include "offscreen.h"
#include "widemenu.h"

namespace
{
constexpr int kW = 960, kH = 540;
constexpr int kArtW = 640, kArtH = 480;
// The menu palette's text fonts and headings font (RES.001 #247, #248 highlighted, #250).
constexpr DWORD kMenuFont = 0x800100F7, kMenuFontHot = 0x800100F8, kHeadingFont = 0x800100FA;

// Column layout on the screen, inside the carved panel at (672, 14).
constexpr int kColumnX = 690, kTop = 36, kRowW = 252, kRowH = 43, kHeadingH = 22, kGroupGap = 4;
constexpr int kLabelDX = 48, kLabelDY = 15, kMedallion = 36, kMedallionInset = 3;

struct Row
{
    int hotspot;             // index in the game's hotspot table
    int icon;                // RES.004 #14 frame, or -1: the race's medallion
    const char* label;
};
struct Group
{
    const char* heading;
    Row rows[4];
    int count;
};
const Group kGroups[] = {
    { "NEW CAMPAIGN", { { 1, -1, "THA' ROON" }, { 2, -1, "OBBLINOX" }, { 3, -1, "EAGGRA" }, { 4, -1, "SHAMA' LI" } }, 4 },
    { "GAMES", { { 8, 14, "LOAD CAMPAIGN" }, { 0, 16, "CUSTOM & MULTIPLAYER" } }, 2 },
    { "EXTRAS", { { 5, 26, "OPENING CINEMATIC" }, { 6, 31, "CREDITS" }, { 7, 33, "QUIT GAME" } }, 3 },
};
constexpr int kRaceHotspotFirst = 1;

struct Placed
{
    const Row* row;
    int y;
};

// Walks the column: calls heading(group, y) and row(placed) top to bottom.
template <typename HeadingFn, typename RowFn>
void Walk(HeadingFn heading, RowFn row)
{
    int y = kTop;
    for (const Group& g : kGroups)
    {
        if (&g != kGroups)
            y += kGroupGap;
        heading(g, y);
        y += kHeadingH;
        for (int i = 0; i < g.count; ++i, y += kRowH)
            row(Placed{ &g.rows[i], y });
    }
}

const Row* RowAt(int x, int y)
{
    const Row* found = nullptr;
    Walk([](const Group&, int) {},
         [&](const Placed& p) {
             if (x >= kColumnX && x < kColumnX + kRowW && y >= p.y && y < p.y + kRowH)
                 found = p.row;
         });
    return found;
}

// ---- race medallions: each race's glyph, shrunk from the tablet

BYTE g_medallions[4][kMedallion * kMedallion];
bool g_medallionsBuilt;

bool BuildMedallions()
{
    std::vector<BYTE> art;
    PALETTEENTRY pal[256];
    if (!PaletteRead(pal) || !RenderArt(game::kMainMenuArt, 0, kArtW, kArtH, art))
        return false;
    for (int race = 0; race < 4; ++race)
    {
        const game::MainMenuHotspot& h = game::mainMenuHotspots[kRaceHotspotFirst + race];
        int side = max(h.right - h.left, h.bottom - h.top);
        int x0 = min(max((h.left + h.right - side) / 2, 0), kArtW - side);
        int y0 = min(max((h.top + h.bottom - side) / 2, 0), kArtH - side);
        for (int j = 0; j < kMedallion; ++j)
            for (int i = 0; i < kMedallion; ++i)
            {
                int sx0 = x0 + i * side / kMedallion, sx1 = x0 + (i + 1) * side / kMedallion;
                int sy0 = y0 + j * side / kMedallion, sy1 = y0 + (j + 1) * side / kMedallion;
                int r = 0, g = 0, b = 0, n = 0;
                for (int y = sy0; y < sy1; ++y)
                    for (int x = sx0; x < sx1; ++x, ++n)
                    {
                        const PALETTEENTRY& c = pal[art[(size_t)y * kArtW + x]];
                        r += c.peRed;
                        g += c.peGreen;
                        b += c.peBlue;
                    }
                g_medallions[race][j * kMedallion + i] = n ? PaletteNearest(pal, r / n, g / n, b / n) : 0;
            }
    }
    return true;
}

// ---- drawing over the composed screen

void PaintPlates(IDirectDrawSurface* screen)
{
    PALETTEENTRY pal[256];
    Pixels p;
    if (!PaletteRead(pal) || !LockSurface(screen, &p))
        return;
    PlateTones t = StoneTones(pal);
    Walk(
        [&](const Group&, int y) {
            HudArtFill(p, kColumnX, y + 14, kRowW, 1, t.dark);
            HudArtFill(p, kColumnX, y + 15, kRowW, 1, t.pale);
        },
        [&](const Placed& r) {
            if (r.row->icon >= 0 || !g_medallionsBuilt)
                return;
            int x = kColumnX + kMedallionInset, y = r.y + kMedallionInset;
            PlateSunken(p, x, y, kMedallion, kMedallion, t, t.well);
            const BYTE* m = g_medallions[r.row->hotspot - kRaceHotspotFirst];
            for (int j = 0; j < kMedallion; ++j)
                memcpy(p.bits + (y + j) * p.pitch + x, m + j * kMedallion, kMedallion);
        });
    screen->Unlock(nullptr);
}

void __cdecl Decorate(IDirectDrawSurface* screen)
{
    if (!g_medallionsBuilt)
        g_medallionsBuilt = BuildMedallions();
    int mx, my;
    CanvasScreenMouse(&mx, &my);
    const Row* hot = RowAt(mx, my);

    PaintPlates(screen);
    DrawTarget target(screen, kW, kH);
    DWORD oldFont = WatcomCall(game::fnSelectFont, kHeadingFont);
    Walk([](const Group& g, int y) { PrintAt(kColumnX + 2, y, g.heading); },
         [](const Placed&) {});
    Walk([](const Group&, int) {},
         [&](const Placed& r) {
             bool isHot = r.row == hot;
             if (r.row->icon >= 0)
                 WatcomCall5(game::fnDrawImage, game::kMenuIcons, r.row->icon + (isHot ? 1 : 0), kColumnX, r.y, 0);
             WatcomCall(game::fnSelectFont, isHot ? kMenuFontHot : kMenuFont);
             PrintAt(kColumnX + kLabelDX, r.y + kLabelDY, r.row->label);
         });
    WatcomCall(game::fnSelectFont, oldFont);
}

bool __cdecl Active()
{
    return *game::currentScreenHandler == game::fnMainMenuHandler;
}

// A row stands for its glyph: the game gets the centre of the glyph's hotspot.
bool __cdecl Alias(int screenX, int screenY, int* gameX, int* gameY)
{
    const Row* row = RowAt(screenX, screenY);
    if (!row)
        return false;
    const game::MainMenuHotspot& h = game::mainMenuHotspots[row->hotspot];
    *gameX = (h.left + h.right) / 2;
    *gameY = (h.top + h.bottom) / 2;
    return true;
}
}  // namespace

void MainMenuInstall()
{
    WideMenuRegister({ Active, Decorate, Alias });
    Log("mainmenu: menu column beside the tablet");
}
