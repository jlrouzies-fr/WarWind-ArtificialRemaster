#include "widemenu.h"

#include <windowsx.h>
#include <ddraw.h>
#include <string.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "game.h"
#include "hook.h"
#include "log.h"
#include "menuart.h"
#include "menudraw.h"
#include "offscreen.h"

namespace
{
constexpr int kW = 960, kH = 540;                  // the menu video mode
constexpr int kCanvasW = 1024, kCanvasH = kH;      // game coordinates a screen can draw in
constexpr int kArtW = 640, kArtH = 480;            // the screens' own layout

constexpr DWORD kRaceMenuArt = 0x80040006;         // RES.004 #6 + race: the race options stone
constexpr DWORD kBriefingArt = 0x80040000;         // RES.004 #0 + race, frame 0: the briefing's frame
constexpr DWORD kStonePanel = 0x800100C9;          // RES.001 #201: the menus' carved border tiles
constexpr int kRaceOptions = 3;                    // g_menuScreen
constexpr int kPanelTile = 16;

enum class Kind { MainMenu, RaceOptions, EndScreen, Briefing, Generic };

struct Layout
{
    Kind kind;
    int ox, oy;              // screen position of the game's (0, 0)
    DWORD art;               // resource of the picture the surround continues (0 = what is drawn)
    float falloff, base;     // darkening of the continued picture
    bool frame;              // the carved stone frame around the picture (menu palette)
    bool column;             // a carved panel on the right for the moved buttons
};

// Main menu and race options: the picture on the left, a column panel on the right (plan phases 2
// and 3). Victory / defeat: the picture on the left, its tally in the right-hand column (endscreen).
// Anything else centred.
Layout CurrentLayout()
{
    DWORD handler = *game::currentScreenHandler;
    int race = *game::playerRace & 3;
    if (handler == game::fnMainMenuHandler)
        return { Kind::MainMenu, 16, 30, game::kMainMenuArt, 120.0f, 0.55f, true, true };
    if (handler == game::fnMenuScreenHandler && *game::menuScreen == kRaceOptions)
        return { Kind::RaceOptions, 16, 30, kRaceMenuArt + race, 110.0f, 0.55f, true, true };
    if (handler == game::fnBriefingHandler)
        return { Kind::Briefing, 0, 30, kBriefingArt + race, 0.0f, 0.0f, false, false };
    if (handler == game::fnInGameScreenHandler && *game::endScreen)
    {
        bool won = *game::resultWon || *game::resultWon2;
        return { Kind::EndScreen, -56, 30, (won ? game::kEndPictureWon : game::kEndPictureLost) + race, 60.0f, 0.8f,
                 false, false };
    }
    return { Kind::Generic, (kW - kArtW) / 2, 30, 0, 150.0f, 0.5f, true, false };
}

// Surrounds by screen: kind, art, and (generic screens) handler and menu screen.
using UnderlayKey = std::tuple<int, DWORD, DWORD, int>;

UnderlayKey KeyOf(const Layout& l)
{
    bool generic = l.kind == Kind::Generic;
    return { (int)l.kind, l.art, generic ? *game::currentScreenHandler : 0, generic ? *game::menuScreen : 0 };
}

bool g_enabled;
std::vector<WideMenuScreen> g_screens;
Offscreen g_canvas;
IDirectDrawSurface* g_realBack;
std::map<UnderlayKey, std::unique_ptr<Offscreen>> g_underlays;

void CopyArtFromCanvas(std::vector<BYTE>& art)
{
    Pixels c;
    art.assign((size_t)kArtW * kArtH, 0);
    if (!g_canvas.Lock(&c, true))
        return;
    for (int y = 0; y < kArtH; ++y)
        memcpy(&art[(size_t)y * kArtW], c.bits + y * c.pitch, kArtW);
    g_canvas.Unlock();
}

// n rows continuing a border strip of h rows upwards from its first row, mirrored back and forth:
// the strip row shown at distance d (1 = next to the strip).
int BandRow(int d, int h)
{
    int k = (d - 1) % (2 * h);
    return k < h ? k : 2 * h - 1 - k;
}

// Briefing (ui_menus/mockups.py briefing): the frame's top and bottom borders continue into the
// 30 px bands; right of it, a 320 px panel of the same frame (the lower well's texture, the
// frame's border rows and its right border) holding the text well.
constexpr int kWellX = 646, kWellY = 50, kWellW = 288, kWellH = 440;   // screen

BYTE BriefingPanel(const std::vector<BYTE>& art, int px, int py)
{
    auto at = [&](int x, int y) { return art[(size_t)y * kArtW + x]; };
    if (px >= 300)
        return at(620 + px - 300, py);
    if (py < 20 || py >= kArtH - 20)
        return at(260 + px, py);
    constexpr int sx = 26, sy = 338, sw = 460, sh = 114;   // the lower well's texture, mirror-tiled
    int mx = px % sw, my = py % sh;
    if ((px / sw) & 1)
        mx = sw - 1 - mx;
    if ((py / sh) & 1)
        my = sh - 1 - my;
    return at(sx + mx, sy + my);
}

void BuildBriefingSurround(const std::vector<BYTE>& art, const Layout& l, BYTE* out)
{
    for (int y = 0; y < kH; ++y)
    {
        int ay = y - l.oy;
        // rows of the 640x480 layout for this screen row (bands mirror the border strips)
        int row = ay < 0 ? BandRow(-ay, 20) : ay >= kArtH ? kArtH - 1 - BandRow(ay - kArtH + 1, 20) : ay;
        for (int x = 0; x < kW; ++x)
            out[(size_t)y * kW + x] = x < kArtW ? art[(size_t)row * kArtW + x] : BriefingPanel(art, x - kArtW, row);
    }
}

void DrawWellBorder(const Pixels& p)
{
    // The map box's two border lines (colours 51 and 96) around the text well.
    const BYTE colours[2] = { 51, 96 };
    for (int inset = 0; inset < 2; ++inset)
    {
        int x0 = kWellX - 2 + inset, y0 = kWellY - 2 + inset;
        int w = kWellW + 4 - 2 * inset, h = kWellH + 4 - 2 * inset;
        memset(p.bits + y0 * p.pitch + x0, colours[inset], w);
        memset(p.bits + (y0 + h - 1) * p.pitch + x0, colours[inset], w);
        for (int y = y0; y < y0 + h; ++y)
            p.bits[y * p.pitch + x0] = p.bits[y * p.pitch + x0 + w - 1] = colours[inset];
    }
}

std::unique_ptr<Offscreen> BuildUnderlay(const Layout& l)
{
    Shades shades;
    std::vector<BYTE> art;
    if (!shades.Build())
        return nullptr;
    if (l.art)
    {
        if (!RenderArt(l.art, 0, kArtW, kArtH, art))
            return nullptr;
    }
    else
        CopyArtFromCanvas(art);

    auto underlay = std::make_unique<Offscreen>();
    Pixels p;
    if (!underlay->Create(kW, kH) || !underlay->Lock(&p))
        return nullptr;
    std::vector<BYTE> picture((size_t)kW * kH);
    if (l.kind == Kind::Briefing)
        BuildBriefingSurround(art, l, picture.data());
    else
        ExtendArt(art.data(), kArtW, kArtH, l.ox, l.oy, picture.data(), kW, kH, l.falloff, l.base, shades);
    for (int y = 0; y < kH; ++y)
        memcpy(p.bits + y * p.pitch, &picture[(size_t)y * kW], kW);
    if (l.kind == Kind::Briefing)
        DrawWellBorder(p);
    underlay->Unlock();

    // The carved frame around the picture (its inside is covered by the picture) and the column.
    DrawTarget target(underlay->Surface(), kW, kH);
    constexpr int kFrameRows = kArtH / kPanelTile, kFrameCols = kArtW / kPanelTile;
    if (l.frame)
        WatcomCall5(game::fnDrawPanel9, kStonePanel, l.ox - kPanelTile, l.oy - kPanelTile, kFrameRows, kFrameCols);
    if (l.column)
        WatcomCall5(game::fnDrawPanel9, kStonePanel, kArtW + 2 * kPanelTile, l.oy - kPanelTile, kFrameRows,
                    (kW - kArtW - 2 * kPanelTile) / kPanelTile - 2);
    if (l.kind == Kind::Briefing)
        DrawStoneFill(*game::playerRace & 3, kWellX, kWellY, kWellX + kWellW, kWellY + kWellH);
    return underlay;
}

Offscreen* Underlay(const Layout& l)
{
    UnderlayKey key = KeyOf(l);
    auto it = g_underlays.find(key);
    if (it != g_underlays.end())
        return it->second.get();
    std::unique_ptr<Offscreen> built = BuildUnderlay(l);
    if (!built)
        return nullptr;
    Log("widemenu: surround built (kind %d, art %08X, handler %08X, menu screen %d)", (int)l.kind, l.art,
        std::get<2>(key), std::get<3>(key));
    return (g_underlays[key] = std::move(built)).get();
}

const WideMenuScreen* RegisteredScreen()
{
    for (const WideMenuScreen& s : g_screens)
        if (s.active())
            return &s;
    return nullptr;
}

void Compose()
{
    Layout l = CurrentLayout();
    Offscreen* underlay = Underlay(l);
    Pixels src, dst, und = {};
    if (!g_canvas.Lock(&src, true))
        return;
    if (LockSurface(g_realBack, &dst))
    {
        bool haveUnderlay = underlay && underlay->Lock(&und, true);
        int x0 = l.ox > 0 ? l.ox : 0, x1 = l.ox + kCanvasW < kW ? l.ox + kCanvasW : kW;
        int artX0 = l.ox > 0 ? l.ox : 0, artX1 = l.ox + kArtW < kW ? l.ox + kArtW : kW;
        for (int y = 0; y < kH; ++y)
        {
            BYTE* d = dst.bits + y * dst.pitch;
            if (haveUnderlay)
                memcpy(d, und.bits + y * und.pitch, kW);
            else
                memset(d, 0, kW);
            int gy = y - l.oy;
            if (gy < 0 || gy >= kCanvasH)
                continue;
            const BYTE* s = src.bits + gy * src.pitch - l.ox;   // indexed by screen x
            bool artRow = gy < kArtH;
            for (int x = x0; x < x1; ++x)
            {
                if (artRow && x == artX0)
                {
                    memcpy(d + artX0, s + artX0, artX1 - artX0);
                    x = artX1 - 1;
                }
                else if (s[x])
                    d[x] = s[x];
            }
        }
        if (haveUnderlay)
            underlay->Unlock();
        g_realBack->Unlock(nullptr);
    }
    g_canvas.Unlock();
    const WideMenuScreen* screen = RegisteredScreen();
    if (screen && screen->decorate)
        screen->decorate(g_realBack);
}

// A menu screen is redrawn: what the previous state drew beside the art goes.
void __cdecl BeforeDrawMenu()
{
    if (WideMenuShown())
        WideMenuClearOutside();
}
}  // namespace

bool WideMenuInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("UI", "WideMenus", 1, iniPath) || !MenuDrawHook(BeforeDrawMenu, nullptr))
        return false;
    g_enabled = true;
    Log("widemenu: full-screen menus in %dx%d", kW, kH);
    return true;
}

void WideMenuModeFor(DWORD handler, int* w, int* h)
{
    bool wide = g_enabled && (handler == game::fnMainMenuHandler || handler == game::fnBriefingHandler);
    *w = wide ? kW : kArtW;
    *h = wide ? kH : kArtH;
}

void WideMenuModeForEndScreen(int* w, int* h)
{
    *w = g_enabled ? kW : kArtW;
    *h = g_enabled ? kH : kArtH;
}

void WideMenuModeChanging()
{
    if (!g_canvas.Valid())
        return;
    *game::lpBackBuffer = g_realBack;
    g_canvas.Release();
    g_realBack = nullptr;
    g_underlays.clear();
    Log("widemenu: canvas detached");
}

void WideMenuModeSet()
{
    if (!g_enabled || *game::screenWidth != kW || *game::screenHeight != kH)
        return;
    Pixels p;
    if (!g_canvas.Create(kCanvasW, kCanvasH) || !g_canvas.Lock(&p))
    {
        Log("widemenu: canvas %dx%d not created", kCanvasW, kCanvasH);
        g_canvas.Release();
        return;
    }
    for (int y = 0; y < kCanvasH; ++y)
        memset(p.bits + y * p.pitch, 0, kCanvasW);
    g_canvas.Unlock();
    g_realBack = (IDirectDrawSurface*)*game::lpBackBuffer;
    *game::lpBackBuffer = g_canvas.Surface();
    const int clip[4] = { 0, kCanvasW - 1, 0, kCanvasH - 1 };
    memcpy(game::clipRect, clip, sizeof(clip));
    memcpy(game::clipBounds, clip, sizeof(clip));
    Log("widemenu: canvas %dx%d attached", kCanvasW, kCanvasH);
}

void WideMenuBeforeFlip()
{
    if (!g_canvas.Valid())
        return;
    Compose();
    *game::lpBackBuffer = g_realBack;
}

void WideMenuAfterFlip()
{
    if (g_canvas.Valid())
        *game::lpBackBuffer = g_canvas.Surface();
}

void WideMenuScreenChanging()
{
    if (WideMenuShown())
        WideMenuClearOutside();
}

bool WideMenuShown()
{
    return g_canvas.Valid();
}

void WideMenuOrigin(int* x, int* y)
{
    Layout l = CurrentLayout();
    *x = l.ox;
    *y = l.oy;
}

void WideMenuRegister(const WideMenuScreen& screen)
{
    g_screens.push_back(screen);
}

bool WideMenuMouse(LPARAM* lParam)
{
    if (!WideMenuShown())
        return false;
    int sx = GET_X_LPARAM(*lParam), sy = GET_Y_LPARAM(*lParam), gx, gy;
    const WideMenuScreen* screen = RegisteredScreen();
    if (!screen || !screen->alias || !screen->alias(sx, sy, &gx, &gy))
    {
        int ox, oy;
        WideMenuOrigin(&ox, &oy);
        gx = sx - ox;
        gy = sy - oy;
    }
    *lParam = MAKELPARAM(gx, gy);
    return true;
}

void WideMenuClearOutside()
{
    Pixels p;
    if (!g_canvas.Lock(&p))
        return;
    for (int y = 0; y < kCanvasH; ++y)
    {
        BYTE* row = p.bits + y * p.pitch;
        if (y < kArtH)
            memset(row + kArtW, 0, kCanvasW - kArtW);
        else
            memset(row, 0, kCanvasW);
    }
    g_canvas.Unlock();
}
