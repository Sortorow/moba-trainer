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

const SkillShotLevel& SkillShotConfig::level(int aiLevel) const {
    static const SkillShotLevel kFallback;
    if (levels.empty()) return kFallback;
    const int i = std::clamp(aiLevel, 1, static_cast<int>(levels.size())) - 1;
    return levels[static_cast<std::size_t>(i)];
}

namespace {

/// [最小, 最大] の 2 つの数の配列を読む（無い・形が違う時は何もしない）。最大は最小より小さくしない
void readRange(const nlohmann::json& item, const char* key, float lowest, float& min, float& max) {
    const auto it = item.find(key);
    if (it == item.end() || !it->is_array() || it->size() != 2) return;
    min = std::max(lowest, (*it)[0].get<float>());
    max = std::max(min, (*it)[1].get<float>());
}

/// "skillShot" の部分を読む
/// @return 読めなかった理由（空なら成功）
std::string loadSkillShot(const nlohmann::json& j, SkillShotConfig& c) {
    c.targetRadius = std::clamp(j.value("targetRadius", c.targetRadius), 10.0f, 300.0f);
    c.targetMoveSpeed = std::clamp(j.value("targetMoveSpeed", c.targetMoveSpeed), 0.0f, 1500.0f);
    readRange(j, "distance", 100.0f, c.distanceMin, c.distanceMax);
    c.maxVerticalOffset = std::max(50.0f, j.value("maxVerticalOffset", c.maxVerticalOffset));
    c.dodgeMargin = std::max(0.0f, j.value("dodgeMargin", c.dodgeMargin));
    for (const nlohmann::json& item : j.at("levels")) {
        SkillShotLevel level;
        level.name = item.value("name", std::format("{}", c.levels.size() + 1));
        level.reactionTime = std::max(0.0f, item.value("reactionTime", level.reactionTime));
        level.dodgeChance = std::clamp(item.value("dodgeChance", level.dodgeChance), 0.0f, 1.0f);
        level.readCast = item.value("readCast", level.readCast);
        readRange(item, "moveInterval", 0.1f, level.moveIntervalMin, level.moveIntervalMax);
        level.stopChance = std::clamp(item.value("stopChance", level.stopChance), 0.0f, 1.0f);
        level.sideRandom = std::clamp(item.value("sideRandom", level.sideRandom), 0.0f, 1.0f);
        c.levels.push_back(std::move(level));
    }
    return c.levels.empty() ? std::string("\"skillShot\" の \"levels\" が空です") : std::string{};
}

}  // namespace

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
            readRange(item, "castInterval", 0.1f, level.castIntervalMin, level.castIntervalMax);
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
        if (c.levels.empty()) {
            result.error = std::format("{}: \"levels\" が空です", toUtf8(file.filename()));
            return result;
        }
        if (const std::string error = loadSkillShot(j.at("skillShot"), c.skillShot); !error.empty())
            result.error = std::format("{}: {}", toUtf8(file.filename()), error);
    } catch (const nlohmann::json::exception& e) {
        result.error = std::format("{}: {}", toUtf8(file.filename()), e.what());
    }
    return result;
}

}  // namespace moba
