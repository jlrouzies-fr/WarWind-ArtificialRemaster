#include "canvas.h"

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <mmsystem.h>

#include <string.h>

#include "camera.h"
#include "cursorrect.h"
#include "fx.h"
#include "game.h"
#include "hud.h"
#include "hook.h"
#include "layout.h"
#include "log.h"
#include "motion.h"
#include "patch.h"
#include "widemenu.h"

using namespace layout;

namespace
{
IDirectDrawSurface* g_canvas;
IDirectDrawSurface* g_realBack;
int g_presentIntervalMs;
DWORD g_lastPresent;
int g_shownX, g_shownY;
int g_missionLoop;       // >0 while InGameFrame runs: its flips show the mission as it plays

// Screen mouse position laid out for the cursor routine, which reads "dword [p] >> 16":
// x through &pad, y through &x.
struct CursorPos
{
    short pad;
    short x;
    short y;
    short pad2;
} g_cursor;

// mov ebx, [0x4C2C23] (x) and mov eax, [0x4C2C25] (y) in DrawCursor (0x413E30).
constexpr DWORD kCursorReadX = 0x413E73, kCursorReadY = 0x413E62;
const BYTE kCursorReadXOrig[] = { 0x8B, 0x1D, 0x23, 0x2C, 0x4C, 0x00 };
const BYTE kCursorReadYOrig[] = { 0xA1, 0x25, 0x2C, 0x4C, 0x00 };

// Flip waits for the vertical blank twice (WaitForVerticalBlank, then Flip with DDFLIP_WAIT), which
// under cnc-ddraw costs two refresh periods per flip. While the canvas is attached, flips are paced
// by the game's frame deadline and PresentHz instead: the wait call becomes "add esp, 12" (its three
// stdcall arguments) and the flip flag 0.
constexpr DWORD kFlipWaits = 0x4145FC;
const BYTE kFlipWaitsOrig[] = { 0xFF, 0x52, 0x58, 0x6A, 0x01 };     // call [edx+58h] ; push DDFLIP_WAIT
const BYTE kFlipWaitsNone[] = { 0x83, 0xC4, 0x0C, 0x6A, 0x00 };     // add esp, 0Ch ; push 0

void* t_SetVideoMode;
void* t_Flip;
void* t_InGameFrame;

struct Locked
{
    BYTE* bits;
    int pitch;
};

bool Lock(IDirectDrawSurface* s, DWORD flags, Locked* out)
{
    DDSURFACEDESC desc = { sizeof(desc) };
    if (FAILED(s->Lock(nullptr, &desc, flags | DDLOCK_WAIT, nullptr)))
        return false;
    out->bits = (BYTE*)desc.lpSurface;
    out->pitch = desc.lPitch;
    return true;
}

void Attach()
{
    auto dd = (IDirectDraw*)*game::lpDirectDraw;
    DDSURFACEDESC desc = { sizeof(desc) };
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = kCanvasW;
    desc.dwHeight = kCanvasH;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    HRESULT hr = dd->CreateSurface(&desc, &g_canvas, nullptr);
    if (FAILED(hr))
    {
        Log("canvas: CreateSurface %dx%d failed (%08X)", kCanvasW, kCanvasH, hr);
        g_canvas = nullptr;
        return;
    }
    Locked c;
    if (Lock(g_canvas, DDLOCK_WRITEONLY, &c))
    {
        for (int y = 0; y < kCanvasH; ++y)
            memset(c.bits + y * c.pitch, 0, kCanvasW);
        g_canvas->Unlock(nullptr);
    }
    g_realBack = (IDirectDrawSurface*)*game::lpBackBuffer;
    *game::lpBackBuffer = g_canvas;
    PatchWrite(kFlipWaits, kFlipWaitsNone, sizeof(kFlipWaitsNone));
    const int canvasClip[4] = { 0, kCanvasW - 1, 0, kCanvasH - 1 };
    memcpy(game::clipRect, canvasClip, sizeof(canvasClip));
    memcpy(game::clipBounds, canvasClip, sizeof(canvasClip));
    CameraReset();
    Log("canvas: %dx%d attached", kCanvasW, kCanvasH);
}

void Detach()
{
    if (!g_canvas)
        return;
    *game::lpBackBuffer = g_realBack;
    PatchWrite(kFlipWaits, kFlipWaitsOrig, sizeof(kFlipWaitsOrig));
    g_canvas->Release();
    g_canvas = nullptr;
    g_realBack = nullptr;
    Log("canvas: detached");
}

void __cdecl BeforeSetVideoMode()
{
    Detach();
    WideMenuModeChanging();
}

void __cdecl AfterSetVideoMode()
{
    if (*game::screenWidth == kScreenW && *game::screenHeight == kScreenH)
        Attach();
    else
        WideMenuModeSet();
}

// Copies the map at the camera offset and the panels to the real back buffer.
void Compose()
{
    CameraUpdate();
    CameraOffset(&g_shownX, &g_shownY);
    if (g_missionLoop)
        MotionReplay();
    FxCompose(g_shownX, g_shownY);
    Locked src, dst;
    if (!Lock(g_canvas, DDLOCK_READONLY, &src))
        return;
    if (Lock(g_realBack, DDLOCK_WRITEONLY, &dst))
    {
        for (int y = 0; y < kScreenViewH; ++y)
            memcpy(dst.bits + y * dst.pitch, src.bits + (y + g_shownY) * src.pitch + g_shownX, kScreenViewW);
        for (int y = 0; y < kHudH; ++y)
            memcpy(dst.bits + (kScreenViewH + y) * dst.pitch, src.bits + (kHudY + y) * src.pitch, kScreenW);
        int count;
        const HudBlock* blocks = HudBlocks(&count);
        for (int i = 0; i < count; ++i)
        {
            const HudBlock& b = blocks[i];
            for (int y = 0; y < b.h; ++y)
                memcpy(dst.bits + (b.screenY + y) * dst.pitch + b.screenX,
                       src.bits + (b.canvasY + y) * src.pitch + b.canvasX, b.w);
        }
        CursorRectDraw(dst.bits, dst.pitch, g_shownX, g_shownY);
        g_realBack->Unlock(nullptr);
    }
    g_canvas->Unlock(nullptr);

    // Keep what the game sees under a still mouse in step with the moving camera.
    if (*game::currentScreenHandler == game::fnInGameScreenHandler && g_cursor.x < kScreenViewW &&
        g_cursor.y < kScreenViewH)
    {
        *game::mouseX = (short)(g_cursor.x + g_shownX);
        *game::mouseY = (short)(g_cursor.y + g_shownY);
    }
}

// The original flip draws the cursor into, and flips, the real back buffer.
void __cdecl BeforeFlip()
{
    if (!g_canvas)
    {
        WideMenuBeforeFlip();
        return;
    }
    Compose();
    *game::lpBackBuffer = g_realBack;
}

void __cdecl AfterFlip()
{
    if (!g_canvas)
    {
        WideMenuAfterFlip();
        return;
    }
    *game::lpBackBuffer = g_canvas;
    g_lastPresent = timeGetTime();
}

void __cdecl BeforeInGameFrame()
{
    ++g_missionLoop;
}

// Between game frames the mission idle loop presents the canvas again with the camera moved.
void __cdecl AfterInGameFrame()
{
    if (g_canvas && g_presentIntervalMs && *game::currentScreenHandler == game::fnInGameScreenHandler &&
        !*game::endScreen && timeGetTime() - g_lastPresent >= (DWORD)g_presentIntervalMs)
        WatcomCall(game::fnFlip);
    --g_missionLoop;
}

WRAP(SetVideoMode, BeforeSetVideoMode, AfterSetVideoMode)
WRAP(Flip, BeforeFlip, AfterFlip)
WRAP(InGameFrame, BeforeInGameFrame, AfterInGameFrame)

const BYTE kSetVideoModeOrig[] = { 0x51, 0x56, 0x57, 0x55, 0x89, 0xE5 };   // push ecx/esi/edi/ebp ; mov ebp, esp
const BYTE kFlipOrig[] = { 0x53, 0x51, 0x52, 0x57, 0x55 };                 // push ebx/ecx/edx/edi/ebp
const BYTE kInGameFrameOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57 };          // push ebx/ecx/edx/esi/edi

void PatchOperand(DWORD instr, int operandOffset, const void* target)
{
    DWORD address = (DWORD)target;
    PatchWrite(instr + operandOffset, &address, sizeof(address));
}
}  // namespace

void CanvasInstall(const char* iniPath)
{
    if (!HookVerify(game::fnSetVideoMode, kSetVideoModeOrig, sizeof(kSetVideoModeOrig), "canvas SetVideoMode") ||
        !HookVerify(game::fnFlip, kFlipOrig, sizeof(kFlipOrig), "canvas Flip") ||
        !HookVerify(game::fnInGameFrame, kInGameFrameOrig, sizeof(kInGameFrameOrig), "canvas InGameFrame") ||
        !HookVerify(kCursorReadX, kCursorReadXOrig, sizeof(kCursorReadXOrig), "canvas cursor x") ||
        !HookVerify(kCursorReadY, kCursorReadYOrig, sizeof(kCursorReadYOrig), "canvas cursor y") ||
        !HookVerify(kFlipWaits, kFlipWaitsOrig, sizeof(kFlipWaitsOrig), "canvas flip waits"))
        return;
    int hz = GetPrivateProfileIntA("Video", "PresentHz", 60, iniPath);
    g_presentIntervalMs = hz > 0 ? 1000 / hz : 0;
    g_cursor.x = *game::mouseX;
    g_cursor.y = *game::mouseY;
    PatchOperand(kCursorReadX, 2, &g_cursor.pad);
    PatchOperand(kCursorReadY, 1, &g_cursor.x);
    t_SetVideoMode = HookDetour(game::fnSetVideoMode, kSetVideoModeOrig, sizeof(kSetVideoModeOrig), Wrap_SetVideoMode, "canvas SetVideoMode");
    t_Flip = HookDetour(game::fnFlip, kFlipOrig, sizeof(kFlipOrig), Wrap_Flip, "canvas Flip");
    t_InGameFrame = HookDetour(game::fnInGameFrame, kInGameFrameOrig, sizeof(kInGameFrameOrig), Wrap_InGameFrame, "canvas InGameFrame");
    Log("canvas: installed (%dx%d, map margin %d, present %d Hz)", kCanvasW, kCanvasH, kMargin, hz);
}

bool CanvasActive()
{
    return g_canvas != nullptr;
}

void* CanvasSurface()
{
    return g_canvas;
}

void CanvasShownOffset(int* x, int* y)
{
    *x = g_shownX;
    *y = g_shownY;
}

void CanvasSetScreenMouse(int x, int y)
{
    g_cursor.x = (short)x;
    g_cursor.y = (short)y;
}

void CanvasScreenMouse(int* x, int* y)
{
    *x = g_cursor.x;
    *y = g_cursor.y;
}

LPARAM CanvasTranslateMouse(LPARAM screen)
{
    int x = GET_X_LPARAM(screen), y = GET_Y_LPARAM(screen);
    if (y < kScreenViewH)
        return MAKELPARAM(x + g_shownX, y + g_shownY);
    int count;
    const HudBlock* blocks = HudBlocks(&count);
    for (int i = 0; i < count; ++i)
    {
        const HudBlock& b = blocks[i];
        if (x >= b.screenX && x < b.screenX + b.w && y >= b.screenY && y < b.screenY + b.h)
            return MAKELPARAM(x - b.screenX + b.canvasX, y - b.screenY + b.canvasY);
    }
    // The rest of the HUD: an empty part of the column, where the game finds nothing to click.
    return MAKELPARAM(kColumnX + kColumnW / 2, kColumnH + 40);
}
