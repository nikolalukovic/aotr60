#pragma once
#include <string>

// Minimal append-only text log (aotr60.log in the data directory). Safe to call before LogOpen (no-op).
void LogOpen(const std::wstring& path);
void Log(const char* format, ...);
