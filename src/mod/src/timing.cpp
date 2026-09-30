#include "timing.h"

#include <windows.h>
#include <mmsystem.h>

#include "game.h"
#include "hook.h"
#include "log.h"

#pragma comment(lib, "winmm.lib")

namespace
{
// Body of the game's timer thread: push 20 / call Sleep / add [clock], 20. It is followed by the
// music streaming and cursor animation, which stay as they are.
constexpr DWORD kTickStep = 0x410B07;
constexpr DWORD kTickStepResume = 0x410B17;
const BYTE kTickStepOrig[] = { 0x6A, 0x14, 0x2E, 0xFF, 0x15, 0xC4, 0x02, 0x62, 0x00,
                               0x83, 0x05, 0xEC, 0x29, 0x4C, 0x00, 0x14 };

constexpr DWORD kBodyPeriodMs = 20;   // the thread body's original cadence

DWORD g_lastReal;
DWORD g_lastBody;
DWORD g_remainder;               // clock advance below 1 ms, in 1/100 ms
int g_percent = 100;             // game clock speed relative to real time
bool g_started;

// Advances the clock with real time in 2 ms steps and returns to the thread body (music streaming,
// cursor animation) every 20 ms, its original cadence. The clock is advanced by atomic additions,
// like the original "add [clock], 20", so values the game stores itself (a loaded save's clock,
// a mission restart) stay the base its frame deadline is measured against.
void __cdecl ClockStep()
{
    for (;;)
    {
        Sleep(2);
        DWORD now = timeGetTime();
        if (!g_started)
        {
            g_lastReal = now;
            g_lastBody = now;
            g_started = true;
        }
        unsigned long long scaled = (unsigned long long)(now - g_lastReal) * g_percent + g_remainder;
        g_lastReal = now;
        g_remainder = (DWORD)(scaled % 100);
        InterlockedExchangeAdd((volatile LONG*)game::clockMs, (LONG)(scaled / 100));
        if (now - g_lastBody >= kBodyPeriodMs)
        {
            g_lastBody = now;
            return;
        }
    }
}

__declspec(naked) void ClockStepThunk()
{
    __asm
    {
        pushad
        call ClockStep
        popad
        push kTickStepResume
        ret
    }
}
}  // namespace

void TimingInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("Video", "PreciseClock", 1, iniPath))
        return;
    if (!HookVerify(kTickStep, kTickStepOrig, sizeof(kTickStepOrig), "precise clock"))
        return;
    g_percent = GetPrivateProfileIntA("Video", "GameSpeedPercent", 70, iniPath);
    g_percent = g_percent < 10 ? 10 : g_percent > 400 ? 400 : g_percent;
    timeBeginPeriod(1);
    HookJump(kTickStep, ClockStepThunk, sizeof(kTickStepOrig));
    Log("timing: game clock follows timeGetTime at %d%% (frames every %d ms)", g_percent, 62 * 100 / g_percent);
}
