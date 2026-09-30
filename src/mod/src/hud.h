#pragma once
#include <windows.h>

// Bottom HUD of hi-res missions. The screen's HUD strip shows the canvas HUD strip (background
// and command grid, drawn here) with pieces of the game's own right-hand column and status bar
// copied over it (HudBlocks); mouse positions over those pieces map back to the column, so the
// game's panel code works unchanged. The command grid shows the commands of the selection in
// the order of the grid hotkeys; a click presses the button exactly like its hotkey does.
void HudInstall(const char* iniPath);

bool HudGridEnabled();

// A rectangle of the canvas shown at a screen position.
struct HudBlock
{
    int screenX, screenY, w, h;
    int canvasX, canvasY;
};
const HudBlock* HudBlocks(int* count);

// After the game drew a mission frame into the canvas: draws the HUD strip.
void HudDraw();

// Mouse message in screen coordinates; returns true when the HUD consumed it.
bool HudOnMouse(UINT msg, int x, int y);
