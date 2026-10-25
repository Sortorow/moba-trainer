#pragma once
// キャラクター（自キャラ・敵・ダミー）

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/GameTypes.h"
#include "game/StatusEffects.h"
#include "raylib.h"

namespace moba {

// 設置物などを指定するスキル（エマの R）の対象の種類
enum class PropTargetKind { None, Dove, Hat, Rabbit };

/// 対象の種類の、設定ファイル（params の targetKinds）での表記（"dove" / "hat" / "rabbit"）
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
    Vector2 cursor{};                  // キーを押した時のカーソル位置（ワールド座標）
    EntityId targetUnit = kInvalidId;  // 対象指定スキルの対象（兎になった敵もここ）
    PropTargetKind propKind = PropTargetKind::None;
    EntityId propId = kInvalidId;  // 鳩・帽子を対象にする場合の設置物 ID

    // エマの Q の溜めの間に、R で鳩と入れ替わった時の鳩の元の位置（鳩のカードは入れ替わる前の位置から出る）。
    // 詠唱の情報の一部なので、詠唱が中断されれば一緒に消える
    std::optional<Vector2> doveCardOrigin;
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

    std::optional<Vector2> moveTarget;  // 右クリックで指定した目的地
    // 目的地までの道のり（壁を回り込む点の並び。World が moveTarget から作る）
    std::vector<Vector2> path;
    std::optional<Vector2> pathGoal;  // path を作った時の moveTarget（変わったら作り直す）
    float revealTime = 0.0f;          // ブッシュの中でも、外から見える残り時間（対象指定のスキルを使った後）
    StatusEffects status;
    CastState cast;
    std::optional<CastRequest> pendingCast;          // 射程外の対象へ近づいている途中のスキル
    std::optional<CastRequest> bufferedCast;         // 詠唱中に押されたスキル（詠唱が終わったら発動する）
    std::array<float, kSkillSlotCount> cooldowns{};  // 各スキルのクールダウンの残り時間

    // HP ゲージ（スキルドッジの自キャラ）。maxHp が 0 なら HP を使わない
    int hp = 0;
    int maxHp = 0;
    int deaths = 0;  // HP が 0 になって全回復した回数

    float hitFlash = 0.0f;  // 被弾した時の光る演出の残り時間
    bool alive = true;

    /// 今の移動速度（キャラの移動速度に、スロウなどの倍率を掛けたもの）
    float moveSpeed() const { return baseMoveSpeed * status.moveSpeedMultiplier(); }
};

}  // namespace moba
