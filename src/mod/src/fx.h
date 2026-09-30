#pragma once
// Modern Graphics: builds the per-pixel side planes (what each pixel shows, projected shadow) and
// the light list for every presented mission frame and hands them to cnc-ddraw's D3D9
// post-process (WWFX_* exports). Nothing changes on screen while it is off or unavailable.
#include <windows.h>

void FxInstall(const char* iniPath, const char* gameDir);

// RedrawMapSurface is (re)drawing terrain and doodads into the map surface.
void FxMapRedrawBegin(bool full);
void FxMapRedrawEnd();
// Something that is not part of the scene (panel fill, pixel) was drawn at canvas coordinates.
void FxMarkUi(int x, int y, int w, int h);
// Compose copied the canvas map at (shownX, shownY) to the screen: build the screen planes.
void FxCompose(int shownX, int shownY);
// Something that is not part of the scene was drawn on the composed screen after FxCompose.
void FxMarkScreenUi(int x, int y, int w, int h);

// The toggle key; true when consumed.
bool FxOnKeyDown(WPARAM vk);
void FxToggle();
// The player's Modern Graphics setting (saved in WarWindHD.ini).
bool FxEnabled();
void FxSetEnabled(bool on);

// Developer channel: writes the next presented frame's planes for tools/fxpreview.
void FxRequestDump(const char* path);
// Developer channel: saves the next frame as rendered by the D3D9 post-process (PNG).
bool FxRequestScreenshot(const char* path);
