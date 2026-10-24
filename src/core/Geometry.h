#pragma once
// 当たり判定などで使う 2D の幾何計算
// 座標系は画面と同じ（+X が右、+Y が下）。距離の単位はゲーム内単位（LoL と同じ。100 ≒ ER の 1m）

#include <cmath>
#include <format>
#include <string>

#include "raylib.h"
#include "raymath.h"

namespace moba::geo {

// 点 p から線分 ab までの最短距離の 2 乗
inline float distanceSqToSegment(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab = b - a;
    const float lengthSq = Vector2LengthSqr(ab);
    if (lengthSq <= 1e-6f) return Vector2DistanceSqr(p, a);
    const float t = Clamp(Vector2DotProduct(p - a, ab) / lengthSq, 0.0f, 1.0f);
    return Vector2DistanceSqr(p, a + ab * t);
}

// 半径 movingRadius の円が a から b へ動く間に、中心 center・半径 radius の円に触れるか
inline bool sweptCircleHitsCircle(Vector2 a, Vector2 b, float movingRadius, Vector2 center, float radius) {
    const float r = movingRadius + radius;
    return distanceSqToSegment(center, a, b) <= r * r;
}

inline bool circlesOverlap(Vector2 a, float radiusA, Vector2 b, float radiusB) {
    const float r = radiusA + radiusB;
    return Vector2DistanceSqr(a, b) <= r * r;
}

// 正規化する。長さがほぼ 0 なら fallback を返す
inline Vector2 normalizedOr(Vector2 v, Vector2 fallback) {
    const float length = Vector2Length(v);
    return length > 1e-4f ? v / length : fallback;
}

// from から to の方向へ、最大 maxDistance だけ進んだ点
inline Vector2 clampDistance(Vector2 from, Vector2 to, float maxDistance) {
    const Vector2 delta = to - from;
    const float length = Vector2Length(delta);
    if (length <= maxDistance || length <= 1e-4f) return to;
    return from + delta * (maxDistance / length);
}

// 向きベクトルに垂直なベクトル（進行方向に対して右手側）
inline Vector2 perpendicular(Vector2 dir) { return Vector2{-dir.y, dir.x}; }

// 向きベクトルの角度（度）。raylib の描画関数と同じく、+X 方向が 0 度で画面上の時計回りが正
inline float angleDegrees(Vector2 dir) { return std::atan2(dir.y, dir.x) * RAD2DEG; }

// ログ用の座標表記 "(x, y)"
inline std::string str(Vector2 v) { return std::format("({:.0f}, {:.0f})", v.x, v.y); }

}  // namespace moba::geo
