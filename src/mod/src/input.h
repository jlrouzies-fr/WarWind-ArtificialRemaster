#pragma once

// Window-procedure hook: routes camera input (camera.h), translates in-mission mouse messages
// to canvas coordinates (canvas.h), selects every visible unit of a double-clicked unit's type,
// and optionally gives modern RTS clicks (right button orders the selection, left button only
// selects, left click on ground deselects).
void InputInstall(const char* iniPath);

// A box selection is under way: the left button is held and went down on the map view. (The game
// records every press as the drag anchor, but only one on the map view starts a box.)
bool InputBoxSelecting();
