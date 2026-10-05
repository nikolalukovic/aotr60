#pragma once
#include <string>
#include <string_view>

// User settings, read from %APPDATA%\Age of the Ring\aotr60\aotr60.ini.

enum class Pacing {
    Stock,    // 33.000 ms per A+B pair: exactly stock game speed
    Nominal,  // 1000/60 ms per render: even cadence, game runs 1 % slower (opt-in)
};

struct Config {
    bool enabled = true;
    Pacing pacing = Pacing::Stock;
    int telemetry = 0; // 0 off, 1 invariants/rates, 2 determinism traces
    bool fallback = true;
    bool allowDelayfix = false;
    bool allowLivingWorldMap = false;
    bool unitInterpolation = true;   // phase 2b: units drawn at half-steps on the inserted frames
    bool cameraInterpolation = true; // phase 3: camera picture interpolated on the inserted frames
};

// Parses "Key = Value" lines; '#' or ';' start comments, [sections] and unknown keys are ignored.
// Every key not present keeps its default. Problems are appended to warnings (one per line).
Config ParseConfig(std::string_view text, std::string* warnings = nullptr);

// Template written on first run.
extern const char kDefaultConfigText[];
