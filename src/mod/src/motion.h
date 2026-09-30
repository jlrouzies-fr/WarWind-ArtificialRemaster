#pragma once
#include <windows.h>

// Smooth motion: the game simulates and draws one frame per tick (about 11 per second), so units
// step across the ground. Every composed screen between ticks re-renders the tick's sprite list
// with each thing's sprites placed between where it was drawn on the previous tick and where it is
// drawn now, following the game clock towards the tick's flip deadline. The simulation, animation
// frames and timing are untouched; only the positions shown between ticks change.
//
// Per tick (DrawGameFrame): nodes are tagged with the thing that queued them (QueueThingSprites),
// the list render records each thing's sprite positions, the canvas after the fog pass is kept,
// and what the game draws over it afterwards (messages, order lines...) is kept as an overlay.
// Each replay redraws terrain, sprites, move marker and fog through the game's own routines (so
// the Modern Graphics annotation follows), rebuilds the click rectangles at the shown positions,
// then puts the overlay back.
void MotionInstall(const char* iniPath);

// DrawGameFrame starts / has finished (hires.cpp wraps it).
void MotionFrameStart();
void MotionFrameDone();

// Called while composing a screen presented by the mission loop, before the canvas is read.
void MotionReplay();
