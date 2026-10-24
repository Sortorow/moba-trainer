#include "data/Settings.h"

#include <algorithm>
#include <format>

#include "core/JsonFile.h"
#include "core/Paths.h"

namespace moba {

SettingsLoadResult loadSettings(const std::filesystem::path& file) {
    SettingsLoadResult result;
    const JsonLoadResult json = loadJsonFile(file);
    if (!json.ok()) {
        result.error = json.error;
        return result;
    }

    const nlohmann::json& j = json.value;
    Settings& s = result.settings;
    try {
        const std::string castMode = j.value("castMode", std::string("quick"));
        if (castMode != "quick") {
            result.warnings.push_back(std::format("castMode \"{}\" はまだ対応していないため \"quick\" を使います", castMode));
        }
        const std::string cameraMode = j.value("cameraMode", std::string("locked"));
        if (cameraMode != "locked") {
            result.warnings.push_back(
                std::format("cameraMode \"{}\" はまだ対応していないため \"locked\" を使います", cameraMode));
        }

        s.cameraViewHeight = std::clamp(j.value("cameraViewHeight", s.cameraViewHeight), 400.0f, 6000.0f);
        s.cooldownScale = std::max(0.0f, j.value("cooldownScale", s.cooldownScale));
        s.showHitboxes = j.value("showHitboxes", s.showHitboxes);
        s.uiScale = std::clamp(j.value("uiScale", s.uiScale), 0.5f, 3.0f);

        if (const auto it = j.find("window"); it != j.end() && it->is_object()) {
            s.windowWidth = std::max(640, it->value("width", s.windowWidth));
            s.windowHeight = std::max(360, it->value("height", s.windowHeight));
        }

        const std::string level = j.value("logLevel", std::string("debug"));
        if (!log::parseLevel(level, s.logLevel)) {
            result.warnings.push_back(std::format("logLevel \"{}\" は不明なため \"debug\" を使います", level));
            s.logLevel = log::Level::Debug;
        }
    } catch (const nlohmann::json::exception& e) {
        result.error = std::format("{}: {}", toUtf8(file.filename()), e.what());
    }
    return result;
}

}  // namespace moba
