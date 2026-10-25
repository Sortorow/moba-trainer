#include "data/CharacterDef.h"

#include <algorithm>
#include <format>
#include <set>

#include "core/JsonFile.h"
#include "core/Log.h"
#include "core/Paths.h"

namespace moba {
namespace {

using nlohmann::json;

// 同じ警告を何度も出さないための記録
void warnMissingOnce(const SkillDef& skill, std::string_view key) {
    static std::set<std::string> warned;
    const std::string id = std::format("{}.{}.{}", skill.characterId, toString(skill.slot), key);
    if (warned.insert(id).second) {
        log::warn("data", "{} の {} に params.{} がありません。既定値を使います", skill.characterId,
                  toString(skill.slot), key);
    }
}

const json* findParam(const json& params, std::string_view key) {
    const auto it = params.find(std::string(key));
    return it == params.end() ? nullptr : &*it;
}

std::optional<SkillDef> parseSkill(const json& j, const std::string& characterId, SkillSlot slot, std::string& error) {
    SkillDef skill;
    skill.characterId = characterId;
    skill.slot = slot;
    skill.name = j.value("name", std::string(toString(slot)));
    skill.handler = j.value("handler", std::string{});
    if (skill.handler.empty()) {
        error = "\"handler\" がありません";
        return std::nullopt;
    }
    const std::string targeting = j.value("targeting", std::string("direction"));
    if (!targetingFromString(targeting, skill.targeting)) {
        error = std::format("\"targeting\": \"{}\" は不明です", targeting);
        return std::nullopt;
    }
    skill.castTime = std::max(0.0f, j.value("castTime", 0.0f));
    skill.cooldown = std::max(0.0f, j.value("cooldown", 0.0f));
    skill.range = std::max(0.0f, j.value("range", 0.0f));
    skill.countsForHitRate = j.value("countsForHitRate", false);
    skill.movementSkill = j.value("movementSkill", false);
    skill.usedByEnemy = j.value("usedByEnemy", false);
    skill.aiPriority = std::max(0.0f, j.value("aiPriority", 1.0f));
    for (const json& item : j.value("usableDuringCast", json::array())) {
        const std::string name = item.is_string() ? item.get<std::string>() : std::string{};
        const auto it = std::find_if(kAllSkillSlots.begin(), kAllSkillSlots.end(),
                                     [&](SkillSlot s) { return toString(s) == name; });
        if (it == kAllSkillSlots.end()) {
            error = std::format("\"usableDuringCast\" の \"{}\" は不明なキーです（Q/W/E/R/D/F で書いてください）", name);
            return std::nullopt;
        }
        skill.usableDuringCast.push_back(*it);
    }
    skill.params = j.value("params", json::object());
    if (!skill.params.is_object()) {
        error = "\"params\" は { ... } の形で書いてください";
        return std::nullopt;
    }
    return skill;
}

}  // namespace

std::string_view toString(Targeting targeting) {
    switch (targeting) {
        case Targeting::None: return "none";
        case Targeting::Direction: return "direction";
        case Targeting::Point: return "point";
        case Targeting::Unit: return "unit";
        case Targeting::Prop: return "prop";
    }
    return "?";
}

bool targetingFromString(std::string_view text, Targeting& out) {
    if (text == "none") out = Targeting::None;
    else if (text == "direction") out = Targeting::Direction;
    else if (text == "point") out = Targeting::Point;
    else if (text == "unit") out = Targeting::Unit;
    else if (text == "prop") out = Targeting::Prop;
    else return false;
    return true;
}

float SkillDef::num(std::string_view key, float fallback) const {
    const json* value = findParam(params, key);
    if (!value || !value->is_number()) {
        warnMissingOnce(*this, key);
        return fallback;
    }
    return value->get<float>();
}

float SkillDef::numOr(std::string_view key, float fallback) const {
    const json* value = findParam(params, key);
    return value && value->is_number() ? value->get<float>() : fallback;
}

bool SkillDef::flagOr(std::string_view key, bool fallback) const {
    const json* value = findParam(params, key);
    return value && value->is_boolean() ? value->get<bool>() : fallback;
}

std::string SkillDef::text(std::string_view key, std::string_view fallback) const {
    const json* value = findParam(params, key);
    return value && value->is_string() ? value->get<std::string>() : std::string(fallback);
}

std::vector<std::string> SkillDef::texts(std::string_view key) const {
    std::vector<std::string> result;
    const json* value = findParam(params, key);
    if (!value || !value->is_array()) return result;
    for (const json& item : *value) {
        if (item.is_string()) result.push_back(item.get<std::string>());
    }
    return result;
}

Color SkillDef::color(std::string_view key, Color fallback) const {
    const json* value = findParam(params, key);
    return value ? colorFromJson(*value, fallback) : fallback;
}

std::vector<EffectSpec> SkillDef::effects(std::string_view key) const {
    const json* value = findParam(params, key);
    if (!value) return {};
    return parseEffects(*value, std::format("{} の {} の params.{}", characterId, toString(slot), key));
}

const SkillDef* CharacterDef::skill(SkillSlot slot) const {
    const auto& entry = skills[static_cast<std::size_t>(index(slot))];
    return entry ? &*entry : nullptr;
}

Color colorFromJson(const json& value, Color fallback) {
    if (!value.is_array() || value.size() < 3) return fallback;
    auto channel = [&](std::size_t i, int defaultValue) {
        if (i >= value.size() || !value[i].is_number()) return static_cast<unsigned char>(defaultValue);
        return static_cast<unsigned char>(std::clamp(value[i].get<int>(), 0, 255));
    };
    return Color{channel(0, 255), channel(1, 255), channel(2, 255), channel(3, 255)};
}

CharacterLoadResult loadCharacterFile(const std::filesystem::path& file) {
    CharacterLoadResult result;
    const JsonLoadResult loaded = loadJsonFile(file);
    if (!loaded.ok()) {
        result.error = loaded.error;
        return result;
    }

    const json& root = loaded.value;
    const std::string fileName = toUtf8(file.filename());
    try {
        CharacterDef def;
        def.sourceFile = file;
        def.id = root.at("id").get<std::string>();
        def.displayName = root.value("displayName", def.id);
        def.origin = root.value("origin", std::string{});
        def.moveSpeed = std::max(0.0f, root.value("moveSpeed", def.moveSpeed));
        def.hitboxRadius = std::max(1.0f, root.value("hitboxRadius", def.hitboxRadius));
        def.color = colorFromJson(root.value("color", json::array()), def.color);

        const json& skills = root.at("skills");
        for (SkillSlot slot : kAllSkillSlots) {
            const std::string key(toString(slot));
            const auto it = skills.find(key);
            if (it == skills.end() || it->is_null()) continue;
            std::string error;
            std::optional<SkillDef> skill = parseSkill(*it, def.id, slot, error);
            if (!skill) {
                result.error = std::format("{}: スキル {} の設定エラー: {}", fileName, key, error);
                return result;
            }
            def.skills[static_cast<std::size_t>(index(slot))] = std::move(*skill);
        }
        result.def = std::move(def);
    } catch (const json::exception& e) {
        result.error = std::format("{}: {}", fileName, e.what());
    }
    return result;
}

}  // namespace moba
