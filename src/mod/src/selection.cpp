#include "selection.h"

#include "game.h"
#include "hook.h"

bool SelectionAdd(WORD id, bool exclusive)
{
    DWORD fn = game::fnSelectThing, clan = *game::playerClan, thing = id, only = exclusive, result;
    __asm
    {
        push ebx
        mov eax, thing
        mov edx, only
        xor ebx, ebx
        mov ecx, 1
        push 0
        push clan
        call fn
        mov result, eax
        pop ebx
    }
    return result != 0;
}

void SelectionRemove(WORD id)
{
    WatcomCall(game::fnDeselectThing, id);
}

void SelectionClear()
{
    while (WORD id = *game::firstSelected)
        SelectionRemove(id);
}

WORD SelectionFirst()
{
    return *game::firstSelected;
}

WORD SelectionNext(WORD id)
{
    return *(WORD*)((BYTE*)&game::things[id] + game::kNextSelected);
}

void SelectionUpdateCurrent()
{
    WORD first = SelectionFirst();
    *game::curSelected = first && SelectionNext(first) ? 0x801 : first;
}
