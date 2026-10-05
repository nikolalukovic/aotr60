#include "paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

bool EnsureDirectory(const std::wstring& path)
{
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

std::wstring DataDirectory()
{
    wchar_t appData[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::wstring dir = std::wstring(appData) + L"\\Age of the Ring";
    if (!EnsureDirectory(dir)) {
        return {};
    }
    dir += L"\\aotr60";
    if (!EnsureDirectory(dir)) {
        return {};
    }
    return dir;
}

std::wstring ExecutablePath()
{
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return (len == 0 || len >= MAX_PATH) ? std::wstring{} : std::wstring(path, len);
}
