#include "log.h"

#include <cstdarg>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

HANDLE g_logFile = INVALID_HANDLE_VALUE;

} // namespace

void LogOpen(const std::wstring& path)
{
    // Keep the previous session's log next to the new one.
    std::wstring previous = path + L".prev";
    MoveFileExW(path.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING);
    g_logFile = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Log(const char* format, ...)
{
    if (g_logFile == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[1024];
    int n = std::snprintf(line, sizeof(line), "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond,
                          t.wMilliseconds);
    va_list args;
    va_start(args, format);
    int m = std::vsnprintf(line + n, sizeof(line) - n - 2, format, args);
    va_end(args);
    if (m < 0) {
        return;
    }
    size_t len = n + ((static_cast<size_t>(m) < sizeof(line) - n - 2) ? m : sizeof(line) - n - 3);
    line[len++] = '\r';
    line[len++] = '\n';
    DWORD written;
    WriteFile(g_logFile, line, static_cast<DWORD>(len), &written, nullptr);
}
