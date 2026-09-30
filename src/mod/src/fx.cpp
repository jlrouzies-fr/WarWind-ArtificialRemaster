#include "fx.h"

#include <windows.h>
#include <ddraw.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "camera.h"
#include "canvas.h"
#include "game.h"
#include "hook.h"
#include "layout.h"
#include "log.h"
#include "wwfx.h"

using namespace layout;

namespace
{
// ---- what each RES.001 sprite is (units, buildings, effects ...); everything else is UI

constexpr BYTE kFogDither = 0xFF;   // pseudo-class: the dithered "not in sight" circles

BYTE SpriteClass(DWORD res)
{
    if ((res & 0x7FFF0000) != 0x00010000)
        return WWFX_CLASS_NONE;
    int i = res & 0xFFFF;
    if (i >= 3 && i <= 24) return WWFX_CLASS_TERRAIN;
    if (i >= 25 && i <= 27) return kFogDither;
    if (i >= 28 && i <= 105) return WWFX_CLASS_UNIT;
    if (i == 110 || i == 111) return WWFX_CLASS_EFFECT;          // building explosions
    if (i >= 106 && i <= 163) return WWFX_CLASS_BUILDING;
    if (i >= 164 && i <= 167) return WWFX_CLASS_DOODAD;          // rubble
    if (i >= 168 && i <= 172) return WWFX_CLASS_UNIT;            // vehicles
    if (i >= 175 && i <= 180) return WWFX_CLASS_DOODAD;          // fences, rocks, walls, trees
    if (i == 181) return WWFX_CLASS_EFFECT;
    if (i == 182 || i == 183) return WWFX_CLASS_DOODAD;
    if (i == 264 || i == 265) return WWFX_CLASS_TERRAIN;          // gravel roads, rubble: flat ground
    if (i == 255 || i == 256 || (i >= 258 && i <= 270)) return WWFX_CLASS_EFFECT;
    if (i == 275) return WWFX_CLASS_UNIT;
    return WWFX_CLASS_NONE;                                      // markers, icons, panels, fonts
}

// Near-black indices the art dithers shadows and fog with (one pixel on, one off).
bool IsDitherIndex(BYTE v)
{
    return v == 96 || v == 97 || v == 98 || v == 112 || v == 113 || v == 114;
}

// ---- settings

struct Settings
{
    bool enabled;
    UINT toggleKey;
    int shadowDX, shadowDY;               // unit drop shadows: offset of the silhouette, pixels
    float lightScale;
} g_settings;

WWFX_SETUP g_setup;
WWFX_SETUP_PROC g_wwfxSetup;
WWFX_SUBMIT_PROC g_wwfxSubmit;
WWFX_AVAILABLE_PROC g_wwfxAvailable;
WWFX_SCREENSHOT_PROC g_wwfxScreenshot;

float IniFloat(const char* key, float def, const char* ini)
{
    char buf[32], defText[32];
    sprintf_s(defText, "%g", def);
    GetPrivateProfileStringA("Graphics", key, defText, buf, sizeof(buf), ini);
    return (float)atof(buf);
}

void SetEmissive(int first, int last, BYTE r, BYTE g, BYTE b, int strengthFirst, int strengthLast)
{
    for (int i = first; i <= last; ++i)
    {
        int s = last == first ? strengthLast : strengthFirst + (strengthLast - strengthFirst) * (i - first) / (last - first);
        g_setup.emissive[i][0] = r;
        g_setup.emissive[i][1] = g;
        g_setup.emissive[i][2] = b;
        g_setup.emissive[i][3] = (BYTE)s;
    }
}

void LoadSettings(const char* ini, const char* gameDir)
{
    g_settings.enabled = GetPrivateProfileIntA("Graphics", "Modern", 1, ini) != 0;
    char key[16];
    GetPrivateProfileStringA("Graphics", "ToggleKey", "0x78", key, sizeof(key), ini);
    g_settings.toggleKey = strtoul(key, nullptr, 0);
    float angle = IniFloat("SunAngle", 35, ini) * 3.14159265f / 180;
    float length = IniFloat("ShadowLength", 5, ini);
    g_settings.shadowDX = (int)lroundf(cosf(angle) * length);
    g_settings.shadowDY = (int)lroundf(sinf(angle) * length);
    g_settings.lightScale = IniFloat("LightRadius", 0.5f, ini);

    g_setup.version = WWFX_VERSION;
    _snprintf_s(g_setup.shader_path, sizeof(g_setup.shader_path), _TRUNCATE, "%s\\WarWindHD\\shaders\\wwfx.hlsl", gameDir);
    WWFX_PARAMS& p = g_setup.params;
    p.shadow_opacity = IniFloat("ShadowOpacity", 0.5f, ini);
    p.shadow_softness = IniFloat("ShadowSoftness", 3.0f, ini);
    p.ao_strength = IniFloat("AmbientOcclusion", 0.45f, ini);
    p.ao_radius = IniFloat("AmbientOcclusionRadius", 10.0f, ini);
    p.ambient = IniFloat("Ambient", 0.92f, ini);
    p.light_intensity = IniFloat("LightIntensity", 1.0f, ini);
    p.emissive_glow = IniFloat("GlowRadius", 5.0f, ini);
    p.bloom = IniFloat("Bloom", 0.25f, ini);
    p.contrast = IniFloat("Contrast", 1.06f, ini);
    p.saturation = IniFloat("Saturation", 1.08f, ini);
    p.vignette = IniFloat("Vignette", 0.25f, ini);
    p.sharpen = IniFloat("Sharpen", 0.2f, ini);
    p.shadow_tint[0] = 0.62f;
    p.shadow_tint[1] = 0.66f;
    p.shadow_tint[2] = 0.80f;
    p.fog_opacity = IniFloat("FogOpacity", 0.45f, ini);
    p.water_waves = IniFloat("WaterWaves", 0.8f, ini);
    p.water_glints = IniFloat("WaterGlints", 1.0f, ini);
    p.water_foam = IniFloat("WaterFoam", 1.0f, ini);

    // Palette ramps that glow: fire (dark red -> yellow), bright cyan and bright blue.
    SetEmissive(8, 9, 255, 90, 40, 40, 70);
    SetEmissive(10, 15, 255, 150, 60, 110, 255);
    SetEmissive(244, 247, 90, 220, 255, 90, 150);
    SetEmissive(93, 95, 110, 170, 255, 70, 120);
}

char g_ddrawPath[MAX_PATH];
char g_iniPath[MAX_PATH];

// cnc-ddraw may not be initialised yet when the mod installs; its exports are looked up on first use.
bool ResolveExports()
{
    if (g_wwfxSubmit)
        return true;
    HMODULE ddraw = GetModuleHandleA(g_ddrawPath);
    if (!ddraw)
        return false;
    g_wwfxSetup = (WWFX_SETUP_PROC)GetProcAddress(ddraw, "WWFX_Setup");
    g_wwfxAvailable = (WWFX_AVAILABLE_PROC)GetProcAddress(ddraw, "WWFX_Available");
    g_wwfxScreenshot = (WWFX_SCREENSHOT_PROC)GetProcAddress(ddraw, "WWFX_Screenshot");
    auto submit = (WWFX_SUBMIT_PROC)GetProcAddress(ddraw, "WWFX_Submit");
    if (!g_wwfxSetup || !g_wwfxAvailable || !submit)
    {
        static bool logged;
        if (!logged)
            Log("fx: %s has no WWFX exports (not the War Wind HD cnc-ddraw build); Modern Graphics unavailable", g_ddrawPath);
        logged = true;
        return false;
    }
    g_wwfxSetup(&g_setup);
    g_wwfxSubmit = submit;
    Log("fx: renderer exports found, Modern Graphics %s", g_settings.enabled ? "on" : "off");
    return true;
}

volatile LONG g_forceFrames;   // developer dumps annotate frames even without the D3D9 renderer

bool Available()
{
    if (!CanvasActive() || !ResolveExports())
        return false;
    return g_forceFrames > 0 || (g_settings.enabled && g_wwfxAvailable());
}

// ---- planes (canvas coordinates) for the frame being drawn

BYTE g_mask[kCanvasH][kCanvasW];
BYTE g_shadow[kCanvasH][kCanvasW];
BYTE g_fog[kCanvasH][kCanvasW];
// The map surface's planes (same coordinates as the canvas map view): terrain, doodads, their shadows.
BYTE g_mapMask[kCanvasH][kCanvasW];
BYTE g_mapShadow[kCanvasH][kCanvasW];
bool g_mapValid;             // the map planes describe the map surface's current contents
bool g_mapActive;            // a map surface redraw is being annotated
WWFX_LIGHT g_lights[WWFX_MAX_LIGHTS];
int g_lightCount;
bool g_frameActive;          // the current game frame is being annotated
PALETTEENTRY g_palette[256];

// Screen planes, composed at every present.
BYTE g_screenMask[kScreenH][kScreenW];
BYTE g_screenShadow[kScreenH][kScreenW];
BYTE g_screenFog[kScreenH][kScreenW];
WWFX_LIGHT g_screenLights[WWFX_MAX_LIGHTS];
int g_screenLightCount;
int g_screenWorldX, g_screenWorldY;   // world pixel at the screen's top-left
bool g_screenReady;

char g_dumpPath[MAX_PATH];
volatile LONG g_dumpPending;     // write at the next present
volatile LONG g_dumpRequested;   // arm at the next frame start, once frames are annotated

void ReadPalette()
{
    auto* primary = (IDirectDrawSurface*)*game::lpPrimary;
    IDirectDrawPalette* palette = nullptr;
    if (primary && SUCCEEDED(primary->GetPalette(&palette)) && palette)
    {
        palette->GetEntries(0, 0, 256, g_palette);
        palette->Release();
    }
}

void Cover(BYTE (*plane)[kCanvasW], int x, int y)
{
    if (x >= 0 && y >= 0 && x < kViewW && y < kViewH)
        plane[y][x] = 255;
}

void AddLight(float x, float y, float radius, float r, float g, float b, float flicker)
{
    if (g_lightCount >= WWFX_MAX_LIGHTS)
        return;
    WWFX_LIGHT& l = g_lights[g_lightCount++];
    l.x = x;
    l.y = y;
    l.radius = radius;
    l.r = r;
    l.g = g;
    l.b = b;
    l.flicker = flicker;
    l.seed = fmodf(x * 0.137f + y * 0.311f, 6.2831853f);
}

// ---- sprite blits. DrawSprite's entry records the resource; its final SurfaceBlt call is bracketed
// by a snapshot of the destination, so the pixels the sprite covered are known. Two destinations
// are annotated: the canvas (things, effects, UI) and the map surface (terrain tiles, trees and
// other doodads), which PresentMapView copies 1:1 into the canvas map view every frame.

void* t_DrawSprite;
DWORD g_spriteRes;
DWORD g_surfaceBlt = 0x412FE0;
auto* const g_lpMapSurface = (IDirectDrawSurface**)0x542528;

struct Target
{
    IDirectDrawSurface* surface;
    BYTE (*mask)[kCanvasW];
    BYTE (*shadow)[kCanvasW];
    bool canvas;                 // things: lights, fog, projected shadows
};

struct Blit
{
    bool active;
    Target target;
    BYTE cls;
    int x0, y0, x1, y1;          // destination rect, exclusive right/bottom
    std::vector<BYTE> before;
    std::vector<BYTE> changed;
} g_blit;

struct Locked
{
    BYTE* bits;
    int pitch;
};

bool LockSurface(IDirectDrawSurface* surface, Locked* out)
{
    DDSURFACEDESC desc = { sizeof(desc) };
    if (!surface || FAILED(surface->Lock(nullptr, &desc, DDLOCK_WAIT, nullptr)))
        return false;
    out->bits = (BYTE*)desc.lpSurface;
    out->pitch = desc.lPitch;
    return true;
}

bool IsBaseTerrainTile(DWORD res)
{
    int i = res & 0xFFFF;
    return (res & 0x7FFF0000) == 0x00010000 && i >= 3 && i <= 12;   // opaque 48x48 ground
}

void __cdecl BeforeSpriteBlit(void* dst, const int* rect)
{
    g_blit.active = false;
    if (g_frameActive && dst == CanvasSurface())
        g_blit.target = { (IDirectDrawSurface*)dst, g_mask, g_shadow, true };
    else if (g_mapActive && dst == *g_lpMapSurface)
        g_blit.target = { (IDirectDrawSurface*)dst, g_mapMask, g_mapShadow, false };
    else
        return;
    int limitW = g_blit.target.canvas ? kCanvasW : kViewW, limitH = g_blit.target.canvas ? kCanvasH : kViewH;
    int x0 = max(rect[0], 0), y0 = max(rect[1], 0), x1 = min(rect[2], limitW), y1 = min(rect[3], limitH);
    if (x0 >= x1 || y0 >= y1)
        return;
    Locked c;
    if (!LockSurface(g_blit.target.surface, &c))
        return;
    int w = x1 - x0, h = y1 - y0;
    g_blit.before.resize((size_t)w * h);
    for (int y = 0; y < h; ++y)
        memcpy(&g_blit.before[(size_t)y * w], c.bits + (size_t)(y0 + y) * c.pitch + x0, w);
    g_blit.target.surface->Unlock(nullptr);
    g_blit.cls = SpriteClass(g_spriteRes);
    g_blit.x0 = x0;
    g_blit.y0 = y0;
    g_blit.x1 = x1;
    g_blit.y1 = y1;
    g_blit.active = true;
}

// Emissive pixels of one sprite, for a light at their centre.
struct Glow
{
    float x, y, weight, r, g, b;

    void Add(int px, int py, BYTE v)
    {
        float s = g_setup.emissive[v][3] / 255.0f;
        if (s <= 0)
            return;
        const PALETTEENTRY& c = g_palette[v];
        x += px * s;
        y += py * s;
        weight += s;
        r += c.peRed / 255.0f * g_setup.emissive[v][0] / 255.0f * s;
        g += c.peGreen / 255.0f * g_setup.emissive[v][1] / 255.0f * s;
        b += c.peBlue / 255.0f * g_setup.emissive[v][2] / 255.0f * s;
    }

    void Emit(BYTE cls)
    {
        if (weight < 1.5f)
            return;
        float k = cls == WWFX_CLASS_EFFECT ? 1.6f : 1.0f;
        float radius = min(12.0f + 7.0f * sqrtf(weight), 150.0f) * k * g_settings.lightScale;
        float n = 1.0f / weight;
        float warm = r > b ? 1.0f : 0.3f;
        AddLight(x * n, y * n, radius, r * n * 1.4f * k, g * n * 1.4f * k, b * n * 1.4f * k, warm);
    }
};

// Units are seen nearly from above: their shadow is their own silhouette, offset away from the sun.
// It touches the unit whichever way it faces; the post-process shows it on the ground only.
void DropShadow(const BYTE* changed, int w, int h)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (changed[y * w + x] == 1)
                Cover(g_shadow, g_blit.x0 + x + g_settings.shadowDX, g_blit.y0 + y + g_settings.shadowDY);
}

void __cdecl AfterSpriteBlit()
{
    if (!g_blit.active)
        return;
    g_blit.active = false;
    const Target& t = g_blit.target;
    Locked c;
    if (!LockSurface(t.surface, &c))
        return;
    int w = g_blit.x1 - g_blit.x0, h = g_blit.y1 - g_blit.y0;
    g_blit.changed.assign((size_t)w * h, 0);
    BYTE* changed = g_blit.changed.data();
    for (int y = 0; y < h; ++y)
    {
        const BYTE* row = c.bits + (size_t)(g_blit.y0 + y) * c.pitch + g_blit.x0;
        const BYTE* was = &g_blit.before[(size_t)y * w];
        for (int x = 0; x < w; ++x)
            changed[y * w + x] = row[x] != was[x];
    }

    BYTE cls = g_blit.cls;
    if (!t.canvas && IsBaseTerrainTile(g_spriteRes))
    {
        // Fresh ground over the whole tile: whatever stood there before is gone.
        for (int y = g_blit.y0; y < g_blit.y1; ++y)
        {
            memset(&t.mask[y][g_blit.x0], WWFX_CLASS_TERRAIN, w);
            memset(&t.shadow[y][g_blit.x0], 0, w);
        }
        t.surface->Unlock(nullptr);
        return;
    }

    bool scene = cls != WWFX_CLASS_NONE;
    bool dithers = scene && cls != WWFX_CLASS_TERRAIN;
    int dithered = 0;
    Glow glow = {};
    for (int y = 0; y < h; ++y)
    {
        BYTE* row = c.bits + (size_t)(g_blit.y0 + y) * c.pitch + g_blit.x0;
        int cy = g_blit.y0 + y;
        for (int x = 0; x < w; ++x)
        {
            if (!changed[y * w + x])
                continue;
            int cx = g_blit.x0 + x;
            BYTE v = row[x];
            bool isolated = (x == 0 || !changed[y * w + x - 1]) && (x == w - 1 || !changed[y * w + x + 1]) &&
                            (y == 0 || !changed[(y - 1) * w + x]) && (y == h - 1 || !changed[(y + 1) * w + x]);
            if (dithers && isolated && IsDitherIndex(v) && cy < kViewH && cx < kViewW)
            {
                // A checkerboard shadow or fog pixel: show what was under it again and let the
                // post-process shade the whole area smoothly instead.
                row[x] = g_blit.before[(size_t)y * w + x];
                changed[y * w + x] = 2;
                BYTE (*plane)[kCanvasW] = cls == kFogDither ? g_fog : t.shadow;
                Cover(plane, cx, cy);
                Cover(plane, cx - 1, cy);
                Cover(plane, cx + 1, cy);
                Cover(plane, cx, cy - 1);
                Cover(plane, cx, cy + 1);
                ++dithered;
                continue;
            }
            if (cls == kFogDither)
                continue;
            t.mask[cy][cx] = cls;
            if (cls == WWFX_CLASS_TERRAIN && !t.canvas)   // redrawn map ground; flat things on it keep shadows
                t.shadow[cy][cx] = 0;
            if (scene && t.canvas)
                glow.Add(cx, cy, v);
        }
    }
    t.surface->Unlock(nullptr);

    if (!t.canvas)
        return;
    if (cls == WWFX_CLASS_UNIT && dithered == 0)
        DropShadow(changed, w, h);
    if (scene && cls != kFogDither)
        glow.Emit(cls);
}

__declspec(naked) void DrawSpriteThunk()
{
    __asm
    {
        mov dword ptr [g_spriteRes], eax
        jmp dword ptr [t_DrawSprite]
    }
}

// Replaces "call SurfaceBlt" at the end of DrawSprite (eax src, edx src rect, ebx dst surface,
// ecx dst rect, stack: 2 arguments, callee pops them).
__declspec(naked) void SpriteBlitThunk()
{
    __asm
    {
        pushad
        push ecx
        push ebx
        call BeforeSpriteBlit
        add esp, 8
        popad
        push dword ptr [esp + 8]
        push dword ptr [esp + 8]
        call dword ptr [g_surfaceBlt]
        pushad
        call AfterSpriteBlit
        popad
        ret 8
    }
}

// ---- cursor: DrawCursor runs inside Flip after Compose, into the real back buffer

DWORD g_drawCursor = 0x413E30;
constexpr int kCursorBox = 64;
BYTE g_cursorBefore[kCursorBox * kCursorBox];
int g_cursorX0, g_cursorY0;
bool g_cursorSaved;

bool LockBack(Locked* out)
{
    auto* back = (IDirectDrawSurface*)*game::lpBackBuffer;
    DDSURFACEDESC desc = { sizeof(desc) };
    if (!back || FAILED(back->Lock(nullptr, &desc, DDLOCK_WAIT, nullptr)))
        return false;
    out->bits = (BYTE*)desc.lpSurface;
    out->pitch = desc.lPitch;
    return true;
}

void UnlockBack()
{
    ((IDirectDrawSurface*)*game::lpBackBuffer)->Unlock(nullptr);
}

void WriteDump(const Locked& back);

void __cdecl BeforeCursor()
{
    g_cursorSaved = false;
    if (!g_screenReady || *game::screenWidth != kScreenW)
        return;
    int mx, my;
    CanvasScreenMouse(&mx, &my);
    g_cursorX0 = min(max(mx - kCursorBox / 2, 0), kScreenW - kCursorBox);
    g_cursorY0 = min(max(my - kCursorBox / 2, 0), kScreenH - kCursorBox);
    Locked b;
    if (!LockBack(&b))
        return;
    for (int y = 0; y < kCursorBox; ++y)
        memcpy(g_cursorBefore + y * kCursorBox, b.bits + (size_t)(g_cursorY0 + y) * b.pitch + g_cursorX0, kCursorBox);
    UnlockBack();
    g_cursorSaved = true;
}

void __cdecl AfterCursor()
{
    if (!g_screenReady)
        return;
    g_screenReady = false;
    Locked b;
    bool locked = LockBack(&b);
    if (locked && g_cursorSaved)
        for (int y = 0; y < kCursorBox; ++y)
            for (int x = 0; x < kCursorBox; ++x)
                if (b.bits[(size_t)(g_cursorY0 + y) * b.pitch + g_cursorX0 + x] != g_cursorBefore[y * kCursorBox + x])
                    g_screenMask[g_cursorY0 + y][g_cursorX0 + x] = WWFX_CLASS_NONE;
    if (!locked && g_dumpPending)
        Log("fx: back buffer lock failed, dump postponed");
    if (locked && InterlockedExchange(&g_dumpPending, 0))
        WriteDump(b);
    if (locked)
        UnlockBack();

    WWFX_FRAME frame = {};
    frame.width = kScreenW;
    frame.height = kScreenH;
    frame.mask = &g_screenMask[0][0];
    frame.mask_pitch = kScreenW;
    frame.shadow = &g_screenShadow[0][0];
    frame.shadow_pitch = kScreenW;
    frame.fog = &g_screenFog[0][0];
    frame.fog_pitch = kScreenW;
    frame.lights = g_screenLights;
    frame.light_count = g_screenLightCount;
    frame.world_x = (float)g_screenWorldX;
    frame.world_y = (float)g_screenWorldY;
    g_wwfxSubmit(&frame);
    // After a developer dump the map surface goes back to what the renderer in use shows.
    if (g_forceFrames > 0 && InterlockedDecrement(&g_forceFrames) == 0)
        CameraRedrawMap();
}

__declspec(naked) void CursorThunk()
{
    __asm
    {
        pushad
        call BeforeCursor
        popad
        call dword ptr [g_drawCursor]
        pushad
        call AfterCursor
        popad
        ret
    }
}

// ---- developer dump for tools/fxpreview

void WriteDump(const Locked& back)
{
    FILE* f = nullptr;
    if (fopen_s(&f, g_dumpPath, "wb") != 0 || !f)
    {
        Log("fx: cannot write %s", g_dumpPath);
        return;
    }
    fwrite("WWFXDMP3", 1, 8, f);
    int size[2] = { kScreenW, kScreenH };
    fwrite(size, sizeof(size), 1, f);
    BYTE pal[256][4];
    for (int i = 0; i < 256; ++i)
    {
        pal[i][0] = g_palette[i].peRed;
        pal[i][1] = g_palette[i].peGreen;
        pal[i][2] = g_palette[i].peBlue;
        pal[i][3] = 0;
    }
    fwrite(pal, sizeof(pal), 1, f);
    for (int y = 0; y < kScreenH; ++y)
        fwrite(back.bits + (size_t)y * back.pitch, 1, kScreenW, f);
    fwrite(g_screenMask, sizeof(g_screenMask), 1, f);
    fwrite(g_screenShadow, sizeof(g_screenShadow), 1, f);
    fwrite(g_screenFog, sizeof(g_screenFog), 1, f);
    float world[2] = { (float)g_screenWorldX, (float)g_screenWorldY };
    fwrite(world, sizeof(world), 1, f);
    fwrite(&g_screenLightCount, sizeof(int), 1, f);
    fwrite(g_screenLights, sizeof(WWFX_LIGHT), g_screenLightCount, f);
    fwrite(&g_setup, sizeof(g_setup), 1, f);
    fclose(f);
    Log("fx: dump %s (%d lights)", g_dumpPath, g_screenLightCount);
}

// ---- frame start: PresentMapView has just copied the terrain into the canvas map view

void __cdecl TerrainPresented()
{
    g_frameActive = Available();
    if (!g_frameActive)
        return;
    ReadPalette();
    if (!g_mapValid)
        CameraRedrawMap();   // trees and their shadows are annotated from the next full redraw on
    for (int y = 0; y < kViewH; ++y)
    {
        if (g_mapValid)
        {
            memcpy(g_mask[y], g_mapMask[y], kViewW);
            memcpy(g_shadow[y], g_mapShadow[y], kViewW);
        }
        else
        {
            memset(g_mask[y], WWFX_CLASS_TERRAIN, kViewW);
            memset(g_shadow[y], 0, kViewW);
        }
        memset(g_fog[y], 0, kViewW);
    }
    g_lightCount = 0;
    if (g_mapValid && InterlockedExchange(&g_dumpRequested, 0))
        InterlockedExchange(&g_dumpPending, 1);
}

void __cdecl NoOp() {}

void* t_PresentMapView;
WRAP(PresentMapView, NoOp, TerrainPresented)

constexpr DWORD kPresentMapView = 0x4193D4;
const BYTE kPresentMapViewOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };   // push ebx/ecx/edx/esi/edi/ebp
constexpr DWORD kDrawSprite = 0x4138B0;
const BYTE kDrawSpriteOrig[] = { 0x56, 0x57, 0x55, 0x83, 0xEC, 0x28 };   // push esi/edi/ebp ; sub esp, 28h
constexpr DWORD kSpriteBlitCall = 0x413A52;
const BYTE kSpriteBlitCallOrig[] = { 0xE8, 0x89, 0xF5, 0xFF, 0xFF };      // call SurfaceBlt
constexpr DWORD kCursorCall = 0x4145C0;
const BYTE kCursorCallOrig[] = { 0xE8, 0x6B, 0xF8, 0xFF, 0xFF };          // call DrawCursor
}  // namespace

void FxInstall(const char* iniPath, const char* gameDir)
{
    LoadSettings(iniPath, gameDir);
    lstrcpynA(g_iniPath, iniPath, MAX_PATH);
    _snprintf_s(g_ddrawPath, sizeof(g_ddrawPath), _TRUNCATE, "%s\\ddraw.dll", gameDir);
    if (!HookVerify(kPresentMapView, kPresentMapViewOrig, sizeof(kPresentMapViewOrig), "fx PresentMapView") ||
        !HookVerify(kDrawSprite, kDrawSpriteOrig, sizeof(kDrawSpriteOrig), "fx DrawSprite") ||
        !HookVerify(kSpriteBlitCall, kSpriteBlitCallOrig, sizeof(kSpriteBlitCallOrig), "fx sprite blit") ||
        !HookVerify(kCursorCall, kCursorCallOrig, sizeof(kCursorCallOrig), "fx cursor"))
        return;
    t_PresentMapView = HookDetour(kPresentMapView, kPresentMapViewOrig, sizeof(kPresentMapViewOrig), Wrap_PresentMapView, "fx PresentMapView");
    t_DrawSprite = HookDetour(kDrawSprite, kDrawSpriteOrig, sizeof(kDrawSpriteOrig), DrawSpriteThunk, "fx DrawSprite");
    if (!t_PresentMapView || !t_DrawSprite)
        return;
    HookCall(kSpriteBlitCall, SpriteBlitThunk);
    HookCall(kCursorCall, CursorThunk);
    Log("fx: Modern Graphics hooks installed (%s, toggle key %02X)", g_settings.enabled ? "on" : "off", g_settings.toggleKey);
}

void FxMapRedrawBegin(bool full)
{
    g_mapActive = Available();
    if (!g_mapActive)
    {
        g_mapValid = false;
        return;
    }
    if (full)
    {
        for (int y = 0; y < kViewH; ++y)
        {
            memset(g_mapMask[y], WWFX_CLASS_TERRAIN, kViewW);
            memset(g_mapShadow[y], 0, kViewW);
        }
        g_mapValid = true;
    }
    else if (!g_mapValid)
        g_mapActive = false;
}

void FxMapRedrawEnd()
{
    g_mapActive = false;
}

void FxMarkUi(int x, int y, int w, int h)
{
    if (!g_frameActive)
        return;
    int x0 = max(x, 0), y0 = max(y, 0), x1 = min(x + w, kCanvasW), y1 = min(y + h, kCanvasH);
    for (int row = y0; row < y1; ++row)
        if (x1 > x0)
            memset(&g_mask[row][x0], WWFX_CLASS_NONE, x1 - x0);
}

void FxMarkScreenUi(int x, int y, int w, int h)
{
    if (!g_screenReady)
        return;
    int x0 = max(x, 0), y0 = max(y, 0), x1 = min(x + w, kScreenW), y1 = min(y + h, kScreenH);
    for (int row = y0; row < y1; ++row)
        if (x1 > x0)
            memset(&g_screenMask[row][x0], WWFX_CLASS_NONE, x1 - x0);
}

void FxCompose(int shownX, int shownY)
{
    if (!g_frameActive || !Available())
    {
        g_frameActive = false;
        return;
    }
    for (int y = 0; y < kScreenViewH; ++y)
    {
        memcpy(g_screenMask[y], &g_mask[y + shownY][shownX], kScreenW);
        memcpy(g_screenShadow[y], &g_shadow[y + shownY][shownX], kScreenW);
        memcpy(g_screenFog[y], &g_fog[y + shownY][shownX], kScreenW);
    }
    memset(g_screenMask[kScreenViewH], WWFX_CLASS_NONE, (size_t)kHudH * kScreenW);
    memset(g_screenShadow[kScreenViewH], 0, (size_t)kHudH * kScreenW);
    memset(g_screenFog[kScreenViewH], 0, (size_t)kHudH * kScreenW);
    g_screenLightCount = 0;
    for (int i = 0; i < g_lightCount; ++i)
    {
        WWFX_LIGHT l = g_lights[i];
        l.x -= shownX;
        l.y -= shownY;
        if (l.x + l.radius < 0 || l.y + l.radius < 0 || l.x - l.radius > kScreenW || l.y - l.radius > kScreenViewH)
            continue;
        g_screenLights[g_screenLightCount++] = l;
    }
    g_screenWorldX = *game::smallStartX * kTile + shownX;
    g_screenWorldY = *game::smallStartY * kTile + shownY;
    g_screenReady = true;
}

bool FxOnKeyDown(WPARAM vk)
{
    if (!g_wwfxSubmit || vk != g_settings.toggleKey)
        return false;
    FxToggle();
    return true;
}

void FxToggle()
{
    FxSetEnabled(!g_settings.enabled);
}

bool FxEnabled()
{
    return g_settings.enabled;
}

void FxSetEnabled(bool on)
{
    if (on == g_settings.enabled)
        return;
    g_settings.enabled = on;
    CameraRedrawMap();   // trees and other doodads carry the shadows of the new mode
    WritePrivateProfileStringA("Graphics", "Modern", on ? "1" : "0", g_iniPath);
    Log("fx: Modern Graphics %s", on ? "on" : "off");
}

void FxRequestDump(const char* path)
{
    lstrcpynA(g_dumpPath, path, MAX_PATH);
    InterlockedExchange(&g_forceFrames, 40);   // a few game frames, so one is annotated from its start
    InterlockedExchange(&g_dumpRequested, 1);
}

bool FxRequestScreenshot(const char* path)
{
    return g_wwfxScreenshot && g_wwfxScreenshot(path);
}
