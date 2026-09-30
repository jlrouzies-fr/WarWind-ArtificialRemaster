#pragma once
#include <windows.h>

#include "offscreen.h"

// HUD materials and an 8-bit software painter. The materials come from the player race's panel
// column art (carved stone, raised rim) and its colour ramp, so the HUD matches each race.
enum class HudTone
{
    Well,    // near-black inside of plates and slots
    Dark,    // bevel shadow
    Mid,
    Light,   // bevel light, plate edges
    Pale,
    Blue,    // health bars
};


// Captures the race's column art (once per race); false until the game's DirectDraw exists.
bool HudArtPrepare();

BYTE HudColor(HudTone tone);

// Carved stone, mirror-tiled, with the raised rim along the top edge.
void HudArtBackground(const Pixels& p, int x, int y, int w, int h);

// Sunken plate in the race's tones (PlateSunken).
void HudArtWell(const Pixels& p, int x, int y, int w, int h, HudTone fill = HudTone::Well);

void HudArtFill(const Pixels& p, int x, int y, int w, int h, BYTE color);

// A plate colour ramp, darkest to lightest: the race's HUD tones in missions (HudPlateTones), or
// the nearest colours of another screen's palette.
struct PlateTones
{
    BYTE well, dark, mid, light, pale;
};
PlateTones HudPlateTones();
// Dark stone tones from any palette (screens without the race's HUD colours).
PlateTones StoneTones(const PALETTEENTRY pal[256]);

// The 2 px sunken bevel outside a rect (shadow top/left, light bottom/right).
void PlateBevel(const Pixels& p, int x, int y, int w, int h, const PlateTones& t);
// Sunken plate: the bevel and the fill inside.
void PlateSunken(const Pixels& p, int x, int y, int w, int h, const PlateTones& t, BYTE fill);
// Raised plate (buttons, boxes): light top/left, shadow bottom/right; pressed turns it inside out.
void PlateRaised(const Pixels& p, int x, int y, int w, int h, const PlateTones& t, BYTE fill, bool pressed);

// The palette shown now (the primary surface's); false before one exists.
bool PaletteRead(PALETTEENTRY pal[256]);
// The index of the colour closest to (r, g, b), weighted for perceived brightness.
BYTE PaletteNearest(const PALETTEENTRY pal[256], int r, int g, int b);
