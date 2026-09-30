#pragma once
// Addresses in WW.EXE "War Wind version 1.2 DX3" (Watcom, image base 0x400000, no ASLR).
// Game functions use the Watcom register convention: eax, edx, ebx, ecx, then stack.
#include <windows.h>

#include <stddef.h>

namespace game
{
    // DirectDraw state owned by the game's video module.
    inline void** const lpDirectDraw = (void**)0x4C2B28;
    inline void** const lpPrimary    = (void**)0x4C2C04;
    inline void** const lpClipper    = (void**)0x4C2B38;
    inline void** const lpBackBuffer = (void**)0x4C2B30;   // surface every draw primitive targets
    // Clip rectangle {left, right, top, bottom} (inclusive) and the surface bounds it is reset to;
    // SetVideoMode sets both to the mode size.
    inline int* const   clipRect     = (int*)0x4C2120;
    inline int* const   clipBounds   = (int*)0x4C2130;
    inline HWND* const  hwndMain     = (HWND*)0x4C2C20;
    inline int* const   screenWidth  = (int*)0x4C2BF0;
    inline int* const   screenHeight = (int*)0x4C2BEC;

    // Keyboard modifier state maintained by the game's message handlers.
    // Polled each frame from GetKeyState(VK_CONTROL / VK_MENU / VK_SHIFT) at 0x41FD60.
    inline volatile BYTE* const ctrlDown  = (BYTE*)0x4B276C;
    inline volatile BYTE* const altDown   = (BYTE*)0x4B276D;
    inline volatile BYTE* const shiftDown = (BYTE*)0x4B276E;

    // Game clock in milliseconds (all screens step a frame every 62), advanced by a timer thread.
    inline volatile DWORD* const clockMs = (DWORD*)0x4C29EC;

    // Options (registry backed).
    inline volatile BYTE* const optCinematicSubtitles = (BYTE*)0x4B76B8;
    inline volatile BYTE* const scrollSpeedOption = (BYTE*)0x4B76B7;   // "Screen Scroll Speed", 0 = slowest

    // Set to 1 by PlayVideo while a cinematic is running.
    inline volatile BYTE* const videoPlaying = (BYTE*)0x4C2D57;

    // Display-suspend nesting: PlayVideo decrements it on entry (calling SuspendDisplay when it
    // drops from 1), and the callers' ResumeDisplayLevel (0x413FF8) increments it back afterwards.
    inline volatile signed char* const displaySuspend = (signed char*)0x4C2C24;
    inline volatile int* const  displayFlag4C2BF4 = (int*)0x4C2BF4;
    inline volatile BYTE* const displayFlag4C2DAE = (BYTE*)0x4C2DAE;
    constexpr DWORD fnSuspendDisplay = 0x413C40;   // (eax = primary surface, edx = flag)

    // Screen handlers: SetScreenHandler stores the new one here after sending 0x1403 to the old one.
    inline DWORD* const currentScreenHandler = (DWORD*)0x4B25A4;
    constexpr DWORD fnSetScreenHandler   = 0x414048;   // (eax = handler)
    constexpr DWORD fnInGameScreenHandler = 0x41FD58;

    // Menus (MenuScreenHandler also runs the in-game menu over a paused mission). The screen's
    // buttons are a list built by AddMenuItem; DrawMenu 0x46D014 draws the current dialog.
    constexpr DWORD fnMenuScreenHandler = 0x4713E8;
    constexpr DWORD fnMainMenuHandler = 0x468BF0;
    constexpr DWORD fnBriefingHandler = 0x46C06C;         // mission briefing (map / Hall of Heroes, text pages)
    constexpr DWORD kMainMenuArt = 0x80040005;          // RES.004 #5: the main menu's tablet
    struct MainMenuHotspot                               // a glyph of the tablet
    {
        WORD left, top, right, bottom, id, text;         // caption: RES.000 0x25E + text
    };
    inline const MainMenuHotspot* const mainMenuHotspots = (const MainMenuHotspot*)0x4B6EA6;   // 9
    constexpr DWORD kMenuIcons = 0x8004000E;            // RES.004 #14: menu button icons (+1 = pressed)
    inline volatile int* const menuScreen   = (int*)0x4B6E84;   // 3 race options, 6 in-game (ESC) menu
    inline volatile int* const menuDialog   = (int*)0x4B7708;   // 0 base, 3 ideology, 4 sound, 5 goal, 6 options
    inline volatile int* const menuConfirm  = (int*)0x4B7744;   // a Yes / No question replaces the dialog
    inline volatile int* const menuHover    = (int*)0x4B76FC;   // id of the item under the mouse
    inline volatile int* const menuPanelX   = (int*)0x4B772C;   // dialog origin (640x480 layout)
    inline volatile int* const menuPanelY   = (int*)0x4B7730;
    inline volatile BYTE* const menuRedraw  = (BYTE*)0x5E1D97;
    inline DWORD* const menuPanelArt        = (DWORD*)0x5E1D78; // DrawPanel9 tiles (RES.001 #197 + race)
#pragma pack(push, 1)
    struct MenuItem
    {
        WORD left, top, right, bottom, id;
        DWORD sound;
        MenuItem* next;
    };
#pragma pack(pop)
    static_assert(offsetof(MenuItem, next) == 0xE, "MenuItem layout");
    inline MenuItem* const* const menuItems = (MenuItem* const*)0x4B7700;
    constexpr DWORD kMenuLabelBase = 0x80000267;        // RES.000 label of a menu item (+ id)
    constexpr DWORD fnDrawPanel9 = 0x46C6CC;            // (eax res, edx x, ebx y, ecx rows, [stack] cols): 16 px tiles
    inline const int* const missionIndex = (int*)0x4B66E8;
    constexpr DWORD kScenarioGoalBase = 0x8000049C;     // RES.000 goal: + 20 * race + 2 * mission
    inline const char* const leaderName = (const char*)0x5440F8;
    inline const char* const clanName   = (const char*)0x54410D;
    constexpr DWORD fnSetVideoMode = 0x414214;  // (eax = width, edx = height, ebx = bpp)
    constexpr DWORD fnSetClipRect  = 0x414A54;  // (eax = top, edx = bottom, ebx = left, ecx = right, [stack] RECT* old), ret 4
    constexpr DWORD fnDrawImage    = 0x413A7C;  // (eax = res id, edx = frame, ebx = x, ecx = y, [stack] flags), ret 4
    constexpr DWORD fnDrawImageOffset = 0x413A98;  // same, frames placed by their stored offset
    constexpr DWORD fnSetTextPos   = 0x43FD2C;  // (ax = x, dx = y)
    constexpr DWORD fnPutPixel     = 0x4149EC;  // (eax = x, edx = y, bl = colour)
    inline BYTE* const playerRace  = (BYTE*)0x4B342E;
    constexpr DWORD kFrameArtBase  = 0x800100C1; // + race: frame 0 right column (518,0 122x453), 1 bottom bar (0,453 640x27)

    // Window procedure (stdcall) and in-game input internals.
    constexpr DWORD fnWndProc            = 0x414EEC;
    constexpr DWORD fnKeyScroll          = 0x41F00C;  // (eax = VK_LEFT..VK_DOWN) -> 1 if handled
    constexpr DWORD fnRedrawMapSurface   = 0x41913C;  // (eax = full; the game passes *displayHackOption)
    constexpr DWORD fnFindThingAtScreenPos = 0x422598; // (eax = x, edx = y, ebx = clan filter 0) -> id or 0
    constexpr DWORD fnSelectThing        = 0x41C730;  // (eax = id, edx = exclusive, ebx = 0, ecx = 1, [stack] clan, announce), ret 8
    constexpr DWORD fnDeselectThing      = 0x41CB5C;  // (eax = id)
    inline int* const   displayHackOption = (int*)0x4B6E94;
    inline WORD* const  modalDialog       = (WORD*)0x4B2648;
    inline WORD* const  firstSelected     = (WORD*)0x4B2640;
    inline WORD* const  curSelected       = (WORD*)0x4B274C;   // id, or 0x801 for several
    inline BYTE* const  panelMode         = (BYTE*)0x4B60EE;
    inline BYTE* const  panelDirtyBits    = (BYTE*)0x4B264C;
    inline BYTE* const  playerClan        = (BYTE*)0x4B342C;
    inline short* const mouseX            = (short*)0x4C2C25;  // game coordinates (scaled in windowed mode)
    inline short* const mouseY            = (short*)0x4C2C27;
    inline BYTE* const  smallStartX       = (BYTE*)0x5440E5;   // small-tile scroll origin
    inline BYTE* const  smallStartY       = (BYTE*)0x5440E4;
    inline BYTE* const  bigStartX         = (BYTE*)0x5440E3;
    inline BYTE* const  bigStartY         = (BYTE*)0x5440E2;
    inline WORD* const  tileThings        = (WORD*)0x4F1F28;   // [192][192] first thing id per small tile (& 0x7FF)

    // Click handling. WM_LBUTTONUP selects things or, with commandable units selected, orders them
    // (IssueDefaultOrder); WM_RBUTTONUP on an own unit opens its command popup, elsewhere deselects all.
    // panelMode: 1 normal, 2 command popup open, >=5 targeting / placement for pendingCmd.
    constexpr DWORD fnGetConcealment   = 0x423830;  // (eax = Thing*, edx = check fog) -> 4 if hidden from the player
    constexpr DWORD fnClearPopupFlags  = 0x41C6C0;
    constexpr DWORD fnResetPanel       = 0x44C994;
    constexpr DWORD fnClosePopupLevel  = 0x44FED4;
    constexpr DWORD fnShowCursor       = 0x413FF8;   // cursor level (displaySuspend) +1, drawn from 1
    constexpr DWORD fnHideCursor       = 0x413FC8;   // level -1, the cursor erased when it leaves 1
    constexpr DWORD fnRestoreCursor    = 0x41403C;
    inline short* const pendingCmd      = (short*)0x4B60EC;
    inline int* const   altPanelOpen    = (int*)0x4B2644;     // "Show Active Units" / Inn panel
    inline int* const   cursorHidden    = (int*)0x4B2748;     // placement modes hide the system cursor
    inline int* const   lButtonHeld     = (int*)0x4B33C4;
    inline int* const   rButtonHeld     = (int*)0x4B33C8;
    inline short* const dragAnchorX     = (short*)0x4B33CC;   // left-press position, -1 when none
    inline short* const dragAnchorY     = (short*)0x4B33CE;
    inline BYTE* const  waypointCount   = (BYTE*)0x5440E0;    // Alt+click waypoint mode when nonzero

    // Command panel ("popup") state: a stack of button levels, 14 bytes each at panelLevels:
    // +0 page (word), +2 enabled-button mask (dword), +6 highlighted button, +8 button count.
    // panelDepth levels exist; panelActive is the level buttons and hotkeys act on.
    constexpr DWORD kPanelLevels = 0x4B60F4, kPanelLevelSize = 14;
    inline WORD* const  panelDepth      = (WORD*)0x4B6168;
    inline WORD* const  panelActive     = (WORD*)0x4B6156;
    inline DWORD* const pageFirstButton = (DWORD*)0x4B6190;  // per page: index of its first command
    inline DWORD* const raceLabelBase   = (DWORD*)0x4B6158;  // per race: RES.000 index of command 0's label
    constexpr DWORD fnOpenCommandPopup  = 0x44E7CC;  // level 0 for the selection -> 0 when there is none
    constexpr DWORD fnPanelPressButton  = 0x44FF1C;  // (eax = button bit, edx = visible slot)
    constexpr DWORD fnPopupClick        = 0x41D0B8;  // (eax = 0 from a hotkey, 1 from a click)
    constexpr DWORD kCommandIcons       = 0x800100CA; // frame 2 * (pageFirstButton + bit) (+1 highlighted)
    // OpenCommandPopup copies each selected unit's state byte: unit ext +0x15 -> +0x28 (0x78-byte
    // records at 0x4EA7A0), resource ext +0x14 -> +0x18 (0x28-byte records at 0x4E9800).
    constexpr DWORD kUnitExtCopy = 0x4EA7C8, kUnitExtSize = 0x78;
    constexpr DWORD kResExtCopy  = 0x4E9818, kResExtSize = 0x28;
    constexpr int   kNextSelected = 0x45;              // Thing: word id of the next selected thing

    // Text: fonts per race at 0x800100E9 + 4 * race (panel font), SelectFont returns the old one.
    constexpr DWORD fnSelectFont   = 0x43FCFC;   // (eax = font resource) -> previous
    constexpr DWORD fnPrintString  = 0x43FE50;   // (eax = const char*) at the text position
    constexpr DWORD kPanelFontBase = 0x800100E9;
    constexpr DWORD fnTextWidth    = 0x43FFBC;   // (eax = const char*, edx = length) -> pixels
    // Status line (hover help, messages): shown while statusTimer != 0 (0xFFFF = until replaced,
    // else frames left); the text is the resource statusResource when nonzero, else statusText.
    inline const char* const statusText = (const char*)0x544016;
    inline const DWORD* const statusResource = (DWORD*)0x4B271C;
    inline const WORD* const statusTimer = (WORD*)0x4B2720;
    constexpr DWORD kUnitNameBase = 0x80000000 + 0x15A;   // RES.000 name of a unit type (+ type)
    constexpr int kMapTiles = 192;

    struct Thing   // stride 0x7C at 0x5044A0
    {
        BYTE type;          // +0
        BYTE category;      // +1: 1 building, 2/3 unit, 8
        WORD ext;           // +2
        short tileX, tileY; // +4, +6
        BYTE pad0[6];
        WORD nextOnTile;    // +0x0E
        WORD flags;         // +0x10: bit0 selected, 0x20 removed
        WORD flags2;        // +0x12
        BYTE pad1[4];
        BYTE owner;         // +0x18
        BYTE pad2[0x7C - 0x19];
    };
    inline Thing* const things = (Thing*)0x5044A0;

    constexpr DWORD fnPlayVideo   = 0x411D38;   // (eax = const char* path, edx = flags)
    constexpr DWORD fnFillRect    = 0x41468C;   // (eax x, edx y, ebx w, ecx h, [stack] color), ret 4
    constexpr DWORD fnFlip        = 0x414588;   // draws the cursor into the back buffer, then flips
    constexpr DWORD fnInGameFrame = 0x41BB84;   // mission idle step: draw, wait for the 62 ms deadline, flip
    inline volatile DWORD* const nextFrameClock = (DWORD*)0x4B2744;   // clockMs deadline of the pending flip
    constexpr DWORD fnPresentMapView = 0x4193D4;  // map surface -> canvas map view (frame start)
    constexpr int kDrawNodeSize = 36;             // draw list node: next, u16 type, record
    constexpr DWORD fnAssignGroup = 0x47ADAC;   // (eax = group) store selection
    constexpr DWORD fnRecallGroup = 0x47AE00;   // (eax = group) select group
    constexpr DWORD fnResourceText = 0x41094C;  // (eax = 0x80000000 | archive << 16 | index) -> const char*
    constexpr DWORD fnSetStatusText = 0x440100; // (eax = const char*, edx = colour) status bar, max 89 chars
    constexpr DWORD fnPanelEnabledMask = 0x44FEE0;

    // End of mission: InGameFrame sets the result words, loads the end picture's palette and sets
    // endScreen; DrawGameFrame then only draws the 640x480 victory / defeat picture.
    inline volatile WORD* const endScreen   = (WORD*)0x4B26CA;
    inline volatile WORD* const resultWon   = (WORD*)0x4B274E;
    inline volatile WORD* const resultLost  = (WORD*)0x4B2750;
    inline volatile WORD* const resultWon2  = (WORD*)0x4B2752;
    constexpr DWORD fnSetPaletteResource = 0x41191C;  // (eax = palette resource id)

    // Mission message boxes (modalDialog != 0): drawn by DrawGameFrame; the in-game button handlers
    // pass clicks with eax = packed mouse position (x low word, y high word).
    constexpr DWORD fnDrawModalDialog = 0x48D9C0;
    constexpr DWORD fnModalPress      = 0x48DAFC;
    constexpr DWORD fnModalRelease    = 0x48DC78;
    constexpr DWORD fnGetMousePos     = 0x413EA0;  // (eax = short[2] out)
    constexpr DWORD kEndPictureWon  = 0x80040013;     // + race: RES.004 #19..22 victory, 640x480
    constexpr DWORD kEndPictureLost = 0x8004001B;     // + race: RES.004 #27..30 defeat
    constexpr DWORD kEndPaletteWon  = 0x8004000F;     // + race
    constexpr DWORD kEndPaletteLost = 0x80040017;     // + race

    // Import table slot the game calls GetKeyState through (modifier polling in the screen handlers).
    inline void** const iatGetKeyState = (void**)0x6203D0;

    // Bytes at the SetDisplayMode call site; used to verify the executable version.
    constexpr DWORD kVersionCheckAddr = 0x4684C7;
    inline const BYTE kVersionCheckBytes[] = { 0xBB, 0x08, 0x00, 0x00, 0x00 };  // mov ebx, 8
}
