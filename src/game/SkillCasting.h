#pragma once
// スキル発動の流れ
//
//   キー入力 → requestCast（クールダウン・対象・射程の確認）
//            → 射程外の対象指定スキルなら、対象へ近づく（pendingCast）
//            → 詠唱開始（この間は移動しない。詠唱中に押された次のスキルは予約される）
//            → 詠唱終了で各スキルの処理（skills/*.cpp）を実行

#include <string_view>

#include "game/Unit.h"
#include "raylib.h"

namespace moba {
class World;
}

namespace moba::casting {

// 右クリックの移動命令。詠唱中なら、詠唱が終わってから移動する
void orderMove(World& world, Unit& unit, Vector2 destination);

// スキルの発動を依頼する
void requestCast(World& world, Unit& caster, const CastRequest& request);

// 詠唱の進行と、射程外の対象への接近を処理する（毎刻み）
void update(World& world, Unit& caster, float dt);

// 詠唱を中断する（行動不能になった時など）
void cancelCast(World& world, Unit& caster, std::string_view reason);

}  // namespace moba::casting
