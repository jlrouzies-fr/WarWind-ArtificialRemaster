#pragma once

// DrawMenu (0x46D014) draws the current menu dialog. Features that draw with it register here, so
// the function has one detour whatever the number of features. Callbacks run in registration
// order, before and after the game's DrawMenu.
using MenuDrawFn = void(__cdecl*)();

// False when DrawMenu could not be hooked (another WW.EXE build).
bool MenuDrawHook(MenuDrawFn before, MenuDrawFn after);
