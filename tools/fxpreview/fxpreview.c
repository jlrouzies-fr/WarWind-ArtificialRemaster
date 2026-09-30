/*
 * Renders a Modern Graphics frame dump through the same wwfx pass chain cnc-ddraw uses, offline.
 *
 *   fxpreview <dump.wwfx> <out.png> [--scale N] [--shader path] [--plain] [--light-scale K]
 *             [--time seconds] [--set name=value ...]
 *
 * The dump (written by the War Wind HD dev command "fxdump") holds the frame's palette indices,
 * palette, side planes, lights and the WWFX_SETUP in use. The D3D9 device renders into the back
 * buffer of a window that is never shown; nothing appears on the desktop.
 */
#define WWFX_INTERNAL
#include "wwfx.h"

#include <windows.h>
#include <d3d9.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lodepng.h"

#define DUMP_MAGIC "WWFXDMP3"

typedef struct DUMP
{
    int width, height;
    unsigned char palette[256][4];   /* r, g, b, 0 */
    unsigned char* index;
    unsigned char* mask;
    unsigned char* shadow;
    unsigned char* fog;
    float world[2];
    WWFX_LIGHT lights[WWFX_MAX_LIGHTS];
    int light_count;
    WWFX_SETUP setup;
} DUMP;

static int fail(const char* what)
{
    fprintf(stderr, "fxpreview: %s\n", what);
    return 1;
}

static BOOL read_exact(FILE* f, void* p, size_t n)
{
    return fread(p, 1, n, f) == n;
}

static BOOL load_dump(const char* path, DUMP* d)
{
    FILE* f = NULL;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return FALSE;
    char magic[8];
    BOOL ok = read_exact(f, magic, 8) && memcmp(magic, DUMP_MAGIC, 8) == 0 &&
              read_exact(f, &d->width, 4) && read_exact(f, &d->height, 4) &&
              d->width > 0 && d->height > 0 && d->width <= 4096 && d->height <= 4096 &&
              read_exact(f, d->palette, sizeof(d->palette));
    size_t n = ok ? (size_t)d->width * d->height : 0;
    if (ok)
    {
        d->index = malloc(n);
        d->mask = malloc(n);
        d->shadow = malloc(n);
        d->fog = malloc(n);
        ok = d->index && d->mask && d->shadow && d->fog && read_exact(f, d->index, n) && read_exact(f, d->mask, n) &&
             read_exact(f, d->shadow, n) && read_exact(f, d->fog, n) && read_exact(f, d->world, sizeof(d->world)) &&
             read_exact(f, &d->light_count, 4) &&
             d->light_count >= 0 && d->light_count <= WWFX_MAX_LIGHTS &&
             read_exact(f, d->lights, sizeof(WWFX_LIGHT) * d->light_count) &&
             read_exact(f, &d->setup, sizeof(d->setup));
    }
    fclose(f);
    return ok;
}

static BOOL set_param(WWFX_PARAMS* p, const char* assignment)
{
    static const struct { const char* name; size_t offset; } kNames[] = {
        { "shadow_opacity", offsetof(WWFX_PARAMS, shadow_opacity) },
        { "shadow_softness", offsetof(WWFX_PARAMS, shadow_softness) },
        { "ao_strength", offsetof(WWFX_PARAMS, ao_strength) },
        { "ao_radius", offsetof(WWFX_PARAMS, ao_radius) },
        { "ambient", offsetof(WWFX_PARAMS, ambient) },
        { "light_intensity", offsetof(WWFX_PARAMS, light_intensity) },
        { "emissive_glow", offsetof(WWFX_PARAMS, emissive_glow) },
        { "bloom", offsetof(WWFX_PARAMS, bloom) },
        { "contrast", offsetof(WWFX_PARAMS, contrast) },
        { "saturation", offsetof(WWFX_PARAMS, saturation) },
        { "vignette", offsetof(WWFX_PARAMS, vignette) },
        { "sharpen", offsetof(WWFX_PARAMS, sharpen) },
        { "fog_opacity", offsetof(WWFX_PARAMS, fog_opacity) },
        { "water_waves", offsetof(WWFX_PARAMS, water_waves) },
        { "water_glints", offsetof(WWFX_PARAMS, water_glints) },
        { "water_foam", offsetof(WWFX_PARAMS, water_foam) },
    };
    const char* eq = strchr(assignment, '=');
    if (!eq)
        return FALSE;
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
    {
        if (strlen(kNames[i].name) == (size_t)(eq - assignment) &&
            strncmp(kNames[i].name, assignment, eq - assignment) == 0)
        {
            *(float*)((char*)p + kNames[i].offset) = (float)atof(eq + 1);
            return TRUE;
        }
    }
    return FALSE;
}

static int write_plain(const DUMP* d, const char* out, int scale)
{
    int w = d->width * scale, h = d->height * scale;
    unsigned char* rgb = malloc((size_t)w * h * 3);
    if (!rgb)
        return fail("out of memory");
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            const unsigned char* c = d->palette[d->index[(y / scale) * d->width + x / scale]];
            memcpy(rgb + ((size_t)y * w + x) * 3, c, 3);
        }
    unsigned err = lodepng_encode24_file(out, rgb, w, h);
    free(rgb);
    return err ? fail(lodepng_error_text(err)) : 0;
}

static BOOL fill_texture(IDirect3DTexture9* tex, const unsigned char* src, int width, int height, int bpp)
{
    D3DLOCKED_RECT lr;
    RECT rc = { 0, 0, width, height };
    if (FAILED(IDirect3DTexture9_LockRect(tex, 0, &lr, &rc, 0)))
        return FALSE;
    for (int y = 0; y < height; y++)
        memcpy((unsigned char*)lr.pBits + (size_t)y * lr.Pitch, src + (size_t)y * width * bpp, (size_t)width * bpp);
    IDirect3DTexture9_UnlockRect(tex, 0);
    return TRUE;
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return fail("usage: fxpreview <dump.wwfx> <out.png> [--scale N] [--shader path] [--plain] [--set name=value]");

    static DUMP d;
    if (!load_dump(argv[1], &d))
        return fail("cannot read dump");

    int scale = 3;
    float time = 0;
    BOOL plain = FALSE;
    for (int i = 3; i < argc; i++)
    {
        if (!strcmp(argv[i], "--scale") && i + 1 < argc)
            scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shader") && i + 1 < argc)
            strncpy_s(d.setup.shader_path, sizeof(d.setup.shader_path), argv[++i], _TRUNCATE);
        else if (!strcmp(argv[i], "--light-scale") && i + 1 < argc)
        {
            float k = (float)atof(argv[++i]);
            for (int l = 0; l < d.light_count; l++)
                d.lights[l].radius *= k;
        }
        else if (!strcmp(argv[i], "--time") && i + 1 < argc)
            time = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--plain"))
            plain = TRUE;
        else if (!strcmp(argv[i], "--set") && i + 1 < argc)
        {
            if (!set_param(&d.setup.params, argv[++i]))
                return fail("unknown --set parameter");
        }
        else
            return fail("unknown argument");
    }
    if (scale < 1 || scale > 4)
        return fail("scale must be 1..4");
    if (plain)
        return write_plain(&d, argv[2], scale);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "fxpreview";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "fxpreview", WS_POPUP, 0, 0, 16, 16, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd)
        return fail("CreateWindow failed");

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
        return fail("Direct3DCreate9 failed");
    D3DPRESENT_PARAMETERS pp = { 0 };
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.BackBufferWidth = d.width * scale;
    pp.BackBufferHeight = d.height * scale;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9* dev = NULL;
    if (FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev)))
        return fail("CreateDevice failed");

    int tex_w = 1, tex_h = 1;
    while (tex_w < d.width) tex_w <<= 1;
    while (tex_h < d.height) tex_h <<= 1;
    IDirect3DTexture9* index_tex = NULL;
    IDirect3DTexture9* palette_tex = NULL;
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, tex_w, tex_h, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, &index_tex, NULL)) ||
        FAILED(IDirect3DDevice9_CreateTexture(dev, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &palette_tex, NULL)))
        return fail("texture creation failed");
    unsigned char bgrx[256][4];
    for (int i = 0; i < 256; i++)
    {
        bgrx[i][0] = d.palette[i][2];
        bgrx[i][1] = d.palette[i][1];
        bgrx[i][2] = d.palette[i][0];
        bgrx[i][3] = 255;
    }
    fill_texture(index_tex, d.index, d.width, d.height, 1);
    fill_texture(palette_tex, &bgrx[0][0], 256, 1, 4);   /* like cnc-ddraw: 256x256, palette in row 0 */

    if (!wwfx_gpu_create(dev, d.width, d.height, tex_w, tex_h))
        return fail("wwfx_gpu_create failed");
    d.setup.version = WWFX_VERSION;
    wwfx_setup(&d.setup);
    WWFX_FRAME frame = { d.width, d.height, d.mask, d.width, d.shadow, d.width, d.fog, d.width, d.lights, d.light_count, d.world[0], d.world[1] };
    wwfx_submit(&frame);
    wwfx_latch();
    wwfx_gpu_upload();

    WWFX_TARGET target = { index_tex, palette_tex, tex_w, tex_h, { 0, 0, pp.BackBufferWidth, pp.BackBufferHeight, 0, 1 }, time };
    IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 255), 1.0f, 0);
    IDirect3DDevice9_BeginScene(dev);
    BOOL drawn = wwfx_gpu_render(&target);
    IDirect3DDevice9_EndScene(dev);
    if (!drawn)
        return fail("wwfx_gpu_render failed (see wwfx.log next to fxpreview.exe)");

    IDirect3DSurface9* back = NULL;
    IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    wwfx_request_screenshot(argv[2]);
    wwfx_gpu_capture(back);
    IDirect3DSurface9_Release(back);

    wwfx_gpu_release();
    IDirect3DTexture9_Release(index_tex);
    IDirect3DTexture9_Release(palette_tex);
    IDirect3DDevice9_Release(dev);
    IDirect3D9_Release(d3d);
    DestroyWindow(hwnd);
    printf("%s\n", argv[2]);
    return 0;
}
