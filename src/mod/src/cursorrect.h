#pragma once
#include <windows.h>

// The rectangle that follows the mouse on the map: the box-selection drag and the building /
// placement footprint. The game recomputes it once per game frame (InGameMouseTick) and draws it
// into the canvas (DrawGameFrame 0x41ABD3); in hi-res missions that draw is recorded instead and
// the rectangle is drawn on every composed screen from the live mouse position.
void CursorRectInstall();

// A game frame starts: the rectangle shows only if this frame's DrawGameFrame records one.
void CursorRectFrameStart();

// Draws the rectangle into the locked, composed screen (map area) whose map shows the canvas
// at (shownX, shownY).
void CursorRectDraw(BYTE* bits, int pitch, int shownX, int shownY);
