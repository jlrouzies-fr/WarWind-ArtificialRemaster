#pragma once
#include <windows.h>

// Widescreen menus (ui_menus/plan.md): the full-screen menus run in a 960x540 mode, which scales
// exactly to 1080p and 4K. The game keeps drawing its 640x480 screens in its own coordinates, into
// a menu canvas; every flip composes the screen from the canvas placed at the screen's origin and
// a surround built at runtime from the screen's own art (mirrored stone, carved frames and panels).
// What a screen draws outside its 640x480 area (buttons moved beside the art) is composed over the
// surround. Screens without a wide layout keep the 640x480 mode. [UI] WideMenus=0 turns it off.

// False when turned off: the screens' own wide layouts (racemenu, mainmenu) depend on it.
bool WideMenuInstall(const char* iniPath);

// The video mode for a screen handler outside missions, and for a mission's victory / defeat picture.
void WideMenuModeFor(DWORD handler, int* w, int* h);
void WideMenuModeForEndScreen(int* w, int* h);

// canvas.cpp: the video mode is about to change / has changed; a flip is about to happen / done.
void WideMenuModeChanging();
void WideMenuModeSet();
void WideMenuBeforeFlip();
void WideMenuAfterFlip();

// A screen's own additions: drawn over every composed frame (screen coordinates), and the mouse
// positions it takes over (a screen position -> the game's coordinates).
struct WideMenuScreen
{
    bool(__cdecl* active)();
    void(__cdecl* decorate)(struct IDirectDrawSurface* screen);                // optional
    bool(__cdecl* alias)(int screenX, int screenY, int* gameX, int* gameY);   // optional
};
void WideMenuRegister(const WideMenuScreen& screen);

// A new screen handler takes over (its screen is drawn next).
void WideMenuScreenChanging();

// While a wide menu is shown: a screen mouse position -> the game's coordinates. False otherwise.
bool WideMenuMouse(LPARAM* lParam);

// While a wide menu is shown: the game's draw target and the screen origin of its coordinates.
bool WideMenuShown();
void WideMenuOrigin(int* x, int* y);

// Clears what the game drew outside the 640x480 area (before a screen redraws it).
void WideMenuClearOutside();
