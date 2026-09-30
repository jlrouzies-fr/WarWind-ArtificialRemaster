#pragma once
#include <windows.h>

// Offscreen canvas for hi-res missions (layout.h). While a 1280x720 mode is set, the game's
// back-buffer pointer designates the canvas; every flip composes the screen from it (map at
// the camera offset, panels at their screen positions) and flips the real buffers. Between game
// frames the mission idle loop presents extra composed frames so the camera moves at display rate.
void CanvasInstall(const char* iniPath);

bool CanvasActive();

// The canvas surface itself (developer captures), or nullptr outside hi-res missions.
void* CanvasSurface();

// Map offset (pixels) of the screen inside the canvas map view, as last composed.
void CanvasShownOffset(int* x, int* y);

// Mouse position in screen coordinates for every mouse message (the game's own globals hold
// canvas coordinates in missions); the software cursor is drawn from it.
void CanvasSetScreenMouse(int x, int y);
void CanvasScreenMouse(int* x, int* y);

// Screen -> canvas coordinates for the in-mission mouse (map area, or the column / status bar
// pieces the HUD shows).
LPARAM CanvasTranslateMouse(LPARAM screen);
