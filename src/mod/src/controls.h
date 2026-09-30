#pragma once

// Modern RTS controls:
//  - Grid hotkeys: command-panel buttons are triggered by the key at their on-screen
//    slot (GridKeys in WarWindHD.ini) instead of the command's first letter.
//  - Ctrl+digit assigns a control group (Shift+digit keeps working as in the original).
void ControlsInstall(const char* iniPath);

// Key of a visible command-panel slot (0 when grid hotkeys are off or the slot has none).
char ControlsGridKey(int slot);
