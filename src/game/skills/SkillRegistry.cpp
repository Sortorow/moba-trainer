#include <array>
#include <utility>

#include "game/skills/Skill.h"

namespace moba::skills {
namespace {

/// 対応表の 1 行（設定ファイルの "handler" に書く名前・処理・params のルール）
struct HandlerEntry {
    std::string_view name;
    SkillHandler handler;
    ParamRules (*rules)();
};

// 設定ファイルの "handler" に書く名前と、処理・params のルールの対応表。新しいスキルの処理を作ったらここに追加する
constexpr std::array<HandlerEntry, 11> kHandlers = {{
    {"projectile", handleProjectile, projectileRules},
    {"homing_projectile", handleHomingProjectile, homingProjectileRules},
    {"blink", handleBlink, blinkRules},
    {"ground_zone", handleGroundZone, groundZoneRules},
    {"ground_blast", handleGroundBlast, groundBlastRules},
    {"targeted_blast", handleTargetedBlast, targetedBlastRules},
    {"ezreal_e", handleEzrealE, ezrealERules},
    {"emma_q", handleEmmaQ, emmaQRules},
    {"emma_w", handleEmmaW, emmaWRules},
    {"emma_r", handleEmmaR, emmaRRules},
    {"brand_r", handleBrandR, brandRRules},
}};

/// 名前で対応表の行を探す。無ければ nullptr
const HandlerEntry* findEntry(std::string_view name) {
    for (const HandlerEntry& entry : kHandlers) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

}  // namespace

SkillHandler findHandler(std::string_view name) {
    const HandlerEntry* entry = findEntry(name);
    return entry ? entry->handler : nullptr;
}

bool handlerExists(std::string_view name) { return findEntry(name) != nullptr; }

std::vector<std::string_view> handlerNames() {
    std::vector<std::string_view> names;
    for (const HandlerEntry& entry : kHandlers) names.push_back(entry.name);
    return names;
}

std::optional<ParamRules> findParamRules(std::string_view name) {
    const HandlerEntry* entry = findEntry(name);
    if (!entry) return std::nullopt;
    return entry->rules();
}

}  // namespace moba::skills
