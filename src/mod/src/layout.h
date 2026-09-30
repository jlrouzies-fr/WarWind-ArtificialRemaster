#pragma once

// In-mission layout. The screen shows the map over its full width and a bottom HUD; the game
// draws into an offscreen canvas holding the map view (the screen's map area plus kMargin on
// every side), its original right-hand panel column and status bar beside and below it, and a
// HUD strip the mod draws itself. canvas.cpp composes the screen from those regions.
// tools/gen_hires_patch.py derives its immediates from the same numbers (keep them in step).
//
//   canvas:  +--------------------------------------+--------+
//            | map view  kViewW x kViewH            | column |  column: kColumnW x 453
//            |                                      |        |
//            +--------------------------------------+        |
//            | status bar (27)                               |
//            +-----------------------------------------------+
//            | HUD strip kScreenW x kHudH (mod-drawn)        |
//            +-----------------------------------------------+
namespace layout
{
constexpr int kTile = 24;                                // small map tile, pixels
constexpr int kMapTiles = 192;

constexpr int kScreenW = 1280, kScreenH = 720;           // video mode, 3x to 4K
constexpr int kHudH = 128;                               // bottom HUD on screen
constexpr int kScreenViewW = kScreenW, kScreenViewH = kScreenH - kHudH;   // map area on screen
constexpr int kMargin = 3 * kTile;                       // off-screen map border in the canvas

constexpr int kViewW = kScreenViewW + 2 * kMargin, kViewH = kScreenViewH + 2 * kMargin;
constexpr int kDX = kViewW - 518, kDY = kViewH - 453;    // growth over the 640x480 layout
constexpr int kColumnW = 640 - 518, kColumnH = 453;      // original right panel column
constexpr int kBarH = 480 - 453;                         // original status bar
constexpr int kColumnX = kViewW, kBarY = kViewH;
constexpr int kHudY = kBarY + kBarH;                     // mod-drawn HUD strip in the canvas
constexpr int kCanvasW = kViewW + kColumnW, kCanvasH = kHudY + kHudH;
static_assert(kCanvasW >= kScreenW, "HUD strip must fit the canvas");

constexpr int kViewCols = (kViewW + kTile - 1) / kTile;      // must match COLS / ROWS in the generator
constexpr int kViewRows = (kViewH + kTile - 1) / kTile + 1;
constexpr int kMaxOriginX = kMapTiles - kViewCols;           // game scroll clamps
constexpr int kMaxOriginY = kMapTiles - (kViewRows - 1);
}  // namespace layout
