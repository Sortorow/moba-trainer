#include "data/AiConfig.h"

#include <algorithm>
#include <cmath>
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

std::string SkillShotConfig::sizeName(float radius) const {
    for (const TargetSize& size : sizes) {
        if (std::fabs(size.radius - radius) < 0.5f) return size.name;
    }
    return {};
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
    for (const nlohmann::json& item : j.at("sizes")) {
        TargetSize size;
        size.name = item.value("name", std::format("{}", c.sizes.size() + 1));
        size.radius = std::clamp(item.value("radius", size.radius), 10.0f, 300.0f);
        c.sizes.push_back(std::move(size));
    }
    for (const nlohmann::json& item : j.at("moveSpeeds"))
        c.moveSpeeds.push_back(std::clamp(item.get<float>(), 0.0f, 1500.0f));
    c.minDistance = std::max(0.0f, j.value("minDistance", c.minDistance));
    c.maxDistance = std::max(c.minDistance + 100.0f, j.value("maxDistance", c.maxDistance));
    c.dodgeMargin = std::max(0.0f, j.value("dodgeMargin", c.dodgeMargin));
    for (const nlohmann::json& item : j.at("levels")) {
        SkillShotLevel level;
        level.name = item.value("name", std::format("{}", c.levels.size() + 1));
        level.reactionTime = std::max(kMinTargetReactionTime, item.value("reactionTime", level.reactionTime));
        level.dodgeChance = std::clamp(item.value("dodgeChance", level.dodgeChance), 0.0f, 1.0f);
        level.readCast = item.value("readCast", level.readCast);
        level.sideRandom = std::clamp(item.value("sideRandom", level.sideRandom), 0.0f, 1.0f);
        if (const auto it = item.find("patterns"); it != item.end() && it->is_object()) {
            level.weightStrafe = std::max(0.0f, it->value("strafe", 0.0f));
            level.weightApproach = std::max(0.0f, it->value("approach", 0.0f));
            level.weightWander = std::max(0.0f, it->value("wander", 0.0f));
            level.weightStop = std::max(0.0f, it->value("stop", 0.0f));
        }
        readRange(item, "strafeSwings", 1.0f, level.strafeSwingsMin, level.strafeSwingsMax);
        readRange(item, "strafeWidth", 30.0f, level.strafeWidthMin, level.strafeWidthMax);
        level.rhythmBreak = std::clamp(item.value("rhythmBreak", level.rhythmBreak), 0.0f, 1.0f);
        readRange(item, "stopTime", 0.1f, level.stopTimeMin, level.stopTimeMax);
        c.levels.push_back(std::move(level));
    }
    if (c.sizes.empty()) return "\"skillShot\" の \"sizes\" が空です";
    if (c.moveSpeeds.empty()) return "\"skillShot\" の \"moveSpeeds\" が空です";
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
        c.punishMinInterval = std::max(0.0f, j.value("punishMinInterval", c.punishMinInterval));
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
