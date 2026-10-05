#include "config.h"

#include <cctype>
#include <cstdlib>

const char kDefaultConfigText[] =
    "; AotR60 settings. Delete this file to restore the defaults.\n"
    "\n"
    "; 1 = render at 60 FPS in campaign, Living World battles, skirmish and tutorials.\n"
    "; 0 = stock 30 FPS (the DLL only forwards DirectInput).\n"
    "Enabled = 1\n"
    "\n"
    "; stock   = exact stock game speed (33.000 ms per two rendered frames).\n"
    "; nominal = perfectly even 1000/60 ms frames; the game runs 1 % slower than stock.\n"
    "Pacing = stock\n"
    "\n"
    "; 0 = off, 1 = speed/invariant checks in aotr60.log and aotr60_rates.csv (testing).\n"
    "Telemetry = 0\n"
    "\n"
    "; 1 = fall back to 30 FPS automatically when the PC cannot hold 60.\n"
    "Fallback = 1\n"
    "\n"
    "; 1 = smooth unit movement / camera on the inserted frames; 0 = those frames repeat the previous positions.\n"
    "UnitInterpolation = 1\n"
    "CameraInterpolation = 1\n"
    "\n"
    "; Not supported yet; keep at 0.\n"
    "AllowDelayfix = 0\n"
    "AllowLivingWorldMap = 0\n";

namespace {

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool EqualsNoCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool ParseBool(std::string_view value, bool& out)
{
    if (value == "1" || EqualsNoCase(value, "true") || EqualsNoCase(value, "yes") || EqualsNoCase(value, "on")) {
        out = true;
        return true;
    }
    if (value == "0" || EqualsNoCase(value, "false") || EqualsNoCase(value, "no") || EqualsNoCase(value, "off")) {
        out = false;
        return true;
    }
    return false;
}

void Warn(std::string* warnings, std::string_view key, std::string_view value)
{
    if (warnings) {
        warnings->append("invalid value '").append(value).append("' for ").append(key).append("\n");
    }
}

} // namespace

Config ParseConfig(std::string_view text, std::string* warnings)
{
    Config cfg;
    while (!text.empty()) {
        size_t eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = (eol == std::string_view::npos) ? std::string_view{} : text.substr(eol + 1);

        size_t comment = line.find_first_of("#;");
        line = Trim(line.substr(0, comment));
        if (line.empty() || line.front() == '[') {
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        std::string_view key = Trim(line.substr(0, eq));
        std::string_view value = Trim(line.substr(eq + 1));

        bool ok = true;
        if (EqualsNoCase(key, "Enabled")) {
            ok = ParseBool(value, cfg.enabled);
        }
        else if (EqualsNoCase(key, "Fallback")) {
            ok = ParseBool(value, cfg.fallback);
        }
        else if (EqualsNoCase(key, "AllowDelayfix")) {
            ok = ParseBool(value, cfg.allowDelayfix);
        }
        else if (EqualsNoCase(key, "AllowLivingWorldMap")) {
            ok = ParseBool(value, cfg.allowLivingWorldMap);
        }
        else if (EqualsNoCase(key, "UnitInterpolation")) {
            ok = ParseBool(value, cfg.unitInterpolation);
        }
        else if (EqualsNoCase(key, "CameraInterpolation")) {
            ok = ParseBool(value, cfg.cameraInterpolation);
        }
        else if (EqualsNoCase(key, "PresentPacing")) {
            ok = ParseBool(value, cfg.presentPacing);
        }
        else if (EqualsNoCase(key, "Pacing")) {
            if (EqualsNoCase(value, "stock")) {
                cfg.pacing = Pacing::Stock;
            }
            else if (EqualsNoCase(value, "nominal")) {
                cfg.pacing = Pacing::Nominal;
            }
            else {
                ok = false;
            }
        }
        else if (EqualsNoCase(key, "Telemetry")) {
            if (value == "0" || value == "1") {
                cfg.telemetry = value[0] - '0';
            }
            else if (value == "2") {
                cfg.telemetry = 1; // determinism traces are not implemented yet
                if (warnings) {
                    warnings->append("Telemetry=2 (determinism traces) is not implemented yet; using 1\n");
                }
            }
            else {
                ok = false;
            }
        }
        if (!ok) {
            Warn(warnings, key, value);
        }
    }
    return cfg;
}
