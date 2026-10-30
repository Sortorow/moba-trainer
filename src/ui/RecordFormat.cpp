#include "ui/RecordFormat.h"

#include <format>

namespace moba::ui {

std::string difficultyLabel(const MatchResult& r) {
    const MatchConfig& c = r.config;
    if (c.mode == GameMode::SkillDodge) return std::format("敵 {} 体・{}", c.enemyCount, r.aiLevelName);
    const std::string size = r.targetSizeName.empty() ? std::format("{:.0f}", c.targetRadius) : r.targetSizeName;
    return std::format("{}・{}・{:.0f}", r.aiLevelName, size, c.targetMoveSpeed);
}

std::string shortTime(const std::string& playedAt) { return playedAt.size() >= 16 ? playedAt.substr(5, 11) : playedAt; }

std::string percentText(const ResultScore& score) {
    return score.total > 0 ? std::format("{:.1f}%", score.rate() * 100.0f) : std::string("-");
}

ImVec4 rankColor(int rank) {
    switch (rank) {
        case 1: return ImVec4{1.0f, 0.82f, 0.25f, 1.0f};
        case 2: return ImVec4{0.80f, 0.84f, 0.90f, 1.0f};
        case 3: return ImVec4{0.85f, 0.58f, 0.35f, 1.0f};
        default: return ImVec4{0.92f, 0.94f, 0.96f, 1.0f};
    }
}

std::string cooldownLabel(const MatchConfig& config, CooldownLabelStyle style) {
    if (config.cooldownMode == CooldownMode::None) return "なし";
    const std::string scale = std::format("×{:.2f}", config.cooldownScale);
    return style == CooldownLabelStyle::WithMode ? std::format("標準（{}）", scale) : scale;
}

}  // namespace moba::ui
