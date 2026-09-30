#include "motion.h"

#include <ddraw.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "canvas.h"
#include "fx.h"
#include "game.h"
#include "hook.h"
#include "layout.h"
#include "log.h"

using namespace layout;

namespace
{
// ---- game side

// g_drawList header: +8 nodes used, +0x10 node pool; a record is node + 6 (see kb.h "map sprite chain").
auto* const drawList = (DWORD*)0x5425A0;
constexpr DWORD fnDrawListRender = 0x44442C;         // (eax = list): callback(eax = type, edx = record)
auto* const drawNodeCallback = (DWORD*)0x4B48C0;     // = DrawNodeCallback 0x4227FC while a mission runs
constexpr DWORD fnDrawShroud = 0x419B84;
auto* const shroudEnabled = (BYTE*)0x4B33D0;
constexpr DWORD fnDrawOrderLines = 0x419FE0;

// Click rectangles: RecordSpriteRect appends to list[cur ^ 1], FindThingAtScreenPos reads list[cur].
auto* const rectListCur = (BYTE*)0x4B33AC;
auto* const rectListCount = (int*)0x4B33B0;

// DrawGameFrame call sites.
constexpr DWORD kRenderCall = 0x41A9E6;              // DrawListRender(g_drawList)
const BYTE kRenderCallOrig[] = { 0xE8, 0x41, 0x9A, 0x02, 0x00 };
constexpr DWORD kMarkerCall = 0x41AAB4;              // DrawImage(move marker #192, frame, x, y, [0])
const BYTE kMarkerCallOrig[] = { 0xE8, 0xC3, 0x8F, 0xFF, 0xFF };
constexpr DWORD kOrderLinesCall = 0x41AAE1;          // DrawOrderLines, the first thing drawn after the fog
const BYTE kOrderLinesCallOrig[] = { 0xE8, 0xFA, 0xF4, 0xFF, 0xFF };
// QueueThingSprites (eax = category, edx = Thing*, ebx = id) queues every node a thing owns.
constexpr DWORD kQueueThingSprites = 0x42048C;
const BYTE kQueueThingSpritesOrig[] = { 0x56, 0x57, 0x55, 0x89, 0xE5 };   // push esi/edi/ebp ; mov ebp, esp

DWORD g_nodeCallback;        // the game's callback, called by NodeThunk
DWORD g_drawImage = game::fnDrawImage;
DWORD g_drawOrderLines = fnDrawOrderLines;
void* t_QueueThingSprites;

// ---- tick state

constexpr int kThings = 0x800;
constexpr int kOwnerCap = 8192;          // node pool slots tracked (hires pool is 8000)
constexpr int kAnchors = 4;              // sprites drawn with the thing's own id (shield ring, body, icons)
constexpr int kMaxStep = 32;             // larger moves in one tick are jumps (id reuse, loading), not motion

struct Anchor
{
    DWORD res;
    int x, y;                // world pixels
};
Anchor g_anchor[2][kThings][kAnchors];
BYTE g_anchorCount[2][kThings];
DWORD g_anchorFrame[2];
int g_cur;                   // anchors of the current tick

WORD g_owner[kOwnerCap];     // thing that queued each pool node this tick (0 = none)
int g_ownerFilled;
int g_queueDepth;
WORD g_queueId;
int g_queueStart;

short g_deltaX[kThings], g_deltaY[kThings];   // previous - current drawn position
short g_offX[kThings], g_offY[kThings];       // offset applied by the running replay
std::vector<WORD> g_moving;

enum Mode { kPass, kRecord, kReplay } g_mode;
BYTE g_record[30];

struct Tick
{
    bool valid;              // this DrawGameFrame rendered the sprite list
    bool snapped;            // the canvas after the fog pass is in g_snapshot
    bool done;               // DrawGameFrame finished; the overlay is known
    int originX, originY;    // small-tile scroll origin the list was queued for
    int clip[4];
    DWORD start, deadline;   // game clock when drawn / when the game flips it
} g_tick;
DWORD g_frame;

struct Marker
{
    bool valid;
    DWORD res, frame, x, y, flags;
} g_marker;

std::vector<BYTE> g_snapshot;
struct Run
{
    int offset, len;         // into the map view (y * kViewW + x)
};
std::vector<Run> g_runs;
std::vector<BYTE> g_overlay;

bool g_enabled;

// ---- node ownership

int NodesUsed()
{
    int used = (int)drawList[2];
    return used < kOwnerCap ? used : kOwnerCap;
}

void FillOwners(int end, WORD id)
{
    for (int i = g_ownerFilled; i < end; ++i)
        g_owner[i] = id;
    g_ownerFilled = end;
}

void __cdecl QueueEnter(DWORD id)
{
    if (g_queueDepth++)
        return;
    int start = NodesUsed();
    FillOwners(start, 0);
    g_queueId = (WORD)(id & (kThings - 1));
    g_queueStart = start;
}

void __cdecl QueueLeave()
{
    if (--g_queueDepth)
        return;
    FillOwners(NodesUsed(), g_queueId);
}

__declspec(naked) void QueueThingSpritesThunk()
{
    __asm
    {
        pushad
        push ebx
        call QueueEnter
        add esp, 4
        popad
        call dword ptr [t_QueueThingSprites]
        pushad
        call QueueLeave
        popad
        ret
    }
}

WORD OwnerOf(const BYTE* record)
{
    const BYTE* node = record - 6;
    const BYTE* pool = (const BYTE*)drawList[4];
    if (node < pool)
        return 0;
    int offset = (int)(node - pool);
    int index = offset / game::kDrawNodeSize;
    if (offset % game::kDrawNodeSize || index >= g_ownerFilled)
        return 0;
    return g_owner[index];
}

// ---- list render through the node callback

void RecordAnchor(const BYTE* record)
{
    WORD owner = OwnerOf(record);
    if (!owner || *(const WORD*)(record + 0x14) != owner)
        return;
    BYTE& count = g_anchorCount[g_cur][owner];
    if (count >= kAnchors)
        return;
    Anchor& a = g_anchor[g_cur][owner][count++];
    a.res = *(const DWORD*)(record + 4);
    a.x = *(const short*)record + g_tick.originX * kTile;
    a.y = *(const short*)(record + 2) + g_tick.originY * kTile;
}

const BYTE* __cdecl NodeRecord(DWORD type, const BYTE* record)
{
    if (g_mode == kRecord && type == 1)
        RecordAnchor(record);
    else if (g_mode == kReplay)
    {
        WORD owner = OwnerOf(record);
        if (owner && (g_offX[owner] || g_offY[owner]))
        {
            memcpy(g_record, record, sizeof(g_record));
            *(short*)g_record += g_offX[owner];
            *(short*)(g_record + 2) += g_offY[owner];
            return g_record;
        }
    }
    return record;
}

// Callback for DrawListRender: eax = node type, edx = record; draws NodeRecord's record instead.
__declspec(naked) void NodeThunk()
{
    __asm
    {
        pushad
        push edx
        push eax
        call NodeRecord
        add esp, 8
        mov dword ptr [esp + 20], eax    // the saved edx
        popad
        jmp dword ptr [g_nodeCallback]
    }
}

void RenderList(Mode mode)
{
    g_nodeCallback = *drawNodeCallback;
    *drawNodeCallback = (DWORD)NodeThunk;
    g_mode = mode;
    WatcomCall(fnDrawListRender, (DWORD)drawList);
    g_mode = kPass;
    *drawNodeCallback = g_nodeCallback;
}

// Things drawn this tick and on the previous one at a nearby position move between the two.
void FindMoving()
{
    g_moving.clear();
    int prev = g_cur ^ 1;
    if (g_anchorFrame[prev] + 1 != g_anchorFrame[g_cur])
        return;
    for (int id = 1; id < kThings; ++id)
    {
        int count = g_anchorCount[g_cur][id], prevCount = g_anchorCount[prev][id];
        bool matched = false;
        for (int i = 0; i < count && !matched; ++i)
            for (int j = 0; j < prevCount && !matched; ++j)
            {
                const Anchor& a = g_anchor[g_cur][id][i];
                const Anchor& p = g_anchor[prev][id][j];
                if (a.res != p.res)
                    continue;
                matched = true;
                int dx = p.x - a.x, dy = p.y - a.y;
                if ((dx || dy) && abs(dx) <= kMaxStep && abs(dy) <= kMaxStep)
                {
                    g_deltaX[id] = (short)dx;
                    g_deltaY[id] = (short)dy;
                    g_moving.push_back((WORD)id);
                }
            }
    }
}

void __cdecl RenderTick(DWORD list)
{
    if (list != (DWORD)drawList)
    {
        WatcomCall(fnDrawListRender, list);
        return;
    }
    FillOwners(NodesUsed(), 0);
    g_cur ^= 1;
    memset(g_anchorCount[g_cur], 0, sizeof(g_anchorCount[g_cur]));
    g_anchorFrame[g_cur] = g_frame;
    g_tick.originX = *game::smallStartX;
    g_tick.originY = *game::smallStartY;
    memcpy(g_tick.clip, game::clipRect, sizeof(g_tick.clip));
    g_tick.start = *game::clockMs;
    g_tick.deadline = *game::nextFrameClock;
    RenderList(kRecord);
    FindMoving();
    memset(g_offX, 0, sizeof(g_offX));
    memset(g_offY, 0, sizeof(g_offY));
    g_tick.valid = true;
}

__declspec(naked) void RenderThunk()
{
    __asm
    {
        pushad
        push eax
        call RenderTick
        add esp, 4
        popad
        ret
    }
}

// ---- move marker

__declspec(naked) void MarkerThunk()
{
    __asm
    {
        mov dword ptr [g_marker.res], eax
        mov dword ptr [g_marker.frame], edx
        mov dword ptr [g_marker.x], ebx
        mov dword ptr [g_marker.y], ecx
        push eax
        mov eax, dword ptr [esp + 8]
        mov dword ptr [g_marker.flags], eax
        pop eax
        mov byte ptr [g_marker.valid], 1
        jmp dword ptr [g_drawImage]
    }
}

void DrawMarker()
{
    WatcomCall5(game::fnDrawImage, g_marker.res, g_marker.frame, g_marker.x, g_marker.y, g_marker.flags);
}

// ---- overlay: what the game draws over the map view after the fog pass

struct Locked
{
    BYTE* bits;
    int pitch;
};

bool LockCanvas(DWORD flags, Locked* out)
{
    auto* canvas = (IDirectDrawSurface*)CanvasSurface();
    DDSURFACEDESC desc = { sizeof(desc) };
    if (!canvas || FAILED(canvas->Lock(nullptr, &desc, flags | DDLOCK_WAIT, nullptr)))
        return false;
    out->bits = (BYTE*)desc.lpSurface;
    out->pitch = desc.lPitch;
    return true;
}

void UnlockCanvas()
{
    ((IDirectDrawSurface*)CanvasSurface())->Unlock(nullptr);
}

void __cdecl Snapshot()
{
    if (!g_tick.valid || g_moving.empty())
        return;
    Locked c;
    if (!LockCanvas(DDLOCK_READONLY, &c))
        return;
    for (int y = 0; y < kViewH; ++y)
        memcpy(&g_snapshot[(size_t)y * kViewW], c.bits + y * c.pitch, kViewW);
    UnlockCanvas();
    g_tick.snapped = true;
}

__declspec(naked) void OrderLinesThunk()
{
    __asm
    {
        pushad
        call Snapshot
        popad
        jmp dword ptr [g_drawOrderLines]
    }
}

void BuildOverlay()
{
    g_runs.clear();
    g_overlay.clear();
    Locked c;
    if (!LockCanvas(DDLOCK_READONLY, &c))
        return;
    for (int y = 0; y < kViewH; ++y)
    {
        const BYTE* now = c.bits + y * c.pitch;
        const BYTE* before = &g_snapshot[(size_t)y * kViewW];
        if (!memcmp(now, before, kViewW))
            continue;
        for (int x = 0; x < kViewW;)
        {
            if (now[x] == before[x])
            {
                ++x;
                continue;
            }
            int start = x;
            while (x < kViewW && now[x] != before[x])
                ++x;
            g_runs.push_back({ y * kViewW + start, x - start });
            g_overlay.insert(g_overlay.end(), now + start, now + x);
        }
    }
    UnlockCanvas();
}

void PutOverlay()
{
    if (g_runs.empty())
        return;
    Locked c;
    if (!LockCanvas(0, &c))
        return;
    const BYTE* pixels = g_overlay.data();
    for (const Run& r : g_runs)
    {
        int x = r.offset % kViewW, y = r.offset / kViewW;
        memcpy(c.bits + y * c.pitch + x, pixels, r.len);
        pixels += r.len;
    }
    UnlockCanvas();
    for (const Run& r : g_runs)
        FxMarkUi(r.offset % kViewW, r.offset / kViewW, r.len, 1);
}

// ---- replay

// 0 at the tick's draw, 1 at its flip deadline.
double Progress()
{
    int span = (int)(g_tick.deadline - g_tick.start);
    int elapsed = (int)(*game::clockMs - g_tick.start);
    if (span <= 0 || elapsed >= span)
        return 1.0;
    return elapsed <= 0 ? 0.0 : (double)elapsed / span;
}

void SetOffsets(double progress)
{
    double behind = 1.0 - progress;
    for (WORD id : g_moving)
    {
        g_offX[id] = (short)lround(g_deltaX[id] * behind);
        g_offY[id] = (short)lround(g_deltaY[id] * behind);
    }
}
}  // namespace

void MotionInstall(const char* iniPath)
{
    if (!GetPrivateProfileIntA("Video", "SmoothMotion", 1, iniPath))
        return;
    if (!HookVerify(kRenderCall, kRenderCallOrig, sizeof(kRenderCallOrig), "motion render") ||
        !HookVerify(kMarkerCall, kMarkerCallOrig, sizeof(kMarkerCallOrig), "motion marker") ||
        !HookVerify(kOrderLinesCall, kOrderLinesCallOrig, sizeof(kOrderLinesCallOrig), "motion order lines") ||
        !HookVerify(kQueueThingSprites, kQueueThingSpritesOrig, sizeof(kQueueThingSpritesOrig), "motion queue"))
        return;
    t_QueueThingSprites = HookDetour(kQueueThingSprites, kQueueThingSpritesOrig, sizeof(kQueueThingSpritesOrig),
                                     QueueThingSpritesThunk, "motion queue");
    if (!t_QueueThingSprites)
        return;
    g_snapshot.resize((size_t)kViewW * kViewH);
    HookCall(kRenderCall, RenderThunk);
    HookCall(kMarkerCall, MarkerThunk);
    HookCall(kOrderLinesCall, OrderLinesThunk);
    g_enabled = true;
    Log("motion: things move smoothly between game ticks");
}

void MotionFrameStart()
{
    ++g_frame;
    g_ownerFilled = 0;           // DrawGameFrame resets the list before queueing
    g_tick.valid = g_tick.snapped = g_tick.done = false;
    g_marker.valid = false;
}

void MotionFrameDone()
{
    if (!g_tick.valid)
        return;
    if (g_tick.snapped)
        BuildOverlay();
    g_tick.done = true;
}

void MotionReplay()
{
    if (!g_enabled || !g_tick.done || g_moving.empty() || !CanvasActive() ||
        *game::currentScreenHandler != game::fnInGameScreenHandler || *game::endScreen || *game::modalDialog)
        return;
    SetOffsets(Progress());

    int clip[4];
    memcpy(clip, game::clipRect, sizeof(clip));
    memcpy(game::clipRect, g_tick.clip, sizeof(clip));
    WatcomCall(game::fnPresentMapView);
    // Rebuild the live click rectangles at the positions shown.
    BYTE live = *rectListCur & 1;
    *rectListCur ^= 1;
    rectListCount[live] = 0;
    RenderList(kReplay);
    *rectListCur ^= 1;
    if (g_marker.valid)
        DrawMarker();
    if (*shroudEnabled)
        WatcomCall(fnDrawShroud);
    memcpy(game::clipRect, clip, sizeof(clip));
    PutOverlay();
}
