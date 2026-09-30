#include "music.h"

#include <windows.h>

#include "hook.h"
#include "log.h"

namespace
{
CRITICAL_SECTION g_lock;

// Timer thread: the streaming section starts with "cmp dword [stream], 0" at 0x410B20 and every
// path through it reaches "call CursorTick (0x413C98)" at 0x410B5D.
constexpr DWORD kStreamStart = 0x410B20, kStreamStartResume = 0x410B27;
const BYTE kStreamStartOrig[] = { 0x83, 0x3D, 0x74, 0x2D, 0x4C, 0x00, 0x00 };   // cmp dword ptr [4C2D74h], 0
constexpr DWORD kStreamEnd = 0x410B5D, kStreamEndResume = 0x410B62;
const BYTE kStreamEndOrig[] = { 0xE8, 0x36, 0x31, 0x00, 0x00 };                 // call 413C98h
constexpr DWORD kCursorTick = 0x413C98;

constexpr DWORD kPlayMusic = 0x4114B8, kStopMusic = 0x41165C;
const BYTE kPlayMusicOrig[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };
const BYTE kStopMusicOrig[] = { 0x53, 0x51, 0x56, 0x57, 0x55, 0x83, 0xEC, 0x04 };

void* t_PlayMusic;
void* t_StopMusic;

void __cdecl LockMusic()
{
    EnterCriticalSection(&g_lock);
}

void __cdecl UnlockMusic()
{
    LeaveCriticalSection(&g_lock);
}

__declspec(naked) void StreamStartThunk()
{
    __asm
    {
        pushad
        call LockMusic
        popad
        cmp dword ptr ds:[0x4C2D74], 0
        push kStreamStartResume
        ret
    }
}

// Leaves the section, then runs the displaced call so that it returns to the thread body.
__declspec(naked) void StreamEndThunk()
{
    __asm
    {
        pushad
        call UnlockMusic
        popad
        push kStreamEndResume
        push kCursorTick
        ret
    }
}

WRAP(PlayMusic, LockMusic, UnlockMusic)
WRAP(StopMusic, LockMusic, UnlockMusic)
}  // namespace

void MusicInstall()
{
    if (!HookVerify(kStreamStart, kStreamStartOrig, sizeof(kStreamStartOrig), "music stream start") ||
        !HookVerify(kStreamEnd, kStreamEndOrig, sizeof(kStreamEndOrig), "music stream end") ||
        !HookVerify(kPlayMusic, kPlayMusicOrig, sizeof(kPlayMusicOrig), "music play") ||
        !HookVerify(kStopMusic, kStopMusicOrig, sizeof(kStopMusicOrig), "music stop"))
        return;
    InitializeCriticalSection(&g_lock);
    t_PlayMusic = HookDetour(kPlayMusic, kPlayMusicOrig, sizeof(kPlayMusicOrig), Wrap_PlayMusic, "music play");
    t_StopMusic = HookDetour(kStopMusic, kStopMusicOrig, sizeof(kStopMusicOrig), Wrap_StopMusic, "music stop");
    HookJump(kStreamStart, StreamStartThunk, sizeof(kStreamStartOrig));
    HookJump(kStreamEnd, StreamEndThunk, sizeof(kStreamEndOrig));
    Log("music: streaming thread and music changes serialised");
}
