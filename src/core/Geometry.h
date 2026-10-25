#pragma once
// 当たり判定などで使う 2D の幾何計算
// 座標系は画面と同じ（+X が右、+Y が下）。距離の単位はゲーム内単位（LoL と同じ。100 ≒ ER の 1m）

#include <cmath>
#include <format>
#include <string>

#include "raylib.h"
#include "raymath.h"

namespace moba::geo {

/// 点 p から線分 ab までの最短距離の 2 乗（平方根を取らない分だけ速い。比べる時は相手も 2 乗する）
inline float distanceSqToSegment(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab = b - a;
    const float lengthSq = Vector2LengthSqr(ab);
    if (lengthSq <= 1e-6f) return Vector2DistanceSqr(p, a);
    const float t = Clamp(Vector2DotProduct(p - a, ab) / lengthSq, 0.0f, 1.0f);
    return Vector2DistanceSqr(p, a + ab * t);
}

/// 半径 movingRadius の円が a から b へ動く間に、中心 center・半径 radius の円に触れるか
/// （速い弾が 1 回の更新で敵をすり抜けないように、動いた道のり全体で判定する）
inline bool sweptCircleHitsCircle(Vector2 a, Vector2 b, float movingRadius, Vector2 center, float radius) {
    const float r = movingRadius + radius;
    return distanceSqToSegment(center, a, b) <= r * r;
}

/// 2 つの円が重なっているか（ちょうど触れている場合も重なっているとみなす）
inline bool circlesOverlap(Vector2 a, float radiusA, Vector2 b, float radiusB) {
    const float r = radiusA + radiusB;
    return Vector2DistanceSqr(a, b) <= r * r;
}

/// 長さ 1 の向きにする。長さがほぼ 0（向きが決まらない）なら fallback を返す
inline Vector2 normalizedOr(Vector2 v, Vector2 fallback) {
    const float length = Vector2Length(v);
    return length > 1e-4f ? v / length : fallback;
}

/// from から to の方向へ、最大 maxDistance だけ進んだ点（射程の端に収める時などに使う）
/// @return to までの距離が maxDistance 以内なら to、遠ければ途中の点
inline Vector2 clampDistance(Vector2 from, Vector2 to, float maxDistance) {
    const Vector2 delta = to - from;
    const float length = Vector2Length(delta);
    if (length <= maxDistance || length <= 1e-4f) return to;
    return from + delta * (maxDistance / length);
}

/// 向きベクトルに垂直なベクトル（進行方向に対して右手側。画面上で見た右）
inline Vector2 perpendicular(Vector2 dir) { return Vector2{-dir.y, dir.x}; }

/// 向きベクトルの角度（度）。raylib の描画関数と同じく、+X 方向が 0 度で画面上の時計回りが正
inline float angleDegrees(Vector2 dir) { return std::atan2(dir.y, dir.x) * RAD2DEG; }

/// ログ用の座標表記 "(x, y)"（小数点以下は切り捨てて表示）
inline std::string str(Vector2 v) { return std::format("({:.0f}, {:.0f})", v.x, v.y); }

}  // namespace moba::geo
