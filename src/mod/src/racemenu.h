#pragma once

// Race options menu (g_menuScreen 3) in the wide menus (ui_menus/mockup_campaign_menu.png): the
// race stone is shown whole and the menu's buttons move onto the column panel beside it. The game
// draws and hit-tests every button from its own rectangle, so only the rectangles change.
void RaceMenuInstall();
