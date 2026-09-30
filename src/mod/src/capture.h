#pragma once

// Writes the game's primary surface (8-bit + current palette) to an 8-bit BMP.
bool CaptureFrame(const char* path);

// Same for any 8-bit surface (with the primary's palette), e.g. the in-mission canvas.
bool CaptureSurface(void* surface, const char* path);
