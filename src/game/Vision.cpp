#include "game/Vision.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/Geometry.h"
#include "game/Terrain.h"

namespace moba::vision {
namespace {

constexpr float kNoHit = std::numeric_limits<float>::infinity();
constexpr int kBaseRayCount = 360;  // 1° ごとの光線
// 角・接線の両隣に出す光線の、角度のずれ（ラジアン）。距離 900 で約 0.09 ずれる
constexpr float kEdgeAngleOffset = 1e-4f;

/// 角度（ラジアン。+X 方向が 0）の向きの、長さ 1 のベクトル
Vector2 fromAngle(float angle) { return Vector2{std::cos(angle), std::sin(angle)}; }

/// 長方形の中（辺の上を含む）で、点 p に最も近い点
Vector2 closestPointInRect(Vector2 p, Rectangle r) {
    return Vector2{std::clamp(p.x, r.x, r.x + r.width), std::clamp(p.y, r.y, r.y + r.height)};
}

/// origin から dir（長さ 1）の向きに進んで、長方形 r に入るまでの距離（スラブ法）。
/// maxDistance までに入らなければ kNoHit。origin が中（辺の上を含む）にあれば 0
float entryDistanceRect(Rectangle r, Vector2 origin, Vector2 dir, float maxDistance) {
    float enter = 0.0f;
    float leave = maxDistance;
    const float origins[2] = {origin.x, origin.y};
    const float dirs[2] = {dir.x, dir.y};
    const float mins[2] = {r.x, r.y};
    const float maxs[2] = {r.x + r.width, r.y + r.height};
    for (int axis = 0; axis < 2; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-8f) {
            // この軸の向きには進まない: 幅の外にいれば、ずっと入らない
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return kNoHit;
            continue;
        }
        float t1 = (mins[axis] - origins[axis]) / dirs[axis];
        float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
        if (t1 > t2) std::swap(t1, t2);
        enter = std::max(enter, t1);
        leave = std::min(leave, t2);
        if (enter > leave) return kNoHit;
    }
    return enter;
}

/// origin から dir（長さ 1）の向きに進んで、円に入るまでの距離。
/// maxDistance までに入らなければ kNoHit。origin が中（縁の上を含む）にあれば 0
float entryDistanceCircle(Vector2 center, float radius, Vector2 origin, Vector2 dir, float maxDistance) {
    const Vector2 toOrigin = origin - center;
    const float c = Vector2LengthSqr(toOrigin) - radius * radius;
    if (c <= 0.0f) return 0.0f;
    const float b = Vector2DotProduct(toOrigin, dir);
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return kNoHit;
    const float t = -b - std::sqrt(discriminant);
    return t >= 0.0f && t <= maxDistance ? t : kNoHit;
}

/// origin から dir（長さ 1）の向きに進んで、形 s に入るまでの距離（入らなければ kNoHit）
float entryDistance(const Shape& s, Vector2 origin, Vector2 dir, float maxDistance) {
    if (s.kind == Shape::Kind::Rect) return entryDistanceRect(s.rect, origin, dir, maxDistance);
    return entryDistanceCircle(s.center, s.radius, origin, dir, maxDistance);
}

/// 視線を遮る形の一覧（箱と、ignoreBush のブッシュを除く）
std::vector<const Shape*> sightBlockers(const Terrain& terrain, int ignoreBush) {
    std::vector<const Shape*> blockers;
    for (const Obstacle& obstacle : terrain.obstacles()) {
        if (obstacle.kind != ObstacleKind::Crate) blockers.push_back(&obstacle.shape);  // 箱は低いので遮らない
    }
    const std::vector<Shape>& bushes = terrain.bushes();
    for (std::size_t i = 0; i < bushes.size(); ++i) {
        if (ignoreBush < 0 || terrain.bushGroup(i) != ignoreBush) blockers.push_back(&bushes[i]);
    }
    return blockers;
}

/// 形 s の一部が、origin から radius 以内にあるか
bool shapeWithin(const Shape& s, Vector2 origin, float radius) {
    if (s.kind == Shape::Kind::Rect) return Vector2Distance(origin, closestPointInRect(origin, s.rect)) <= radius;
    return Vector2Distance(origin, s.center) - s.radius <= radius;
}

/// 角度 angle と、その両隣（kEdgeAngleOffset だけずらした向き）を加える
void addEdgeAngles(std::vector<float>& angles, float angle) {
    angles.push_back(angle - kEdgeAngleOffset);
    angles.push_back(angle);
    angles.push_back(angle + kEdgeAngleOffset);
}

/// 角度を -π 以上 π 未満に収める
float wrapAngle(float angle) {
    while (angle >= PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

}  // namespace

bool lineOfSight(const Terrain& terrain, Vector2 from, Vector2 to, int ignoreBush) {
    const float length = Vector2Distance(from, to);
    if (length < 1e-4f) return true;
    const Vector2 dir = (to - from) / length;
    for (const Shape* blocker : sightBlockers(terrain, ignoreBush)) {
        if (entryDistance(*blocker, from, dir, length) <= length) return false;
    }
    return true;
}

std::vector<VisionRay> visionPolygon(const Terrain& terrain, Vector2 origin, float radius, int ignoreBush) {
    // 視界の円にかかる、遮る物だけを調べる
    std::vector<const Shape*> blockers;
    for (const Shape* blocker : sightBlockers(terrain, ignoreBush)) {
        if (shapeWithin(*blocker, origin, radius)) blockers.push_back(blocker);
    }

    // 光線を出す角度: 1° ごと + 長方形の角 + 円の接線
    std::vector<float> angles;
    angles.reserve(kBaseRayCount + blockers.size() * 12);
    for (int i = 0; i < kBaseRayCount; ++i) {
        angles.push_back(-PI + 2.0f * PI * static_cast<float>(i) / static_cast<float>(kBaseRayCount));
    }
    for (const Shape* blocker : blockers) {
        if (blocker->kind == Shape::Kind::Rect) {
            const Rectangle& r = blocker->rect;
            for (const Vector2 corner : {Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y}, Vector2{r.x, r.y + r.height},
                                         Vector2{r.x + r.width, r.y + r.height}}) {
                const Vector2 d = corner - origin;
                addEdgeAngles(angles, std::atan2(d.y, d.x));
            }
        } else {
            const Vector2 d = blocker->center - origin;
            const float distance = Vector2Length(d);
            if (distance <= blocker->radius) continue;  // 円の中にいる（どの向きもすぐ遮られる）
            const float base = std::atan2(d.y, d.x);
            const float half = std::asin(blocker->radius / distance);
            addEdgeAngles(angles, base - half);
            addEdgeAngles(angles, base + half);
        }
    }
    for (float& angle : angles) angle = wrapAngle(angle);
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end(), [](float a, float b) { return b - a < 1e-7f; }),
                 angles.end());

    std::vector<VisionRay> rays;
    rays.reserve(angles.size());
    for (const float angle : angles) {
        VisionRay ray;
        ray.dir = fromAngle(angle);
        ray.distance = radius;
        for (const Shape* blocker : blockers) {
            ray.distance = std::min(ray.distance, entryDistance(*blocker, origin, ray.dir, ray.distance));
        }
        rays.push_back(ray);
    }
    return rays;
}

}  // namespace moba::vision
