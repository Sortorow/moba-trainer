#pragma once
// 1 回のプレイ（モードの開始から終了まで）の設定。メニューで選び、モードの開始時に決まる

#include <string>

namespace moba {

enum class GameMode {
    Sandbox,  // 練習場（ダミー相手にスキルを試す）
};

enum class CooldownMode {
    Standard,  // 標準: キャラ設定のクールダウン × cooldownScale
    None,      // なし: スキルをすぐに再使用できる
};

struct MatchConfig {
    GameMode mode = GameMode::Sandbox;
    std::string characterId;
    CooldownMode cooldownMode = CooldownMode::Standard;
    float cooldownScale = 0.25f;  // 標準モードの倍率（1 = 実戦どおり、0.25 = 4 分の 1）

    // 自キャラのクールダウンに実際に掛ける倍率
    float playerCooldownScale() const { return cooldownMode == CooldownMode::None ? 0.0f : cooldownScale; }
};

}  // namespace moba
