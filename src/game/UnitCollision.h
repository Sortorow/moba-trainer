#pragma once
// キャラ同士の押し合い（重なったら、動いた側を相手の外へ押し出す）と、押し合いを無くす許可（OverlapPass）の管理。
//
// 決まり（claude_directive.txt の「押し合い」）:
//   ・すべてのキャラ（自キャラ・敵・ダミー・的）が互いに押し合う。大きさは押し合いの半径（Unit::collisionRadius）
//   ・動いた側だけが相手の外へずれ、止まっている側は押されない。両方動いていたら半分ずつずれる
//   ・1 刻みごとに押し出すので、動いている側は相手の円に沿って滑る。真正面からぶつかったら、進む向きの右側へ回り込む
//   ・押し出した先が障害物にかかったら、障害物から押し出し直す（障害物が優先。挟まって動けなくなることはある）
//   ・押し合いを無くす許可（Unit::overlapPasses）を持つ組は押し合わない
//   ・道を探す時（経路探索）はキャラを避けない

#include <string>

#include "game/Unit.h"

namespace moba {
class World;
}

namespace moba::collision {

/// 2 体が押し合わない許可があるか（どちらかが、相手を対象に含む許可を持っている）
bool passes(const Unit& a, const Unit& b);

/// 2 体が押し合いの大きさで重なっているか（ちょうど触れているだけなら、重なっていない）
bool overlapping(const Unit& a, const Unit& b);

/// 押し合いの計算。全キャラが動いた後に、World::update が 1 刻みに 1 回呼ぶ。
/// 許可の残り時間と離れたかを調べて外し、重なっている組を押し出し、障害物から押し出し直す。
/// 最後に、次の刻みのために各キャラの位置を覚える
/// @param dt この刻みの長さ（秒。許可の残り時間を減らす）
void resolve(World& world, float dt);

/// unit が今重なっている相手全員と、押し合わない許可を付ける（特例のワープの着地。エマの R で兎へ移動した時など）。
/// 許可は、相手と離れきるか、重なったまま歩いている側が新しくスキルの詠唱を始めたら終わる。誰とも重なっていなければ何もしない
/// @param reason ログに出す理由（スキルの名前など）
void allowLandingOverlap(World& world, Unit& unit, std::string reason);

/// unit が新しくスキルの詠唱を始めた時に呼ぶ（SkillCasting）。unit が歩いていて（直前の刻みで歩いて動いていて）、
/// 「詠唱で終わる許可」で誰かと重なっていれば、その許可を終え、次の押し合いの計算で unit だけを押し出す（「飛ぶ」）。
/// 止まっている時は何もしない（重なったまま）
void onCastStarted(World& world, Unit& unit);

}  // namespace moba::collision
