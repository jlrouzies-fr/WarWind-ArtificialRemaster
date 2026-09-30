#include <windows.h>
#include <ddraw.h>

#include <stdio.h>
#include <vector>

#include "capture.h"
#include "game.h"
#include "log.h"

bool CaptureFrame(const char* path)
{
    return CaptureSurface(*game::lpPrimary, path);
}

bool CaptureSurface(void* target, const char* path)
{
    auto* surface = (IDirectDrawSurface*)target;
    auto* primary = (IDirectDrawSurface*)*game::lpPrimary;
    if (!surface || !primary)
    {
        Log("capture: no surface yet");
        return false;
    }

    PALETTEENTRY pal[256] = {};
    IDirectDrawPalette* palette = nullptr;
    if (SUCCEEDED(primary->GetPalette(&palette)) && palette)
    {
        palette->GetEntries(0, 0, 256, pal);
        palette->Release();
    }

    DDSURFACEDESC sd = {};
    sd.dwSize = sizeof(sd);
    HRESULT hr = surface->Lock(nullptr, &sd, DDLOCK_WAIT | DDLOCK_READONLY, nullptr);
    if (FAILED(hr))
    {
        Log("capture: Lock failed %08X", hr);
        return false;
    }
    const int w = (int)sd.dwWidth, h = (int)sd.dwHeight, pitch = sd.lPitch;
    const int rowBytes = (w + 3) & ~3;
    std::vector<BYTE> pixels((size_t)rowBytes * h);
    for (int y = 0; y < h; ++y)
        memcpy(&pixels[(size_t)(h - 1 - y) * rowBytes], (BYTE*)sd.lpSurface + (size_t)y * pitch, w);
    surface->Unlock(nullptr);

    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    RGBQUAD colors[256];
    for (int i = 0; i < 256; ++i)
        colors[i] = { pal[i].peBlue, pal[i].peGreen, pal[i].peRed, 0 };
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 8;
    ih.biClrUsed = 256;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih) + sizeof(colors);
    fh.bfSize = fh.bfOffBits + (DWORD)pixels.size();

    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") != 0 || !f)
        return false;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fwrite(colors, sizeof(colors), 1, f);
    fwrite(pixels.data(), pixels.size(), 1, f);
    fclose(f);
    Log("capture: %dx%d pitch %d -> %s", w, h, pitch, path);
    return true;
}
