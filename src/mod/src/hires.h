#pragma once

// Hi-res missions: the mission screen runs at 1280x720 (menus stay 640x480) and draws into the
// larger canvas of layout.h; the map view grows (byte patches in feature-hires_720.wwp), and the
// right column / bottom bar panels are moved by hooking the draw primitives while the panel
// routines run. Installs the canvas (canvas.h) and the camera (camera.h).
void HiresInstall(const char* iniPath, const char* patchDir);

// True while the in-game menu is shown centred over a hi-res mission; its drawing is shifted
// by (dx, dy), so mouse input must be shifted back.
bool HiresMenuOffset(int* dx, int* dy);

// While suspended (nesting), the draw primitives are not shifted: the in-game save / load screen
// draws through them into its own surface.
void HiresSuspendShift(bool suspend);

// Current in-game map view size in small (24 px) tiles: 22x20 normally, the canvas view in hi-res.
void HiresViewTiles(int* tilesX, int* tilesY);
