#pragma once
#include <windows.h>

// The in-game (ESC) menu over a hi-res mission as a wide panel (ui_menus/mockup_esc_menu.png): the
// game's buttons in two columns (game / options), the scenario goal between them, the leader and
// clan as the title, and the race's OK / back orb bar for Return to Game / Return to Main Menu.
// The game keeps its own button list and logic: a mouse position on a new button is passed on as
// the centre of that button's original rectangle. [UI] WideMenus=0 keeps the original panel.
void EscMenuInstall(const char* iniPath);

// The in-game menu redrew the mission behind it (DrawGameFrame): dims the map view.
void EscMenuSceneDrawn();

// While the wide menu is shown: the menu position the game gets for a screen mouse position
// (the original rectangle of the button under it, or a point on no button).
bool EscMenuMouse(int screenX, int screenY, LPARAM* menuPos);
