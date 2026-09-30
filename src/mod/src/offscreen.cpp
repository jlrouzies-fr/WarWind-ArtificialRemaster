#include "offscreen.h"

#include <ddraw.h>
#include <string.h>

#include "game.h"

bool Offscreen::Create(int w, int h)
{
    Release();
    auto dd = (IDirectDraw*)*game::lpDirectDraw;
    if (!dd)
        return false;
    DDSURFACEDESC desc = { sizeof(desc) };
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    if (FAILED(dd->CreateSurface(&desc, &surface_, nullptr)))
    {
        surface_ = nullptr;
        return false;
    }
    w_ = w;
    h_ = h;
    return true;
}

void Offscreen::Release()
{
    if (surface_)
        surface_->Release();
    surface_ = nullptr;
    w_ = h_ = 0;
}

bool Offscreen::Lock(Pixels* out, bool readOnly)
{
    return LockSurface(surface_, out, readOnly);
}

void Offscreen::Unlock()
{
    surface_->Unlock(nullptr);
}

bool LockSurface(IDirectDrawSurface* surface, Pixels* out, bool readOnly)
{
    DDSURFACEDESC desc = { sizeof(desc) };
    DWORD flags = DDLOCK_WAIT | (readOnly ? DDLOCK_READONLY : 0);
    if (!surface || FAILED(surface->Lock(nullptr, &desc, flags, nullptr)))
        return false;
    out->bits = (BYTE*)desc.lpSurface;
    out->pitch = desc.lPitch;
    return true;
}

DrawTarget::DrawTarget(IDirectDrawSurface* surface, int w, int h)
    : back_(*game::lpBackBuffer)
{
    memcpy(clip_, game::clipRect, sizeof(clip_));
    const int clip[4] = { 0, w - 1, 0, h - 1 };
    memcpy(game::clipRect, clip, sizeof(clip));
    *game::lpBackBuffer = surface;
}

DrawTarget::~DrawTarget()
{
    *game::lpBackBuffer = back_;
    memcpy(game::clipRect, clip_, sizeof(clip_));
}
