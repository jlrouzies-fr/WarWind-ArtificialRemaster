#pragma once
#include <windows.h>

struct IDirectDrawSurface;

// A locked 8-bit surface.
struct Pixels
{
    BYTE* bits;
    int pitch;
};

// An 8-bit system-memory DirectDraw surface, for the game's draw routines to target (DrawTarget)
// or for the mod's own painting (Lock).
class Offscreen
{
public:
    Offscreen() = default;
    Offscreen(const Offscreen&) = delete;
    Offscreen& operator=(const Offscreen&) = delete;
    ~Offscreen() { Release(); }

    // False before the game's DirectDraw exists.
    bool Create(int w, int h);
    void Release();
    bool Valid() const { return surface_ != nullptr; }
    IDirectDrawSurface* Surface() const { return surface_; }
    int Width() const { return w_; }
    int Height() const { return h_; }

    bool Lock(Pixels* out, bool readOnly = false);
    void Unlock();

private:
    IDirectDrawSurface* surface_ = nullptr;
    int w_ = 0, h_ = 0;
};

// Locks any DirectDraw surface (the game's own ones included).
bool LockSurface(IDirectDrawSurface* surface, Pixels* out, bool readOnly = false);

// While alive, the game's draw routines target `surface` (w x h), clipped to it.
class DrawTarget
{
public:
    DrawTarget(IDirectDrawSurface* surface, int w, int h);
    ~DrawTarget();
    DrawTarget(const DrawTarget&) = delete;
    DrawTarget& operator=(const DrawTarget&) = delete;

private:
    void* back_;
    int clip_[4];
};
