#pragma once
#include <windows.h>

// Pixel-precise mission camera. The screen's map area shows the world from (camX, camY); the
// game renders the canvas from a tile origin chosen around the camera (layout.h margin), so the
// camera can move between game frames. Arrow keys, screen-edge, wheel and middle-drag scrolling
// move it directly; scrolls the game requests itself (minimap, centring on a unit, group recall)
// are detected as writes to the game's scroll origin and glided to.
void CameraInstall(const char* iniPath);

// Before the game draws a mission frame: picks the tile origin and writes it to the game.
void CameraFrameStart();

// Before a composition: advances the camera by the time elapsed since the last update.
void CameraUpdate();

// Pixel offset of the screen's map area inside the canvas map view (0 .. 2 * margin).
void CameraOffset(int* x, int* y);

// Window messages in screen coordinates; returns true when the camera consumed the message.
bool CameraOnMessage(UINT msg, WPARAM wParam, LPARAM lParam);

// World pixel shown at the top-left of the screen's map area.
void CameraWorldPosition(int* x, int* y);

// Centres the screen's map area on a world pixel at once (minimap clicks).
void CameraJumpTo(int worldX, int worldY);

// Leaves the mission: the next frame re-initialises the camera from the game's origin.
void CameraReset();
// Redraws the game's map surface at the next frame start.
void CameraRedrawMap();
