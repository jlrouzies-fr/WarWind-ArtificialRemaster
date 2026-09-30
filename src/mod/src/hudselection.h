#pragma once
#include <windows.h>

#include "hudart.h"

// HUD selection row and control-group plates. For several selected units: a face per unit (a crop
// of its panel portrait) with its health bar; click one to keep only that unit, Shift-click to drop
// it. Group plates 1..0 show each control group's size; a click recalls the group.
//
// hudTop is the canvas y of the HUD strip; x/y below are screen coordinates.

// Wells and health bars, painted into the locked canvas.
void HudSelectionPaint(const Pixels& p, int hudTop);

// Portraits and digits, drawn with the game's primitives (canvas unlocked).
void HudSelectionDraw(int hudTop);

// A screen position of the HUD on a face or plate. act: perform the click (left button released).
bool HudSelectionClick(int x, int y, bool shift, bool act);

// Several units are selected (the face row replaces the single unit's bio diagram).
bool HudSelectionShowsFaces();
