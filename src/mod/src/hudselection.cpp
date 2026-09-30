#include "hudselection.h"

#include <windows.h>

#include <string.h>

#include "game.h"
#include "hook.h"
#include "layout.h"
#include "selection.h"

namespace
{
constexpr int kHudTop = layout::kScreenViewH;     // screen y of the HUD

// Faces: 34x40 crops of the 55x71 panel portraits, two rows under the carved rim.
constexpr int kFacesX = 356, kFacesY = 12, kFaceW = 34, kFaceH = 40, kBarH = 4;
constexpr int kFacePitchX = 42, kFaceRowPitch = 56, kFaceCols = 11, kFaceRows = 2;
constexpr int kMaxFaces = kFaceCols * kFaceRows;
constexpr int kCropX = 10, kCropY = 6;            // face area inside the portrait

// Group plates 1..0 above the info well.
constexpr int kPlatesX = 836, kPlatesY = 14, kPlateW = 19, kPlateH = 24, kPlatePitch = 23, kGroups = 10;

// Unit data (see findings_hud.txt): unit ext records, per-type table, groups.
constexpr DWORD kUnitExt = 0x4EA7A0, kUnitExtSize = 0x78, kHpOffset = 0x10;
constexpr DWORD kTypeTable = 0x4B3C38, kTypeSize = 0x20;
constexpr DWORD kGroupLists = 0x4B99E0, kGroupSize = 18;
constexpr DWORD kPortraits = 0x800100D3;          // + race, frame = type slot - 1
constexpr DWORD kCreaturePortraits = 0x800100DD;  // frame from kCreatureFrames[type]
constexpr DWORD kCreatureFrames = 0x4B33D1;
constexpr DWORD kGroupPortraits = 0x800100DE;     // frame = race

struct Face
{
    WORD id;
    int x, y;    // screen
};

Face g_faces[kMaxFaces];
int g_faceCount;

const BYTE* TypeEntry(int type)
{
    return (const BYTE*)(kTypeTable + type * kTypeSize);
}

void HealthOf(WORD id, int* hp, int* maxHp)
{
    const game::Thing& t = game::things[id];
    *hp = *(const WORD*)(kUnitExt + t.ext * kUnitExtSize + kHpOffset);
    *maxHp = *(const WORD*)(TypeEntry(t.type) + 1);
}

void Portrait(WORD id, DWORD* res, DWORD* frame)
{
    int type = game::things[id].type;
    if (type < 0x0C)
    {
        *res = kCreaturePortraits;
        *frame = *(const BYTE*)(kCreatureFrames + type);
    }
    else if (type < 0x34)
    {
        *res = kPortraits + (type - 0x0C) / 10;
        *frame = TypeEntry(type)[0] - 1;
    }
    else
    {
        *res = kGroupPortraits;
        *frame = *game::playerRace;
    }
}

void CollectFaces()
{
    g_faceCount = 0;
    for (WORD id = SelectionFirst(); id && g_faceCount < kMaxFaces; id = SelectionNext(id))
    {
        const game::Thing& t = game::things[id];
        if (t.category != 2 || t.owner != *game::playerClan)
            continue;
        int n = g_faceCount;
        g_faces[g_faceCount++] = { id, kFacesX + (n % kFaceCols) * kFacePitchX,
                                   kHudTop + kFacesY + (n / kFaceCols) * kFaceRowPitch };
    }
}

int GroupSize(int group)
{
    const WORD* list = (const WORD*)(kGroupLists + group * kGroupSize * 2);
    int n = 0;
    while (n < (int)kGroupSize && list[n])
        ++n;
    return n;
}

int PlateX(int plate)
{
    return kPlatesX + plate * kPlatePitch;
}

// Plates are laid out 1..9, 0 like the number row.
int GroupOfPlate(int plate)
{
    return (plate + 1) % kGroups;
}

void DrawImageClipped(DWORD res, DWORD frame, int x, int y, int clipX, int clipY, int clipW, int clipH)
{
    int saved[4];
    memcpy(saved, game::clipRect, sizeof(saved));
    const int clip[4] = { clipX, clipX + clipW - 1, clipY, clipY + clipH - 1 };
    memcpy(game::clipRect, clip, sizeof(clip));
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
    memcpy(game::clipRect, saved, sizeof(saved));
}

void Print(int x, int y, const char* text)
{
    WatcomCall(game::fnSetTextPos, x, y);
    WatcomCall(game::fnPrintString, (DWORD)text);
}
}  // namespace

bool HudSelectionShowsFaces()
{
    return *game::curSelected == 0x801;
}

void HudSelectionPaint(const Pixels& p, int hudTop)
{
    CollectFaces();
    if (HudSelectionShowsFaces())
        for (int i = 0; i < g_faceCount; ++i)
        {
            int x = g_faces[i].x, y = g_faces[i].y - kHudTop + hudTop;
            HudArtWell(p, x, y, kFaceW, kFaceH + 2 + kBarH);
            int hp, maxHp;
            HealthOf(g_faces[i].id, &hp, &maxHp);
            int w = maxHp > 0 ? (hp * kFaceW + maxHp / 2) / maxHp : 0;
            w = w < 0 ? 0 : w > kFaceW ? kFaceW : w;
            HudArtFill(p, x, y + kFaceH + 2, w ? w : 1, kBarH, HudColor(HudTone::Blue));
        }
    for (int plate = 0; plate < kGroups; ++plate)
        HudArtWell(p, PlateX(plate), hudTop + kPlatesY, kPlateW, kPlateH,
                   GroupSize(GroupOfPlate(plate)) ? HudTone::Well : HudTone::Dark);
}

void HudSelectionDraw(int hudTop)
{
    if (HudSelectionShowsFaces())
        for (int i = 0; i < g_faceCount; ++i)
        {
            int x = g_faces[i].x, y = g_faces[i].y - kHudTop + hudTop;
            DWORD res, frame;
            Portrait(g_faces[i].id, &res, &frame);
            DrawImageClipped(res, frame, x - kCropX, y - kCropY, x, y, kFaceW, kFaceH);
        }
    for (int plate = 0; plate < kGroups; ++plate)
    {
        int group = GroupOfPlate(plate), size = GroupSize(group), x = PlateX(plate), y = hudTop + kPlatesY;
        char digit[2] = { (char)('0' + group), 0 };
        Print(x + 2, y + 1, digit);
        if (size)
        {
            char count[4];
            count[0] = size >= 10 ? (char)('0' + size / 10) : ' ';
            count[1] = (char)('0' + size % 10);
            count[2] = 0;
            Print(x + kPlateW - 1 - (int)WatcomCall(game::fnTextWidth, (DWORD)count, 2), y + 12, count);
        }
    }
}

bool HudSelectionClick(int x, int y, bool shift, bool act)
{
    if (HudSelectionShowsFaces())
        for (int i = 0; i < g_faceCount; ++i)
        {
            const Face& f = g_faces[i];
            if (x < f.x || y < f.y || x >= f.x + kFaceW || y >= f.y + kFaceH + 2 + kBarH)
                continue;
            if (!act)
                return true;
            if (shift)
                SelectionRemove(f.id);
            else
            {
                SelectionClear();
                SelectionAdd(f.id, true);
            }
            SelectionUpdateCurrent();
            *game::panelDirtyBits |= 0xF;
            return true;
        }
    for (int plate = 0; plate < kGroups; ++plate)
    {
        int px = PlateX(plate), py = kHudTop + kPlatesY;
        if (x >= px - 2 && y >= py && x < px - 2 + kPlatePitch && y < py + kPlateH)
        {
            if (act && GroupSize(GroupOfPlate(plate)))
                WatcomCall(game::fnRecallGroup, GroupOfPlate(plate));
            return true;
        }
    }
    return false;
}
