#include "data/EffectSpec.h"

#include <array>
#include <utility>

#include "core/Log.h"

namespace moba {
namespace {

struct EffectName {
    EffectType type;
    std::string_view id;
    std::string_view display;
};

constexpr std::array<EffectName, 6> kEffectNames = {{
    {EffectType::Slow, "slow", "スロウ"},
    {EffectType::Snare, "snare", "拘束"},
    {EffectType::Stun, "stun", "スタン"},
    {EffectType::Knockup, "knockup", "打ち上げ"},
    {EffectType::Polymorph, "polymorph", "変身"},
    {EffectType::Mark, "mark", "マーク"},
}};

}  // namespace

std::string_view toString(EffectType type) {
    for (const EffectName& name : kEffectNames) {
        if (name.type == type) return name.id;
    }
    return "?";
}

std::string_view displayName(EffectType type) {
    for (const EffectName& name : kEffectNames) {
        if (name.type == type) return name.display;
    }
    return "?";
}

bool effectTypeFromString(std::string_view text, EffectType& out) {
    for (const EffectName& name : kEffectNames) {
        if (name.id == text) {
            out = name.type;
            return true;
        }
    }
    return false;
}

std::vector<EffectSpec> parseEffects(const nlohmann::json& value, std::string_view context) {
    std::vector<EffectSpec> effects;
    if (value.is_null()) return effects;
    if (!value.is_array()) {
        log::warn("data", "{}: 効果は [ ... ] の配列で書いてください", context);
        return effects;
    }
    for (const nlohmann::json& item : value) {
        if (!item.is_object()) {
            log::warn("data", "{}: 効果は {{ \"type\": ... }} の形で書いてください", context);
            continue;
        }
        const std::string type = item.value("type", std::string{});
        EffectSpec spec;
        if (!effectTypeFromString(type, spec.type)) {
            log::warn("data", "{}: 効果の種類 \"{}\" は不明です", context, type);
            continue;
        }
        spec.duration = item.value("duration", 0.0f);
        spec.amount = item.value("amount", 0.0f);
        spec.tag = item.value("tag", std::string{});
        effects.push_back(std::move(spec));
    }
    return effects;
}

}  // namespace moba
