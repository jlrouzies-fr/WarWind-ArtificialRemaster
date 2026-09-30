#include "log.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

static CRITICAL_SECTION g_logLock;
static char g_logPath[MAX_PATH];

void LogInit(const char* path)
{
    InitializeCriticalSection(&g_logLock);
    lstrcpynA(g_logPath, path, MAX_PATH);
    DeleteFileA(g_logPath);
}

void Log(const char* fmt, ...)
{
    char line[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(line + n, sizeof(line) - n, _TRUNCATE, fmt, ap);
    va_end(ap);

    EnterCriticalSection(&g_logLock);
    FILE* f = nullptr;
    if (fopen_s(&f, g_logPath, "a") == 0 && f)
    {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
    LeaveCriticalSection(&g_logLock);
}
