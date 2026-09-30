#pragma once
#include <windows.h>

// The player's selection, through the game's own select / deselect routines.

// Adds a thing to the selection (exclusive: replaces it). Returns false when the game refuses.
bool SelectionAdd(WORD id, bool exclusive);

void SelectionRemove(WORD id);

void SelectionClear();

// Sets the "current" selection marker the panels read: the single thing, or 0x801 for several.
void SelectionUpdateCurrent();

// Walks the selection in the game's order; 0 ends it.
WORD SelectionFirst();
WORD SelectionNext(WORD id);
