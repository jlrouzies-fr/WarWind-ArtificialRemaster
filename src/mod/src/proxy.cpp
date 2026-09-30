// Proxy DLL: every export jumps through g_real[] into the system DLL of the same name.
#include "proxy.h"

#include <windows.h>

struct ExportName { const char* name; WORD ordinal; };
#include "proxy_stubs.inc"

extern "C" void* g_real[PROXY_EXPORT_COUNT] = {};

bool ProxyLoadRealDll()
{
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);  // WOW64 redirects System32 -> SysWOW64 for this 32-bit process
    lstrcatA(path, "\\" PROXIED_DLL);
    HMODULE real = LoadLibraryA(path);
    if (!real)
        return false;
    for (int i = 0; i < PROXY_EXPORT_COUNT; ++i)
    {
        const ExportName& e = kExports[i];
        g_real[i] = e.name ? (void*)GetProcAddress(real, e.name) : (void*)GetProcAddress(real, MAKEINTRESOURCEA(e.ordinal));
    }
    return true;
}
