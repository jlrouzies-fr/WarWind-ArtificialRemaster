#pragma once
#include <windows.h>

// Text through the game's own font routines: the font chosen with game::fnSelectFont, drawn into
// the back buffer (coordinates in the surface the game draws to).

// A RES.000 text (resource id 0x80000000 | index).
const char* GameText(DWORD resourceId);

// Copies text without the "<X>" hotkey markers the game's labels carry.
void CopyPlain(const char* text, char* out, size_t size);

int TextWidth(const char* text, int length);

void PrintAt(int x, int y, const char* text);

// Prints at most maxW pixels of text.
void PrintClipped(int x, int y, const char* text, int maxW);

// Centred in [x, x + w), clipped to w.
void PrintCentred(int x, int y, int w, const char* text);

// Word-wraps text into lines of at most maxW pixels, lineH apart from (x, y), at most maxLines.
// A space whose `keep` flag is set is not a break point. Returns the number of lines printed.
int PrintWrapped(int x, int y, int maxW, int lineH, int maxLines, const char* text, const bool* keep = nullptr);
