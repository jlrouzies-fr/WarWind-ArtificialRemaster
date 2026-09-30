#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "log.h"
#include "patch.h"

namespace
{
struct Directive
{
    bool isVerify;
    DWORD addr;
    std::vector<BYTE> bytes;
    int line;
};

bool ParseLine(char* s, int lineNo, std::vector<Directive>& out, const char* file)
{
    if (char* c = strchr(s, '#'))
        *c = 0;
    char* ctx = nullptr;
    char* verb = strtok_s(s, " \t\r\n", &ctx);
    if (!verb)
        return true;
    Directive d{};
    d.line = lineNo;
    if (!_stricmp(verb, "verify"))
        d.isVerify = true;
    else if (_stricmp(verb, "patch"))
    {
        Log("%s:%d unknown directive '%s'", file, lineNo, verb);
        return false;
    }
    char* addr = strtok_s(nullptr, " \t\r\n", &ctx);
    if (!addr)
    {
        Log("%s:%d missing address", file, lineNo);
        return false;
    }
    d.addr = strtoul(addr, nullptr, 16);
    while (char* tok = strtok_s(nullptr, " \t\r\n", &ctx))
        d.bytes.push_back((BYTE)strtoul(tok, nullptr, 16));
    if (d.bytes.empty())
    {
        Log("%s:%d no bytes", file, lineNo);
        return false;
    }
    out.push_back(d);
    return true;
}

bool ApplyFile(const char* path, const char* name);
}  // namespace

bool PatchApplyFile(const char* path)
{
    const char* name = strrchr(path, '\\');
    return ApplyFile(path, name ? name + 1 : path);
}

namespace
{
bool ApplyFile(const char* path, const char* name)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") != 0 || !f)
        return false;
    std::vector<Directive> ds;
    char buf[1024];
    int lineNo = 0;
    bool ok = true;
    while (fgets(buf, sizeof(buf), f))
        ok &= ParseLine(buf, ++lineNo, ds, name);
    fclose(f);
    if (!ok)
        return false;

    for (const Directive& d : ds)
    {
        if (d.isVerify && memcmp((void*)d.addr, d.bytes.data(), d.bytes.size()) != 0)
        {
            Log("%s:%d verify failed at %08X, file skipped", name, d.line, d.addr);
            return false;
        }
    }
    for (const Directive& d : ds)
    {
        if (!d.isVerify && !PatchWrite(d.addr, d.bytes.data(), d.bytes.size()))
        {
            Log("%s:%d write failed at %08X", name, d.line, d.addr);
            return false;
        }
    }
    Log("applied %s (%u directives)", name, (unsigned)ds.size());
    return true;
}
}  // namespace

bool PatchWrite(DWORD addr, const void* bytes, size_t len)
{
    DWORD old;
    if (!VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy((void*)addr, bytes, len);
    VirtualProtect((void*)addr, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, len);
    return true;
}

int PatchApplyDirectory(const char* dir)
{
    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.wwp", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    int applied = 0;
    do
    {
        if (!_strnicmp(fd.cFileName, "feature-", 8))
            continue;
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", dir, fd.cFileName);
        applied += ApplyFile(path, fd.cFileName) ? 1 : 0;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return applied;
}
