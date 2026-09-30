#include "video.h"

#include <windows.h>
#include <mfapi.h>
#include <mfmediaengine.h>

#include <stdio.h>
#include <string.h>

#include "game.h"
#include "hook.h"
#include "log.h"

namespace
{
constexpr DWORD kPlayVideoResume = 0x411D3D;  // after the 5 single-byte pushes we displace
const BYTE kPlayVideoOrig[] = { 0x53, 0x51, 0x56, 0x57, 0x55 };  // push ebx/ecx/esi/edi/ebp
const char kWindowClass[] = "WarWindHDVideo";

class EngineEvents : public IMFMediaEngineNotify
{
public:
    explicit EngineEvents(HANDLE done) : done_(done) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** out) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify))
        {
            *out = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return n;
    }
    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1, DWORD) override
    {
        if (event == MF_MEDIA_ENGINE_EVENT_ENDED)
            SetEvent(done_);
        else if (event == MF_MEDIA_ENGINE_EVENT_ERROR)
        {
            Log("video: media engine error %u", (unsigned)param1);
            SetEvent(done_);
        }
        return S_OK;
    }

private:
    LONG refs_ = 1;
    HANDLE done_;
};

bool FileExists(const char* path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// The game passes "VIDS\TH\TH1CH.AVI" relative to its Data folder (it prepends "DATA\" itself).
// Resolves to "<game>\Data\VIDS_HD\TH\TH1CH[_sub].mp4", or false if that file is absent.
bool ResolveHdPath(const char* gamePath, char* out, size_t outSize)
{
    char full[MAX_PATH];
    if (strchr(gamePath, ':') || gamePath[0] == '\\')
        lstrcpynA(full, gamePath, MAX_PATH);
    else
    {
        char exeDir[MAX_PATH];
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        *strrchr(exeDir, '\\') = 0;
        _snprintf_s(full, sizeof(full), _TRUNCATE, "%s\\Data\\%s", exeDir, gamePath);
    }
    char* vids = nullptr;
    for (char* p = full; (p = strchr(p, '\\')) != nullptr; ++p)
        if (!_strnicmp(p, "\\VIDS\\", 6))
            vids = p;
    char* dot = strrchr(full, '.');
    if (!vids || !dot || dot < vids)
        return false;
    *dot = 0;
    char stem[MAX_PATH];
    _snprintf_s(stem, sizeof(stem), _TRUNCATE, "%.*s\\VIDS_HD\\%s", (int)(vids - full), full, vids + 6);

    if (*game::optCinematicSubtitles)
    {
        _snprintf_s(out, outSize, _TRUNCATE, "%s_sub.mp4", stem);
        if (FileExists(out))
            return true;
    }
    _snprintf_s(out, outSize, _TRUNCATE, "%s.mp4", stem);
    return FileExists(out);
}

LRESULT CALLBACK VideoWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SETCURSOR)
    {
        SetCursor(nullptr);
        return TRUE;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

HWND CreateVideoWindow()
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = VideoWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = kWindowClass;
        registered = RegisterClassA(&wc) != 0;
    }
    // cnc-ddraw fakes GetClientRect/MapWindowPoints to report the game's own resolution;
    // GetWindowInfo is not hooked and gives the real client area in screen coordinates.
    HWND owner = *game::hwndMain;
    WINDOWINFO wi = { sizeof(wi) };
    GetWindowInfo(owner, &wi);
    const RECT& rc = wi.rcClient;
    return CreateWindowExA(0, kWindowClass, "", WS_POPUP | WS_VISIBLE, rc.left, rc.top,
                           rc.right - rc.left, rc.bottom - rc.top, owner, nullptr, GetModuleHandleA(nullptr), nullptr);
}

bool IsSkipKey(WPARAM vk)
{
    return vk == VK_ESCAPE || vk == VK_SPACE || vk == VK_RETURN;
}

// Skip on a key or click message delivered to this thread (the game window and the
// video window both belong to it), plus a foreground poll for the physical keys.
bool SkipRequested(const MSG& m, HWND videoWindow)
{
    if ((m.message == WM_KEYDOWN && IsSkipKey(m.wParam)) || m.message == WM_LBUTTONDOWN)
        return true;
    HWND fg = GetForegroundWindow();
    if (fg != videoWindow && fg != *game::hwndMain)
        return false;
    const int keys[] = { VK_ESCAPE, VK_SPACE, VK_RETURN, VK_LBUTTON };
    for (int vk : keys)
        if (GetAsyncKeyState(vk) & 1)
            return true;
    return false;
}

void DrainInput()
{
    MSG m;
    while (PeekMessageA(&m, nullptr, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}
    while (PeekMessageA(&m, nullptr, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {}
}

bool PlayFile(const char* path)
{
    HRESULT coInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    HWND window = CreateVideoWindow();

    IMFMediaEngineClassFactory* factory = nullptr;
    IMFAttributes* attrs = nullptr;
    IMFMediaEngine* engine = nullptr;
    auto* events = new EngineEvents(done);
    bool played = false;

    HRESULT hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = MFCreateAttributes(&attrs, 2);
    if (SUCCEEDED(hr))
        hr = attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, events);
    if (SUCCEEDED(hr))
        hr = attrs->SetUINT64(MF_MEDIA_ENGINE_PLAYBACK_HWND, (UINT64)(UINT_PTR)window);
    if (SUCCEEDED(hr))
        hr = factory->CreateInstance(0, attrs, &engine);
    if (SUCCEEDED(hr))
    {
        wchar_t wide[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH);
        BSTR url = SysAllocString(wide);
        hr = engine->SetSource(url);
        SysFreeString(url);
    }
    if (SUCCEEDED(hr))
        hr = engine->Play();

    if (SUCCEEDED(hr))
    {
        played = true;
        *game::videoPlaying = 1;
        const DWORD started = GetTickCount();
        const char* outcome = "ended";
        GetAsyncKeyState(VK_ESCAPE); GetAsyncKeyState(VK_SPACE); GetAsyncKeyState(VK_RETURN); GetAsyncKeyState(VK_LBUTTON);
        bool skip = false;
        while (!skip && MsgWaitForMultipleObjects(1, &done, FALSE, 15, QS_ALLINPUT) != WAIT_OBJECT_0)
        {
            MSG m = {};
            while (!skip && PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE))
            {
                skip = SkipRequested(m, window);
                if (m.message >= WM_KEYFIRST && m.message <= WM_KEYLAST)
                    continue;  // the game must not see keys pressed during the cinematic
                if (m.message >= WM_MOUSEFIRST && m.message <= WM_MOUSELAST)
                    continue;
                TranslateMessage(&m);
                DispatchMessageA(&m);
            }
            skip = skip || SkipRequested(MSG{}, window);
        }
        if (skip)
            outcome = "skipped";
        DrainInput();
        *game::videoPlaying = 0;
        Log("video: %s after %u ms", outcome, GetTickCount() - started);
    }
    else
        Log("video: cannot play %s (hr=%08X), using original player", path, hr);

    if (engine)
    {
        engine->Shutdown();
        engine->Release();
    }
    if (attrs)
        attrs->Release();
    if (factory)
        factory->Release();
    events->Release();
    DestroyWindow(window);
    CloseHandle(done);
    MFShutdown();
    if (SUCCEEDED(coInit))
        CoUninitialize();
    return played;
}

void ClearScreen()
{
    DWORD w = *game::screenWidth, h = *game::screenHeight, fill = game::fnFillRect, flip = game::fnFlip;
    __asm
    {
        push 0
        xor eax, eax
        xor edx, edx
        mov ebx, w
        mov ecx, h
        call fill
        call flip
    }
}

// Mirrors the bookkeeping the original PlayVideo does around playback so the
// callers' resume logic finds the state it expects.
void EnterVideoState()
{
    if (*game::displaySuspend == 1 && *game::displayFlag4C2BF4 == 0)
        WatcomCall(game::fnSuspendDisplay, (DWORD)*game::lpPrimary, *game::displayFlag4C2DAE ^ 1);
    --*game::displaySuspend;
}

void LeaveVideoState()
{
    ClearScreen();
    ClearScreen();
    *game::videoPlaying = 0;
}

bool __cdecl PlayVideoHD(const char* path, DWORD)
{
    char hd[MAX_PATH];
    if (!ResolveHdPath(path, hd, sizeof(hd)))
    {
        Log("video: %s: no HD version, original player", path);
        return false;
    }
    Log("video: %s -> %s", path, hd);
    EnterVideoState();
    if (!PlayFile(hd))
    {
        ++*game::displaySuspend;
        return false;
    }
    LeaveVideoState();
    return true;
}

// Replaces PlayVideo's first 5 bytes. Watcom callers expect every register except eax preserved.
__declspec(naked) void PlayVideoThunk()
{
    __asm
    {
        push eax
        push ecx
        push edx
        push edx
        push eax
        call PlayVideoHD
        add esp, 8
        test al, al
        pop edx
        pop ecx
        pop eax
        jz original
        ret
    original:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        push kPlayVideoResume
        ret
    }
}
}  // namespace

void VideoInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("Cutscenes", "HDVideos", 1, iniPath))
        return;
    if (!HookVerify(game::fnPlayVideo, kPlayVideoOrig, sizeof(kPlayVideoOrig), "HD videos"))
        return;
    HookJump(game::fnPlayVideo, PlayVideoThunk, sizeof(kPlayVideoOrig));
    Log("HD video player installed");
}
