#include "hudart.h"

#include <windows.h>
#include <ddraw.h>

#include <limits.h>
#include <string.h>

#include "game.h"
#include "hook.h"
#include "log.h"

namespace
{
constexpr int kArtW = 122, kArtH = 453;            // column art, frame 0 of kFrameArtBase + race
constexpr int kArtOffsetX = 518;                   // the frame's stored placement
constexpr int kCarvedX = 76, kCarvedY = 54, kCarvedW = 44, kCarvedH = 42;   // plain carved stone
constexpr int kRimW = 8;                           // raised left rim of the column

// Panel colour ramps per race (Tha'Roon, Obblinox, Eaggra, Shama'Li): dark, mid, light, pale.
const BYTE kRamps[4][4] = {
    { 66, 67, 69, 70 },
    { 104, 105, 107, 109 },
    { 181, 121, 123, 124 },
    { 154, 157, 159, 36 },
};
constexpr BYTE kWell = 98, kBlue = 93;

BYTE g_art[kArtH][kArtW];
int g_race = -1;

// Draws the column art into a scratch surface with the game's own DrawImage.
bool CaptureColumnArt(int race)
{
    Offscreen scratch;
    if (!scratch.Create(kArtW, kArtH))
        return false;
    {
        DrawTarget target(scratch.Surface(), kArtW, kArtH);
        WatcomCall5(game::fnDrawImage, game::kFrameArtBase + race, 0, (DWORD)-kArtOffsetX, 0, 0);
    }
    Pixels p;
    if (!scratch.Lock(&p, true))
        return false;
    for (int y = 0; y < kArtH; ++y)
        memcpy(g_art[y], p.bits + y * p.pitch, kArtW);
    scratch.Unlock();
    return true;
}

BYTE Carved(int x, int y)
{
    int tx = x / kCarvedW, ty = y / kCarvedH, lx = x % kCarvedW, ly = y % kCarvedH;
    if (tx & 1)
        lx = kCarvedW - 1 - lx;
    if (ty & 1)
        ly = kCarvedH - 1 - ly;
    return g_art[kCarvedY + ly][kCarvedX + lx];
}

// The column's left rim turned on its side: its outer edge along the top.
BYTE Rim(int x, int y)
{
    return g_art[x % kArtH][y];
}
}  // namespace

bool HudArtPrepare()
{
    int race = *game::playerRace & 3;
    if (race == g_race)
        return true;
    if (!CaptureColumnArt(race))
        return false;
    g_race = race;
    Log("hud: panel art captured for race %d", race);
    return true;
}

BYTE HudColor(HudTone tone)
{
    int race = g_race < 0 ? 0 : g_race;
    switch (tone)
    {
    case HudTone::Well: return kWell;
    case HudTone::Dark: return kRamps[race][0];
    case HudTone::Mid: return kRamps[race][1];
    case HudTone::Light: return kRamps[race][2];
    case HudTone::Pale: return kRamps[race][3];
    case HudTone::Blue: return kBlue;
    }
    return kWell;
}

void HudArtBackground(const Pixels& p, int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; ++y)
    {
        BYTE* row = p.bits + (y0 + y) * p.pitch + x0;
        for (int x = 0; x < w; ++x)
            row[x] = y < kRimW ? Rim(x, y) : Carved(x, y - kRimW);
    }
}

void HudArtFill(const Pixels& p, int x, int y, int w, int h, BYTE color)
{
    for (int j = 0; j < h; ++j)
        memset(p.bits + (y + j) * p.pitch + x, color, w);
}

void HudArtWell(const Pixels& p, int x, int y, int w, int h, HudTone fill)
{
    PlateSunken(p, x, y, w, h, HudPlateTones(), HudColor(fill));
}

PlateTones HudPlateTones()
{
    return { HudColor(HudTone::Well), HudColor(HudTone::Dark), HudColor(HudTone::Mid), HudColor(HudTone::Light),
             HudColor(HudTone::Pale) };
}

PlateTones StoneTones(const PALETTEENTRY pal[256])
{
    return { PaletteNearest(pal, 8, 6, 4), PaletteNearest(pal, 34, 28, 20), PaletteNearest(pal, 60, 50, 38),
             PaletteNearest(pal, 104, 88, 66), PaletteNearest(pal, 152, 132, 100) };
}

void PlateBevel(const Pixels& p, int x, int y, int w, int h, const PlateTones& t)
{
    HudArtFill(p, x - 2, y - 2, w + 4, 1, t.dark);
    HudArtFill(p, x - 2, y - 1, w + 4, 1, t.mid);
    HudArtFill(p, x - 2, y + h, w + 4, 1, t.light);
    HudArtFill(p, x - 2, y + h + 1, w + 4, 1, t.pale);
    HudArtFill(p, x - 2, y - 2, 1, h + 4, t.dark);
    HudArtFill(p, x - 1, y - 1, 1, h + 2, t.mid);
    HudArtFill(p, x + w, y - 1, 1, h + 2, t.light);
    HudArtFill(p, x + w + 1, y - 2, 1, h + 4, t.pale);
}

void PlateSunken(const Pixels& p, int x, int y, int w, int h, const PlateTones& t, BYTE fill)
{
    PlateBevel(p, x, y, w, h, t);
    HudArtFill(p, x, y, w, h, fill);
}

void PlateRaised(const Pixels& p, int x, int y, int w, int h, const PlateTones& t, BYTE fill, bool pressed)
{
    BYTE hi1 = pressed ? t.well : t.pale, hi2 = pressed ? t.dark : t.light;
    BYTE lo1 = pressed ? t.pale : t.well, lo2 = pressed ? t.light : t.dark;
    HudArtFill(p, x, y, w, h, fill);
    HudArtFill(p, x, y, w, 1, hi1);
    HudArtFill(p, x, y + 1, w - 1, 1, hi2);
    HudArtFill(p, x, y, 1, h, hi1);
    HudArtFill(p, x + 1, y + 1, 1, h - 2, hi2);
    HudArtFill(p, x, y + h - 1, w, 1, lo1);
    HudArtFill(p, x + 1, y + h - 2, w - 2, 1, lo2);
    HudArtFill(p, x + w - 1, y, 1, h, lo1);
    HudArtFill(p, x + w - 2, y + 1, 1, h - 2, lo2);
}

bool PaletteRead(PALETTEENTRY pal[256])
{
    auto primary = (IDirectDrawSurface*)*game::lpPrimary;
    IDirectDrawPalette* palette = nullptr;
    if (!primary || FAILED(primary->GetPalette(&palette)))
        return false;
    bool ok = SUCCEEDED(palette->GetEntries(0, 0, 256, pal));
    palette->Release();
    return ok;
}

BYTE PaletteNearest(const PALETTEENTRY pal[256], int r, int g, int b)
{
    int best = 0, bestD = INT_MAX;
    for (int i = 0; i < 256; ++i)
    {
        int dr = pal[i].peRed - r, dg = pal[i].peGreen - g, db = pal[i].peBlue - b;
        int d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return (BYTE)best;
}
