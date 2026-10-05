// game.dat statically imports DINPUT8!DirectInput8Create (keyboard init at 0x4985FF). This DLL stands in for
// dinput8.dll and forwards the call to the real system DLL, loaded lazily on first use (never inside DllMain).

#include "log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace {

// The last parameter is an IUnknown* (COM aggregation outer object); passed through untouched.
using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, const IID&, void**, void*);

DirectInput8CreateFn LoadRealDirectInput8Create()
{
    wchar_t systemDir[MAX_PATH];
    UINT len = GetSystemDirectoryW(systemDir, MAX_PATH); // WOW64 redirects this to SysWOW64 for a 32-bit process
    if (len == 0 || len >= MAX_PATH) {
        Log("proxy: GetSystemDirectoryW failed (%lu)", GetLastError());
        return nullptr;
    }
    std::wstring path = std::wstring(systemDir) + L"\\dinput8.dll";
    HMODULE real = LoadLibraryW(path.c_str());
    if (!real) {
        Log("proxy: loading the system dinput8.dll failed (%lu)", GetLastError());
        return nullptr;
    }
    auto fn = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(real, "DirectInput8Create"));
    if (!fn) {
        Log("proxy: DirectInput8Create not found in the system dinput8.dll");
    }
    return fn;
}

} // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, const IID& riid, void** out,
                                             void* outer)
{
    static const DirectInput8CreateFn real = LoadRealDirectInput8Create();
    if (!real) {
        if (out) {
            *out = nullptr;
        }
        return E_FAIL;
    }
    HRESULT hr = real(instance, version, riid, out, outer);
    Log("proxy: DirectInput8Create(version 0x%04lX) forwarded, hr=0x%08lX", version, static_cast<unsigned long>(hr));
    return hr;
}
