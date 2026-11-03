#include "game/ai/SearchMap.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <utility>

#include "core/Geometry.h"
#include "data/AiConfig.h"
#include "data/Settings.h"
#include "game/Terrain.h"
#include "game/World.h"

namespace moba::ai {
namespace {

/// 形の中心（長方形なら真ん中、円なら中心）
Vector2 shapeCenter(const Shape& shape) {
    if (shape.kind == Shape::Kind::Circle) return shape.center;
    return Vector2{shape.rect.x + shape.rect.width * 0.5f, shape.rect.y + shape.rect.height * 0.5f};
}

}  // namespace

void SearchMap::build(const Terrain& terrain, const SearchConfig& config, float unitRadius) {
    points_.clear();
    areas_.clear();
    const Rectangle bounds = terrain.bounds();
    const int areaCols = std::max(1, static_cast<int>(std::ceil(bounds.width / config.areaSize)));
    const int areaRows = std::max(1, static_cast<int>(std::ceil(bounds.height / config.areaSize)));
    // (0, ブッシュの番号) か (1, 地面のマスの番号) → エリアの番号
    std::map<std::pair<int, int>, int> areaIndex;

    // raw の近くに点を置く。障害物から押し出し、maxShift より大きく動く所（建物の奥）と、
    // 押し出しても立てない所（狭い隙間）には置かない
    const auto addPoint = [&](Vector2 raw, float maxShift) {
        const Vector2 pos = terrain.pushOut(raw, unitRadius);
        if (Vector2Distance(pos, raw) > maxShift || terrain.blocked(pos, unitRadius)) return;
        const int bush = terrain.bushAt(pos);
        const int col = std::clamp(static_cast<int>((pos.x - bounds.x) / config.areaSize), 0, areaCols - 1);
        const int row = std::clamp(static_cast<int>((pos.y - bounds.y) / config.areaSize), 0, areaRows - 1);
        const std::pair<int, int> key = bush >= 0 ? std::pair{0, bush} : std::pair{1, row * areaCols + col};
        const auto [it, added] = areaIndex.try_emplace(key, static_cast<int>(areas_.size()));
        if (added) {
            Area area;
            area.bush = bush;
            area.center = Vector2{bounds.x + (static_cast<float>(col) + 0.5f) * config.areaSize,
                                  bounds.y + (static_cast<float>(row) + 0.5f) * config.areaSize};
            areas_.push_back(area);
        }
        areas_[static_cast<std::size_t>(it->second)].points.push_back(points_.size());
        points_.push_back(Point{pos, it->second, false});
    };

    const float spacing = config.pointSpacing;
    const int cols = std::max(1, static_cast<int>(std::ceil(bounds.width / spacing)));
    const int rows = std::max(1, static_cast<int>(std::ceil(bounds.height / spacing)));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            addPoint(Vector2{bounds.x + (static_cast<float>(col) + 0.5f) * spacing,
                             bounds.y + (static_cast<float>(row) + 0.5f) * spacing},
                     spacing * 0.5f);
        }
    }
    // 点の間隔より小さいブッシュにも、必ず点を置く（ブッシュの中心）
    for (const Shape& bush : terrain.bushes()) addPoint(shapeCenter(bush), spacing * 0.5f);

    // ブッシュのエリアの中心は、点の平均（ログ用）
    for (Area& area : areas_) {
        if (area.bush < 0) continue;
        Vector2 sum{};
        for (const std::size_t i : area.points) sum = sum + points_[i].pos;
        area.center = sum / static_cast<float>(area.points.size());
    }
    reset();
    built_ = true;
}

void SearchMap::reset() {
    for (Point& point : points_) point.seen = false;
    for (Area& area : areas_) {
        area.unseen = static_cast<int>(area.points.size());
        area.explored = false;
        area.probed = false;
    }
}

void SearchMap::markSeen(const World& world) {
    for (Point& point : points_) {
        if (point.seen || !world.canSeePoint(Team::Enemy, point.pos)) continue;
        point.seen = true;
        Area& area = areas_[static_cast<std::size_t>(point.area)];
        if (--area.unseen <= 0) area.explored = true;
    }
}

void SearchMap::giveUp(int area) { areas_[static_cast<std::size_t>(area)].explored = true; }

int SearchMap::exploredCount() const {
    return static_cast<int>(
        std::count_if(areas_.begin(), areas_.end(), [](const Area& area) { return area.explored; }));
}

int SearchMap::areaNear(Vector2 pos) const {
    int best = -1;
    float bestDistance = std::numeric_limits<float>::max();
    for (const Point& point : points_) {
        const float distance = Vector2DistanceSqr(point.pos, pos);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = point.area;
        }
    }
    return best;
}

std::string SearchMap::areaName(int area) const {
    const Area& a = areas_[static_cast<std::size_t>(area)];
    if (a.bush >= 0) return std::format("ブッシュ {}（{} 付近）", a.bush, geo::str(a.center));
    return std::format("地面（{} 付近）", geo::str(a.center));
}

int SearchMap::chooseArea(Vector2 from, Vector2 lostAt, const std::vector<int>& avoid) const {
    const auto avoided = [&](int area) { return std::find(avoid.begin(), avoid.end(), area) != avoid.end(); };
    // 1 回目は他の敵が向かっているエリアを除いて選び、選べなければ 2 回目は除かずに選ぶ
    for (int pass = 0; pass < 2; ++pass) {
        int best = -1;
        float bestScore = std::numeric_limits<float>::max();
        for (const Point& point : points_) {
            if (point.seen || explored(point.area) || (pass == 0 && avoided(point.area))) continue;
            const float score = Vector2Distance(from, point.pos) + Vector2Distance(lostAt, point.pos);
            if (score < bestScore) {
                bestScore = score;
                best = point.area;
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

Vector2 SearchMap::nearestUnseenPoint(int area, Vector2 from) const {
    Vector2 best = from;
    float bestDistance = std::numeric_limits<float>::max();
    for (const std::size_t i : areas_[static_cast<std::size_t>(area)].points) {
        const Point& point = points_[i];
        if (point.seen) continue;
        const float distance = Vector2DistanceSqr(from, point.pos);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = point.pos;
        }
    }
    return best;
}

std::optional<SearchMap::ProbeTarget> SearchMap::chooseProbe(const World& world, Vector2 from, float range,
                                                             Vector2 lostAt, std::mt19937& rng) const {
    // 視界の半径より遠い所は、ただ遠いだけで物陰とは限らないので撃たない
    const float reach = std::min(range, world.settings().visionRadius);
    int bestArea = -1;
    float bestDistance = std::numeric_limits<float>::max();
    std::vector<Vector2> bestPoints;
    for (int a = 0; a < areaCount(); ++a) {
        const Area& area = areas_[static_cast<std::size_t>(a)];
        if (area.explored || area.probed) continue;
        std::vector<Vector2> candidates;
        float nearest = std::numeric_limits<float>::max();
        for (const std::size_t i : area.points) {
            const Point& point = points_[i];
            if (point.seen || Vector2Distance(from, point.pos) > reach) continue;
            if (world.canSeePoint(Team::Enemy, point.pos)) continue;  // 見えている所は確かめるまでもない
            candidates.push_back(point.pos);
            nearest = std::min(nearest, Vector2Distance(lostAt, point.pos));
        }
        if (!candidates.empty() && nearest < bestDistance) {
            bestDistance = nearest;
            bestArea = a;
            bestPoints = std::move(candidates);
        }
    }
    if (bestArea < 0) return std::nullopt;
    const std::size_t pick = std::uniform_int_distribution<std::size_t>(0, bestPoints.size() - 1)(rng);
    return ProbeTarget{bestArea, bestPoints[pick]};
}

}  // namespace moba::ai
