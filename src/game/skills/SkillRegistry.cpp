#include <array>
#include <utility>

#include "game/skills/Skill.h"

namespace moba::skills {
namespace {

// 設定ファイルの "handler" に書く名前と、処理の対応表。新しいスキルの処理を作ったらここに追加する
constexpr std::array<std::pair<std::string_view, SkillHandler>, 11> kHandlers = {{
    {"projectile", handleProjectile},
    {"homing_projectile", handleHomingProjectile},
    {"blink", handleBlink},
    {"ground_zone", handleGroundZone},
    {"ground_blast", handleGroundBlast},
    {"targeted_blast", handleTargetedBlast},
    {"ezreal_e", handleEzrealE},
    {"emma_q", handleEmmaQ},
    {"emma_w", handleEmmaW},
    {"emma_r", handleEmmaR},
    {"brand_r", handleBrandR},
}};

}  // namespace

SkillHandler findHandler(std::string_view name) {
    for (const auto& [handlerName, handler] : kHandlers) {
        if (handlerName == name) return handler;
    }
    return nullptr;
}

bool handlerExists(std::string_view name) { return findHandler(name) != nullptr; }

std::vector<std::string_view> handlerNames() {
    std::vector<std::string_view> names;
    for (const auto& entry : kHandlers) names.push_back(entry.first);
    return names;
}

}  // namespace moba::skills
