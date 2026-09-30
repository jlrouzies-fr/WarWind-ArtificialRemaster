#include "menuart.h"

#include <string.h>

#include "game.h"
#include "hook.h"
#include "hudart.h"
#include "offscreen.h"

bool RenderArt(DWORD res, int frame, int w, int h, std::vector<BYTE>& out)
{
    Offscreen scratch;
    if (!scratch.Create(w, h))
        return false;
    {
        DrawTarget target(scratch.Surface(), w, h);
        WatcomCall5(game::fnDrawImage, res, frame, 0, 0, 0);
    }
    Pixels p;
    if (!scratch.Lock(&p, true))
        return false;
    out.resize((size_t)w * h);
    for (int y = 0; y < h; ++y)
        memcpy(&out[(size_t)y * w], p.bits + y * p.pitch, w);
    scratch.Unlock();
    return true;
}

bool Shades::Build()
{
    static const float kScale[kLevels] = { 1.0f, 0.82f, 0.66f, 0.52f, 0.40f, 0.30f, 0.22f };
    PALETTEENTRY pal[256];
    if (!PaletteRead(pal))
        return false;
    for (int k = 0; k < kLevels; ++k)
    {
        for (int i = 0; i < 256; ++i)
            lut_[k][i] = PaletteNearest(pal, (int)(pal[i].peRed * kScale[k]), (int)(pal[i].peGreen * kScale[k]),
                                        (int)(pal[i].peBlue * kScale[k]));
        lut_[k][0] = 0;
    }
    return true;
}

BYTE Shades::Apply(BYTE index, float amount, int x, int y) const
{
    static const BYTE kBayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    if (amount <= 0)
        return index;
    float pos = (amount >= 1 ? 1.0f : amount) * (kLevels - 1);
    int level = (int)pos;
    if (pos - level > kBayer[y & 3][x & 3] / 16.0f)
        ++level;
    return lut_[level < kLevels ? level : kLevels - 1][index];
}

void DrawStoneFill(int race, int x0, int y0, int x1, int y1)
{
    constexpr DWORD kStoneTiles = 0x800100C5, fnRand = 0x49A7CC, fnSrand = 0x49A7F0, fnClock = 0x410988;
    static const BYTE kFrames[4] = { 9, 10, 17, 18 };
    int saved[4];
    memcpy(saved, game::clipRect, sizeof(saved));
    const int clip[4] = { x0, x1 - 1, y0, y1 - 1 };
    memcpy(game::clipRect, clip, sizeof(clip));
    WatcomCall(fnSrand, 0x8000);
    for (int y = y0; y < y1; y += 16)
        for (int x = x0; x < x1; x += 16)
            WatcomCall5(game::fnDrawImage, kStoneTiles + race, kFrames[WatcomCall(fnRand) % 4], x, y, 0);
    WatcomCall(fnSrand, WatcomCall(fnClock));
    memcpy(game::clipRect, saved, sizeof(saved));
}

namespace
{
// Mirror index: ... 2 1 0 | 0 1 2 ... n-1 | n-1 n-2 ...
int Reflect(int i, int n)
{
    int m = i % (2 * n);
    if (m < 0)
        m += 2 * n;
    return m < n ? m : 2 * n - 1 - m;
}
}  // namespace

void ExtendArt(const BYTE* art, int aw, int ah, int ox, int oy, BYTE* out, int w, int h, float falloff,
               float base, const Shades& shades)
{
    for (int y = 0; y < h; ++y)
    {
        int ay = Reflect(y - oy, ah);
        int dy = oy - y > y - (oy + ah - 1) ? oy - y : y - (oy + ah - 1);
        for (int x = 0; x < w; ++x)
        {
            int dx = ox - x > x - (ox + aw - 1) ? ox - x : x - (ox + aw - 1);
            int dist = dx > dy ? dx : dy;
            BYTE index = art[(size_t)ay * aw + Reflect(x - ox, aw)];
            float amount = dist > 0 ? base + dist / falloff : 0.0f;
            out[(size_t)y * w + x] = shades.Apply(index, amount, x, y);
        }
    }
}
