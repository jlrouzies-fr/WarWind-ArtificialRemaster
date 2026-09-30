#pragma once

// Developer channel: polls <gameDir>\WarWindHD.cmd and executes one command per line,
// then deletes the file. Lets tools drive a game running on a hidden desktop without
// touching the user's desktop, cursor or focus.
//   capture <file.bmp>         dump the primary surface
//   capcanvas <file.bmp>       dump the in-mission canvas (the game's drawing surface)
//   key <vk hex>               WM_KEYDOWN + WM_KEYUP to the game window
//   keydown <vk hex> / keyup <vk hex>   post a key down / up
//   mod <vk hex> <0|1>         hold or release a modifier as seen by GetKeyState
//   click <x> <y> [r]          left (or right) click at game coordinates
//   move <x> <y>               WM_MOUSEMOVE
//   dblclick <x> <y>           double-click sequence
//   clickid / rclickid <hex id>  left / right click on a thing's sprite
//   wheel <delta>              WM_MOUSEWHEEL (120 per notch)
//   mdrag / ldrag <x0> <y0> <x1> <y1>  middle / left button drag
//   peek <hexaddr> <count>     log bytes of game memory
//   poke <hexaddr> <hex bytes...>  write bytes to game memory
//   sleep <ms>
void DevCmdStart(const char* gameDir);
