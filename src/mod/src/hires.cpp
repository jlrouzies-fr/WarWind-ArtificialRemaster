#include "hires.h"

#include <windows.h>

#include <stdio.h>

#include "briefing.h"
#include "camera.h"
#include "canvas.h"
#include "cursorrect.h"
#include "endscreen.h"
#include "escmenu.h"
#include "fx.h"
#include "game.h"
#include "hook.h"
#include "hud.h"
#include "layout.h"
#include "log.h"
#include "mainmenu.h"
#include "motion.h"
#include "patch.h"
#include "racemenu.h"
#include "widemenu.h"

namespace
{
using layout::kDX;
using layout::kDY;
constexpr int kWidth = layout::kScreenW, kHeight = layout::kScreenH;

// Draw-list node pools, 36 bytes per node. The game's static pools (2000 and 1500 nodes) are
// sized for a 22x20 tile view and the list insert never checks the count.
constexpr int kDrawNodes = 8000, kOverlayNodes = 6000;
BYTE g_drawNodes[kDrawNodes * game::kDrawNodeSize];
BYTE g_overlayNodes[kOverlayNodes * game::kDrawNodeSize];
constexpr DWORD kDrawPoolImm = 0x41F38D;     // mov ebx, 004C4238h (operand)

// DrawListInsert (0x4443A0, eax = key, edx = list) appends a node to bucket `key` without any
// bounds check. The replacement clamps the key to the buckets and drops nodes once the pool is
// full, so an unexpected sprite position can never write outside the lists.
// List: +0 bucket count, +4 node capacity, +8 nodes used, +0xC buckets {head, tail}, +0x10 nodes.
constexpr DWORD kDrawListInsert = 0x4443A0;
const BYTE kDrawListInsertOrig[] = { 0x53, 0x56, 0x89, 0xC6, 0x8B, 0x5A };
BYTE g_droppedNode[game::kDrawNodeSize];

BYTE* __cdecl DrawListInsert(int key, DWORD* list)
{
    int buckets = (int)list[0], capacity = (int)list[1], used = (int)list[2];
    if (used >= capacity)
        return g_droppedNode + 4;
    list[2] = used + 1;
    BYTE* node = (BYTE*)list[4] + used * game::kDrawNodeSize;
    *(DWORD*)node = 0;
    key = key < 0 ? 0 : key >= buckets ? buckets - 1 : key;
    DWORD* bucket = (DWORD*)list[3] + 2 * key;
    if (bucket[0])
        *(DWORD*)bucket[1] = (DWORD)node;
    else
        bucket[0] = (DWORD)node;
    bucket[1] = (DWORD)node;
    return node + 4;
}

__declspec(naked) void DrawListInsertThunk()
{
    __asm
    {
        push ecx
        push edx
        push edx
        movsx eax, ax
        push eax
        call DrawListInsert
        add esp, 8
        pop edx
        pop ecx
        ret
    }
}
constexpr DWORD kOverlayPoolImm = 0x41F397;  // mov esi, 004DA4F8h (operand)

// RecordSpriteRect (0x4226B4) appends a click rectangle to g_rectList[cur ^ 1] (0x4C2DE8, two lists
// of 256 ten-byte records) without a bound; the hi-res view can draw more sprites than that. The
// append now stops at a full list.
constexpr DWORD kRectAppend = 0x422784, kRectAppendResume = 0x42278A, kRectAppendSkip = 0x4227E1;
const BYTE kRectAppendOrig[] = { 0x8B, 0x35, 0xAC, 0x33, 0x4B, 0x00 };   // mov esi, [g_rectListCur]

__declspec(naked) void RectAppendThunk()
{
    __asm
    {
        mov esi, dword ptr ds:[0x4B33AC]
        xor esi, 1
        cmp dword ptr [esi * 4 + 0x4B33B0], 256     // records per list
        mov esi, dword ptr ds:[0x4B33AC]
        jae full
        push kRectAppendResume
        ret
    full:
        push kRectAppendSkip
        ret
    }
}

bool g_enabled;
int g_columnMode;   // >0 while a right-column panel routine runs: x < 640 gets +kDX
int g_barMode;      // >0 while the bottom bar routine runs: y < 480 gets +kDY
int g_sceneMode;    // >0 while the mission scene is redrawn (also under the in-game menu)
int g_dialogMode;   // >0 while a mission message box (modal dialog) is drawn
int g_shiftSuspended;  // >0 while a mod screen draws into its own surface

void* t_SetScreenHandler;
void* t_SetClipRect;
void* t_DrawImage;
void* t_SetTextPos;
void* t_FillRect;
void* t_PutPixel;
void* t_DrawPanelHeader;
void* t_DrawMinimapFrame;
void* t_DrawCommandPanel;
void* t_DrawMinimap;
void* t_DrawMinimapDots;
void* t_DrawAltPanel;
void* t_PanelHover;
void* t_DrawBottomBar;
void* t_RedrawMapSurface;
void* t_BlitMapView;
void* t_DrawGameFrame;
void* t_DrawModalDialog;
void* t_ModalPress;
void* t_ModalRelease;

// 640x480 screens drawn over the mission are centred on the screen's map area, which shows the
// canvas at the camera offset.
constexpr int kMenuDX = (layout::kScreenViewW - 640) / 2, kMenuDY = (layout::kScreenViewH - 480) / 2;

int CentredDX()
{
    int x, y;
    CameraOffset(&x, &y);
    return kMenuDX + x;
}

int CentredDY()
{
    int x, y;
    CameraOffset(&x, &y);
    return kMenuDY + y;
}

// The in-game menu is a 640x480 screen drawn over the paused mission; in hi-res its own drawing
// is centred, while the mission scene it redraws underneath keeps the hi-res layout.
bool MenuShift()
{
    return g_enabled && g_sceneMode == 0 && *game::screenWidth == kWidth &&
           *game::currentScreenHandler == game::fnMenuScreenHandler;
}

// Mission message boxes are laid out for the 518x453 view; centring them in the hi-res view
// takes the same offset as the menu.
bool DialogShift()
{
    return g_enabled && g_dialogMode > 0 && *game::screenWidth == kWidth;
}

bool Centered()
{
    return MenuShift() || DialogShift();
}

// True while the mission screen, or the menu opened over it, runs in the hi-res mode.
bool InGame()
{
    DWORD handler = *game::currentScreenHandler;
    return g_enabled && g_shiftSuspended == 0 && *game::screenWidth == kWidth &&
           (handler == game::fnInGameScreenHandler || handler == game::fnMenuScreenHandler);
}

// ---- register adjusters (called from the naked thunks with pointers to saved registers)

void __cdecl OnSetScreenHandler(DWORD handler)
{
    if (!g_enabled)
        return;
    Log("hires: screen handler %08X -> %08X", *game::currentScreenHandler, handler);
    if (handler == game::fnMenuScreenHandler)   // menus keep the mode (the in-game menu stays over the mission)
        return;
    bool toGame = handler == game::fnInGameScreenHandler;
    // The incremental scroll redraw (shift the map surface, draw the exposed strip) is written
    // for the 518x453 view; the full redraw ("DisplayHack" option) handles any view size.
    // A mission (re)starts: the camera takes the mission's start position at its first frame
    // (a restart from the in-game menu keeps the video mode, so the canvas is not re-attached).
    if (toGame)
    {
        *game::displayHackOption = 1;
        CameraReset();
    }
    WideMenuScreenChanging();
    int w = kWidth, h = kHeight;
    if (!toGame)
        WideMenuModeFor(handler, &w, &h);
    if (*game::screenWidth == w && *game::screenHeight == h)
        return;
    DWORD ok = WatcomCall3(game::fnSetVideoMode, w, h, 8);
    Log("hires: video mode %dx%d for handler %08X -> %s", w, h, handler, ok ? "ok" : "FAILED");
}

// r[0] = top, r[1] = bottom, r[2] = right, r[3] = left
void __cdecl AdjustClip(DWORD* r)
{
    if (!InGame())
        return;
    if (Centered())
    {
        r[0] += CentredDY();
        r[1] += CentredDY();
        r[2] += CentredDX();
        r[3] += CentredDX();
        return;
    }
    if ((int)r[3] >= 0x200)
    {
        r[3] += kDX;
        r[2] += kDX;
    }
    if ((int)r[0] >= 0x1C0)
    {
        r[0] += kDY;
        r[1] += kDY;
    }
}

void ShiftPoint(DWORD& x, DWORD& y)
{
    if (!InGame())
        return;
    if (Centered())
    {
        x += CentredDX();
        y += CentredDY();
        return;
    }
    bool column = g_columnMode > 0 || *(int*)0x4C2120 >= 0x206 + kDX;  // g_clipLeft in the right column
    if (column && (int)x < 640)
        x += kDX;
    if (g_barMode > 0 && (int)y < 480)
        y += kDY;
}

// r[0] = y, r[1] = x
void __cdecl AdjustXY(DWORD* r)
{
    ShiftPoint(r[1], r[0]);
}

// r[0] = y (dx), r[1] = x (ax): 16-bit values in the low words.
// Glyphs are drawn with DrawImage at the stored text position, so the centred-menu offset is
// applied there only.
void __cdecl AdjustTextPos(DWORD* r)
{
    if (Centered())
        return;
    DWORD x = r[1] & 0xFFFF, y = r[0] & 0xFFFF;
    ShiftPoint(x, y);
    r[1] = (r[1] & 0xFFFF0000) | (x & 0xFFFF);
    r[0] = (r[0] & 0xFFFF0000) | (y & 0xFFFF);
}

// r[0] = x, r[1] = y, r[2] = h, r[3] = w
void __cdecl AdjustFill(DWORD* r)
{
    ShiftPoint(r[0], r[1]);
    FxMarkUi((int)r[0], (int)r[1], (int)r[3], (int)r[2]);
}

// r[0] = y, r[1] = x
void __cdecl AdjustPixel(DWORD* r)
{
    ShiftPoint(r[1], r[0]);
    FxMarkUi((int)r[1], (int)r[0], 1, 1);
}

// Message-box click handlers get the packed mouse position (x low word, y high word) in eax.
void __cdecl AdjustDialogClick(DWORD* packed)
{
    if (!InGame() || *game::currentScreenHandler != game::fnInGameScreenHandler)
        return;
    short x = (short)LOWORD(*packed) - CentredDX(), y = (short)HIWORD(*packed) - CentredDY();
    *packed = MAKELONG(x, y);
}

void __cdecl AdjustDialogHover(short* point)
{
    if (!InGame() || *game::currentScreenHandler != game::fnInGameScreenHandler)
        return;
    point[0] -= CentredDX();
    point[1] -= CentredDY();
}

// ---- naked thunks: adjust registers in place, then continue in the original

__declspec(naked) void ThunkSetScreenHandler()
{
    __asm
    {
        pushad
        push eax
        call OnSetScreenHandler
        add esp, 4
        popad
        jmp dword ptr [t_SetScreenHandler]
    }
}

__declspec(naked) void ThunkSetClipRect()
{
    __asm
    {
        push ebx
        push ecx
        push edx
        push eax
        push esp
        call AdjustClip
        add esp, 4
        pop eax
        pop edx
        pop ecx
        pop ebx
        jmp dword ptr [t_SetClipRect]
    }
}

__declspec(naked) void ThunkDrawImage()
{
    __asm
    {
        push eax
        push edx
        push ebx
        push ecx
        push esp
        call AdjustXY
        add esp, 4
        pop ecx
        pop ebx
        pop edx
        pop eax
        jmp dword ptr [t_DrawImage]
    }
}

__declspec(naked) void ThunkSetTextPos()
{
    __asm
    {
        push ecx
        push eax
        push edx
        push esp
        call AdjustTextPos
        add esp, 4
        pop edx
        pop eax
        pop ecx
        jmp dword ptr [t_SetTextPos]
    }
}

__declspec(naked) void ThunkFillRect()
{
    __asm
    {
        push ebx
        push ecx
        push edx
        push eax
        push esp
        call AdjustFill
        add esp, 4
        pop eax
        pop edx
        pop ecx
        pop ebx
        jmp dword ptr [t_FillRect]
    }
}

#define DIALOG_CLICK_THUNK(name)                  \
    __declspec(naked) void Thunk##name()          \
    {                                             \
        __asm { push ecx }                        \
        __asm { push edx }                        \
        __asm { push eax }                        \
        __asm { push esp }                        \
        __asm { call AdjustDialogClick }          \
        __asm { add esp, 4 }                      \
        __asm { pop eax }                         \
        __asm { pop edx }                         \
        __asm { pop ecx }                         \
        __asm { jmp dword ptr [t_##name] }        \
    }

DIALOG_CLICK_THUNK(ModalPress)
DIALOG_CLICK_THUNK(ModalRelease)

// Replaces the GetMousePos call in the message-box hover handler (eax = short[2] out).
DWORD g_getMousePos = game::fnGetMousePos;
__declspec(naked) void DialogGetMousePos()
{
    __asm
    {
        push ecx
        push edx
        push eax
        call dword ptr [g_getMousePos]
        call AdjustDialogHover
        pop eax
        pop edx
        pop ecx
        ret
    }
}

__declspec(naked) void ThunkPutPixel()
{
    __asm
    {
        push ecx
        push eax
        push edx
        push esp
        call AdjustPixel
        add esp, 4
        pop edx
        pop eax
        pop ecx
        jmp dword ptr [t_PutPixel]
    }
}

// ---- panel wrappers

void __cdecl ColumnEnter() { ++g_columnMode; }
void __cdecl ColumnLeave() { --g_columnMode; }
void __cdecl BarEnter() { ++g_barMode; }
void __cdecl BarLeave() { --g_barMode; }
void __cdecl SceneEnter() { ++g_sceneMode; }
void __cdecl SceneLeave() { --g_sceneMode; }
void __cdecl DialogEnter() { ++g_dialogMode; }
void __cdecl DialogLeave() { --g_dialogMode; }

// The victory / defeat picture is a 640x480 screen: show it in the menus' mode (widemenu lays it
// out, or 640x480 boxed by cnc-ddraw over its backdrop). The mode switch resets the palette.
void __cdecl FrameEnter()
{
    ++g_sceneMode;
    MotionFrameStart();
    if (!InGame())
        return;
    if (*game::currentScreenHandler != game::fnInGameScreenHandler || !*game::endScreen)
    {
        CameraFrameStart();
        CursorRectFrameStart();
        return;
    }
    int w, h;
    WideMenuModeForEndScreen(&w, &h);
    DWORD ok = WatcomCall3(game::fnSetVideoMode, w, h, 8);
    Log("hires: end screen, video mode %dx%d -> %s", w, h, ok ? "ok" : "FAILED");
    if (*game::resultWon || *game::resultWon2)
        WatcomCall(game::fnSetPaletteResource, game::kEndPaletteWon + *game::playerRace);
    else if (*game::resultLost)
        WatcomCall(game::fnSetPaletteResource, game::kEndPaletteLost + *game::playerRace);
}

// The mission frame is complete in the canvas: add the HUD strip (still part of the scene, so the
// in-game menu's centring does not apply to it).
void __cdecl FrameLeave()
{
    MotionFrameDone();
    EscMenuSceneDrawn();
    if (CanvasActive() && *game::screenWidth == kWidth && !*game::endScreen)
        HudDraw();
    --g_sceneMode;
}

void __cdecl MapRedrawEnter(DWORD full)
{
    ++g_sceneMode;
    FxMapRedrawBegin(full != 0);
}

void __cdecl MapRedrawLeave()
{
    FxMapRedrawEnd();
    --g_sceneMode;
}

// RedrawMapSurface (eax = full redraw): scene mode, and Modern Graphics annotates the terrain.
__declspec(naked) void Wrap_RedrawMapSurface()
{
    __asm
    {
        pushad
        push eax
        call MapRedrawEnter
        add esp, 4
        popad
        call dword ptr [t_RedrawMapSurface]
        push eax
        pushad
        call MapRedrawLeave
        popad
        pop eax
        ret
    }
}

WRAP(DrawPanelHeader, ColumnEnter, ColumnLeave)
WRAP(DrawMinimapFrame, ColumnEnter, ColumnLeave)
WRAP(DrawCommandPanel, ColumnEnter, ColumnLeave)
WRAP(DrawMinimap, ColumnEnter, ColumnLeave)
WRAP(DrawMinimapDots, ColumnEnter, ColumnLeave)
WRAP(DrawAltPanel, ColumnEnter, ColumnLeave)
WRAP(PanelHover, ColumnEnter, ColumnLeave)
WRAP(DrawBottomBar, BarEnter, BarLeave)
WRAP(BlitMapView, SceneEnter, SceneLeave)
WRAP(DrawGameFrame, FrameEnter, FrameLeave)
WRAP(DrawModalDialog, DialogEnter, DialogLeave)

struct Detour
{
    const char* name;
    DWORD addr;
    const BYTE* expected;
    size_t len;
    const void* hook;
    void** trampoline;
};

const BYTE kSetScreenHandler[] = { 0x53, 0x51, 0x52, 0x89, 0xC1 };
const BYTE kSetClipRect[] = { 0x56, 0x57, 0x89, 0xC6, 0x89, 0xD7 };
const BYTE kDrawImage[] = { 0x56, 0x57, 0x6A, 0x00, 0x6A, 0x00 };
const BYTE kSetTextPos[] = { 0x66, 0xA3, 0x80, 0x69, 0x55, 0x00 };
const BYTE kFillRect[] = { 0x83, 0xEC, 0x74, 0x89, 0x54, 0x24, 0x68 };
const BYTE kPutPixel[] = { 0x51, 0x56, 0x57, 0x83, 0xEC, 0x6C };
const BYTE kDrawPanelHeader[] = { 0x53, 0x51, 0x52, 0x83, 0xEC, 0x1C };   // push ebx/ecx/edx ; sub esp, 1Ch
const BYTE kDrawMinimapFrame[] = { 0x53, 0x51, 0x52, 0x83, 0xEC, 0x10 };
const BYTE kPush6[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };        // push ebx/ecx/edx/esi/edi/ebp
const BYTE kDrawAltPanel[] = { 0x52, 0x83, 0xEC, 0x04, 0x89, 0xC2 };
const BYTE kDrawModalDialog[] = { 0x53, 0x51, 0x52, 0x31, 0xC0 };   // push ebx/ecx/edx ; xor eax, eax
const BYTE kModalPress[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x83, 0xEC, 0x04 };
const BYTE kModalRelease[] = { 0x53, 0x51, 0x52, 0x56, 0x83, 0xEC, 0x04 };
const BYTE kDrawBottomBar[] = { 0x53, 0x51, 0x52, 0x8A, 0x25, 0x4C, 0x26, 0x4B, 0x00 };

// DrawMinimap's view rectangle (call DrawRectOutline at 0x4185BC, stack: left, top, right,
// bottom, 1, colour) is derived from the game's tile origin and the original 12x11 view. It is
// redrawn from the camera: one minimap pixel per 48 world pixels, the screen's map area in size.
constexpr DWORD kMinimapRectCall = 0x4185BC;
DWORD g_drawRectOutline = 0x414888;
const BYTE kMinimapRectCallOrig[] = { 0xE8, 0xC7, 0xC2, 0xFF, 0xFF };

void __cdecl MinimapViewRect(int* rect)
{
    constexpr int kLeft = 0x212 + kDX, kTop = 0x88, kSize = 0x60, kPixel = 2 * layout::kTile;
    int x, y;
    CameraWorldPosition(&x, &y);
    int w = layout::kScreenViewW / kPixel, h = layout::kScreenViewH / kPixel;
    int left = min(max(x / kPixel, 0), kSize - 1 - w), top = min(max(y / kPixel, 0), kSize - 1 - h);
    rect[0] = kLeft + left;
    rect[1] = kTop + top;
    rect[2] = kLeft + left + w;
    rect[3] = kTop + top + h;
}

__declspec(naked) void MinimapViewRectThunk()
{
    __asm
    {
        pushad
        lea eax, [esp + 36]      // first stack argument, past pushad and the return address
        push eax
        call MinimapViewRect
        add esp, 4
        popad
        jmp dword ptr [g_drawRectOutline]
    }
}

// call GetMousePos inside the message-box hover handler (0x48DD4C)
constexpr DWORD kDialogHoverCall = 0x48DD54;
const BYTE kDialogHoverCallOrig[] = { 0xE8, 0x47, 0x61, 0xF8, 0xFF };

const Detour kDetours[] = {
    { "hires SetScreenHandler", game::fnSetScreenHandler, kSetScreenHandler, sizeof(kSetScreenHandler), ThunkSetScreenHandler, &t_SetScreenHandler },
    { "hires SetClipRect", game::fnSetClipRect, kSetClipRect, sizeof(kSetClipRect), ThunkSetClipRect, &t_SetClipRect },
    { "hires DrawImage", game::fnDrawImage, kDrawImage, sizeof(kDrawImage), ThunkDrawImage, &t_DrawImage },
    { "hires SetTextPos", game::fnSetTextPos, kSetTextPos, sizeof(kSetTextPos), ThunkSetTextPos, &t_SetTextPos },
    { "hires FillRect", game::fnFillRect, kFillRect, sizeof(kFillRect), ThunkFillRect, &t_FillRect },
    { "hires PutPixel", game::fnPutPixel, kPutPixel, sizeof(kPutPixel), ThunkPutPixel, &t_PutPixel },
    { "hires DrawPanelHeader", 0x416FB0, kDrawPanelHeader, 6, Wrap_DrawPanelHeader, &t_DrawPanelHeader },
    { "hires DrawMinimapFrame", 0x4180FC, kDrawMinimapFrame, 6, Wrap_DrawMinimapFrame, &t_DrawMinimapFrame },
    { "hires DrawCommandPanel", 0x417280, kPush6, 6, Wrap_DrawCommandPanel, &t_DrawCommandPanel },
    { "hires DrawMinimap", 0x4184C8, kPush6, 6, Wrap_DrawMinimap, &t_DrawMinimap },
    { "hires DrawMinimapDots", 0x4214C0, kPush6, 6, Wrap_DrawMinimapDots, &t_DrawMinimapDots },
    { "hires DrawAltPanel", 0x4571F8, kDrawAltPanel, 6, Wrap_DrawAltPanel, &t_DrawAltPanel },
    { "hires PanelHover", 0x457F40, kPush6, 6, Wrap_PanelHover, &t_PanelHover },
    { "hires DrawBottomBar", 0x4171D4, kDrawBottomBar, 9, Wrap_DrawBottomBar, &t_DrawBottomBar },
    { "hires RedrawMapSurface", game::fnRedrawMapSurface, kPush6, 6, Wrap_RedrawMapSurface, &t_RedrawMapSurface },
    { "hires BlitMapView", 0x4216FC, kPush6, 6, Wrap_BlitMapView, &t_BlitMapView },
    { "hires DrawGameFrame", 0x41A4C0, kPush6, 6, Wrap_DrawGameFrame, &t_DrawGameFrame },
    { "hires DrawModalDialog", game::fnDrawModalDialog, kDrawModalDialog, sizeof(kDrawModalDialog), Wrap_DrawModalDialog, &t_DrawModalDialog },
    { "hires ModalPress", game::fnModalPress, kModalPress, sizeof(kModalPress), ThunkModalPress, &t_ModalPress },
    { "hires ModalRelease", game::fnModalRelease, kModalRelease, sizeof(kModalRelease), ThunkModalRelease, &t_ModalRelease },
};
}  // namespace

bool HiresMenuOffset(int* dx, int* dy)
{
    *dx = kMenuDX;
    *dy = kMenuDY;
    return MenuShift();
}

void HiresSuspendShift(bool suspend)
{
    g_shiftSuspended += suspend ? 1 : -1;
}

void HiresViewTiles(int* tilesX, int* tilesY)
{
    *tilesX = g_enabled ? layout::kViewCols : 22;
    *tilesY = g_enabled ? layout::kViewRows : 20;
}

void HiresInstall(const char* iniPath, const char* patchDir)
{
    if (!GetPrivateProfileIntA("Video", "HiRes", 0, iniPath))
        return;
    for (const Detour& d : kDetours)
        if (!HookVerify(d.addr, d.expected, d.len, d.name))
            return;
    const BYTE drawPoolOrig[] = { 0x38, 0x42, 0x4C, 0x00 }, overlayPoolOrig[] = { 0xF8, 0xA4, 0x4D, 0x00 };
    if (!HookVerify(kDrawPoolImm, drawPoolOrig, 4, "hires draw pool") ||
        !HookVerify(kOverlayPoolImm, overlayPoolOrig, 4, "hires overlay pool") ||
        !HookVerify(kDrawListInsert, kDrawListInsertOrig, sizeof(kDrawListInsertOrig), "hires draw list insert") ||
        !HookVerify(kMinimapRectCall, kMinimapRectCallOrig, sizeof(kMinimapRectCallOrig), "hires minimap rect") ||
        !HookVerify(kDialogHoverCall, kDialogHoverCallOrig, sizeof(kDialogHoverCallOrig), "hires dialog hover") ||
        !HookVerify(kRectAppend, kRectAppendOrig, sizeof(kRectAppendOrig), "hires rect list"))
        return;
    char patchFile[MAX_PATH];
    _snprintf_s(patchFile, sizeof(patchFile), _TRUNCATE, "%s\\feature-hires_720.wwp", patchDir);
    if (!PatchApplyFile(patchFile))
    {
        Log("hires: %s not applied, feature disabled", patchFile);
        return;
    }
    for (const Detour& d : kDetours)
        *d.trampoline = HookDetour(d.addr, d.expected, d.len, d.hook, d.name);
    void* drawPool = g_drawNodes;
    void* overlayPool = g_overlayNodes;
    PatchWrite(kDrawPoolImm, &drawPool, sizeof(drawPool));
    PatchWrite(kOverlayPoolImm, &overlayPool, sizeof(overlayPool));
    HookJump(kDrawListInsert, DrawListInsertThunk, sizeof(kDrawListInsertOrig));
    HookJump(kRectAppend, RectAppendThunk, sizeof(kRectAppendOrig));
    HookCall(kMinimapRectCall, MinimapViewRectThunk);
    HookCall(kDialogHoverCall, DialogGetMousePos);
    g_enabled = true;
    CanvasInstall(iniPath);
    CursorRectInstall();
    MotionInstall(iniPath);
    EscMenuInstall(iniPath);
    if (WideMenuInstall(iniPath))
    {
        RaceMenuInstall();
        MainMenuInstall();
        EndScreenInstall();
        BriefingInstall();
    }
    CameraInstall(iniPath);
    Log("hires: %dx%d in-game mode enabled", kWidth, kHeight);
}
