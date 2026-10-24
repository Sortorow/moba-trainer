#include "game/StatusEffects.h"

#include <algorithm>

namespace moba {

void StatusEffects::apply(const EffectSpec& spec, EntityId source) {
    for (StatusEffect& effect : effects_) {
        if (effect.type == spec.type && effect.tag == spec.tag) {
            effect.remaining = std::max(effect.remaining, spec.duration);
            effect.duration = std::max(effect.duration, spec.duration);
            effect.amount = std::max(effect.amount, spec.amount);
            effect.source = source;
            return;
        }
    }
    effects_.push_back(StatusEffect{spec.type, spec.duration, spec.duration, spec.amount, spec.tag, source});
}

void StatusEffects::update(float dt) {
    for (StatusEffect& effect : effects_) effect.remaining -= dt;
    std::erase_if(effects_, [](const StatusEffect& effect) { return effect.remaining <= 0.0f; });
}

bool StatusEffects::has(EffectType type) const {
    return std::any_of(effects_.begin(), effects_.end(), [type](const StatusEffect& e) { return e.type == type; });
}

float StatusEffects::remaining(EffectType type) const {
    float result = 0.0f;
    for (const StatusEffect& effect : effects_) {
        if (effect.type == type) result = std::max(result, effect.remaining);
    }
    return result;
}

bool StatusEffects::hasMark(std::string_view tag) const {
    return std::any_of(effects_.begin(), effects_.end(),
                       [tag](const StatusEffect& e) { return e.type == EffectType::Mark && e.tag == tag; });
}

bool StatusEffects::consumeMark(std::string_view tag) {
    const auto removed = std::erase_if(
        effects_, [tag](const StatusEffect& e) { return e.type == EffectType::Mark && e.tag == tag; });
    return removed > 0;
}

bool StatusEffects::canMove() const {
    return !has(EffectType::Snare) && !has(EffectType::Stun) && !has(EffectType::Knockup);
}

bool StatusEffects::canCast() const {
    return !has(EffectType::Stun) && !has(EffectType::Knockup) && !has(EffectType::Polymorph);
}

float StatusEffects::moveSpeedMultiplier() const {
    float strongestSlow = 0.0f;
    for (const StatusEffect& effect : effects_) {
        if (effect.type == EffectType::Slow || effect.type == EffectType::Polymorph) {
            strongestSlow = std::max(strongestSlow, effect.amount);
        }
    }
    return std::clamp(1.0f - strongestSlow, 0.0f, 1.0f);
}

}  // namespace moba
