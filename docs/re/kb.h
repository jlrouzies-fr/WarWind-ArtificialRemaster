// War Wind 1.2 DX3 (WW.EXE, Watcom C/C++ 10.x, image base 0x400000, no ASLR)
// Calling convention: Watcom register (eax, edx, ebx, ecx, then stack), callee saves regs.
// Sections: BEGTEXT 0x410000 (code), DGROUP 0x4B0000 (data), .bss 0x4C0000.

// ---------------------------------------------------------------- video / DirectDraw
$ 0x4C2B28 IDirectDraw* g_lpDD
$ 0x4C2C04 IDirectDrawSurface* g_lpPrimary
$ 0x4C2B30 IDirectDrawSurface* g_lpBackOrWork      // Locked/unlocked around many draws
$ 0x4C2B38 IDirectDrawClipper* g_lpClipper
$ 0x4C2C20 HWND g_hwndMain                          // class "Win95GameClassDFI"
$ 0x4C2BF0 int g_screenWidth                        // written by SetVideoMode; only ~8 readers
$ 0x4C2BEC int g_screenHeight
$ 0x4C2BFC int g_screenBpp
$ 0x4C2C1C HRESULT g_lastDDResult
$ 0x4C2C08 int g_haveExclusive
$ 0x54252C IDirectDrawSurface* g_lpMinimapSurface
@ 0x414214 int __watcall SetVideoMode(int width /*eax*/, int height /*edx*/, int bpp /*ebx*/);
@ 0x4684A6 void InitDisplay_CallSite(void);         // mov ebx,8 / mov edx,480 / mov eax,640 / call SetVideoMode
@ 0x41468C void __watcall FillRect(int x /*eax*/, int y /*edx*/, int w /*ebx*/, int h /*ecx*/, int color /*stack*/);
@ 0x414588 void Flip(void);
@ 0x414048 void __watcall SetScreenHandler(void* handler /*eax*/);   // per-screen message proc
@ 0x468BF0 void MainMenuScreenHandler(void);
@ 0x4713E8 void ScreenHandler_4713E8(void);
@ 0x41191C void __watcall ShowResourceImage(int resId /*eax*/);      // e.g. 0x80010001
@ 0x4490BD void ScreenCapture_LBM(void);            // hardcodes 640x480 copy loop

// ---------------------------------------------------------------- input / hotkeys
@ 0x47AF90 char __watcall VirtualKeyToChar(int vk /*eax*/);   // MapVirtualKeyA(vk,2) + toupper + shift map
@ 0x47B400 int __watcall InGameKeyDispatch(int vk);           // switch on char; digits -> RecallGroup
@ 0x47AF28 int __watcall PanelHotkey(char c /*al*/);          // matches c against g_hotkeyTable[page]
@ 0x47AE00 void __watcall RecallGroup(int n /*eax*/);         // deselect all, select group n, refresh panel
@ 0x44FEA8 int GetPanelPage(void);                             // 1-based, 0 = no panel
@ 0x44FEE0 DWORD GetPanelEnabledMask(void);
@ 0x44FF1C void __watcall PanelPressButton(int index /*eax*/, int slot /*edx*/);
// PanelHotkey loop: eax = button index 0..31, edx = visible slot (counts enabled buttons only).
// Instruction at 0x47AF65: cmp ch, [ebx+eax+0x4B9B48]   (3A AC 03 48 9B 4B 00), ebx = page*30
$ 0x4B9B48 char g_hotkeyTable[6][30]
//   row 0 "MASBDUGCYOWRTNHIUSHTWQJ\0L" unit commands (RES.000 text 2..24)
//   row 1 "CIHWTALSORPKGUBDN"        buildings / build options (27..43)
//   row 2 "LBFMCFCBPLGTRMNSHIPU"     spells (44..63)
//   row 3 "SEVPR"                    upgrades (64..68)
//   row 4 "WSMA"                     train (69..72)
//   row 5 "LBFMCFCBPLGTRMNSHIPUA"    spells, variant
$ 0x4B6154 int g_panelStateIndex                    // >>16 = index into g_panelState
$ 0x4B60F4 PanelState g_panelState[]                // stride 14: +0 page(word) +2 mask(dword) +8 first(word)
$ 0x4B6168 short g_panelActive
$ 0x4B276C char g_ctrlDown                        // GetKeyState(VK_CONTROL) polled at 0x41FD60
$ 0x4B276D char g_altDown                         // GetKeyState(VK_MENU)
$ 0x4B276E char g_shiftDown                       // GetKeyState(VK_SHIFT)

// ---------------------------------------------------------------- selection / groups
$ 0x5044A0 Thing g_things[]                         // stride 0x7C
//   +0x10 flags (bit 0x20 tested by RecallGroup), +0x18 owner clan, +0x45 next selected (word)
$ 0x4B2640 short g_firstSelected                    // head of selected linked list
$ 0x4B99E0 short g_groups[10][18]                   // stride 0x24 bytes per group
$ 0x4B342C char g_playerClan
$ 0x4B342E char g_playerRace
@ 0x41CB5C void __watcall DeselectThing(int id);
@ 0x41C730 void __watcall SelectThing(int id /*eax*/, ...);
@ 0x42196C void __watcall RefreshPanelForThing(int id /*eax*/);

// ---------------------------------------------------------------- options (registry values)
@ 0x478A6C int __watcall RegGetOption(const char* name /*eax*/, int def /*edx*/);
@ 0x4789EC void __watcall RegSetOption(const char* name /*eax*/, int value /*edx*/);
$ 0x4B76BA char g_optFullScreenCinematics
$ 0x4B76B8 char g_optCinematicSubtitles

// ---------------------------------------------------------------- video playback (AVI, Cinepak via ICOpen)
// "vids.cap" referenced at 0x4124A5, codec error at 0x412450, BltFast at 0x412972
$ 0x4B15A8 const char* g_videoPaths[]               // "VIDS\\TH\\TH%01dCH.AVI" ...

// ---------------------------------------------------------------- groups (found 2026-09-26)
@ 0x47AD1C void __watcall RemoveThingFromGroups(int id /*eax*/);
@ 0x47AD88 void ClearGroups(void);
@ 0x47ADAC void __watcall AssignGroup(int n /*eax*/);    // Shift+digit ('!','@',...) -> MapVirtualKey(vk,2)-'0'
// InGameKeyDispatch: digits -> RecallGroup only when !ctrl && !alt (0x47B4EE); Ctrl+S (no alt) -> 0x449170

// ---------------------------------------------------------------- video
@ 0x411D38 void __watcall PlayVideo(const char* path /*eax*/, int flags /*edx*/);  // result unused by callers
$ 0x4C2D57 char g_videoPlaying
$ 0x5E1DA0 int g_videoFlags                          // passed as PlayVideo flags (0x40 = full screen)
@ 0x478530 void __watcall PlayChapterVideo(int race /*eax*/, int n /*edx*/);   // VIDS\XX\XX%dCH.AVI
// Caption index N in Data\VIDS.CAP = RES.000 string 746+N

// ---------------------------------------------------------------- resources
// RES.00x: u32 count, u32 offset[count]. D3GR: flags@4, dataStart@8, frameCount@0x18, frameOffset[]@0x1C.
// Frame: u32 size, u32 type(2=raw 8bpp), i16 x, i16 y, u16 h, u16 w, pixels. Palette D3GR: 256 x 6-bit RGB @0x20.
// RES.004 #0-3: in-game 640x480 frame art per race (layout vs. code placement unverified; code puts map view at 0,0 and minimap at x=530)
// RES.004 #5-9: carved-stone menu backgrounds (palette RES.001 #1); #19-30 race splash art

// ---------------------------------------------------------------- hi-res analysis (640x480 dependencies, 2026-09-26)
// Renderer has NO software back buffer and NO hardcoded pitch: all draws are DD surface ops; every Lock keeps
// DDSURFACEDESC on the stack and uses lPitch(+0x10)/lpSurface(+0x24) locally. Regenerate tables: tools/hires_scan.py.
@ 0x411AA4 int __watcall CreateOffscreenSurface(int h /*eax*/, int w /*edx*/, IDirectDrawSurface** out /*ebx*/, DDSURFACEDESC* d /*ecx*/, int caps);
@ 0x4138B0 int __watcall DrawSprite(int resId /*eax*/, int frame /*edx*/, int x /*ebx*/, int y /*ecx*/, int flags, IDirectDrawSurface* dst, int a, int b); // adds frame hotspot (+8 x,+0xA y words), clips vs g_clip, blits cached frame surface
@ 0x413A7C int __watcall DrawImage(int resId /*eax*/, int frame /*edx*/, int x /*ebx*/, int y /*ecx*/, int flags); // DrawSprite(dst=g_lpBackOrWork); UI, menus, font glyphs
@ 0x4135E4 IDirectDrawSurface* __watcall GetFrameSurface(int resId /*eax*/, int frame /*edx*/); // per-frame surface cache
@ 0x412FE0 int SurfaceBlt(void);                     // Blt(+0x14)/BltFast(+0x1C) or Lock src+dst + soft copy 0x44A4A0.. with both lPitch
@ 0x414818 void FillRectClipped(void);               // clip vs g_clip (0x411A34) then FillRect
@ 0x414720 void DrawLine(void);                      // GDI: GetDC(back), IntersectClipRect(g_clip), MoveTo/LineTo
@ 0x414888 void DrawRectOutline(void);               // minimap view rect, selection box
@ 0x4149EC void __watcall PutPixel(int x /*eax*/, int y /*edx*/, char color /*bl*/);  // Lock back, y*lPitch+x
@ 0x414A54 void __watcall SetClipRect(int top /*eax*/, int bottom /*edx*/, int left /*ebx*/, int right /*ecx*/, RECT* old);
@ 0x411A34 int ClipRect(void);                       // clip src/dst rect pair vs RECT* (ecx=&g_clip)
@ 0x43FD2C void __watcall SetTextPos(short x /*ax*/, short y /*dx*/);
@ 0x43FE50 void __watcall DrawText(const char* s /*eax*/);          // bitmap font, glyphs via DrawImage
@ 0x43FDDC void __watcall DrawResText(int id /*eax*/);
@ 0x413EA0 void __watcall GetMousePos(short* xy /*eax*/);
@ 0x411AEC void VideoFrameToBack(void);              // Lock back, copy rows via 0x4493F0 / 0x44A4A0
@ 0x4490A0 void ScreenCaptureCopy(void);             // hardcoded 640x480 copy from primary into malloc(0x4B000) 0x4B4AF8
$ 0x4C2120 int g_clipLeft                            // g_clip RECT (inclusive right/bottom): 0x4C2120 left,
$ 0x4C2124 int g_clipRight                           //   0x4C2124 right, 0x4C2128 top, 0x4C212C bottom
$ 0x4C2128 int g_clipTop
$ 0x4C212C int g_clipBottom
$ 0x4C2130 RECT g_screenRect                         // set by SetVideoMode (0,0,w-1,h-1)
$ 0x4C2BF4 int g_windowed
$ 0x4C2110 RECT g_clientRect                         // windowed: mouse scaled by g_screenWidth/Height vs this
$ 0x4C2C25 short g_mouseX                            // raw lParam from WM_MOUSEMOVE (0x4151A5)
$ 0x4C2C27 short g_mouseY
$ 0x556980 short g_textX
$ 0x556982 short g_textY
// ---- in-game map viewport. Layout @640x480: map (0,0)-(518,453) | right column x518..640 | bottom bar y453..480.
// (Code-verified: map surface blitted to back at (0,0) in 0x4193D4; minimap blitted to (530,136) in 0x4184C8.)
$ 0x542528 IDirectDrawSurface* g_lpMapSurface       // 518x453 (0x206 x 0x1C5), created 0x41F454
$ 0x542530 int g_frameCounter
$ 0x542534 DDSURFACEDESC g_minimapDesc               // lpSurface 0x542558, lPitch 0x542544
$ 0x5440E3 uchar g_bigStartX                         // save "bigstartx" (48px tiles) = smallx/2
$ 0x5440E2 uchar g_bigStartY                         // save "bigstarty"
$ 0x5440E5 uchar g_smallStartX                       // save "smallstartx" (24px tiles), max 0xAA (=192-22)
$ 0x5440E4 uchar g_smallStartY                       // save "smallstarty", max 0xAD (=192-19)
$ 0x4B272A short g_lastSmallX                        // last drawn scroll pos (incremental scroll)
$ 0x4B272C short g_lastSmallY
$ 0x4B264C char g_panelDirtyBits                     // 1 header,2 minimap,4 command,8 bottom -> counters 0x4B33F8..0x4B33FB
$ 0x605DD0 uchar g_fog[192*192]
$ 0x4C2DE8 char g_rectList[2][0xA00]                 // selection/overlay rects (10-byte recs), counts 0x4B33B0[2]
@ 0x41A4C0 void InGameFrame(void);                   // map present/fog, sprites (clip 518x453), DrawPanels, minimap, text
@ 0x41FD58 int __watcall InGameScreenHandler(int msg /*eax*/, int wParam /*edx*/, int lParam /*ebx*/);
@ 0x41F454 void __watcall CreateOrRestoreMapSurfaces(int create /*eax*/);   // 0x41F474/0x41F479 = w 0x206 / h 0x1C5
@ 0x41913C void __watcall RedrawMapSurface(int full /*eax*/);              // 12 big tiles (0xC) per axis, clip 518x453
@ 0x418764 void ScrollMapSurfaceUp(void);            // self-blit + strip redraw; also 0x4189D4 down, 0x418C58 left, 0x418EBC right
@ 0x4193D4 void PresentMapView(void);                // blit map surf (0,0,518,453) -> back; fog mode: 22x19 FillRects of 24px
@ 0x4185F0 void __watcall RedrawMapTile(int sx /*eax*/, int sy /*edx*/);   // visible if sx<smallx+0x16, sy<smally+0x14
@ 0x419FE0 void DrawWaypointLines(void);             // NOT a sprite pass: DrawLine 0x414720 waypoint/rally lines of the selection (clip 0x206 x 0x1C6)
@ 0x41F00C int __watcall KeyScroll(int vk /*eax*/);  // VK_LEFT..VK_DOWN, clamps 0xAA / 0xAD
@ 0x41AF08 void InGameMouseTick(void);               // minimap drag (x-0x217 clamp 0x55, y-0x8D clamp 0x56), edge scroll x>0x27E y>0x1DE
@ 0x41DB1C void __watcall InGameLButtonDown(int wParam /*eax*/, int lParam /*edx*/); // tile = x/24+smallx; minimap; x>0x205 -> PanelClick
@ 0x41EC88 void __watcall InGameRButton(int wParam, int lParam);
@ 0x41B868 void UpdateCursorShape(void);             // x>=0x205 -> panel cursor
@ 0x41BE20 void __watcall DragRectToTiles(short* xy /*eax*/);  // clamps x to 0x205
@ 0x47AEBC void SelectAllOnScreen(void);             // rect 0x206 x 0x1DF
// ---- in-game UI (all coordinates are code immediates, no DGROUP rect tables)
@ 0x4181B4 void DrawPanels(void);                    // dispatch by dirty counters
@ 0x416FB0 void DrawPanelHeader(void);               // clip y0..0x83 x0x206..0x280; art 0x800100C1+race @0,0 and 0x800100D3+race @0x213,0x1B
@ 0x4180FC void DrawMinimapFrame(void);              // clip y0x83..0x116 x0x206..0x280
@ 0x417280 void DrawCommandPanel(void);              // clip y0x116..0x1C5 x0x200..0x280; buttons x 0x213..
@ 0x4171D4 void DrawBottomBar(void);                 // art frame 1 @0,0; status text SetTextPos(0x1E,0x1CC)
@ 0x4184C8 void DrawMinimap(void);                   // 96x96 -> (0x212,0x88)-(0x272,0xE8); view rect via DrawRectOutline colour 0x4D
@ 0x4216FC void BuildMinimap(void);                  // Lock 0x54252C, 96 rows
@ 0x4214C0 void DrawMinimapDots(void);               // +0x212 / +0x88. Really QueueMinimapDots(eax=Thing*): type-4 nodes into overlay list 0x5424A4 (rendered by DrawListRender at 0x418534)
@ 0x41CE50 void __watcall PanelClick(int xy /*eax*/, int right /*edx*/);    // x 0x213..0x249
@ 0x457F40 void PanelHover(void);                    // x 0x211..0x273, tooltips
@ 0x4571F8 void DrawAltPanel(void);                  // replaces minimap when 0x4B2644 (0x457248/0x4574E8)
// hi-res plan: 1280x720 => every in-game x const >=0x200 +0x280, y const >=0x1C0 +0xF0; viewport 1158x693;
// tiles: vis 0x16->0x31, 0x14->0x1E; margins 0x1A->0x35, 0x18->0x22; big 0xC->0x1A(x)/0x10(y);
// fog 0x16/0x13 -> 0x31/0x1D; clamps 0xAA->0x8F, 0xAD->0xA3 (0xAC->0xA2), 0x55->0x47, 0x56->0x51; edge 0x27E->0x4FE, 0x1DE->0x2CE.

// ---------------------------------------------------------------- verified in game (2026-09-26, sandbox run)
// Modifier bytes 0x4B276C/D/E (ctrl/alt/shift) are refreshed every frame from GetKeyState(VK_MENU/CONTROL/SHIFT)
//   in the screen handlers at 0x41FD63.., 0x46C07A.., 0x4713F6.. (IAT slot 0x6203D0; cnc-ddraw re-hooks it).
// Command menu = right-click popup on the selected unit: a horizontal row of icons; hovering one shows its
//   label in the status bar (0x44CD82 path, text index = raceBase[race] + pageBase[page] + button index).
//   Grid keys verified: with popup open Q -> Move, W -> Attack (visible-slot order).
// Right column in game: portrait/header y 0..131, minimap (530,136) 96x96, "Show Active Units" (549,255),
//   "Show Units in Inn" (612,255), selected-unit portrait + bio-upgrade grid (528..632, 398..452); bottom bar y 453..480.
// Chapter/mission videos: PlayVideo gets "VIDS\EA\EA1MS.AVI" (relative to Data, game prepends DATA\ itself).
$ 0x544016 char g_statusText[90]                   // set by 0x440100 (string) / 0x44013C (text index in 0x4B271C)

// ---------------------------------------------------------------- window / message routing (2026-09-27)
// WNDCLASSA.style = 8 (CS_DBLCLKS) -> Windows sends WM_LBUTTONDBLCLK (0x203) for the 2nd click; WndProc drops it.
@ 0x414EEC LRESULT __stdcall WndProc(HWND, UINT msg, WPARAM, LPARAM);   // esi=msg edi=wParam ebp=lParam
$ 0x4B25A4 int (__watcall *g_screenHandler)(int msg, int wParam, int lParam);  // set by SetScreenHandler; in-game == 0x41FD58
$ 0x4B2638 int g_screenChanged                      // set to 1 by SetScreenHandler
// SetScreenHandler: old(0x1403), g_screenHandler=new, new(0x1402). Main loop 0x41577A sends 0x1401 (frame tick) each frame;
// 0x4155B3 sends 0x1400 (init); 0x413DA6 sends 0x1404; 0x412BB4 sends 0x1405 (video); 0x410494/0x414EBA send 0x1C.
// WndProc -> handler(eax=msg, edx=wParam, ebx=lParam):
//   0x100/0x101/0x104/0x105 key msgs (0x41529B): ret 1 -> swallow; else DefWindowProc (fullscreen also swallows VK_MENU/F10)
//   0x201,0x202,0x204,0x205,0x207,0x208 (0x415247): only if g_cursorShown 0x4C2C24 > 0 or g_videoPlaying; windowed:
//     lParam is REPLACED by scaled g_mouseX | g_mouseY<<16; ret 1 -> return 0 else DefWindowProc
//   0x200 WM_MOUSEMOVE (0x4151A0): NOT forwarded. Stores lParam at 0x4C2C25 (x)/0x4C2C27 (y); windowed:
//     x = x*g_screenWidth/(g_clientRect.right-left), y = y*g_screenHeight/(bottom-top); then 0x413DC0 (fullscreen cursor blit)
//   0x203/0x206/0x209 DBLCLK and 0x20A WM_MOUSEWHEEL: DefWindowProc, never reach the handler
//   0x0F WM_PAINT, 0x20 WM_SETCURSOR, 0x3B9 MM_MCINOTIFY, 0x1500, 0x1600: forwarded
//   3/5 WM_MOVE/WM_SIZE (0x4150A6): windowed -> GetClientRect + ClientToScreen -> g_clientRect (0x4C2110 / 0x4C2118)
$ 0x4C2C24 char g_cursorShown                       // ShowCursor-style counter; mouse buttons ignored when <= 0
$ 0x4B2648 int g_modalDialog                        // nonzero: in-game modal dialog (briefing/menu); KeyScroll & clicks go to it
$ 0x4B26CA short g_gameEnding                       // nonzero: in-game handler only accepts 2/0x1C/0x202/0x1401
$ 0x4B33C4 int g_lButtonHeld                        // set on 0x201, cleared on 0x202
$ 0x4B33C8 int g_rButtonHeld
$ 0x4B33CC short g_dragAnchorX                      // -1 when no drag; set by InGameLButtonPress
$ 0x4B33CE short g_dragAnchorY
// InGameScreenHandler dispatch (g_gameEnding==0): 0x100/0x104 -> KeyScroll(vk); 0x101 -> InGameKeyUp(vk,0); 0x105 -> InGameKeyUp(vk,1)
//   0x201 -> InGameLButtonPress(lParam); 0x202 -> InGameLButtonDown(wParam,lParam) [it is really the RELEASE/click handler]
//   0x204 -> 0x41FCB4(lParam); 0x205 -> InGameRButton(wParam,lParam); 0x207/0x208 middle button -> return 1 (ignored)
//   0x1401 -> InGameTick 0x41BB84 (InGameFrame + Flip); 0x1402 -> level start (0x41FF67, zeroes scroll pos); 0x1C -> restore surfaces
@ 0x41EB00 void __watcall InGameLButtonPress(int lParam /*eax*/);   // sets g_dragAnchor=-1, minimap/panel press
@ 0x41BB84 void InGameTick(void);                    // 0x1401 handler; calls InGameFrame 0x41A4C0
@ 0x41EFC0 int __watcall InGameKeyUp(int vk /*eax*/, int sys /*edx*/);

// ---------------------------------------------------------------- scrolling (2026-09-27)
$ 0x4B76B7 char g_optScrollSpeed                    // registry "Screen Scroll Speed"; KeyScroll step = speed+1 small tiles
$ 0x4B6E94 int g_optDisplayHack                     // registry "DisplayHack"; passed as RedrawMapSurface(full) by KeyScroll
$ 0x4B60EE short g_panelMode                        // 1 normal, 2 = KeyScroll sets g_panelDirtyBits=0xFF, >=5 special modes
// KeyScroll(eax=vk): requires g_modalDialog==0. VK_LEFT/UP: pos -= step, underflow -> 0. VK_RIGHT: x += step clamp 0xAA.
//   VK_DOWN: y += step clamp 0xAD. Then big = small/2, RedrawMapSurface(g_optDisplayHack), return 1. Non-arrow vk -> chat/
//   InGameKeyDispatch, so only pass 0x25..0x28. Callable from any message-path context (it is called from WM_KEYDOWN).
// Direct scroll recipe: write 0x5440E5/0x5440E4 (clamped 0..0xAA / 0..0xAD), 0x5440E3/E2 = half, RedrawMapSurface(eax=*(int*)0x4B6E94).

// ---------------------------------------------------------------- Thing struct + selection (2026-09-27)
struct Thing {                                       // 0x5044A0 + id*0x7C, id 1..0x7FF (tile map stores id & 0x7FF)
  uchar type;        // +0x00 unit/building type index (name text: units 0x80020071+type, buildings 0x80020098+type (<0x18))
  uchar category;    // +0x01 1 building, 2 unit, 3 (selectable, ext table 0x4E980C stride 0x28), 8 (selectable), 0 free
  ushort ext;        // +0x02 index into per-category extended record (cat 2: stride 0x78; state byte 0x4EA7B5[ext*0x78] 2/3 = dying/dead -> not selectable)
  short tileX;       // +0x04 small (24px) tile x
  short tileY;       // +0x06 small tile y
  char pad8[6];
  ushort nextOnTile; // +0x0E next thing on same tile (chain from g_tileThings)
  uchar flags;       // +0x10 bit0 selected, bit2 secondary-select, bit5 (0x20) removed/dead (RecallGroup skips)
  char pad11;
  uchar flags2;      // +0x12 bit5 (0x20) inside carrier -> 0x43C5F4(id) gives host
  char pad13[5];
  uchar owner;       // +0x18 clan (compare g_playerClan); 0xB/0xC also selectable by SelectThing
  char pad19[2];
  uchar hidden;      // +0x1B ==1 && byte +0x26 != g_playerClan -> DragRect skips
  char pad1c[0x27];
  ushort prevSel;    // +0x43 selection list prev
  ushort nextSel;    // +0x45 selection list next
};
$ 0x4F1F28 ushort g_tileThings[192][192]            // & 0x7FF = head thing id on that small tile (row stride 0x180 bytes)
$ 0x4B274C ushort g_curSelected                     // 0 none, id if exactly one selected, 0x801 if several
$ 0x4B33AC int g_rectListCur                        // which of g_rectList[2] holds last frame's sprite rects
$ 0x4B33B0 int g_rectListCount[2]
// g_rectList record (10 bytes): u16 thingId, i16 y1, i16 x1, i16 y2, i16 x2 (screen px). Written by DrawSpritesPass.
@ 0x422598 int __watcall FindThingAtScreenPos(int x /*eax*/, int y /*edx*/, int clanFilter /*ebx, 0=any*/); // walks g_rectList top-down, returns id or 0
@ 0x41C730 void __watcall SelectThing(int id /*eax*/, int exclusive /*edx: 1 = DeselectAllOfClan first + make primary*/, int secondary /*ebx: also set flag bit2, UI passes 0*/, int link /*ecx: must be 1*/, int clan /*stack1 = g_playerClan*/, int announce /*stack2: 1 = speak/status text*/); // ret 8. No-op unless tile visible to clan (TileVisibleToClan). Owner must be clan/0xB/0xC and category 2/3/8.
@ 0x41C1BC void __watcall LinkSelected(int id /*eax*/);     // sorted insert; cap 16 units (cat 2/3): evicts lowest 0x4B3374[class] priority
@ 0x41C590 void __watcall DeselectAllOfClan(int clan /*eax*/); // clears bit0/bit2 on every listed thing, g_firstSelected=0
@ 0x41C100 void __watcall UnlinkSelected(int id /*eax*/);
@ 0x41CCDC int __watcall ClickSelectThing(int id /*eax*/, int unused, int forceAdd /*ebx*/); // click: deselects if selected else SelectThing(id,1,0,1,clan,1)
@ 0x424204 void __watcall FlagOp(uint* p /*eax*/, uint mask /*edx*/, int op /*bl: 1 set, 2 clear, else toggle*/);
@ 0x42AA9C int __watcall TileVisibleToClan(int clan /*eax*/, int zero /*edx*/, int bigY /*ebx*/, int bigX /*ecx*/);
@ 0x42B3C0 int __watcall GetThingRank(int id /*eax*/);      // <0 none
$ 0x4B3C38 uchar g_unitTypeTable[][0x20]            // [type][0] = unit class (1 worker-like, 2/3, other); 0x4B3374[class] = select priority
$ 0x4B5E4C uchar g_buildingTypeTable[][0x18]
// 0x42196C "RefreshPanelForThing" is really CenterViewOnThing(id): smallStart = tile-(0xB,0xA) clamped, RedrawMapSurface(1).
@ 0x42196C void __watcall CenterViewOnThing(int id /*eax*/);
// DragRectToTiles(eax=short xy[2] second corner; anchor in g_dragAnchorX/Y): tile box, if !shift DeselectThing loop, then for
//   each tile chain: skip hidden; if category 2..3 && !(flags&1) && owner==g_playerClan -> SelectThing(id,0,0,1,clan,0).
// SelectAllOnScreen (Ctrl+A path 0x47B280): deselect all, anchor=(0,0), DragRectToTiles((0x206,0x1DF)).
// InGameLButtonDown (WM_LBUTTONUP): tile=(x/24+smallX, y/24+smallY); id=FindThingAtScreenPos(x,y,0); alt or waypoint mode -> 0;
//   0x423830(id,1)!=4 || owner==player -> switch category: 1 -> SelectThing(id,1,0,1,clan,1) if nothing/only-buildings selected;
//   2,3,8 -> ClickSelectThing. Click on empty ground with a selection -> move order via 0x428004 / 0x4287EC.

// ---------------------------------------------------------------- mouse clicks / orders (2026-09-27, static)
@ 0x48A684 void __watcall SendOrder(int direct /*eax = *g_orderDirect*/, int unit /*edx*/, int cmd /*ebx*/, int param /*ecx: target thing id, or issuer clan for 0x56/0x58*/, int tile /*stack: y*192+x, or shift flag for 0x56*/); // ret 4. direct==0 -> queued to lockstep net buffer (0x489144), else executed now
$ 0x4BA4B4 int g_orderDirect                        // 0 in network games (orders queued), nonzero single player. ALWAYS pass it as eax to SendOrder
@ 0x489144 void __watcall NetQueueOrder(void);        // SendOrder(direct=0) -> 12-byte slot at 0x616550
@ 0x4287EC void __watcall IssueDefaultOrder(int tileY /*eax*/, int tileX /*edx*/, int clan /*bl*/, int screenXY /*ecx = lParam*/); // left-click on ground/target with units selected
@ 0x428004 void __watcall IssueOrderAt(int tileY /*eax*/, int tileX /*edx*/, int cmd /*ebx = g_pendingCmd*/, int screenXY /*ecx*/); // target click while panelMode>=5
// Both: pick cmd/target, set feedback marker, SendOrder 0x79 chain over the selection, then per selected unit
//   SendOrder(dir, unit, cmd, target, tileIdx+formationOffset) (+0x76 if more follow), finally SendOrder(dir, unit, 0x75, 0, 0) for all.
// IssueDefaultOrder cmd: 7 land move (terrain g_tileThings&0xF800 in {0,0x800}), 8 water (0x3000/0x3800), 0x6B Ctrl formation;
//   visible enemy/other thing under cursor (FindThingAtScreenPos(x,y,playerClan)) -> 1 (attack/interact, 0x72 if 0x4B2770);
//   resource (cat 3 with 0x4E9815[ext*0x28]) -> 6; own building -> move to its centre; building type byte0==0 -> 9.
// Order codes: 0 move (popup Move), 1 attack (popup Attack; no target -> 0), 0x55 build/place, 0x56 select+open popup
//   (SelectThing(u, !shift, secondary=1, ...) then panelMode=2), 0x57 clear popup flag (Thing+0x10 bit2), 0x58 deselect
//   (DeselectThing on the issuer's machine, 0x57 on the others), 0x91 clear ext+0x61 before a new order, 0x75 end of batch.
$ 0x4B60EC short g_pendingCmd                       // order code chosen in the popup (0 Move, 1 Attack, 0x55 build, 0x6B ...)
// g_panelMode 0x4B60EE: 1 normal, 2 command popup open (left click -> PopupClick 0x41D0B8), 5 target reticle (next left click
//   -> IssueOrderAt), 6 building placement (type 0x544070), >=7 other placement. Cursor 0x80010114 (reticle) when >2 else 0x800100BE.
$ 0x4B60F0 short g_pendingCursor                    // cursor/target kind set with g_pendingCmd (1 attack, 2 move, 3 build, 0x13 ...)
$ 0x4B2748 int g_placementCursorHidden              // 1 in modes 6/>=7: system cursor hidden (0x413FC8/0x414034); undo 0x413FF8 + 0x41403C
$ 0x4B2644 int g_altPanel                           // "Show Active Units/Inn" panel open; any left release just closes it
$ 0x5440E0 uchar g_waypointCount                    // Alt+click waypoint mode (0 off); max 0x5440E1; points at 0x4E97E8 (x,y words)
$ 0x4B2658 uchar g_rbPortraitHeld                   // right button held on portrait (0x41FCB4)
$ 0x4B2659 uchar g_ackVoiceToggle
$ 0x4B265A short g_targetFlashId                    // + 0x4B265C timer (0x14) target highlight after an order
$ 0x4B33B8 int g_moveMarker                         // + 0x5424B8 tileX / 0x5424BA tileY
@ 0x44ED10 void __watcall PopupCommandAction(int button /*eax 0..0x18*/, int arg /*edx*/); // sets g_pendingCmd/panelMode/cursor or sends immediate orders
@ 0x44E7CC int OpenCommandPopup(void);                // build popup pages for selection, 0 = none
@ 0x44FED4 void ClosePopupLevel(void);                // word 0x4B6168 = 0 (popup page depth; NOT an "open" flag)
@ 0x44C994 void ResetPanel(void);                     // 0x4B6156=0, 0x4B6168=0, 0x440160, g_panelDirtyBits|=0xF
@ 0x41D0B8 void __watcall PopupClick(int fromMouse /*eax*/); // left release while panelMode==2
@ 0x41C6C0 void ClearPopupFlags(void);                // SendOrder 0x57 for every selected own thing with flags bit2
@ 0x41FCB4 int __watcall InGameRButtonPress(int lParam /*eax*/); // WM_RBUTTONDOWN: portrait -> 0x4B2658, else g_rButtonHeld=1
@ 0x423830 int __watcall GetConcealment(Thing* t /*eax*/, int checkFog /*edx*/); // 4 = invisible to player
@ 0x413FC8 void HideCursor(void);                     // --g_cursorShown (0x4C2C24); ShowCursor 0x413FF8 = ++
// FindThingAtScreenPos ebx = clan whose things are SKIPPED (0 = keep all owned things).
// InGameScreenHandler: 0x202 -> g_lButtonHeld=0 then InGameLButtonDown(eax=wParam, edx=lParam); 0x205 -> g_rButtonHeld=0 then InGameRButton.
// Click-vs-drag (0x41DBCB): box select (DragRectToTiles) iff anchorX>=0 && !g_altPanel && (|x-ax|>16 || |y-ay|>16); tested BEFORE pending mode.
// ESC (KeyScroll 0x41F1CD): panelMode<=1 -> SendOrder(dir, *(u16*)(0x544192+(clan-1)*0x101), 0x66, 0, 0); panelMode>1 -> swallowed, cancels nothing.

// ---------------------------------------------------------------- view culling / menus / end screen (2026-09-27)
// Per-frame culling windows are "scroll origin + N" (rows from 0x5440E4 + 0x18, cols from 0x5440E5 + 0x1A):
//   unit producer caller 0x42920B/0x429226, tile-thing pass 0x419E4C/66, markers 0x421444/62, object grid 0x47BBC6/B7,
//   terrain redraw 0x41989D/BB; exact on-screen tests +0x16 cols / +0x14 rows at 0x45B36C.., 0x47C23C.. (all in feature-hires_720).
// cmp 0x18/0x1A/0x1B at 0x4204C6, 0x420D6C, 0x47BDFB, 0x47C0FB compare THING TYPES, not tiles.
@ 0x4713E8 int __watcall MenuScreenHandler(int msg, ...); // ESC menu over a mission AND main-menu dialogs; 0x1401 redraws the
//   mission (0x41913C, 0x4216FC, 0x41A4C0) when 0x4B6E84 is 5/6, then its dialog (0x46D014, 0x46E094), Flip, twice.
@ 0x4216FC void BlitMapView(void);                   // = BuildMinimap (locks g_lpMinimapSurface); not called per frame by DrawGameFrame
@ 0x41A4C0 void DrawGameFrame(void);                   // end state (0x4B26CA): only DrawImage(0x80040013/1B + race, 0, 0, 0)
@ 0x41191C void __watcall SetPaletteResource(int res /*eax*/);
$ 0x4B26CA short g_endScreen                          // set by InGameFrame after result words 0x4B274E/0x4B2750(lost)/0x4B2752
@ 0x43FDDC void __watcall PrintResourceText(int res /*eax*/); // glyphs via DrawImage at text cursor 0x556980/0x556982
// All glyph printers (0x43FD80, 0x43FDDC, 0x43FE50, 0x43FEB8) draw with DrawImage at the stored text cursor.

// ---------------------------------------------------------------- scroll / centring / timing (2026-09-27 pm)
@ 0x418764 void ScrollMapUp(void);  @ 0x4189D4 void ScrollMapDown(void);  @ 0x418C58 void ScrollMapLeft(void);  @ 0x418EBC void ScrollMapRight(void);
// incremental redraw after a scroll; built for the 518x453 view (strip start 0x418A62 / 0x418F4A etc.). The mod forces the
// full redraw instead: $ 0x4B6E94 int g_displayHack (registry "DisplayHack"; 1 = RedrawMapSurface(1) on every scroll).
@ 0x42196C void __watcall CentreViewOnThing(int id /*eax*/);  // half view 11x10, clamps 170/172 (also inline copy at 0x4474B0)
//   callers: portrait click 0x41CF7F (single selection only), group recall 0x47AEB1, 0x41AEBD
@ 0x44CA14 void DrawCommandPopup(void);  // skipped when unit col - view col > 0x15 (0x44CA47); side by col >= 12 (0x44CA4F, 0x44FF4A)
@ 0x48DD4C void ModalDialogHover(void);  // GetMousePos at 0x48DD54; called from DrawModalDialog 0x48D9C0 and InGameMouseTick 0x41AF1A
$ 0x4C29EC DWORD g_clockMs               // advanced by timer thread 0x410B00: Sleep(20); += 20; then music stream + cursor anim
// Every screen loop steps a frame when clock >= g_nextFrame (0x4B2744), then g_nextFrame = now + 0x3E (62). Mission:
// InGameFrame 0x41BB84 -> DrawGameFrame, Flip at 0x41BD7B, sim step per frame. 20 ms clock steps => 80 ms frames (12.5 fps).
@ 0x41595C void __watcall SetGameSpeed(int speed1to6 /*eax*/); // tick-duration tables 0x4B26A0/0x4B26B2 = base(0x4B268A)/speed

// ---------------------------------------------------------------- canvas / camera (2026-09-27 evening)
$ 0x4C2B30 IDirectDrawSurface* g_backBuffer   // every draw primitive (Lock/Blt) targets it; the mod points it at a 1424x864 canvas
$ 0x4C2120 int g_clip[4]                      // {left, right, top, bottom} inclusive; 0x4C2130 = bounds; SetVideoMode sets both to the mode
@ 0x414588 int Flip(void);                    // draws the cursor into g_backBuffer (0x413E30), WaitForVerticalBlank + Flip(DDFLIP_WAIT) (0x4145FC)
@ 0x413E30 void __watcall DrawCursor(IDirectDrawSurface* s /*eax*/, int buffer /*edx*/); // reads mouse x/y at 0x413E73/0x413E62
// Mouse globals 0x4C2C25/27 are only written by the window procedure (0x4151E2) from lParam.
// Edge scroll in InGameMouseTick: x > 0x27E (0x41B044), x < 1 (0x41B0A0), y > 0x1DE (0x41B104), y < 1 (0x41B161).
// Main loop 0x41571C: no message -> handler(0x1401) (mission: InGameFrame 0x41BB84, ~3M calls/s idle); draw ~1 ms per frame.

// ---------------------------------------------------------------- command panel / grid (2026-09-27)
// Panel levels at 0x4B60F4, 14 bytes: +0 page, +2 mask, +6 highlighted, +8 count; depth 0x4B6168, active 0x4B6156.
// Icons: DrawImageOffset 0x413A98(0x800100CA, 2*(pageFirstButton[page] 0x4B6190 + bit) + hl, x-0x136, y-0xE6).
// Labels: RES.000 index raceLabelBase[race] 0x4B6158 + pageFirstButton[page] + bit (0x44CD82 -> SetStatusText).
// Key dispatcher 0x47B788: depth ? PanelHotkey : (own first selected -> OpenCommandPopup; panelMode=2; PanelHotkey;
//   no match -> panelMode=1, ResetPanel). PanelHotkey 0x47AF28 -> PanelPressButton 0x44FF1C(bit, slot) -> page
//   handler table 0x4B6174[page](bit, owner) ; then PopupClick 0x41D0B8(0).
// OpenCommandPopup copies ext+0x15 -> +0x28 (units, 0x4EA7A0/0x78) and ext+0x14 -> +0x18 (cat 3, 0x28) per selected thing.
// DrawImage 0x413A7C(res, frame, x, y): frames carry a stored offset (column art frame 0 = (518,0)); 0x413A98 = flag -1.
// Timer thread body (0x410B17..): music stream (buffer 0x4C2D74, races with its release: fault 0x410AA3) + cursor anim.
@ 0x42B3C0 int __watcall GetUnitRace(int id /*eax*/);           // race 0..3 of a unit TYPE (0xC..0x33 -> (t-12)/10, specials table), -1 creatures
@ 0x416EE4 void __watcall DrawSpecialPortrait(int id /*eax*/);  // types 0x34..0x46, table 0x416E98, at (0x213,0x12E)
@ 0x42E2E4 int __watcall HpBarWidth(int id /*eax*/);            // ScaleBar(U+0x10 word, TT+1 word)
@ 0x42E334 int __watcall ManaBarWidth(int id /*eax*/);          // ScaleBar(U+0x12 byte, TT+0x12 byte)
@ 0x429998 int __watcall ScaleBar(int v /*eax*/, int max /*edx*/); // 0..95 px
@ 0x427864 int __watcall FindTimedEffect(int id /*eax*/, char kind /*dl*/); // index into 9-byte recs 0x5425B8 (1..*0x4B3AE4), 0 none
@ 0x44A720 int __watcall BuildingHpBarWidth(int id /*eax*/);
@ 0x43F90C int __watcall Cat3HpBarWidth(int id /*eax*/);
@ 0x44A6D0 int __watcall GetBuildingRace(int id /*eax*/);
@ 0x440034 void __watcall DrawTextCentredRes(int x /*eax*/, int y /*edx*/, int width /*ebx*/, int resId /*ecx*/);
@ 0x440074 void __watcall DrawTextCentred(int x /*eax*/, int y /*edx*/, int width /*ebx*/, const char* s /*ecx*/);
@ 0x41094C void* __watcall GetResPtr(int resId /*eax*/);        // RES file (id>>16)&0x7FFF, entry id&0xFFFF; table 0x4C2A00 stride 20
@ 0x43FF30 int __watcall TextWidthRes(int resId /*eax*/);
@ 0x43FF7C int __watcall TextWidth(const char* s /*eax*/);
@ 0x47AD1C void __watcall RemoveThingFromGroups(int id /*eax*/);
@ 0x47ADAC void __watcall AssignGroup(int n /*eax 0..9*/);     // selection (cat != 8) -> g_groups[n], 0-terminated
@ 0x47AE00 void __watcall RecallGroup(int n /*eax 0..9*/);     // '0' -> 0; stops at first member with flags&0x20
$ 0x4B99E0 ushort g_groups[10][18]                  // stride 0x24, 0-terminated id lists
$ 0x4EA7A0 UnitExt g_unitExt[]                      // stride 0x78 (cat 2 and 8): +0x10 u16 hp, +0x12 u8 mana, +0x15 state, +0x1E stealth, +0x23 bio-upgrade bits, +0x2B char name[] (custom)
$ 0x4B3C38 uchar g_unitTypeTable[][0x20]           // +0 slot/class (1..10 race units, 11 creature, 12/13 special), +1 u16 maxHp, +0x12 u8 maxMana
$ 0x4B33D1 uchar g_creaturePortraitFrame[0x0C]     // frame in 0x800100DD for types < 0xC
$ 0x416E98 void* g_specialPortraitJump[19]         // types 0x34..0x46
$ 0x5425B8 TimedEffect g_timedEffects[]            // 9 bytes: +0 u16 id, +3 u16 cur, +5 u16 total, +7 u8 kind; count *(int*)0x4B3AE4
$ 0x4B2675 char g_showEnemyDetails                 // nonzero: foreign units get full portrait/name (unidentified flag)
$ 0x544192 ushort g_clanLeaderId                   // + (clan-1)*0x101; name string at 0x5440F8 + (clan-1)*0x101
// (2026-09-27) 0x42B3C0 returns the race of a unit type (not a rank). Unit names: RES.000 0x15A + type.
// GDI palette: 0x414C78 / 0x48652F build a 236-entry logical palette (entries 10..245) and RealizePalette it;
//   cnc-ddraw maps it to DirectDraw 0..235 -> fix-gdi-palette.wwp skips the realize calls.
// Draw-order buckets: key base 931 (= 1023 - 22*4 - 4); DrawListInsert 0x4443A0 replaced by a bounds-checked copy.
// Minimap view rect: DrawRectOutline call at 0x4185BC (stack left, top, right, bottom, 1, 0x4D).
// Music streaming race (2026-09-27): timer thread 0x410B20..0x410B5D tests/refills stream buffer 0x4C2D74;
//   PlayMusic 0x4114B8 / StopMusic 0x41165C release it on the main thread -> faults 0x4109BE / 0x410AA3.
//   music.cpp serialises both with a critical section.

// ---- Save / load (2026-09-27): file dialogs replaced by mod/src/saveload.cpp (in-game screen)
@ 0x448990 int SaveGame(void);                        // network (0x4B66EC==1) -> 0x444680 + 0x488278, else 0x4445B0; fopen(0x4B48E2,"wb")
@ 0x448C30 int LoadGame(void);                        // network -> 0x444820, else 0x444750; fopen(0x4B49E6,"rb"), clear+flip x2, loaders
@ 0x4445B0 BOOL SaveCampaignDialog(void);             // GetSaveFileNameA into 0x4B48E2, title RES.000 0x294, *.SAV
@ 0x444680 BOOL SaveNetworkDialog(void);              // GetSaveFileNameA into 0x4B48E2, title 0x296, *.NSV
@ 0x444750 BOOL LoadCampaignDialog(void);             // GetOpenFileNameA into 0x4B49E6, title 0x295, *.SAV
@ 0x444820 BOOL LoadNetworkDialog(void);              // GetOpenFileNameA into 0x4B49E6, title 0x297, *.NSV
@ 0x410274 void FlipToGDISurface(void);               // before the dialogs
@ 0x414AA4 HBITMAP CaptureScreenBitmap(int flag, int x, int y, int w, int h /*stack*/); // GDI copy for WM_PAINT while a dialog is up
@ 0x46EB58 void EnterDialogScreen(void);              // ESC-menu callers: capture bitmap -> 0x5E1D8C, flag 0x4B76BB=1, HideCursor
@ 0x46EBA4 void LeaveDialogScreen(int menu, int loaded); // DeleteObject, flag 0, ShowCursor, redraw for menu 6
$ 0x4B48E2 char g_savePath[0x104]
$ 0x4B49E6 char g_loadPath[0x104]
$ 0x4B4AF4 const char* g_saveDir                      // "SAVES\"
$ 0x4B4AEC const char* g_extSav                       // "SAV" (0x4B4AF0 "NSV")
$ 0x4B76BB char g_dialogBitmapShown                   // ESC menu 0x4713E8: WM_PAINT blits 0x5E1D8C, WM_SETCURSOR system cursor
$ 0x4B74BC char g_dialogBitmapShown2                  // main-menu load/save screen (bitmap 0x5E1CFC)
// Callers: main menu load 0x468A97 (handler 0x468BF0), 0x46BB90/0x46BC1F, ESC menu 0x46F688/0x46F71A.

// ---------------------------------------------------------------- fog / terrain / palette / options (2026-09-27, static, fx2)
// CORRECTION: 0x605DD0 is NOT fog. It is the pathfinding region-label map (flood fill of connected land/water areas,
//   built at mission start by 0x481778 -> 0x47FB80 / 0x47F9AC, merged by 0x480850/0x480880, read by GetRegion 0x47F980).
//   PresentMapView's 0x4B33FC mode (FillRect colour = label) is a debug region view, not fog.
$ 0x605DD0 uchar g_regionMap[192][192]            // small-tile region ids (pathfinder), 0 = unlabelled
@ 0x47F980 uchar __watcall GetRegion(int x /*eax*/, int y /*edx*/);
@ 0x481778 void BuildRegionMap(void);
$ 0x4B33FC int g_debugRegionView                  // PresentMapView: nonzero -> 22x19 FillRects coloured by region id
// Fog bitmaps: 96x96 BIG tiles (48 px), 96 bits per row = 3 dwords, bit for x = 0x80000000 >> (x&31) (table 0x4B3BAC),
//   dword index = y*3 + x/32. Low-mask table 0x4B3B28[n] = (1<<n)-1.
$ 0x545190 uint g_fogNotVisible[96][3]            // player clan: bit SET = not currently seen. All ones every 16 frames
                                                  //   (0x42A42C from InGameFrame unless g_revealMap), then vision spans clear bits.
$ 0x556410 uint g_explored[96][3]                 // bit SET = explored; only drives the minimap (BuildMinimap 0x4216FC, RevealSpan)
$ 0x544D10 uint g_forestMask[96][3]               // big tiles with tree cover (built 0x42A324 at load); vision mode 1/2 below
$ 0x545610 uint g_detect[8][5][96][3]             // stride 0x1680 per clan, 0x480 per level: stealth-detection planes, bit clear = detected
@ 0x42A494 uint __watcall IsFogged(int bigX /*eax*/, int bigY /*edx*/);   // g_fogNotVisible bit
@ 0x42A4DC void __watcall ClearFogBit(int bigX /*eax*/, int bigY /*edx*/);
@ 0x42A0D8 uint __watcall IsExplored(int bigX /*eax*/, int bigY /*edx*/);  // g_explored bit
@ 0x42A1BC void ClearExplored(void);               // memset g_explored 0 (mission load)
@ 0x42A42C void FogAllHidden(void);                // g_fogNotVisible = all ones
@ 0x42A450 void RevealWholeMap(void);              // RevealSpan every row, then g_fogNotVisible = 0 (load when g_revealMap)
@ 0x42A578 void __watcall RevealSpan(int bigY /*eax*/, int x0 /*edx*/, int x1 /*ebx*/, int mode /*ecx: 0 all, 1 only forest, 2 only non-forest*/);
//   per big tile: if !explored -> write minimap pixel (colour 0x4B3C2D[baseSet]) + set explored bit; tree classes 2/3/8/9 -> +2
//   (seen-cut) + RedrawMapTile; then clear g_fogNotVisible bits x0..x1 (masked by g_forestMask per mode).
@ 0x439E78 void UnitVisionRect(void);              // rounded rect (first/last row trimmed 1) of RevealSpan + DetectSpan; mode from 8 neighbour tree tiles
@ 0x455BE0 void BuildingVisionRect(void);
@ 0x42AD50 void __watcall DetectSpan(void);        // clears g_detect bits (stealth detection), same row/mode scheme
@ 0x42AA9C uint __watcall TileDetectedForClan(int clan /*eax*/, int stealth /*edx*/, int bigX /*ebx*/, int bigY /*ecx*/);
//   (was "TileVisibleToClan": it tests the stealth-detection planes 0x545610, not the fog bitmap; stealth 0 -> always 1)
@ 0x419B84 void DrawFogOverlay(void);              // after sprites: for each fogged big tile DrawImage 0x80010019 (1 tile),
//   0x8001001A (2 in a row), 0x8001001B (3 in a row) frame 0 at x=(bx-bigStartX)*48-0x93-(smallStartX&1)*24, y=(by-bigStartY)*48-0x58-(smallStartY&1)*24, + frame offset (x+135.., y+77..).
//   Sprites are 71x71 circles (117x71, 165x73 merged) of a 1:1 checkerboard of palette index 112 (=RGB 0,0,0) over index 0
//   (transparent) -> 50% dither, round because the circles overlap. No remap/darkening table exists.
$ 0x4B33D0 char g_fogOverlayEnabled                // initialised 1, never written: patch to 0 to suppress DrawFogOverlay
$ 0x4B266D char g_revealMap                        // cheat/debug: no 16-frame fog reset, whole map revealed at load
// ---- terrain
$ 0x4D5B78 ushort g_terrainMap[96][96]            // save field "terrain map" (big 48 px tiles). w&3 = base frame,
//   (w>>2)&7 -> base set g_baseSetArray[...] (res 0x80010003+n, 4 frames 48x48, opaque blit), (w>>6)&0x3F = border frame,
//   w>>12 < 8 -> border set g_borderSetArray[w>>12] (res 0x8001000D+n, 64 frames, colour-keyed), >=8 none.
$ 0x4DA4E0 uchar g_baseSetArray[8]                // save field "base set array"
$ 0x4DA4F0 uchar g_borderSetArray[8]              // save field "border set array"
// g_tileThings 0x4F1F28 ("thing map") bits 11..15 = small-tile object class (w>>11):
//   0,1 trees (doodad frames 0/1) - default click order 7; 2,3 tree cut while unseen (drawn as tree while fogged, stump when seen);
//   4,5 stump (frame 2); 6,7 resource deposit (frames 6/7) - default click order 8; 8,9 deposit exhausted while unseen;
//   10,11 exhausted (frame 8); 12..26 doodads (frames 12..26); 28 (0xE000) impassable/void (black on minimap, no region);
//   29 (0xE800) special (unconfirmed); 30 (0xF000) water; 31 (0xF800) open ground.
//   CORRECTION to the IssueDefaultOrder note: {0,0x800}/{0x3000,0x3800} are tree/deposit tiles, not land/water.
$ 0x4B2768 int g_doodadRes                         // 0x800100B2/B3/B4 by climate (set in 0x41F64C), 27 frames
@ 0x419880 void DrawTileDoodads(void);             // per small tile in view: class -> frame of g_doodadRes into map surface
@ 0x4194C8 void __watcall QueueDoodadsNearThing(int id /*ax*/);  // re-inserts nearby trees into the depth-sorted draw list
@ 0x4216FC void BuildMinimap(void);                // (not BlitMapView) explored ? 0x4B3420[baseSet] : 0; class 28 -> 0
@ 0x41F64C void StartMission(void);                // normalises tree classes, sets g_doodadRes, BuildMinimap, RedrawMapSurface(1)
// ---- palette (DirectDraw)
$ 0x4C2C00 IDirectDrawPalette* g_lpPalette
$ 0x4C2140 PALETTEENTRY g_palShadow[256]          // flags 4 (PC_NOCOLLAPSE)
@ 0x411748 void __watcall SetPaletteEntries(const uchar* rgb6 /*eax*/, int first /*edx*/, int count /*ebx*/);
//   6-bit -> <<2, windowed forces 0..9/246..255 from 0x4B25AA/0x4B25D2, fullscreen only 0/255; first call CreatePalette(4)
//   + primary->SetPalette, later g_lpPalette->SetEntries(0,0,256) (0x41186D is the ONLY SetEntries in the exe).
@ 0x4118D4 void __watcall SetPaletteRGBQuad(const RGBQUAD* p /*eax*/);   // video frames (0x412324, 0x4126DD)
// SetPaletteResource 0x41191C callers: 0x80010000 in-game (0x41F805 mission start, ESC-menu resume paths), 0x80010001 menus,
//   0x80040017+race end screen (0x41A88D), 0x8004005D (0x4719D1). No cycling / fades / day-night: the palette only changes
//   on screen transitions. Restore paths re-attach it (SetPalette at 0x410488 and 0x414E3x).
// ---- ESC menu / Change Game Options
$ 0x4B6E84 int g_menuScreen                        // save "currentmenuscreen"; 5/6 = menus over a running mission
$ 0x4B7708 int g_menuDialog                        // sub-dialog: 0 base, 4 sound options, 6 game options, 3/5 others
$ 0x4B772C int g_menuPanelX                        // 0x148 (0x90 when g_menuScreen==5), set by MenuInit 0x47130C
$ 0x4B7730 int g_menuPanelY                        // 0x3B
$ 0x4B7700 MenuItem* g_menuItems                   // linked list; count 0x4B76F8
struct MenuItem { ushort left, top, right, bottom, id; uint clickSound; MenuItem* next; };  // 0x12 bytes; sound = RES.002 id for 0x4111F8
$ 0x4B76F4 int g_menuPressedId                     // set on WM_LBUTTONDOWN by hit test, -1 none
$ 0x5E1D97 char g_menuRedraw                       // 2 = redraw request
$ 0x5E1D7C int g_menuButtonRes                     // button art: frame 2 = radio lit, 3 = unlit, 6 OK/Cancel bar, 7/8 pressed
$ 0x5E1D78 int g_menuPanelRes
@ 0x46CBA0 void __watcall AddMenuItem(int id /*eax*/, int left /*edx*/, int top /*ebx*/, int right /*ecx*/, int bottom, int sound); // ret 8
@ 0x46CB04 int __watcall MenuHitTest(int x /*eax*/, int y /*edx*/);   // id or -1
@ 0x471020 void BuildBaseMenu(void);               // ESC menu buttons; id 0xE = CHANGE GAME OPTIONS (handler 0x46F5B6)
@ 0x46FF30 void BuildGameOptions(void);            // items below, then g_menuDialog = 6 (0x4705A3)
@ 0x46D014 void DrawMenu(void);                    // game options block 0x46D791..0x46DD4C (labels + item loop, unknown ids skipped)
@ 0x46E194 void MenuTick(void);                    // 0x1401: slider drags (value = ((mx-(X+0xC1))*5+0x1C)/0x39), redraw
@ 0x46FB48 int __watcall MenuLButtonDown(int lParam /*eax*/);
@ 0x46EF18 int __watcall MenuLButtonUp(int wParam /*eax*/, int lParam /*edx*/);  // id==pressed && id<0x3F -> jump table 0x46EE1C[id]
@ 0x46E7B8 int __watcall MenuKeyDown(int vk /*eax*/);
@ 0x4682E0 void LoadOptions(void);                 // RegGetOption at startup (+ "DisplayHack" at 0x4684A8)
// Game options rows (labels at (X+0x1F, Y+0x22+0x20*i)); item rects relative to panel X/Y:
//   0 Game Speed           0x2E (X+B1..C1) 0x2F (X+101..111) 0x36 slider (X+C1..10A), y 0x20..0x30 -> 0x4B76B5 (0..5)
//   1 Opponent Difficulty  0x30 0x31 0x37, y 0x40..0x50 -> 0x4B76B6
//   2 Screen Scroll Speed  0x32 0x33 0x38, y 0x60..0x70 -> 0x4B76B7
//   3 Cinematic Subtitles  0x35 OFF (X+DF..EF) 0x34 ON (X+101..111), y 0x80..0x90 -> 0x4B76B8
//   4 Show Health Bars     0x3A OFF 0x39 ON, y 0xA0..0xB0 -> 0x4B76B9 (registry "Enemy Health Bars")
//   5 Show tips at startup 0x3C OFF 0x3B ON, y 0xC0..0xD0 -> 0x48DD1C get / 0x48DDE0 set
//   6 Full screen cinematics 0x3E OFF 0x3D ON, y 0xE0..0xF0 -> 0x4B76BA
//   OK 0x16 (X+2D..63, Y+115..14B), Cancel 0x17 (X+DF..115): both -> 0x46F305: RegSetOption for all six + SetGameSpeed / net order 0x77.
// Registry: HKLM\Software\Strategic Simulations Inc.\War Wind\1.0, REG_DWORD: "Game Speed", "Opponent Difficulty",
//   "Screen Scroll Speed", "Cinematic Subtitles", "Enemy Health Bars", "Full Screen Cinematics", "DisplayHack" (read only).
$ 0x4B76B5 char g_optGameSpeed
$ 0x4B76B6 char g_optDifficulty
$ 0x4B76B9 char g_optEnemyHealthBars

// ---------------------------------------------------------------- map sprite chain (2026-09-27, static; see findings_fx.md "Sprite chain")
// Draw lists: header {+0 bucketCount, +4 nodeCap, +8 nodesUsed, +0xC bucket[] {head,tail}, +0x10 node pool}; 36-byte nodes,
//   node+0 next, node+4 u16 nodeType (1 sprite, 2 unused, 3 box/bar, 4 minimap pixel), node+6.. record R.
// Type-1 record R (= node+6): +0 i16 x, +2 i16 y (node space: screen px of tile top-left - (0x93,0x58)), +4 u32 resId,
//   +8 i16 frame, +0xA u16 flags, +0xC u16 category (Thing+1; 0 = not clickable), +0xE u32 tileX*192+tileY,
//   +0x12 u16 owner clan (never read by the renderer), +0x14 u16 thingId (0 = terrain occluder / effect / fog ghost).
// R+0xA flags: 1 mirror (DrawSprite flags 2), 2 mirror axis 0xA2 (units), 4 axis 0xAE (vehicles), 8 translucent
//   (flags|0x90000000 -> blend table), 0x10 clip bottom y+0x70, 0x20 clip right x+0xAB, 0x40 fog ghost (draw Thing+0xA
//   frame Thing+0x4F-1), 0x80 clip bottom y+0x58, 0x100 clip right x+0x93. Buckets are rendered from high key to low.
$ 0x5425A0 DrawList g_drawList                        // 1024 buckets (0x4E77E8), 2000 nodes (0x4C4238); init 0x41F380, reset each frame 0x41A8BC
$ 0x5424A4 DrawList g_overlayList                     // 5 buckets (0x4C2DC0), 1500 nodes (0x4DA4F8): minimap dots
$ 0x4B48C0 void* g_drawNodeCallback                   // = 0x4227FC (set at 0x41F3EB via 0x444470)
@ 0x4443E0 void __watcall DrawListReset(int buckets /*eax*/, int nodes /*edx*/, DrawList* l /*ebx*/);
@ 0x44442C void __watcall DrawListRender(DrawList* l /*eax*/); // buckets count-1..0, callback(eax=nodeType, edx=node+6)
@ 0x444470 void __watcall SetDrawNodeCallback(void* fn /*eax*/);
@ 0x444478 void __watcall DrawListAddSprite(int resId /*eax*/, int frame /*edx*/, int x /*ebx*/, int y /*ecx*/, int flags, int key, int thingId, int category, int tileIndex, int owner, DrawList* l); // ret 0x1C
@ 0x4444D8 void __watcall DrawListAddBox(int x /*eax*/, int y /*edx*/, int w /*ebx*/, int h /*ecx*/, int color, int barTotalW, int isBar, int key, DrawList* l); // ret 0x14; isBar 0 -> DrawRectOutline, else FillRectClipped bar
@ 0x444570 void __watcall DrawListAddPixel(int x /*eax*/, int y /*edx*/, int color /*ebx*/, int key /*ecx*/, DrawList* l); // ret 4
@ 0x4227FC void __watcall DrawNodeCallback(int nodeType /*eax*/, void* rec /*edx*/); // type 1 -> DrawImage (calls 0x422c01..0x422d39)
@ 0x4226B4 void __watcall RecordSpriteRect(int thingId /*eax*/, int x /*edx*/, int y /*ebx*/); // g_rectList[g_rectListCur^1]: y1=y+0x58, x1=x+0x93, size by category
@ 0x4295A8 void RunThings(void);                       // "RunThing OINK OINK": per thing (list 0x4B26D0, next +0x4D): queue sprites + minimap dots + AI
@ 0x429134 void __watcall QueueThingIfVisible(int category /*eax*/, Thing* t /*edx*/, int id /*ebx*/); // view window rows -4..+0x18, cols -4..+0x1A
@ 0x42048C void __watcall QueueThingSprites(int category /*eax*/, Thing* t /*edx*/, int id /*ebx*/); // main node builder (jump table 0x42046C)
@ 0x4194C8 void __watcall QueueTerrainOccluders(int id /*eax*/); // re-queues foreground terrain (res [0x4B2768]) around the thing, thingId 0
@ 0x422148 void __watcall QueueShotPuffs(Thing* t /*eax*/); // ranged shot: up to 4 puffs RES.001 #266+race (#270 vehicles), key 1
@ 0x4201B8 void QueueStatusIcons(void);                // #257/#271/#272 marker icons above units (key y-4 etc.)
@ 0x421408 void QueueRallyMarker(void);                // #274 at 0x5440F4
@ 0x47BB34 void QueueFogGhosts(void);                  // last-seen objects from grid 0x4B9C1C via 0x47BDB8
@ 0x4293EC void UpdateVision(void);                    // per-thing sight radius (0x43A4C8 etc.), no drawing
@ 0x419B84 void DrawShroud(void);                      // RES.001 #25..#27 over unexplored 48px tiles, after the sprite pass
@ 0x433074 void __watcall StartShot(void);              // shooter Thing+0x1D/+0x21 = (target-shooter)/4 px, +0x25 = 1
@ 0x424264 int __watcall AllocThing(int category /*al*/); // links at head 0x4B26D0 (next +0x4D, prev +0x4B)
@ 0x426B34 int __watcall SpawnThing(int type /*al*/, int category /*dl*/, int param /*bx -> Thing+0x19*/);
@ 0x426CFC int __watcall SpawnItem(int type /*al*/, ...); // category 6, res #226, frame table 0x4B3B08
@ 0x42E1DC int __watcall UnitSpriteX(int id /*eax*/, int tileX /*dl*/); // (tx-scrollX)*24 + move offset - 0x93
@ 0x42E24C int __watcall UnitSpriteY(int id /*eax*/, int tileY /*bl*/); // (ty-scrollY)*24 + move offset - 0x58
@ 0x412FE0 int __watcall SurfaceBlt(IDirectDrawSurface* src /*eax*/, RECT* srcR /*edx*/, RECT* dstR /*ecx*/, IDirectDrawSurface* dst /*ebx*/, int flags, int useKey); // ret 8
@ 0x412E60 void __watcall EnsureColorKey(IDirectDrawSurface* s /*eax*/, int key /*edx*/); // GetColorKey/SetColorKey(DDCKEY_SRCBLT)
@ 0x412EA8 void __watcall LoadBlendTable(int resId /*eax*/); // 64K table -> [0x4C2B24] (mission: 0x80010002); dst = T[src<<8|dst]
@ 0x411730 void __watcall SetMirrorAxis(short ax /*ax*/);   // [0x4B25A8]; initial -1, sticky
$ 0x4C2B24 uchar* g_blendTable
$ 0x4B25A8 short g_mirrorAxis
$ 0x4B26D0 ushort g_thingListHead
// DrawSprite stack args are (flags, dst, saveUnder, colorKey): colorKey >= 0 -> EnsureColorKey + keyed blit, -1 -> opaque.
//   No palette remap exists in the blit path. 0x413A98 = DrawImage with colorKey -1 (opaque), not an offset variant.
// SurfaceBlt flags: 0 BltFast; else Blt DDBLT_DDFX with dwDDFX = flags (2 = DDBLTFX_MIRRORLEFTRIGHT); bit31 -> software
//   loops (0x44A56C keyed, 0x44A5FC keyed mirrored, 0x44A4A0/0x44A4BC opaque), bit28 (+table) -> 0x44A50C/0x44A53C blend.
// Thing extra fields: +0x08 u8 anim frame, +0x09 u8 move step, +0x0A u32 resId, +0x11 bit6 shield ring #273,
//   +0x14 u32 timed-status bits, +0x19 u16 spawn param, +0x1C u8 hit-effect counter (1-4 #262, 5-8 #258, 9-12 #259),
//   +0x1D i32 shot step x, +0x21 i32 shot step y, +0x25 u8 shot phase 1..7, +0x4B/+0x4D u16 thing list prev/next,
//   +0x4F u16 last-seen frame+1. Categories: 4 scaffold #175, 5 explosions/debris (#181..183, #264/265), 6 items #226,
//   7 spell effects (#260 etc., key 4), 8 creatures/pods.

// ---- interpolation replay (2026-09-28)
// DrawGameFrame with g_menuScreen 6/7 (0x41A524..0x41A58F) re-renders the previous tick's g_drawList over PresentMapView
// without RunThings: re-rendering a list is safe. The list lives until DrawListReset at 0x41A8BC of the next tick.
// Rect lists: DrawGameFrame 0x41A4CC..0x41A508 zeroes g_rectList[cur], count[cur] = 0, then xor byte [0x4B33AC],1;
//   RecordSpriteRect appends to list[cur^1] (no bound: 256 records of 10 bytes per list); FindThingAtScreenPos reads list[cur].
// DrawListAddBox record (node+6): +0 x, +2 y, +4 h (ebx), +6 w (ecx), +8 colour, +0xA barW, +0xC isBar (map-view px).
//   CORRECTION: ebx is the height and ecx the width. isBar: black (barW+2)x(h+2) at (x-1,y-1), then w x h colour.
@ 0x4444D8 void __watcall DrawListAddBox(int x /*eax*/, int y /*edx*/, int h /*ebx*/, int w /*ecx*/, int color, int barTotalW, int isBar, int key, DrawList* l); // ret 0x14
@ 0x444528 void __watcall DrawListAddLine(void);        // node type 2 (callback only printfs "Draw Line"); no callers
// Per-thing node order inside QueueThingSprites (key K = 0x258 - dy*22 - dx): unit health bar (type 3, x+0x9A, y+0x79, h 3,
//   barW 20, key 6, only when not moving), selection box (type 3, x+0x93|0x90, y+0x58, key K), QueueShotPuffs (key 1,
//   thingId 0), shield ring #273, main sprite, QueueStatusIcons (thingId = id), hit effects (key K-1); buildings add roof
//   frame+1 (key 5) and smoke #263 (key 4). Everything with thingId 0 and outside QTS (occluders, rally, ghosts) is static.
// QueueThingSprites also advances building smoke 0x503F2C[ext*7] every 6 frames; QueueThingIfVisible writes Thing+0x4F.
@ 0x423200 short* __watcall MoveOffset(int id /*ax*/, int dir /*dl*/, int step /*bl*/); // -> {i16 dx, i16 dy}; table by category/speed
$ 0x4B3590 short g_moveOfs3px[8][7][2]               // speed 4,5 (8 steps/tile); 0x4B3670 4 px (6,7 and cat 1,4..7), 0x4B3750 6 px (8,9)
$ 0x4B3830 short g_moveOfs8px[8][7][2]               // speed 10..19, 2 steps/tile (+8 then +16); dir 2 step 2 typo (0,-8) unreachable
$ 0x4B3910 short g_moveOfs12px[8][7][2]              // cat 3 vehicles, cat 8, other speeds; 2 steps/tile
$ 0x4B47EC uchar g_stepsPerTile[20]                   // [ext 0x4EA7F9 speed]; AI: step = (step+1) % n (0x438B1E, 0x43AD64)
// UnitSpriteX/Y (0x42E1DC/0x42E24C) are pure but clobber ebx and edx. Offset applied only while Thing+0x10 bit1 (moving) && step.
@ 0x43C9A0 int __watcall VehicleSpriteX(int id /*ax*/, int tileX /*dl*/); // dir = ext 0x4E9817[ext*0x28], -0x93
@ 0x43CA1C int __watcall VehicleSpriteY(int id /*ax*/, int tileY /*bl*/); // -0x58; vehicle step = (step+1)%2 (0x43E5FC)
// RunThings queues each thing (QueueThingIfVisible) BEFORE its AI (0x4391E8 units, 0x43E314/0x43E858 vehicles, 0x45DC20
//   creatures, 0x44A808 buildings): after DrawGameFrame the Thing state already holds the position tick F+1 will draw.
@ 0x488BC0 int GetGameTick(void);                     // [0x61A1EC]
$ 0x61A1EC int g_gameTick
@ 0x429AE0 void AiScratchTick(void);                  // DrawGameFrame 0x41A50F: when GetGameTick>>8 changes (cache 0x556890) -> 0x47F5CC(1)
@ 0x47F5CC void __watcall ClearClanAiScratch(int clear /*eax*/); // clear: zero 0x2400 at 0x580A80+0xBE31+k*0x7628 (8 clans), 0x4BA488=0
@ 0x42AA64 void ResetDetectPlanes(void);              // every 16 frames: g_detect 0x545610 (0xB400 bytes) = 0xFF, once per 0x4B3C2C
$ 0x4B3C2C char g_detectPlanesReset
@ 0x451768 void __watcall RegenUnits(int phase /*eax = frame % 420*/); // every 28 frames: cat-2 ext+0x12 +1 (cap), ext+0x10 hp +1 (type 6 / ext flag 8)
@ 0x456EA4 int GetTickFlagA(void);                    // [0x5568A0]: 0 at RunThings start, 1 from unit AI 0x4392C2 (eax ignored)
@ 0x456EAC int GetTickFlagB(void);                    // [0x5440F0]: set by 0x426F58 at RunThings start; change -> 0x4B264C |= 2
@ 0x413EB8 void SetCursorImage(void);                 // eax = cursor res...; writes cursor state 0x4C2C2D/0x4C2D5C/0x4C2D64..
@ 0x41F380 void DrawListsInit(void);                  // pools, callback 0x4227FC, blend table 0x80010002, rect lists 0, cur 0, g_firstFrameRedraw 1
$ 0x4B33C0 char g_firstFrameRedraw                    // DrawGameFrame 0x41A985: RedrawMapSurface(1) + PresentMapView once, then 0
$ 0x4B265A ushort g_flashTarget                       // attacked thing; its selection box blinks while (g_flashTimer/2)%2 == 0
$ 0x4B265C ushort g_flashTimer                        // set 0x14 by 0x42838B/0x428C8F; -- per DrawGameFrame (0x41A9EB)
$ 0x4B33B8 int g_moveMarkerAnim                       // 1..8: DrawImage(#192 0x800100C0, n-1) at 0x41AAB4; ++ per tick, 9 -> 0
$ 0x5424B8 short g_moveMarkerX                        // small tile (set with g_moveMarkerAnim = 1 at 0x4283A6/0x4283C0/0x428CAA)
$ 0x5424BA short g_moveMarkerY
$ 0x5440D9 uchar g_pulsePhase                         // (n+1)%14 per DrawGameFrame (0x41AC29); colour 8 + (n<7 ? n : 14-n)
$ 0x5440DD char g_framePending                        // InGameFrame: DrawGameFrame when 0 (-> 1); Flip at 0x41BD7B clears it
$ 0x4B267C char g_netPaused                           // MP: another clan opened its menu (0x41BAEF/0x41BB4B); InGameFrame skips DrawGameFrame
$ 0x4B266F char g_runThingsEnabled                    // 1 (debug reset block 0x422047); 0 -> DrawGameFrame skips RunThings
$ 0x4B2671 char g_dbgHideSprites                      // DrawNodeCallback returns at once
$ 0x4B2672 char g_dbgHalfRate                         // DrawSprite / DrawPanels skip on odd g_frameCounter
$ 0x4B2668 char g_dbgShowHitRects                     // 0x419D80
$ 0x4B2679 char g_dbgShowOccupied                     // 0x419E3C
@ 0x419D80 void DebugQueueHitRects(void);             // type-3 outline per live g_rectList record, key 5
@ 0x419E3C void DebugQueueOccupiedTiles(void);        // 24x24 bar per occupied small tile, key 0x3FF
@ 0x419FE0 void DrawOrderLines(void);                 // waypoints (g_waypointCount>1) or path preview (0x4B276D): DrawLine between tile
//   centres under the map clip; preview writes waypoint[0] 0x4E97E8 = unit tile. Pure otherwise, re-runnable.
@ 0x41D948 int __watcall CheckSelectionForPath(int announce /*eax*/); // announce -> AddMessage errors; pure with 0
@ 0x41DA90 int __watcall WaypointIsFar(int tile /*eax*/, int idx /*edx*/); // distance 0x472180 > 0x32 (line colour +8)
@ 0x419F3C void __watcall MouseToTile(short* out /*eax*/); // minimap 2*(m-0x212/0x88) or map m/24+smallStart; 0xFF = none
@ 0x4183CC void __watcall AddMessage(const char* text /*eax*/, int frames /*edx*/); // 4 map-view lines, scrolls when full
$ 0x4DA378 char g_msgText[4][0x5A]                    // DrawPanels 0x418340: live lines at (0x1E, 0xF+0x14*n) in the MAP VIEW
$ 0x4DA4E8 ushort g_msgTimer[4]                       // -- per DrawPanels call; 0 -> lines shift up
@ 0x43FCFC int __watcall SelectFont(int res /*eax*/); // returns previous; in-game font set at 0x41F8C5
$ 0x556980 short g_textX                              // SetTextPos 0x43FD2C
$ 0x556982 short g_textY
$ 0x556984 int g_fontRes
$ 0x556988 uchar g_fontSpacing
@ 0x43FD80 void __watcall PrintChar(int c /*al*/);
@ 0x43FE50 void __watcall PrintString(const char* s /*eax*/);
$ 0x4B2764 int g_chatInput                            // nonzero: DrawGameFrame prints g_chatText + '_' at (0x19, 0x1AE)
$ 0x544074 char g_chatText[]
$ 0x4C2DB0 RECT g_cursorRect                          // l,t,r,b (0x4C2DB0..BC) set by InGameMouseTick; outline at 0x41ABD3
$ 0x544072 ushort g_placementColor                    // 0x4D ok, 0xF / 0xA blocked
@ 0x49A7CC int rand(void);                            // Watcom LCG, shared with the simulation (srand 0x49A7F0 per tick in MP)
// ---- smooth motion (mod/src/motion.cpp, 2026-09-28, sandbox-verified)
// Replay per 60 Hz present (flips inside InGameFrame only): clip = tick clip, PresentMapView, DrawListRender(g_drawList)
//   with g_drawNodeCallback swapped for a wrapper that offsets each owner's nodes, move marker DrawImage (recorded at
//   0x41AAB4), DrawShroud, then the overlay = canvas diff between the DrawOrderLines call (0x41AAE1) and DrawGameFrame's end.
// Owners: QueueThingSprites detour records the pool index range each thing queued. Anchors: type-1 nodes whose thingId
//   == owner, matched between consecutive ticks by resId (world px); offsets = (prev - cur) * (1 - progress),
//   progress = (clock - drawClock) / (g_nextFrame - drawClock). |step| > 32 px is a jump, not motion.
// Live click rects rebuilt by the replay: k = cur; cur ^= 1; count[k] = 0; render; cur ^= 1 (FindThing reads list[cur]).
// RecordSpriteRect append bound (256 records per list) patched at 0x422784 (hires.cpp).
// Messages g_msgText are drawn at map-view (0x1E, 0xF + 0x14n): inside the hi-res canvas margin, i.e. off-screen at rest.
// ---- widescreen menus (mod/src/widemenu.cpp, 2026-09-28, sandbox-verified)
// Main menu handler 0x468BF0 -> SetVideoMode 960x540 (MenuScreenHandler keeps it); a 1024x540 "menu canvas" is the
// game's back buffer in its own coordinates; each Flip composes: screen = surround (runtime: RenderArt + mirror/shade
// + DrawPanel9 #201) + canvas at the screen origin (main/race (16,30), end screen (-56,30), others centred (160,30)),
// canvas pixels outside 640x480 composed where nonzero (cleared before every DrawMenu). Mouse: screen - origin.
@ 0x4686B8 void MainMenuDraw(void);                 // DrawImage(tablet 0x80040005) + caption (status line) at y 0x1D1
@ 0x4685F0 int __watcall MainMenuHitTest(int x /*ax*/, int y /*dx*/); // hotspot table 0x4B6EA6 (9 x {l,t,r,b,id,text})
@ 0x468638 void __watcall MainMenuHover(int x, int y);  // SetStatusText(0x8000025E + text) for the hotspot under (x, y)
@ 0x470DF0 void BuildRaceMenu(void);                // race options items (AddMenuItem); MenuInit 0x47130C sets panel X 0x148 first
@ 0x46CC4C void DrawEndTally(void);                 // victory/defeat title (RES.000 0x415/0x416) + result lines, race big font
@ 0x46CC40 void __watcall SetMenuPanel(DWORD art /*eax*/, int x /*edx*/); // [0x5E1D78] = art, [0x4B7728] = x
@ 0x46DD6F /* DrawMenu, g_menuScreen 6: DrawPanel9(art, [0x4B7728], 0x64, 12, 18) then labels until 0x46DDFE */
// ---- briefing (2026-09-28, static)
// Handler 0x46C06C (g_menuScreen = 4). Page by g_heroSelection: 0/1 -> MAP page 0x46B23C (+ Hall of Heroes),
// >= 2 -> TEXT page 0x46ACD4 (+ Yes/No confirm). (ui_menus/plan.md had the two page functions swapped.)
// States: 0 map + Hall browser (m 1..6) / map only (m 0); 1 pick heroes for next scenario (m 1..6, max g_hallMax);
//   2 briefing text; 3 start (confirm "Did you select units?" RES.000 #0 when 0<m<7); -1 transient -> race menu.
// OK (id 2) state++ (1 skipped when m==0 or g_hallMax==0); Back (id 6) state-- (built-in: state 1 -> race menu).
// Text = sprintf(RES.000[0x49B+20r+2m], clanName 0x54410D+(clan-1)*0x101) + "\x01\x01" + goal RES.000[0x49C+20r+2m];
//   m 0..6 campaign, 7..9 built-in scenarios (0x4B66EC==2, no map/labels/hall). ESC "Review goal" 0x46CE90 same id.
@ 0x46C06C int __watcall BriefingScreenHandler(int msg /*eax*/, int wParam /*edx*/, int lParam /*ebx*/);
@ 0x46B4FC void BriefingTick(void);                 // 0x1401: draw page when !g_briefDrawn; clock>=g_nextFrame -> 0x46B464, Flip, +62ms
@ 0x46B22C void BriefingDrawPage(void);             // state >= 2 ? 0x46ACD4 : 0x46B23C
@ 0x46B23C void BriefingDrawMapPage(void);          // full redraw every frame: bg #0+race f0, 0x46A5DC, f17 (m==0), f40 (st0&&m), f41 (st!=0),
                                                    //   pressed frame, 8 hall portraits (#211+race) + outline 0x4E, hint popup | hero info + status (tail 0x46AA59)
@ 0x46ACD4 void BriefingDrawTextPage(void);         // confirm-dialog branch | full redraw while g_briefRedraw>0 | status strip (clip y310..329) always
@ 0x46A5DC void BriefingDrawMissionMap(void);       // DrawImage(0x80040000+r, m+1) (600x288 @20,20, marker baked in) + 3 labels, font 0x800100F9
@ 0x46A674 void __watcall BriefingDrawHeroInfo(int editable /*eax*/, Thing* t /*edx*/, UnitExt* e /*ebx*/, Thing* item1 /*ecx*/, Thing* item2 /*stack*/); // ret 4; lower well
@ 0x46ABC8 int BriefingCountLines(void);            // builds text, SelectFont(0x800100E7+4r), PrintTextBlock(2, 26, 26, 20, 587, buf)
@ 0x4401A8 int __watcall PrintTextWrapped(int firstLine /*eax*/, int lastOffset /*edx*/, int mode /*ebx: 0 left, 1 centred, else measure*/, int x /*ecx*/, int y, int lineHeight, int width, const char* text); // ret 0x10; prints lines first..first+lastOffset; '\x01' = line break; returns lines in window
@ 0x4403A4 int __watcall PrintTextBlock(int mode /*eax*/, int x /*edx*/, int y /*ebx*/, int lineHeight /*ecx*/, int width, const char* text); // ret 8; = PrintTextWrapped(0, 99, ...)
@ 0x46A144 int __watcall BriefingHitTest(int x /*ax*/, int y /*dx*/);   // records 0..14 of g_briefButtons, enabled[state]; -1 none
@ 0x46A20C void __watcall BriefingHoverStatus(int x /*ax*/, int y /*dx*/); // records 0..31 -> status RES.000 0x1F8+type (+ state overrides)
@ 0x4586E8 void BriefingHover(void);                // GetMousePos -> 0x46A20C
@ 0x46B464 void BriefingConfirmHover(void);         // GetMousePos: Yes x265..297 / No x350..374, y250..270 -> 0x5E1D00
@ 0x46BD3C int __watcall BriefingLButtonDown(int lParam /*eax*/);        // pressed = HitTest, click sound
@ 0x46B690 int __watcall BriefingLButtonUp(int wParam /*eax*/, int lParam /*edx*/); // confirm/popup, else jump table 0x46B654[id 0..14]
@ 0x46BE30 int __watcall BriefingRButtonDown(int lParam /*eax*/);       // ids 7..14 -> g_hallInfoSlot
@ 0x46C018 int __watcall BriefingKeyDown(int vk /*eax*/);               // 8 backspace name, 0x78 F9 -> 0x449170, chars -> 0x46BEE8
@ 0x449170 void ScreenshotLBM(void);                 // "c:\WW%02d.lbm", ScreenCaptureCopy 0x4490A0 (640x480)
@ 0x468F94 void HallReset(void);                    // clears hall arrays, curUnit, slot -1, count, g_heroSelection
@ 0x469090 void HallCollectSurvivors(void);         // at entry for campaign m>0 (not while loading 0x61A1E0)
@ 0x4699D4 void __watcall HallBrowse(int step /*eax: 1 next, 0x7FF prev, 0 revalidate*/); // g_hallCurUnit = next own unit (mod 0x800)
@ 0x469AA8 void HallAddCurrentUnit(void);           // "+" (id 3): shown unit (+items) -> target/first free slot, removed from world
@ 0x469FD0 void __watcall HallSlotClick(int slot /*ax*/); // state 0 target slot; state 1 toggle selection (<= g_hallMax)
@ 0x46A578 int __watcall UnitTypeRace(int type /*ax*/);   // 0xC..0x33 -> (t-12)/10, specials 0x34..0x3F, else 0xFFFF
@ 0x46EDC8 int NextBuiltInScenario(void);           // registry "Built In Scenarios": 0/7 -> 7, 1 -> 8, 3 -> 9
@ 0x4787C0 void __watcall PlayVideoThenScreen(int kind /*eax*/, void* handler /*edx*/); // kind 2 chapter (-> briefing), 4 mission (-> 0x41FD58)
@ 0x415A68 void EndOfMissionContinue(void);         // net -> main menu; built-in -> race menu; campaign won -> m++, 0x4787C0(2, 0x46C06C)
@ 0x46CE90 void DrawGoalDialog(void);               // ESC "Review scenario goal" (g_menuDialog 5): font 0x800100E9+4r, lh 19
@ 0x46EC34 void __watcall ReenterAfterLoad(int menuScreen /*eax*/); // g_menuScreen==4 -> SetScreenHandler(0x46C06C)
$ 0x4B6F1C char g_heroSelection                     // save "heroselection": briefing state 0..3 (see above)
$ 0x5E1D02 char g_briefDrawn                        // page drawn this frame, waiting for its flip (0 -> tick draws)
$ 0x4B74B8 int g_briefPressedId                     // -1 none; pressed frames drawn for 0..6
$ 0x4B74C0 int g_briefLineCount                     // PrintTextBlock count (width 587)
$ 0x4B74C4 int g_briefFirstLine                     // scroll: 0..lines-13 (14 rows shown, first..first+13)
$ 0x4B74C8 int g_briefRedraw                        // text page full redraws left (set 2 on any change)
$ 0x4B74CC short g_briefConfirmOpen                 // Yes/No "Did you select units?" on the text page
$ 0x4B74CE short g_briefHintOpen                    // map-page popup RES.000 0x44E; any click closes
$ 0x5E1D00 short g_briefConfirmHover                // 1 Yes, 2 No
$ 0x4B66E8 int g_missionIndex                       // 0..6 campaign, 7..9 built-in scenario
$ 0x4B66EC int g_gameType                           // 0 campaign, 1 network, 2 built-in scenario
$ 0x61A1E0 int g_loadingSave                        // 1 around the menu's LoadGame
$ 0x4B6E88 int g_prevMenuScreen                     // save "previousmenuscreen"
$ 0x4B6F14 int g_hallCurUnit                        // save "curunitshown": Thing id shown in the lower well
$ 0x4B6F18 int g_hallTargetSlot                     // save "curslotselected", -1 none
$ 0x4B6F20 int g_hallOccupied[8]                    // save "unitinhall"
$ 0x4B6F40 int g_hallSelected[8]                    // save "unitselected"
$ 0x4B6F60 int g_hallSelectedCount                  // save "maxunitsselected"
$ 0x4B6F74 int g_hallInfoSlot                       // right button held on slot, -1
$ 0x5E18A0 Thing g_hallThings[8]                    // save "hallthings", stride 0x7C
$ 0x5E14E0 UnitExt g_hallExt[8]                     // save "hallofheroes", stride 0x78
$ 0x5E0CA8 Thing g_hallItems[8][2]                  // save "hallitemthings"
$ 0x4B74D0 int g_hallMax[4][8]                      // heroes allowed per race/mission (0 -> state 1 skipped)
$ 0x4B755E short g_briefLabelPos[84][2]             // {x, y}; label text 0x800004EB + r*21 + m*3 + i
$ 0x4B7550 uchar g_hallPortraitFrame[12]            // unit class -> RES.004 #race frame 8..11 (lower-well portrait)
$ 0x468F90 uchar g_briefTextTiles[4]                // {9,10,17,18}: #197+race tiles for the text background
$ 0x4B1490 char g_briefGoalJoin[]                   // "\x01\x01"
struct BriefButton { ushort enabledFirst[3]; ushort enabledLater[3]; ushort unused[6]; ushort left, top, right, bottom; ushort id; ushort type; ushort pressedFrame; uint clickSound; }; // 42 bytes
$ 0x4B6F78 BriefButton g_briefButtons[32]           // 0 up/prev 1 down/next 2 OK 3 + 4 load 5 save 6 back 7..14 hall slots 15..31 hover-only fields
// Text print call 0x46B122..0x46B13D: width 0x46B124 (0x24B), lh 0x46B129 (0x14), x 0x46B12B (0x1A), window 0x46B130 (0xD),
//   y 0x46B135 (0x1A); count width 0x46AC9E. Clip SetClipRect 0x46B09B..0x46B0AD (x 20..618, y 20..306; restore at 0x46B160
//   reads the RECT it saved at [esp]). Tile loops 0x46B0B8/0x46B0C2/0x46B0FD/0x46B108. Threshold 13 imm8: 0x46A1B9, 0x46A2AB,
//   0x46AE16, 0x46AFE2, 0x46B00D, 0x46BADE, 0x46BAF2. Scroll orb frames 34/35/36 at (0x1BC,0x154): mov pairs 0x46AE23/28,
//   0x46AE43/4A, 0x46AFE7/EE, 0x46B01A/1F, 0x46B03A/41.
// 640x480: WM_PAINT BitBlt 640x480 (0x46C36E); CaptureScreenBitmap(1,0,0,640,480) 0x46BB7E/0x46BC0E; black FillRect 640x480
//   at 0x46B92B, 0x46B9C1, 0x46BCB8; F9 screenshot. No Lock/raw pixel access, no palette fades in the briefing.
// Wide briefing (mod/src/briefing.cpp): handler 0x46C06C runs in the 960x540 wide mode, origin (0,30). The mod prints the
//   briefing text (RES 0x8000049B+20r+2m, sprintf clan name, + "\x01\x01", no goal) in a right panel on both pages via
//   PrintTextWrapped 0x4401A8 (first = [0x4B74C4] on the text page), the goal (RES +1) in the lower well; text page
//   0x46AF9F background call hooked (clear panel, DrawMissionMap 0x46A5DC), block 0x46B099..0x46B177 replaced; count
//   width 0x46AC9E -> 272, goal strcat skipped (0x46AC4F jmp), row threshold 13 -> 20 (7 sites), scroll orb moved
//   (10 operands), orb clicks aliased to ids 0/1 while state >= 2.
