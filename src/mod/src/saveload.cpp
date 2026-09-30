#include "saveload.h"

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "canvas.h"
#include "game.h"
#include "gametext.h"
#include "hires.h"
#include "hook.h"
#include "hudart.h"
#include "layout.h"
#include "log.h"
#include "widemenu.h"

namespace
{
// The four file-dialog wrappers (no arguments): each clears its path buffer, fills an
// OPENFILENAMEA and returns GetSaveFileNameA / GetOpenFileNameA's BOOL. Their callers then open
// the path buffer ("wb" / "rb") and run the game's own serialiser, or stop on 0.
struct DialogSite
{
    DWORD addr;
    const char* name;
};
// Indexed by mode: bit 0 = load, bit 1 = custom / multiplayer (.NSV).
const DialogSite kSites[] = {
    { 0x4445B0, "save campaign dialog" },
    { 0x444750, "load campaign dialog" },
    { 0x444680, "save network dialog" },
    { 0x444820, "load network dialog" },
};
const BYTE kDialogPrologue[] = { 0x53, 0x51, 0x52, 0x56, 0x57, 0x55, 0x83, 0xEC, 0x4C };   // push x6 ; sub esp, 4Ch

char* const kSavePath = (char*)0x4B48E2;
char* const kLoadPath = (char*)0x4B49E6;
constexpr int kPathSize = 0x104;
const char* const* const kSaveDir = (const char* const*)0x4B4AF4;       // "SAVES\"
const char* const* const kExtCampaign = (const char* const*)0x4B4AEC;   // "SAV"
const char* const* const kExtNetwork = (const char* const*)0x4B4AF0;    // "NSV"
constexpr DWORD kTitleBase = 0x80000294;   // + Mode: "War Wind - Save Campaign Game As" ...
constexpr DWORD kTextYes = 0x80000420, kTextNo = 0x80000421;

// Set by the callers while the Windows dialog is up: their screens then paint a GDI copy of the
// screen on WM_PAINT and show the system cursor on WM_SETCURSOR.
BYTE* const kGdiScreenFlags[] = { (BYTE*)0x4B76BB, (BYTE*)0x4B74BC };

// ---- layout (panel coordinates; the panel is centred on the screen's map area or the menu)

struct Rect
{
    int x, y, w, h;
    bool Has(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

constexpr int kPW = 460, kPH = 336;
constexpr int kRim = 8;            // raised rim of the carved background
constexpr int kGlyphH = 10;        // panel font
constexpr int kRowH = 14, kRows = 14, kScrollW = 8;
constexpr int kMaxName = 32;
const Rect kList = { 18, 40, kPW - 36, kRows * kRowH };
const Rect kField = { 76, kList.y + kList.h + 16, kPW - 18 - 76, 16 };
constexpr int kButtonY = kPH - 44, kButtonW = 100, kButtonH = 24;
const Rect kOk = { kPW - 18 - 2 * kButtonW - 14, kButtonY, kButtonW, kButtonH };
const Rect kCancel = { kPW - 18 - kButtonW, kButtonY, kButtonW, kButtonH };
const Rect kDelete = { 18, kButtonY, kButtonW, kButtonH };
const Rect kAsk = { (kPW - 320) / 2, 116, 320, 104 };
const Rect kYes = { kAsk.x + 50, kAsk.y + 64, 90, kButtonH };
const Rect kNo = { kAsk.x + kAsk.w - 50 - 90, kAsk.y + 64, 90, kButtonH };

enum Button { kNone, kButtonOk, kButtonCancel, kButtonDelete, kButtonYes, kButtonNo };
enum Ask { kAskNone, kAskOverwrite, kAskDelete };
enum Tone { kWell, kDark, kMid, kLight, kPale, kBack, kTones };

struct Entry
{
    char name[64];
    char date[24];
    char size[16];
    FILETIME time;
};

struct Screen
{
    int mode = 0;
    bool load = false;
    const char* ext = "SAV";
    char* out = nullptr;
    std::vector<Entry> entries;
    int sel = -1, top = 0;
    char name[kMaxName + 1] = {};
    char message[96] = {};
    Ask ask = kAskNone;
    char askText[128] = {};
    Button pressed = kNone;
    int mouseX = -1, mouseY = -1;   // panel coordinates
    int lastClickRow = -1;
    DWORD lastClickTime = 0;
    bool done = false;
    DWORD result = 0;

    // drawing
    IDirectDrawSurface* panel = nullptr;
    IDirectDrawSurface* target = nullptr;    // the canvas in hi-res missions, else the back buffer
    int screenX = 0, screenY = 0;            // panel on the screen
    int targetX = 0, targetY = 0;            // panel in the target surface
    int snapX = 0, snapY = 0, snapW = 0, snapH = 0;   // what the screen shows, in the target
    std::vector<BYTE> original, dimmed;
    bool art = false;
    BYTE tone[kTones] = {};
};

Screen g_s;
bool g_active;

// ---- helpers

int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

bool Lock(IDirectDrawSurface* surface, DWORD flags, Pixels* out)
{
    DDSURFACEDESC desc = { sizeof(desc) };
    if (!surface || FAILED(surface->Lock(nullptr, &desc, flags | DDLOCK_WAIT, nullptr)))
        return false;
    *out = { (BYTE*)desc.lpSurface, desc.lPitch };
    return true;
}

bool ValidNameChar(int c)
{
    return c >= 32 && c < 127 && !strchr("\\/:*?\"<>|", c);
}

// ---- save files

void ScanSaves()
{
    g_s.entries.clear();
    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s*.%s", *kSaveDir, g_s.ext);
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const char* dot = strrchr(fd.cFileName, '.');
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !dot || _stricmp(dot + 1, g_s.ext) != 0)
            continue;
        Entry e = {};
        _snprintf_s(e.name, sizeof(e.name), _TRUNCATE, "%.*s", (int)(dot - fd.cFileName), fd.cFileName);
        e.time = fd.ftLastWriteTime;
        FILETIME local;
        SYSTEMTIME st;
        FileTimeToLocalFileTime(&fd.ftLastWriteTime, &local);
        FileTimeToSystemTime(&local, &st);
        _snprintf_s(e.date, sizeof(e.date), _TRUNCATE, "%04u-%02u-%02u  %02u:%02u", st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute);
        _snprintf_s(e.size, sizeof(e.size), _TRUNCATE, "%u KB", (fd.nFileSizeLow + 1023) / 1024);
        g_s.entries.push_back(e);
    } while (FindNextFileA(find, &fd));
    FindClose(find);
    std::sort(g_s.entries.begin(), g_s.entries.end(),
              [](const Entry& a, const Entry& b) { return CompareFileTime(&a.time, &b.time) > 0; });
}

// Full path of SAVES\<name>.<ext> into the game's path buffer.
bool BuildPath(const char* name)
{
    char relative[MAX_PATH];
    _snprintf_s(relative, sizeof(relative), _TRUNCATE, "%s%s.%s", *kSaveDir, name, g_s.ext);
    DWORD n = GetFullPathNameA(relative, kPathSize, g_s.out, nullptr);
    return n > 0 && n < (DWORD)kPathSize;
}

// ---- actions

void Finish(DWORD result)
{
    if (!result)
        memset(g_s.out, 0, kPathSize);
    g_s.result = result;
    g_s.done = true;
}

void ScrollTo(int row)
{
    int maxTop = (std::max)(0, (int)g_s.entries.size() - kRows);
    if (row < g_s.top)
        g_s.top = row;
    else if (row >= g_s.top + kRows)
        g_s.top = row - kRows + 1;
    g_s.top = Clamp(g_s.top, 0, maxTop);
}

void Select(int row)
{
    if (g_s.entries.empty())
        return;
    g_s.sel = Clamp(row, 0, (int)g_s.entries.size() - 1);
    ScrollTo(g_s.sel);
    if (!g_s.load)
        _snprintf_s(g_s.name, sizeof(g_s.name), _TRUNCATE, "%s", g_s.entries[g_s.sel].name);
    g_s.message[0] = 0;
}

void TrimmedName(char* out, size_t size)
{
    const char* p = g_s.name;
    while (*p == ' ')
        ++p;
    _snprintf_s(out, size, _TRUNCATE, "%s", p);
    size_t n = strlen(out);
    while (n && (out[n - 1] == ' ' || out[n - 1] == '.'))
        out[--n] = 0;
}

void Confirm()
{
    if (g_s.load)
    {
        if (g_s.sel >= 0 && BuildPath(g_s.entries[g_s.sel].name))
            Finish(1);
        return;
    }
    char name[kMaxName + 1];
    TrimmedName(name, sizeof(name));
    if (!*name)
    {
        strcpy_s(g_s.message, "Type a name for the saved game");
        return;
    }
    if (!BuildPath(name))
    {
        strcpy_s(g_s.message, "That name is too long");
        return;
    }
    CreateDirectoryA(*kSaveDir, nullptr);
    if (GetFileAttributesA(g_s.out) != INVALID_FILE_ATTRIBUTES)
    {
        g_s.ask = kAskOverwrite;
        _snprintf_s(g_s.askText, sizeof(g_s.askText), _TRUNCATE, "Overwrite \"%s\"?", name);
        return;
    }
    Finish(1);
}

void AskDelete()
{
    if (!g_s.load || g_s.sel < 0)
        return;
    g_s.ask = kAskDelete;
    _snprintf_s(g_s.askText, sizeof(g_s.askText), _TRUNCATE, "Delete \"%s\"?", g_s.entries[g_s.sel].name);
}

void Answer(bool yes)
{
    Ask ask = g_s.ask;
    g_s.ask = kAskNone;
    g_s.pressed = kNone;
    if (!yes)
        return;
    if (ask == kAskOverwrite)
        Finish(1);
    else if (ask == kAskDelete && g_s.sel >= 0)
    {
        char name[64];
        strcpy_s(name, g_s.entries[g_s.sel].name);
        bool deleted = BuildPath(name) && DeleteFileA(g_s.out);
        Log("saveload: delete %s -> %s", g_s.out, deleted ? "ok" : "FAILED");
        memset(g_s.out, 0, kPathSize);
        int sel = g_s.sel;
        ScanSaves();
        g_s.sel = -1;
        g_s.top = 0;
        Select(sel);
    }
}

void Activate(Button b)
{
    switch (b)
    {
    case kButtonOk: Confirm(); break;
    case kButtonCancel: Finish(0); break;
    case kButtonDelete: AskDelete(); break;
    case kButtonYes: Answer(true); break;
    case kButtonNo: Answer(false); break;
    default: break;
    }
}

Button ButtonAt(int x, int y)
{
    if (g_s.ask)
        return kYes.Has(x, y) ? kButtonYes : kNo.Has(x, y) ? kButtonNo : kNone;
    if (kOk.Has(x, y))
        return kButtonOk;
    if (kCancel.Has(x, y))
        return kButtonCancel;
    if (g_s.load && kDelete.Has(x, y))
        return kButtonDelete;
    return kNone;
}

bool HasScrollBar()
{
    return (int)g_s.entries.size() > kRows;
}

// ---- input

void OnKey(WPARAM vk)
{
    if (g_s.ask)
    {
        if (vk == VK_RETURN || vk == 'Y')
            Answer(true);
        else if (vk == VK_ESCAPE || vk == 'N')
            Answer(false);
        return;
    }
    int last = (int)g_s.entries.size() - 1;
    switch (vk)
    {
    case VK_ESCAPE: Finish(0); break;
    case VK_RETURN: Confirm(); break;
    case VK_UP: Select(g_s.sel < 0 ? 0 : g_s.sel - 1); break;
    case VK_DOWN: Select(g_s.sel + 1); break;
    case VK_PRIOR: Select(g_s.sel - kRows); break;
    case VK_NEXT: Select(g_s.sel < 0 ? kRows - 1 : g_s.sel + kRows); break;
    case VK_HOME: if (g_s.load) Select(0); break;
    case VK_END: if (g_s.load) Select(last); break;
    case VK_DELETE: AskDelete(); break;
    }
}

void OnChar(WPARAM c)
{
    if (g_s.ask || g_s.load)
        return;
    size_t n = strlen(g_s.name);
    if (c == '\b')
    {
        if (n)
            g_s.name[n - 1] = 0;
    }
    else if (ValidNameChar((int)c) && n < kMaxName)
    {
        g_s.name[n] = (char)c;
        g_s.name[n + 1] = 0;
    }
    else
        return;
    g_s.sel = -1;
    g_s.message[0] = 0;
}

void OnLeftDown(int x, int y)
{
    Button b = ButtonAt(x, y);
    if (b != kNone)
    {
        g_s.pressed = b;
        return;
    }
    if (g_s.ask || !kList.Has(x, y) || g_s.entries.empty())
        return;
    int n = (int)g_s.entries.size();
    if (HasScrollBar() && x >= kList.x + kList.w - kScrollW)
    {
        g_s.top = Clamp((y - kList.y) * (n - kRows + 1) / kList.h, 0, n - kRows);
        return;
    }
    int row = g_s.top + (y - kList.y) / kRowH;
    if (row >= n)
        return;
    DWORD now = GetTickCount();
    bool twice = row == g_s.lastClickRow && now - g_s.lastClickTime <= GetDoubleClickTime();
    Select(row);
    g_s.lastClickRow = twice ? -1 : row;
    g_s.lastClickTime = now;
    if (twice)
        Confirm();
}

void OnMessage(const MSG& m)
{
    switch (m.message)
    {
    case WM_KEYDOWN:
        OnKey(m.wParam);
        return;
    case WM_CHAR:
        OnChar(m.wParam);
        return;
    case WM_MOUSEWHEEL:
        if (!g_s.ask)
            g_s.top = Clamp(g_s.top - GET_WHEEL_DELTA_WPARAM(m.wParam) / WHEEL_DELTA * 3, 0,
                            (std::max)(0, (int)g_s.entries.size() - kRows));
        return;
    }
    if (m.message < WM_MOUSEFIRST || m.message > WM_MOUSELAST)
        return;
    // Screen coordinates (the game's resolution); the game's cursor follows them.
    int sx = GET_X_LPARAM(m.lParam), sy = GET_Y_LPARAM(m.lParam);
    CanvasSetScreenMouse(sx, sy);
    *game::mouseX = (short)sx;
    *game::mouseY = (short)sy;
    g_s.mouseX = sx - g_s.screenX;
    g_s.mouseY = sy - g_s.screenY;
    switch (m.message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        OnLeftDown(g_s.mouseX, g_s.mouseY);
        break;
    case WM_LBUTTONUP:
        if (g_s.pressed != kNone && ButtonAt(g_s.mouseX, g_s.mouseY) == g_s.pressed)
            Activate(g_s.pressed);
        g_s.pressed = kNone;
        break;
    case WM_RBUTTONUP:
        if (g_s.ask)
            Answer(false);
        break;
    }
}

// ---- colours

void PickColours(bool mission)
{
    g_s.art = mission && HudArtPrepare();
    if (g_s.art)
    {
        const HudTone tones[] = { HudTone::Well, HudTone::Dark, HudTone::Mid, HudTone::Light, HudTone::Pale, HudTone::Dark };
        for (int i = 0; i < kTones; ++i)
            g_s.tone[i] = HudColor(tones[i]);
    }

    PALETTEENTRY pal[256] = {};
    bool havePalette = PaletteRead(pal);
    auto nearest = [&](int r, int g, int b) { return PaletteNearest(pal, r, g, b); };

    // Menus use their own palette: a dark stone ramp from its nearest colours, dark enough for the
    // panel font to stand out.
    if (!g_s.art)
    {
        const int rgb[kTones][3] = { { 8, 6, 4 }, { 34, 28, 20 }, { 60, 50, 38 }, { 104, 88, 66 }, { 152, 132, 100 }, { 44, 37, 28 } };
        for (int i = 0; i < kTones; ++i)
            g_s.tone[i] = havePalette ? nearest(rgb[i][0], rgb[i][1], rgb[i][2]) : (BYTE)(i == kWell ? 0 : 7 + i);
    }

    // What the screen shows is dimmed behind the panel.
    g_s.dimmed = g_s.original;
    if (havePalette)
    {
        BYTE dim[256];
        for (int i = 0; i < 256; ++i)
            dim[i] = nearest(pal[i].peRed * 2 / 5, pal[i].peGreen * 2 / 5, pal[i].peBlue * 2 / 5);
        for (BYTE& px : g_s.dimmed)
            px = dim[px];
    }
}

// ---- painting

void Fill(const Pixels& p, int x, int y, int w, int h, Tone tone)
{
    HudArtFill(p, x, y, w, h, g_s.tone[tone]);
}

PlateTones Tones()
{
    return { g_s.tone[kWell], g_s.tone[kDark], g_s.tone[kMid], g_s.tone[kLight], g_s.tone[kPale] };
}

void Sunken(const Pixels& p, const Rect& r, Tone fill)
{
    PlateSunken(p, r.x, r.y, r.w, r.h, Tones(), g_s.tone[fill]);
}

void Raised(const Pixels& p, const Rect& r, Tone fill, bool pressed)
{
    PlateRaised(p, r.x, r.y, r.w, r.h, Tones(), g_s.tone[fill], pressed);
}

void PaintButton(const Pixels& p, const Rect& r, Button b)
{
    bool hot = ButtonAt(g_s.mouseX, g_s.mouseY) == b;
    bool down = hot && g_s.pressed == b;
    Raised(p, r, hot ? kLight : kMid, down);
}

void PaintPanel(const Pixels& p)
{
    if (g_s.art)
        HudArtBackground(p, 0, 0, kPW, kPH);
    else
    {
        Fill(p, 0, 0, kPW, kPH, kBack);
        Fill(p, 0, 0, kPW, kRim, kMid);
        Fill(p, 0, kRim - 2, kPW, 2, kDark);
    }
    // Outer frame: a dark outline around a raised edge.
    Fill(p, 0, 0, kPW, 1, kWell);
    Fill(p, 0, kPH - 1, kPW, 1, kWell);
    Fill(p, 0, 0, 1, kPH, kWell);
    Fill(p, kPW - 1, 0, 1, kPH, kWell);
    Fill(p, 1, 1, kPW - 2, 1, kPale);
    Fill(p, 1, 1, 1, kPH - 2, kPale);
    Fill(p, 1, kPH - 2, kPW - 2, 1, kDark);
    Fill(p, kPW - 2, 1, 1, kPH - 2, kDark);

    Sunken(p, kList, kWell);
    int rowW = kList.w - (HasScrollBar() ? kScrollW : 0);
    int hoverRow = !g_s.ask && kList.Has(g_s.mouseX, g_s.mouseY) && g_s.mouseX < kList.x + rowW
                       ? g_s.top + (g_s.mouseY - kList.y) / kRowH : -1;
    for (int i = 0; i < kRows; ++i)
    {
        int row = g_s.top + i;
        if (row >= (int)g_s.entries.size())
            break;
        if (row == g_s.sel)
            Fill(p, kList.x, kList.y + i * kRowH, rowW, kRowH, kMid);
        else if (row == hoverRow)
        {
            Fill(p, kList.x, kList.y + i * kRowH, rowW, 1, kDark);
            Fill(p, kList.x, kList.y + (i + 1) * kRowH - 1, rowW, 1, kDark);
        }
    }
    if (HasScrollBar())
    {
        int n = (int)g_s.entries.size(), x = kList.x + kList.w - kScrollW;
        int thumbH = (std::max)(12, kList.h * kRows / n);
        int thumbY = kList.y + (kList.h - thumbH) * g_s.top / (n - kRows);
        Fill(p, x, kList.y, kScrollW, kList.h, kDark);
        Raised(p, { x, thumbY, kScrollW, thumbH }, kMid, false);
    }

    if (!g_s.load)
        Sunken(p, kField, kWell);
    PaintButton(p, kOk, kButtonOk);
    PaintButton(p, kCancel, kButtonCancel);
    if (g_s.load)
        PaintButton(p, kDelete, kButtonDelete);

    if (g_s.ask)
    {
        Fill(p, kAsk.x - 1, kAsk.y - 1, kAsk.w + 2, kAsk.h + 2, kWell);
        Raised(p, kAsk, kBack, false);
        PaintButton(p, kYes, kButtonYes);
        PaintButton(p, kNo, kButtonNo);
    }
}

void PrintButton(const Rect& r, Button b, const char* label)
{
    bool down = ButtonAt(g_s.mouseX, g_s.mouseY) == b && g_s.pressed == b;
    PrintCentred(r.x + down, r.y + (r.h - kGlyphH) / 2 + down, r.w, label);
}

void PrintPanel()
{
    char title[96];
    const char* full = GameText(kTitleBase + g_s.mode);
    const char* dash = strstr(full, " - ");
    _snprintf_s(title, sizeof(title), _TRUNCATE, "%s", dash ? dash + 3 : full);
    PrintCentred(0, kRim + 8, kPW, title);

    int rowW = kList.w - (HasScrollBar() ? kScrollW : 0);
    const int dateX = kList.x + 236, nameW = dateX - kList.x - 16;
    for (int i = 0; i < kRows; ++i)
    {
        int row = g_s.top + i;
        if (row >= (int)g_s.entries.size())
            break;
        const Entry& e = g_s.entries[row];
        int y = kList.y + i * kRowH + (kRowH - kGlyphH) / 2;
        PrintClipped(kList.x + 6, y, e.name, nameW);
        PrintAt(dateX, y, e.date);
        PrintAt(kList.x + rowW - 6 - TextWidth(e.size, (int)strlen(e.size)), y, e.size);
    }
    if (g_s.entries.empty())
        PrintCentred(kList.x, kList.y + kList.h / 2 - kGlyphH / 2, kList.w, "No saved games");

    if (!g_s.load)
    {
        PrintAt(18, kField.y + (kField.h - kGlyphH) / 2, "Name:");
        char shown[kMaxName + 2];
        bool caret = (GetTickCount() / 500) & 1;
        _snprintf_s(shown, sizeof(shown), _TRUNCATE, "%s%s", g_s.name, caret && !g_s.ask ? "_" : "");
        PrintClipped(kField.x + 4, kField.y + (kField.h - kGlyphH) / 2, shown, kField.w - 8);
    }
    if (g_s.message[0])
        PrintAt(18, kButtonY + (kButtonH - kGlyphH) / 2, g_s.message);

    PrintButton(kOk, kButtonOk, g_s.load ? "Load" : "Save");
    PrintButton(kCancel, kButtonCancel, "Cancel");
    if (g_s.load)
        PrintButton(kDelete, kButtonDelete, "Delete");

    if (g_s.ask)
    {
        PrintCentred(kAsk.x, kAsk.y + 24, kAsk.w, g_s.askText);
        PrintButton(kYes, kButtonYes, GameText(kTextYes));
        PrintButton(kNo, kButtonNo, GameText(kTextNo));
    }
}

// Text goes through the game's own font routines, pointed at the panel surface.
void PrintIntoPanel()
{
    DrawTarget target(g_s.panel, kPW, kPH);
    DWORD oldFont = WatcomCall(game::fnSelectFont, game::kPanelFontBase + 4 * (*game::playerRace & 3));
    PrintPanel();
    WatcomCall(game::fnSelectFont, oldFont);
}

void CopyRows(const Pixels& dst, int x, int y, const BYTE* src, int w, int h)
{
    for (int j = 0; j < h; ++j)
        memcpy(dst.bits + (y + j) * dst.pitch + x, src + j * w, w);
}

// Puts the screen's content back (dimmed while the panel is open) and the panel over it.
bool Present(bool withPanel)
{
    Pixels panel = {}, target;
    if (withPanel)
    {
        if (!Lock(g_s.panel, 0, &panel))
            return false;
        PaintPanel(panel);
        g_s.panel->Unlock(nullptr);
        PrintIntoPanel();
        if (!Lock(g_s.panel, DDLOCK_READONLY, &panel))
            return false;
    }
    if (Lock(g_s.target, DDLOCK_WRITEONLY, &target))
    {
        CopyRows(target, g_s.snapX, g_s.snapY, (withPanel ? g_s.dimmed : g_s.original).data(), g_s.snapW, g_s.snapH);
        if (withPanel)
            for (int j = 0; j < kPH; ++j)
                memcpy(target.bits + (g_s.targetY + j) * target.pitch + g_s.targetX, panel.bits + j * panel.pitch, kPW);
        g_s.target->Unlock(nullptr);
    }
    if (withPanel)
        g_s.panel->Unlock(nullptr);
    WatcomCall(game::fnFlip);
    return true;
}

// ---- open / close

bool Open()
{
    auto dd = (IDirectDraw*)*game::lpDirectDraw;
    if (!dd || !*game::lpBackBuffer)
        return false;
    DDSURFACEDESC desc = { sizeof(desc) };
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = kPW;
    desc.dwHeight = kPH;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    if (FAILED(dd->CreateSurface(&desc, &g_s.panel, nullptr)))
    {
        g_s.panel = nullptr;
        return false;
    }

    // Hi-res missions: the screen shows the canvas' map area at the camera offset (the menu is
    // drawn there too), composed at every flip; the wide menus compose their canvas likewise.
    // 640x480 menus: the visible picture is the primary; it is copied into the back buffer before
    // every flip, so either flip buffer shows the same.
    IDirectDrawSurface* source;
    int areaW, areaH;
    int areaX = 0, areaY = 0;   // where the area is on the screen
    g_s.target = (IDirectDrawSurface*)*game::lpBackBuffer;
    if (CanvasActive())
    {
        source = g_s.target;
        CanvasShownOffset(&g_s.snapX, &g_s.snapY);
        areaW = layout::kScreenViewW;
        areaH = layout::kScreenViewH;
    }
    else if (WideMenuShown())
    {
        // The wide menus' canvas holds the screen's 640x480 layout in the game's coordinates.
        source = g_s.target;
        g_s.snapX = g_s.snapY = 0;
        areaW = 640;
        areaH = 480;
        WideMenuOrigin(&areaX, &areaY);
    }
    else
    {
        source = (IDirectDrawSurface*)*game::lpPrimary;
        g_s.snapX = g_s.snapY = 0;
        areaW = *game::screenWidth;
        areaH = *game::screenHeight;
    }
    g_s.snapW = areaW;
    g_s.snapH = areaH;
    int panelX = (areaW - kPW) / 2, panelY = (areaH - kPH) / 2;
    g_s.screenX = areaX + panelX;
    g_s.screenY = areaY + panelY;
    g_s.targetX = g_s.snapX + panelX;
    g_s.targetY = g_s.snapY + panelY;

    Pixels src;
    if (!Lock(source, DDLOCK_READONLY, &src))
    {
        g_s.panel->Release();
        g_s.panel = nullptr;
        return false;
    }
    g_s.original.resize((size_t)areaW * areaH);
    for (int j = 0; j < areaH; ++j)
        memcpy(&g_s.original[(size_t)j * areaW], src.bits + (g_s.snapY + j) * src.pitch + g_s.snapX, areaW);
    source->Unlock(nullptr);
    return true;
}

void Close()
{
    Present(false);
    if (!CanvasActive() && !WideMenuShown())
        Present(false);   // both flip buffers
    g_s.panel->Release();
    g_s.panel = nullptr;
}

void DrainInput()
{
    MSG m;
    while (PeekMessageA(&m, nullptr, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}
    while (PeekMessageA(&m, nullptr, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {}
}

DWORD __cdecl RunScreen(int mode)
{
    g_s = Screen{};
    g_s.mode = mode;
    g_s.load = (mode & 1) != 0;
    g_s.ext = (mode & 2) ? *kExtNetwork : *kExtCampaign;
    g_s.out = g_s.load ? kLoadPath : kSavePath;
    memset(g_s.out, 0, kPathSize);

    DWORD handler = *game::currentScreenHandler;
    bool mission = handler == game::fnMenuScreenHandler || handler == game::fnInGameScreenHandler;
    HiresSuspendShift(true);
    if (!Open())
    {
        HiresSuspendShift(false);
        Log("saveload: cannot open the screen (handler %08X)", handler);
        return 0;
    }
    g_active = true;
    PickColours(mission);
    ScanSaves();
    if (g_s.load)
        Select(0);
    CanvasScreenMouse(&g_s.mouseX, &g_s.mouseY);
    g_s.mouseX -= g_s.screenX;
    g_s.mouseY -= g_s.screenY;
    Log("saveload: %s .%s screen (handler %08X, %s colours, %u saves)", g_s.load ? "load" : "save", g_s.ext, handler,
        g_s.art ? "race" : "palette", (unsigned)g_s.entries.size());

    BYTE gdiFlags[2];
    for (int i = 0; i < 2; ++i)
    {
        gdiFlags[i] = *kGdiScreenFlags[i];
        *kGdiScreenFlags[i] = 0;
    }
    // The callers hide the game's cursor for the Windows dialog; flips draw it while it is shown.
    bool showCursor = *game::displaySuspend <= 0;
    if (showCursor)
        WatcomCall(game::fnShowCursor);
    DrainInput();

    while (!g_s.done)
    {
        MSG m;
        while (!g_s.done && PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE))
        {
            if (m.message == WM_QUIT)
            {
                PostQuitMessage((int)m.wParam);
                Finish(0);
                break;
            }
            bool key = m.message >= WM_KEYFIRST && m.message <= WM_KEYLAST;
            bool mouse = m.message >= WM_MOUSEFIRST && m.message <= WM_MOUSELAST;
            if (key || mouse)
            {
                if (m.message == WM_KEYDOWN)
                    TranslateMessage(&m);
                OnMessage(m);
                continue;
            }
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
        if (g_s.done || !Present(true))
            break;
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
    }

    Close();
    if (showCursor)
        WatcomCall(game::fnHideCursor);
    for (int i = 0; i < 2; ++i)
        *kGdiScreenFlags[i] = gdiFlags[i];
    DrainInput();
    g_active = false;
    HiresSuspendShift(false);
    Log("saveload: %s", g_s.result ? g_s.out : "cancelled");
    return g_s.result;
}

// Replace the dialog wrappers: Watcom callers expect every register but eax preserved.
#define DIALOG_THUNK(name, mode)             \
    __declspec(naked) void name()            \
    {                                        \
        __asm { push ecx }                   \
        __asm { push edx }                   \
        __asm { push mode }                  \
        __asm { call RunScreen }             \
        __asm { add esp, 4 }                 \
        __asm { pop edx }                    \
        __asm { pop ecx }                    \
        __asm { ret }                        \
    }

DIALOG_THUNK(SaveCampaignThunk, 0)
DIALOG_THUNK(LoadCampaignThunk, 1)
DIALOG_THUNK(SaveNetworkThunk, 2)
DIALOG_THUNK(LoadNetworkThunk, 3)

void (*const kThunks[])() = { SaveCampaignThunk, LoadCampaignThunk, SaveNetworkThunk, LoadNetworkThunk };
}  // namespace

void SaveLoadInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("UI", "InGameSaveLoad", 1, iniPath))
        return;
    for (const DialogSite& site : kSites)
        if (!HookVerify(site.addr, kDialogPrologue, sizeof(kDialogPrologue), site.name))
            return;
    for (int i = 0; i < 4; ++i)
        HookJump(kSites[i].addr, (const void*)kThunks[i], sizeof(kDialogPrologue));
    Log("saveload: in-game save / load screen installed");
}

bool SaveLoadActive()
{
    return g_active;
}
