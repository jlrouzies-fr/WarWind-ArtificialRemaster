#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    HMODULE ours = GetModuleHandleA("dsound.dll");
    char path[MAX_PATH];
    GetModuleFileNameA(ours, path, MAX_PATH);
    IDirectSound* ds = NULL;
    HRESULT hr = DirectSoundCreate(NULL, &ds, NULL);
    if (ds)
        ds->lpVtbl->Release(ds);
    printf("dsound loaded from: %s\nDirectSoundCreate hr=%08lX\n", path, hr);
    return strstr(path, "mod\test") && SUCCEEDED(hr) ? 0 : 1;
}
