#include "camera.h"

#include <windows.h>
#include <windowsx.h>
#include <math.h>
#include <mmsystem.h>

#include "game.h"
#include "hook.h"
#include "layout.h"
#include "log.h"
#include "patch.h"

using namespace layout;

namespace
{
constexpr int kMarginTiles = kMargin / kTile;
constexpr double kMaxCamX = kMaxOriginX * kTile + 2 * kMargin;
constexpr double kMaxCamY = kMaxOriginY * kTile + 2 * kMargin;
// The canvas is redrawn every game frame (62 ms) around where the camera will be half a frame
// later, so within one frame the camera can travel two margins minus a tile of rounding:
// 120 px per 62 ms, about 1900 px/s.
constexpr double kGameFrameSeconds = 0.062;
constexpr double kMaxSpeed = 1800;
constexpr double kGlideRate = 7;         // glide speed = distance * rate (ease-out), capped at kMaxSpeed
constexpr int kEdgePixels = 2;

bool g_enabled, g_started, g_edgeScroll, g_wheel, g_middleDrag;
int g_speedOverride, g_wheelPixels;

double g_camX, g_camY;          // world pixel at the top-left of the screen's map area
double g_targetX, g_targetY;    // where glides head to
double g_velX, g_velY;          // pixels/s of the last update, for placing the next canvas
int g_originX, g_originY;       // tile origin the canvas is drawn from (the game's scroll origin)
bool g_mapStale;                // the game redrew its map surface for an origin we then undid
bool g_jumped;                  // moved beyond the canvas; shown once the next game frame redraws it
DWORD g_lastUpdate;

bool g_keyLeft, g_keyRight, g_keyUp, g_keyDown;
int g_mouseX = -1, g_mouseY = -1;   // screen
struct
{
    bool active;
    int anchorX, anchorY;
    double camX, camY;
} g_drag;

double Clamp(double v, double lo, double hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

bool PlayerControlsCamera()
{
    return *game::currentScreenHandler == game::fnInGameScreenHandler && !*game::modalDialog && !*game::endScreen;
}

double Speed()
{
    if (g_speedOverride > 0)
        return g_speedOverride < kMaxSpeed ? g_speedOverride : kMaxSpeed;
    double s = 500 + 260 * *game::scrollSpeedOption;
    return s < kMaxSpeed ? s : kMaxSpeed;
}

void SetGameOrigin(int x, int y)
{
    *game::smallStartX = (BYTE)x;
    *game::smallStartY = (BYTE)y;
    *game::bigStartX = (BYTE)(x / 2);
    *game::bigStartY = (BYTE)(y / 2);
}

// The game scrolls by writing its origin (minimap, centring on a unit, group recall, loading).
// Its origin is the canvas top-left, one margin above-left of the screen's map area.
void TakeGameScrollRequest()
{
    int x = *game::smallStartX, y = *game::smallStartY;
    if (x == g_originX && y == g_originY)
        return;
    g_targetX = Clamp(x * kTile + kMargin, 0, kMaxCamX);
    g_targetY = Clamp(y * kTile + kMargin, 0, kMaxCamY);
    SetGameOrigin(g_originX, g_originY);
    g_mapStale = true;
}

void Start()
{
    g_originX = *game::smallStartX;
    g_originY = *game::smallStartY;
    g_camX = g_targetX = Clamp(g_originX * kTile + kMargin, 0, kMaxCamX);
    g_camY = g_targetY = Clamp(g_originY * kTile + kMargin, 0, kMaxCamY);
    g_velX = g_velY = 0;
    g_lastUpdate = timeGetTime();
    g_keyLeft = g_keyRight = g_keyUp = g_keyDown = false;
    g_drag.active = false;
    g_started = true;
}

// Glide towards the target: ease-out, straight line, capped at kMaxSpeed.
void Glide(double dt)
{
    double dx = g_targetX - g_camX, dy = g_targetY - g_camY, dist = sqrt(dx * dx + dy * dy);
    if (dist < 0.5)
    {
        g_camX = g_targetX;
        g_camY = g_targetY;
        return;
    }
    double speed = dist * kGlideRate;
    speed = speed > kMaxSpeed ? kMaxSpeed : speed;
    double step = speed * dt >= dist ? dist : speed * dt;
    g_camX += dx / dist * step;
    g_camY += dy / dist * step;
}

// Edge-scroll tests in InGameMouseTick; the canvas mouse cannot express the screen edge, so the
// camera does its own edge scrolling. The right/bottom immediates also hold +kDX/+kDY patches.
struct EdgeTest
{
    DWORD addr;
    BYTE opcode[5];
    int opcodeLen;
    int immOffset;
    int immSize;
    int neverValue;
};
const EdgeTest kEdgeTests[] = {
    { 0x41B044, { 0x66, 0x81, 0x3C, 0x24 }, 4, 4, 2, 0x7FFF },        // x > right edge
    { 0x41B0A0, { 0x66, 0x83, 0x3C, 0x24, 0x01 }, 5, 4, 1, 0x80 },    // x < 1
    { 0x41B104, { 0x66, 0x81, 0x7C, 0x24, 0x02 }, 5, 5, 2, 0x7FFF },  // y > bottom edge
    { 0x41B161, { 0x66, 0x83, 0x7C, 0x24, 0x02 }, 5, 5, 1, 0x80 },    // y < 1
};

bool DisableGameEdgeScroll()
{
    for (const EdgeTest& e : kEdgeTests)
        if (!HookVerify(e.addr, e.opcode, e.opcodeLen, "camera edge test"))
            return false;
    for (const EdgeTest& e : kEdgeTests)
        PatchWrite(e.addr + e.immOffset, &e.neverValue, e.immSize);
    return true;
}
}  // namespace

void CameraInstall(const char* iniPath)
{
    g_edgeScroll = GetPrivateProfileIntA("Controls", "EdgeScroll", 1, iniPath) != 0;
    g_speedOverride = GetPrivateProfileIntA("Controls", "ScrollSpeed", 0, iniPath);
    g_wheel = GetPrivateProfileIntA("Controls", "WheelScroll", 1, iniPath) != 0;
    g_wheelPixels = GetPrivateProfileIntA("Controls", "WheelPixels", 96, iniPath);
    g_middleDrag = GetPrivateProfileIntA("Controls", "MiddleDrag", 1, iniPath) != 0;
    if (!DisableGameEdgeScroll())
        return;
    g_enabled = true;
    Log("camera: smooth scrolling (edge %d, wheel %d px, middle drag %d)", g_edgeScroll, g_wheel ? g_wheelPixels : 0,
        g_middleDrag);
}

void CameraReset()
{
    g_started = false;
}

void CameraRedrawMap()
{
    g_mapStale = true;
}

void CameraFrameStart()
{
    if (!g_enabled)
        return;
    if (!g_started)
        Start();
    TakeGameScrollRequest();
    double aheadX = g_camX + g_velX * kGameFrameSeconds / 2, aheadY = g_camY + g_velY * kGameFrameSeconds / 2;
    int x = (int)Clamp(floor(aheadX / kTile + 0.5) - kMarginTiles, 0, kMaxOriginX);
    int y = (int)Clamp(floor(aheadY / kTile + 0.5) - kMarginTiles, 0, kMaxOriginY);
    if (x != g_originX || y != g_originY || g_mapStale)
    {
        SetGameOrigin(x, y);
        WatcomCall(game::fnRedrawMapSurface, 1);
        g_mapStale = false;
    }
    g_originX = x;
    g_originY = y;
    g_jumped = false;

    // The minimap's view rectangle follows the camera (hires.cpp); redraw it when it moved.
    static int shownX = -1, shownY = -1;
    int cx = (int)g_camX / (2 * kTile), cy = (int)g_camY / (2 * kTile);
    if (cx != shownX || cy != shownY)
        *game::panelDirtyBits |= 2;
    shownX = cx;
    shownY = cy;
}

void CameraUpdate()
{
    if (!g_enabled || !g_started)
        return;
    DWORD now = timeGetTime();
    double dt = (now - g_lastUpdate) / 1000.0;
    g_lastUpdate = now;
    dt = dt > 0.1 ? 0.1 : dt;
    TakeGameScrollRequest();
    if (!PlayerControlsCamera())
        return;

    double vx = 0, vy = 0, speed = Speed();
    if (!g_drag.active)
    {
        vx = (g_keyRight - g_keyLeft) * speed;
        vy = (g_keyDown - g_keyUp) * speed;
        if (g_edgeScroll && g_mouseX >= 0 && GetForegroundWindow() == *game::hwndMain)
        {
            if (g_mouseX < kEdgePixels)
                vx = -speed;
            else if (g_mouseX >= kScreenW - kEdgePixels)
                vx = speed;
            if (g_mouseY < kEdgePixels)
                vy = -speed;
            else if (g_mouseY >= kScreenH - kEdgePixels)
                vy = speed;
        }
    }
    double oldX = g_camX, oldY = g_camY;
    if (vx != 0 || vy != 0)
    {
        g_camX = g_targetX = Clamp(g_camX + vx * dt, 0, kMaxCamX);
        g_camY = g_targetY = Clamp(g_camY + vy * dt, 0, kMaxCamY);
    }
    else
        Glide(dt);

    // Only what the canvas holds can be shown; the next game frame re-centres the origin.
    if (g_jumped)
        return;
    g_camX = Clamp(Clamp(g_camX, 0, kMaxCamX), g_originX * kTile, g_originX * kTile + 2 * kMargin);
    g_camY = Clamp(Clamp(g_camY, 0, kMaxCamY), g_originY * kTile, g_originY * kTile + 2 * kMargin);
    if (dt > 0)
    {
        g_velX = (g_camX - oldX) / dt;
        g_velY = (g_camY - oldY) / dt;
    }
}

void CameraWorldPosition(int* x, int* y)
{
    if (!g_started)
    {
        *x = *game::smallStartX * kTile + kMargin;
        *y = *game::smallStartY * kTile + kMargin;
        return;
    }
    *x = (int)floor(g_camX + 0.5);
    *y = (int)floor(g_camY + 0.5);
}

void CameraJumpTo(int worldX, int worldY)
{
    if (!g_enabled || !g_started)
        return;
    g_camX = g_targetX = Clamp(worldX - kScreenViewW / 2, 0, kMaxCamX);
    g_camY = g_targetY = Clamp(worldY - kScreenViewH / 2, 0, kMaxCamY);
    g_velX = g_velY = 0;
    g_jumped = true;
}

void CameraOffset(int* x, int* y)
{
    if (!g_started)
    {
        *x = *y = kMargin;
        return;
    }
    *x = (int)Clamp(floor(g_camX + 0.5) - g_originX * kTile, 0, 2 * kMargin);
    *y = (int)Clamp(floor(g_camY + 0.5) - g_originY * kTile, 0, 2 * kMargin);
}

bool CameraOnMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!g_enabled || !g_started)
        return false;
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        bool down = msg == WM_KEYDOWN;
        switch (wParam)
        {
        case VK_LEFT: g_keyLeft = down; break;
        case VK_RIGHT: g_keyRight = down; break;
        case VK_UP: g_keyUp = down; break;
        case VK_DOWN: g_keyDown = down; break;
        default: return false;
        }
        return PlayerControlsCamera();
    }
    case WM_KILLFOCUS:
    case WM_ACTIVATEAPP:
        g_keyLeft = g_keyRight = g_keyUp = g_keyDown = false;
        g_drag.active = false;
        return false;
    case WM_MOUSEMOVE:
        g_mouseX = GET_X_LPARAM(lParam);
        g_mouseY = GET_Y_LPARAM(lParam);
        if (g_drag.active)
        {
            g_camX = g_targetX = Clamp(g_drag.camX - (g_mouseX - g_drag.anchorX), 0, kMaxCamX);
            g_camY = g_targetY = Clamp(g_drag.camY - (g_mouseY - g_drag.anchorY), 0, kMaxCamY);
        }
        return false;
    case WM_MOUSEWHEEL:
    {
        if (!g_wheel || !PlayerControlsCamera())
            return false;
        double step = -GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA * g_wheelPixels;
        if (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)
            g_targetX = Clamp(g_targetX + step, 0, kMaxCamX);
        else
            g_targetY = Clamp(g_targetY + step, 0, kMaxCamY);
        return true;
    }
    case WM_MBUTTONDOWN:
        if (!g_middleDrag || !PlayerControlsCamera())
            return false;
        g_drag = { true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_camX, g_camY };
        SetCapture(*game::hwndMain);
        return true;
    case WM_MBUTTONUP:
        if (!g_drag.active)
            return false;
        g_drag.active = false;
        ReleaseCapture();
        return true;
    }
    return false;
}
