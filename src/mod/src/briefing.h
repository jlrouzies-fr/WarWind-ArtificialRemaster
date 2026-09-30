#pragma once

// Mission briefing in the wide menus (ui_menus/mockup_briefing.png): the 640x480 screen keeps the
// mission map on the left on every page, the briefing text moves to a full-height panel on the
// right (printed by the mod on every page, scrolled on the text page by the game's own scroll
// state), and the scenario goal fills the lower well where the game shows nothing. Built-in
// scenarios have no map: their map box keeps the text page's stone fill. Needs the wide menus.
void BriefingInstall();
