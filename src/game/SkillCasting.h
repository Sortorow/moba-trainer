#pragma once
// スキル発動の流れ
//
//   キー入力 → requestCast（クールダウン・対象・射程の確認）
//            → 射程外の対象指定スキルなら、対象へ近づく（pendingCast）
//            → 詠唱開始（この間は移動しない。詠唱中に押された次のスキルは予約される。
//              ただし詠唱中のスキルの usableDuringCast にあるスキルは、詠唱を続けたまますぐ発動する）
//            → 詠唱終了で各スキルの処理（skills/*.cpp）を実行

#include <string_view>

#include "game/Unit.h"
#include "raylib.h"

namespace moba {
class World;
}

namespace moba::casting {

/// 右クリックの移動命令。詠唱中なら、詠唱が終わってから移動する。射程外の対象への接近はやめる
/// @param destination 目的地（ステージの外なら端に寄せる。壁の向こうなら World が回り込む道を探す）
void orderMove(World& world, Unit& unit, Vector2 destination);

/// スキルの発動を依頼する。クールダウン・行動不能・対象・射程を確かめ、
/// 発動する・詠唱を始める・詠唱後に予約する・対象へ近づく、のどれかにする（使えなければログを出して何もしない）
/// @param request 押したキーと、その時のカーソルの位置
void requestCast(World& world, Unit& caster, const CastRequest& request);

/// 詠唱の進行と、射程外の対象への接近を処理する（固定の時間刻みごとに、World から呼ばれる）
void update(World& world, Unit& caster, float dt);

/// 詠唱を中断する（行動不能になった時など）。予約していたスキルも取り消す
/// @param reason ログに出す中断の理由
void cancelCast(World& world, Unit& caster, std::string_view reason);

}  // namespace moba::casting
