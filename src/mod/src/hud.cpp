#include "hud.h"

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>

#include <stdio.h>
#include <string.h>

#include "canvas.h"
#include "controls.h"
#include "gametext.h"
#include "hudart.h"
#include "hudselection.h"
#include "game.h"
#include "hook.h"
#include "layout.h"
#include "log.h"
#include "patch.h"
#include "selection.h"

using namespace layout;

namespace
{
constexpr int kHudTop = kScreenViewH;   // screen y of the HUD

// HUD layout (screen x, y relative to the HUD top). The HUD is carved stone with sunken plates;
// the game's own panels are shown through pieces of its column (canvas x kColumnX + column x,
// rows as in the original 640x480 panel), and the mouse over a piece reaches the game there.
constexpr int kRim = 8;                                 // raised rim along the HUD top

struct Piece
{
    int x, y, w, h;          // HUD position
    int columnX, columnY;    // source in the column
    bool well;               // shown inside a sunken plate
};

const Piece kMinimap = { 10, 22, 97, 97, 12, 135, true };
const Piece kLeaderName = { 122, 20, 96, 16, 13, 5, true };
const Piece kResources = { 138, 46, 63, 24, 29, 107, false };
const Piece kOrbs = { 109, 76, kColumnW, 48, 0, 232, false };
const Piece kUnitCard = { 238, 10, 106, 116, 8, 278, false };
const Piece kBioDiagram = { 356, 40, 106, 58, 8, 395, false };
const Piece* const kPieces[] = { &kMinimap, &kLeaderName, &kResources, &kOrbs, &kUnitCard };

// Command grid: 5 x 3 slots in hotkey order (GridKeys), right-aligned.
constexpr int kCols = 5, kRows = 3, kSlot = 32, kSlotGap = 6;
constexpr int kGridW = kCols * kSlot + (kCols - 1) * kSlotGap, kGridH = kRows * kSlot + (kRows - 1) * kSlotGap;
constexpr int kGridX = kScreenW - kGridW - 12, kGridY = kHudTop + kRim + (kHudH - kRim - kGridH) / 2;
constexpr int kSlots = kCols * kRows;

// Info well (status text) between the selection and the grid.
constexpr int kInfoX = 836, kInfoY = 50, kInfoW = kGridX - 14 - kInfoX, kInfoH = 70;
constexpr int kLineH = 12;

HudBlock g_blocks[8];
int g_blockCount;

bool g_enabled;

// call DrawCommandPopup in InGameFrame: the grid replaces the floating popup.
constexpr DWORD kPopupDrawCall = 0x41AC24;
const BYTE kPopupDrawCallOrig[] = { 0xE8, 0xEB, 0x1D, 0x03, 0x00 };
const BYTE kNop5[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };

struct Commands
{
    int page;
    DWORD mask;
    int count;
};

int g_hoverSlot = -1;
bool g_pressed;

BYTE* PanelLevel(int level)
{
    return (BYTE*)(game::kPanelLevels + game::kPanelLevelSize * level);
}

bool OwnSelection()
{
    WORD first = SelectionFirst();
    return first && game::things[first].owner == *game::playerClan;
}

// OpenCommandPopup's per-unit state copies (see game.h), saved so a display-only evaluation
// leaves the game exactly as it was.
struct SavedCopies
{
    BYTE* where[0x800];
    BYTE value[0x800];
    int count;

    void Save()
    {
        count = 0;
        for (WORD id = SelectionFirst(); id && count < 0x800; id = SelectionNext(id))
        {
            const game::Thing& t = game::things[id];
            BYTE* p = nullptr;
            if (t.category == 2)
                p = (BYTE*)(game::kUnitExtCopy + t.ext * game::kUnitExtSize);
            else if (t.category == 3)
                p = (BYTE*)(game::kResExtCopy + t.ext * game::kResExtSize);
            if (p)
            {
                where[count] = p;
                value[count++] = *p;
            }
        }
    }

    void Restore()
    {
        for (int i = 0; i < count; ++i)
            *where[i] = value[i];
    }
} g_saved;

// The buttons the active level shows, or level 0 for the selection when no level exists yet
// (as the game computes it for a hotkey).
bool CurrentCommands(Commands* out)
{
    if (*game::panelDepth)
    {
        const BYTE* level = PanelLevel(*game::panelActive);
        out->page = *(WORD*)level;
        out->mask = *(DWORD*)(level + 2);
        out->count = *(WORD*)(level + 8);
        return out->mask != 0;
    }
    if (!OwnSelection())
        return false;
    BYTE panel[0xA0];
    memcpy(panel, (void*)0x4B60F0, sizeof(panel));
    g_saved.Save();
    bool ok = WatcomCall(game::fnOpenCommandPopup) != 0;
    if (ok)
    {
        const BYTE* level = PanelLevel(0);
        out->page = *(WORD*)level;
        out->mask = *(DWORD*)(level + 2);
        out->count = *(WORD*)(level + 8);
    }
    g_saved.Restore();
    memcpy((void*)0x4B60F0, panel, sizeof(panel));
    return ok && out->mask;
}

int BitOfSlot(DWORD mask, int slot)
{
    for (int bit = 0; bit < 32; ++bit)
        if ((mask >> bit) & 1 && slot-- == 0)
            return bit;
    return -1;
}

bool SlotAt(int x, int y, int* slot)
{
    x -= kGridX;
    y -= kGridY;
    if (x < 0 || y < 0 || x >= kGridW || y >= kGridH)
        return false;
    int col = x / (kSlot + kSlotGap), row = y / (kSlot + kSlotGap);
    if (x % (kSlot + kSlotGap) >= kSlot || y % (kSlot + kSlotGap) >= kSlot)
        return false;
    *slot = row * kCols + col;
    return true;
}

// ---- drawing into the canvas HUD strip

void DrawImage(DWORD res, DWORD frame, int x, int y)
{
    DWORD fn = game::fnDrawImage;
    __asm
    {
        push ebx
        mov eax, res
        mov edx, frame
        mov ebx, x
        mov ecx, y
        push 0
        call fn
        pop ebx
    }
}

// Command icons are drawn like the popup does (0x44CCBA): through DrawImage's second entry, which
// applies the frames' stored offset of (310, 230).
void DrawIcon(DWORD frame, int x, int y)
{
    DWORD fn = game::fnDrawImageOffset;
    x -= 0x136;
    y -= 0xE6;
    DWORD res = game::kCommandIcons;
    __asm
    {
        push ebx
        mov eax, res
        mov edx, frame
        mov ebx, x
        mov ecx, y
        push 0
        call fn
        pop ebx
    }
}

void FillRect(int x, int y, int w, int h, int color)
{
    DWORD fn = game::fnFillRect;
    __asm
    {
        push ebx
        mov eax, x
        mov edx, y
        mov ebx, w
        mov ecx, h
        push color
        call fn
        pop ebx
    }
}

int CanvasY(int screenY)
{
    return screenY - kHudTop + kHudY;
}

int SlotX(int slot)
{
    return kGridX + (slot % kCols) * (kSlot + kSlotGap);
}

int SlotY(int slot)
{
    return kGridY + (slot / kCols) * (kSlot + kSlotGap);
}

bool SingleUnitSelected()
{
    WORD cur = *game::curSelected;
    return cur && cur < 0x800;
}

void AddBlock(const Piece& piece)
{
    g_blocks[g_blockCount++] = { piece.x, kHudTop + piece.y, piece.w, piece.h,
                                 kColumnX + piece.columnX, piece.columnY };
}

void UpdateBlocks()
{
    g_blockCount = 0;
    for (const Piece* piece : kPieces)
        AddBlock(*piece);
    if (SingleUnitSelected() && !HudSelectionShowsFaces())
        AddBlock(kBioDiagram);
}

// Background, plates and slot wells, painted straight into the locked canvas.
void PaintPanels(const Pixels& p)
{
    int top = kHudY;
    HudArtBackground(p, 0, top, kScreenW, kHudH);
    for (const Piece* piece : kPieces)
        if (piece->well)
            HudArtWell(p, piece->x, top + piece->y, piece->w, piece->h);
    HudArtWell(p, kInfoX, top + kInfoY, kInfoW, kInfoH);
    HudSelectionPaint(p, top);
    if (g_enabled)
        for (int slot = 0; slot < kSlots; ++slot)
        {
            int x = SlotX(slot), y = CanvasY(SlotY(slot));
            HudArtWell(p, x, y, kSlot, kSlot, slot == g_hoverSlot ? HudTone::Mid : HudTone::Well);
        }
}

bool PaintLocked()
{
    auto canvas = (IDirectDrawSurface*)CanvasSurface();
    DDSURFACEDESC desc = { sizeof(desc) };
    if (!canvas || FAILED(canvas->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr)))
        return false;
    PaintPanels({ (BYTE*)desc.lpSurface, desc.lPitch });
    canvas->Unlock(nullptr);
    return true;
}

void DrawSlotContent(int slot, const Commands& commands)
{
    int bit = BitOfSlot(commands.mask, slot);
    if (bit < 0)
        return;
    int x = SlotX(slot), y = CanvasY(SlotY(slot));
    DWORD command = game::pageFirstButton[commands.page] + bit;
    DrawIcon(2 * command + (slot == g_hoverSlot ? 1 : 0), x + (kSlot - 24) / 2, y + (kSlot - 24) / 2);
    char key = ControlsGridKey(slot);
    if (key)
    {
        char text[2] = { key, 0 };
        PrintAt(x + kSlot - 6, y + kSlot - 8, text);
    }
}

// Copies text without the hotkey markers ("<M>ove" -> "Move").
// What the game's status bar shows now, as the bar itself picks it (0x4171D4).
bool StatusMessage(char* out, size_t size)
{
    if (!*game::statusTimer)
        return false;
    const char* text = *game::statusResource
                           ? GameText(*game::statusResource)
                           : game::statusText;
    CopyPlain(text ? text : "", out, size);
    return out[0] != 0;
}

// Marks a space the word wrap must not break at; printed as a space.
constexpr char kKeepTogether = '';

// Without a message: the selection by unit type, e.g. "3 Executioner, 2 Rogue".
void SelectionSummary(char* out, size_t size)
{
    int types[64] = {}, counts[64] = {}, kinds = 0;
    for (WORD id = SelectionFirst(); id; id = SelectionNext(id))
    {
        const game::Thing& t = game::things[id];
        if (t.category != 2 || t.owner != *game::playerClan)
            continue;
        int k = 0;
        while (k < kinds && types[k] != t.type)
            ++k;
        if (k == kinds && kinds < 64)
            types[kinds++] = t.type;
        if (k < kinds)
            ++counts[k];
    }
    out[0] = 0;
    size_t n = 0;
    for (int k = 0; k < kinds && n + 1 < size; ++k)
    {
        char name[64];
        CopyPlain(GameText(game::kUnitNameBase + types[k]), name, sizeof(name));
        for (char* c = name; *c; ++c)
            if (*c == ' ')
                *c = kKeepTogether;
        n += _snprintf_s(out + n, size - n, _TRUNCATE, "%s%d%c%s", k ? ",  " : "", counts[k], kKeepTogether, name);
    }
}

// Status message (or the selection summary) word-wrapped into the info well.
void DrawInfoText()
{
    char message[256];
    if (!StatusMessage(message, sizeof(message)))
        SelectionSummary(message, sizeof(message));
    bool keep[sizeof(message)] = {};
    for (size_t i = 0; message[i]; ++i)
        if (message[i] == kKeepTogether)
        {
            message[i] = ' ';
            keep[i] = true;
        }
    int y = CanvasY(kHudTop + kInfoY) + 3;
    int lines = (CanvasY(kHudTop + kInfoY + kInfoH) - y) / kLineH;
    PrintWrapped(kInfoX + 5, y, kInfoW - 10, kLineH, lines, message, keep);
}

void ShowLabel(const Commands& commands, int slot)
{
    int bit = BitOfSlot(commands.mask, slot);
    if (bit < 0)
        return;
    DWORD index = game::raceLabelBase[*game::playerRace] + game::pageFirstButton[commands.page] + bit;
    static char label[96];
    CopyPlain(GameText(0x80000000 | index), label, sizeof(label) - 8);
    size_t n = strlen(label);
    char key = ControlsGridKey(slot);
    if (key)
        _snprintf_s(label + n, sizeof(label) - n, _TRUNCATE, "  [%c]", key);
    WatcomCall(game::fnSetStatusText, (DWORD)label, 0xFFFF);
}

// Presses a slot the way the game's hotkey handler does (0x47B7BB): open level 0 for the
// selection when needed, then press the button with the panel in popup mode.
void PressSlot(int slot)
{
    if (*game::panelMode >= 5)
        return;
    if (!*game::panelDepth)
    {
        if (!OwnSelection())
            return;
        if (*game::cursorHidden)
        {
            *game::panelMode = 1;
            WatcomCall(game::fnClosePopupLevel);
            *game::cursorHidden = 0;
            WatcomCall(game::fnShowCursor);
        }
        if (!WatcomCall(game::fnOpenCommandPopup))
            return;
    }
    const BYTE* level = PanelLevel(*game::panelActive);
    int bit = BitOfSlot(*(DWORD*)(level + 2), slot);
    if (bit < 0)
    {
        if (*game::panelMode < 2)
            WatcomCall(game::fnResetPanel);
        return;
    }
    *game::panelMode = 2;
    WatcomCall(game::fnPanelPressButton, bit, slot);
    WatcomCall(game::fnPopupClick, 0);
}
}  // namespace

void HudInstall(const char* iniPath)
{
    g_enabled = GetPrivateProfileIntA("UI", "CommandGrid", 1, iniPath) != 0;
    if (g_enabled && HookVerify(kPopupDrawCall, kPopupDrawCallOrig, sizeof(kPopupDrawCallOrig), "hud popup"))
        PatchWrite(kPopupDrawCall, kNop5, sizeof(kNop5));
    Log("hud: bottom HUD, command grid %s", g_enabled ? "on" : "off");
}

bool HudGridEnabled()
{
    return g_enabled;
}

const HudBlock* HudBlocks(int* count)
{
    *count = g_blockCount;
    return g_blocks;
}

void HudDraw()
{
    if (!HudArtPrepare())
        return;
    UpdateBlocks();
    if (!PaintLocked())
        return;

    int saved[4];
    memcpy(saved, game::clipRect, sizeof(saved));
    const int strip[4] = { 0, kScreenW - 1, kHudY, kHudY + kHudH - 1 };
    memcpy(game::clipRect, strip, sizeof(strip));
    DWORD oldFont = WatcomCall(game::fnSelectFont, game::kPanelFontBase + 4 * *game::playerRace);

    if (g_enabled)
    {
        Commands commands;
        if (CurrentCommands(&commands))
        {
            for (int slot = 0; slot < kSlots; ++slot)
                DrawSlotContent(slot, commands);
            if (g_hoverSlot >= 0)
                ShowLabel(commands, g_hoverSlot);
        }
    }
    DrawInfoText();
    HudSelectionDraw(kHudY);

    WatcomCall(game::fnSelectFont, oldFont);
    memcpy(game::clipRect, saved, sizeof(saved));
}

bool HudOnMouse(UINT msg, int x, int y)
{
    if (!g_enabled)
        return false;
    int slot;
    bool inGrid = SlotAt(x, y, &slot);
    if (msg == WM_MOUSEMOVE)
    {
        g_hoverSlot = inGrid ? slot : -1;
        return false;
    }
    if (!inGrid && HudSelectionClick(x, y, false, false))
    {
        if (msg == WM_LBUTTONUP)
            HudSelectionClick(x, y, (GetKeyState(VK_SHIFT) & 0x8000) != 0, true);
        return msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK;
    }
    if (!inGrid && !(g_pressed && msg == WM_LBUTTONUP))
        return false;
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        g_pressed = true;
        return true;
    case WM_LBUTTONUP:
        if (g_pressed && inGrid)
            PressSlot(slot);
        g_pressed = false;
        return true;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return true;
    }
    return false;
}
