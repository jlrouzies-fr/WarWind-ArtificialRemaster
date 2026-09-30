#include "gametext.h"

#include <string.h>

#include "game.h"
#include "hook.h"

const char* GameText(DWORD resourceId)
{
    return (const char*)WatcomCall(game::fnResourceText, resourceId);
}

void CopyPlain(const char* text, char* out, size_t size)
{
    size_t n = 0;
    for (; *text && n + 1 < size; ++text)
        if (*text != '<' && *text != '>')
            out[n++] = *text;
    out[n] = 0;
}

int TextWidth(const char* text, int length)
{
    return length > 0 ? (int)WatcomCall(game::fnTextWidth, (DWORD)text, length) : 0;
}

void PrintAt(int x, int y, const char* text)
{
    WatcomCall(game::fnSetTextPos, x, y);
    WatcomCall(game::fnPrintString, (DWORD)text);
}

void PrintClipped(int x, int y, const char* text, int maxW)
{
    char line[128];
    int n = 0;
    while (text[n] && n < (int)sizeof(line) - 1 && TextWidth(text, n + 1) <= maxW)
        ++n;
    memcpy(line, text, n);
    line[n] = 0;
    PrintAt(x, y, line);
}

void PrintCentred(int x, int y, int w, const char* text)
{
    int textW = TextWidth(text, (int)strlen(text));
    PrintClipped(x + (textW < w ? (w - textW) / 2 : 0), y, text, w);
}

int PrintWrapped(int x, int y, int maxW, int lineH, int maxLines, const char* text, const bool* keep)
{
    const char* start = text;
    char line[128];
    int lines = 0;
    while (*text && lines < maxLines)
    {
        int n = 0, lastSpace = -1;
        while (text[n] && n < (int)sizeof(line) - 1)
        {
            if (text[n] == ' ' && !(keep && keep[text - start + n]))
                lastSpace = n;
            if (TextWidth(text, n + 1) > maxW)
                break;
            ++n;
        }
        if (text[n] && lastSpace > 0)
            n = lastSpace;
        memcpy(line, text, n);
        line[n] = 0;
        PrintAt(x, y + lines * lineH, line);
        ++lines;
        text += n;
        while (*text == ' ')
            ++text;
    }
    return lines;
}
