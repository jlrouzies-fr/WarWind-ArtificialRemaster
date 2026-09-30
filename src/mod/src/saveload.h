#pragma once

// In-game Save / Load screen. Replaces the game's four Windows file dialogs (save / restore, campaign
// .SAV and custom / multiplayer .NSV): a modal screen drawn in the game's own style lists the saves
// in SAVES\, takes a typed name to save, and writes the chosen path into the buffer the game then
// opens itself. Works over a hi-res mission (drawn into the canvas) and over the 640x480 menus.
// [UI] InGameSaveLoad=0 keeps the Windows dialogs.
void SaveLoadInstall(const char* iniPath);

// True while the screen runs its own message loop; the game's input handlers must ignore input.
bool SaveLoadActive();
