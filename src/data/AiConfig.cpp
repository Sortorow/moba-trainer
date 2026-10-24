#include "data/AiConfig.h"

#include <algorithm>
#include <format>

#include "core/JsonFile.h"
#include "core/Paths.h"

namespace moba {

const AiLevel& AiConfig::level(int aiLevel) const {
    static const AiLevel kFallback;
    if (levels.empty()) return kFallback;
    const int i = std::clamp(aiLevel, 1, static_cast<int>(levels.size())) - 1;
    return levels[static_cast<std::size_t>(i)];
}

AiConfigLoadResult loadAiConfig(const std::filesystem::path& file) {
    AiConfigLoadResult result;
    const JsonLoadResult json = loadJsonFile(file);
    if (!json.ok()) {
        result.error = json.error;
        return result;
    }

    const nlohmann::json& j = json.value;
    AiConfig& c = result.config;
    try {
        c.enemyCooldownScale = std::max(0.0f, j.value("enemyCooldownScale", c.enemyCooldownScale));
        c.firstCastDelay = std::max(0.0f, j.value("firstCastDelay", c.firstCastDelay));
        c.maxEngageRange = std::max(200.0f, j.value("maxEngageRange", c.maxEngageRange));
        c.playerReactionTime = std::max(0.0f, j.value("playerReactionTime", c.playerReactionTime));

        for (const nlohmann::json& item : j.at("levels")) {
            AiLevel level;
            level.name = item.value("name", std::format("{}", c.levels.size() + 1));
            level.reactionTime = std::max(0.0f, item.value("reactionTime", level.reactionTime));
            if (const auto it = item.find("castInterval"); it != item.end() && it->is_array() && it->size() == 2) {
                level.castIntervalMin = std::max(0.1f, (*it)[0].get<float>());
                level.castIntervalMax = std::max(level.castIntervalMin, (*it)[1].get<float>());
            }
            level.aimError = std::max(0.0f, item.value("aimError", level.aimError));
            if (const auto it = item.find("predict"); it != item.end() && it->is_object()) {
                level.predictStay = std::max(0.0f, it->value("stay", 0.0f));
                level.predictContinue = std::max(0.0f, it->value("continue", 0.0f));
                level.predictDodge = std::max(0.0f, it->value("dodge", 0.0f));
            }
            level.adaptive = item.value("adaptive", level.adaptive);
            level.minHitChance = std::clamp(item.value("minHitChance", level.minHitChance), 0.0f, 1.0f);
            level.strafe = std::clamp(item.value("strafe", level.strafe), 0.0f, 1.0f);
            level.flank = item.value("flank", level.flank);
            level.punishCC = item.value("punishCC", level.punishCC);
            level.engageRangeRatio = std::clamp(item.value("engageRangeRatio", level.engageRangeRatio), 0.3f, 1.0f);
            c.levels.push_back(std::move(level));
        }
        if (c.levels.empty()) result.error = std::format("{}: \"levels\" が空です", toUtf8(file.filename()));
    } catch (const nlohmann::json::exception& e) {
        result.error = std::format("{}: {}", toUtf8(file.filename()), e.what());
    }
    return result;
}

}  // namespace moba
