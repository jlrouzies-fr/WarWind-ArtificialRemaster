#pragma once
#include <windows.h>

#include <vector>

// Art for the widescreen menus, built at runtime from the game's own pictures in the current
// palette (nothing derived from the game's art is stored on disk).

// Renders a resource frame, as DrawImage places it, into a w x h index picture.
bool RenderArt(DWORD res, int frame, int w, int h, std::vector<BYTE>& out);

// Darkening levels of the current palette, mixed with a 4x4 ordered dither like the game's own
// checkerboard fog.
class Shades
{
public:
    bool Build();                                        // false without a palette
    BYTE Apply(BYTE index, float amount, int x, int y) const;   // amount 0 = unchanged, 1 = darkest

private:
    static constexpr int kLevels = 7;
    BYTE lut_[kLevels][256] = {};
};

// The briefing text page's stone fill (RES.001 #197 + race, frames 9 10 17 18 in the game's own
// pattern) over [x0, x1) x [y0, y1), clipped to it, into the game's draw target.
void DrawStoneFill(int race, int x0, int y0, int x1, int y1);

// Fills a w x h picture with `art` (aw x ah) placed at (ox, oy) continued outwards by mirroring
// and darkened with the distance from it: base at its edge, darkest `falloff` pixels further.
void ExtendArt(const BYTE* art, int aw, int ah, int ox, int oy, BYTE* out, int w, int h, float falloff,
               float base, const Shades& shades);
