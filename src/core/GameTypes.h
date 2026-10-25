#pragma once
// ゲーム全体で使う基本的な型

#include <array>
#include <cstdint>
#include <string_view>

namespace moba {

// キャラ・弾・設置物などに振る通し番号。0 は「無し」
using EntityId = std::uint32_t;
inline constexpr EntityId kInvalidId = 0;

enum class Team { Player, Enemy };

// スキルのキー。配列の添字としても使う
enum class SkillSlot { Q, W, E, R, D, F, Count };
inline constexpr int kSkillSlotCount = static_cast<int>(SkillSlot::Count);
inline constexpr std::array<SkillSlot, kSkillSlotCount> kAllSkillSlots = {SkillSlot::Q, SkillSlot::W, SkillSlot::E,
                                                                          SkillSlot::R, SkillSlot::D, SkillSlot::F};

constexpr int index(SkillSlot slot) { return static_cast<int>(slot); }

constexpr std::string_view toString(SkillSlot slot) {
    constexpr std::array<std::string_view, kSkillSlotCount> names = {"Q", "W", "E", "R", "D", "F"};
    return names[static_cast<std::size_t>(index(slot))];
}

}  // namespace moba
