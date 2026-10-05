#pragma once
#include <string>

// %APPDATA%\Age of the Ring\aotr60 (created if missing). The AotR launcher deletes unknown files in rotwk\,
// so settings and logs live next to the game's own user data instead. Empty on failure.
std::wstring DataDirectory();

// Full path of the running executable (e.g. ...\rotwk\game.dat).
std::wstring ExecutablePath();
