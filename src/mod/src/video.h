#pragma once

// Replaces the game's AVI player: when Data\VIDS_HD\<race>\<name>.mp4 exists it is
// played with Media Foundation in a borderless window covering the game window
// (the "_sub" variant when in-game Cinematic Subtitles are on). Otherwise the
// original player runs unchanged.
void VideoInstall(const char* iniPath);
