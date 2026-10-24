#pragma once
// キャラクター（自キャラ・敵・ダミー）

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "core/GameTypes.h"
#include "game/StatusEffects.h"
#include "raylib.h"

namespace moba {

// 設置物などを指定するスキル（エマの R）の対象の種類
enum class PropTargetKind { None, Dove, Hat, Rabbit };

constexpr std::string_view toString(PropTargetKind kind) {
    switch (kind) {
        case PropTargetKind::None: return "none";
        case PropTargetKind::Dove: return "dove";
        case PropTargetKind::Hat: return "hat";
        case PropTargetKind::Rabbit: return "rabbit";
    }
    return "?";
}

// スキル発動の依頼（キーを押した瞬間の情報）
struct CastRequest {
    SkillSlot slot = SkillSlot::Q;
    Vector2 cursor{};                    // キーを押した時のカーソル位置（ワールド座標）
    EntityId targetUnit = kInvalidId;    // 対象指定スキルの対象（兎になった敵もここ）
    PropTargetKind propKind = PropTargetKind::None;
    EntityId propId = kInvalidId;        // 鳩・帽子を対象にする場合の設置物 ID
};

// 詠唱中の状態
struct CastState {
    bool active = false;
    CastRequest request;
    float remaining = 0.0f;  // 詠唱の残り時間
    float total = 0.0f;      // 詠唱時間
};

struct Unit {
    EntityId id = kInvalidId;
    Team team = Team::Enemy;
    std::string name;
    std::string characterId;  // 自キャラの設定 ID（ダミーは空）
    Vector2 pos{};
    Vector2 facing{1.0f, 0.0f};  // 向いている方向（単位ベクトル）
    float radius = 65.0f;        // 当たり判定の半径
    float baseMoveSpeed = 325.0f;
    Color color = WHITE;

    std::optional<Vector2> moveTarget;          // 右クリックで指定した目的地
    StatusEffects status;
    CastState cast;
    std::optional<CastRequest> pendingCast;     // 射程外の対象へ近づいている途中のスキル
    std::optional<CastRequest> bufferedCast;    // 詠唱中に押されたスキル（詠唱が終わったら発動する）
    std::array<float, kSkillSlotCount> cooldowns{};  // 各スキルのクールダウンの残り時間

    float hitFlash = 0.0f;  // 被弾した時の光る演出の残り時間
    bool alive = true;

    float moveSpeed() const { return baseMoveSpeed * status.moveSpeedMultiplier(); }
};

}  // namespace moba
