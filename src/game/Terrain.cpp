#include "game/Terrain.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

#include "core/Geometry.h"

namespace moba {
namespace {

// 触れているだけ（めり込んでいない）の時に「かかっている」と判定しないための余裕
constexpr float kTouchTolerance = 0.5f;

/// 長方形 r の中（辺の上を含む）で、点 p に最も近い点
Vector2 closestPointInRect(Vector2 p, Rectangle r) {
    return Vector2{std::clamp(p.x, r.x, r.x + r.width), std::clamp(p.y, r.y, r.y + r.height)};
}

/// 点 p が形 s の中（縁の上を含む）にあるか
bool pointInShape(Vector2 p, const Shape& s) {
    if (s.kind == Shape::Kind::Rect) return CheckCollisionPointRec(p, s.rect);
    return Vector2DistanceSqr(p, s.center) <= s.radius * s.radius;
}

/// 中心 pos・半径 radius の円が、形 s にめり込んでいるか（触れているだけなら false）
bool circleOverlapsShape(Vector2 pos, float radius, const Shape& s) {
    if (pointInShape(pos, s)) return true;
    const float reach = radius - kTouchTolerance;
    if (reach <= 0.0f) return false;
    if (s.kind == Shape::Kind::Rect) return Vector2DistanceSqr(pos, closestPointInRect(pos, s.rect)) < reach * reach;
    const float r = s.radius + reach;
    return Vector2DistanceSqr(pos, s.center) < r * r;
}

/// 円が形 s に触れるだけの位置まで押し出す（最も近い外側へ）
/// @return 押し出した後の円の中心
Vector2 pushOutOfShape(Vector2 pos, float radius, const Shape& s) {
    if (s.kind == Shape::Kind::Circle) {
        const Vector2 dir = geo::normalizedOr(pos - s.center, Vector2{0.0f, -1.0f});
        return s.center + dir * (s.radius + radius);
    }
    const Rectangle& r = s.rect;
    if (CheckCollisionPointRec(pos, r)) {
        // 中心が長方形の中にある: 最も近い辺の外へ出す
        const float left = pos.x - r.x;
        const float right = r.x + r.width - pos.x;
        const float top = pos.y - r.y;
        const float bottom = r.y + r.height - pos.y;
        const float nearest = std::min({left, right, top, bottom});
        if (nearest == left) return Vector2{r.x - radius, pos.y};
        if (nearest == right) return Vector2{r.x + r.width + radius, pos.y};
        if (nearest == top) return Vector2{pos.x, r.y - radius};
        return Vector2{pos.x, r.y + r.height + radius};
    }
    const Vector2 closest = closestPointInRect(pos, r);
    const Vector2 dir = geo::normalizedOr(pos - closest, Vector2{0.0f, -1.0f});
    return closest + dir * radius;
}

/// 2 つの形が重なっているか（ブッシュを 1 つにまとめる判断に使う）
bool shapesOverlap(const Shape& a, const Shape& b) {
    using Kind = Shape::Kind;
    if (a.kind == Kind::Rect && b.kind == Kind::Rect) return CheckCollisionRecs(a.rect, b.rect);
    if (a.kind == Kind::Circle && b.kind == Kind::Circle)
        return CheckCollisionCircles(a.center, a.radius, b.center, b.radius);
    const Shape& rect = a.kind == Kind::Rect ? a : b;
    const Shape& circle = a.kind == Kind::Circle ? a : b;
    return CheckCollisionCircleRec(circle.center, circle.radius, rect.rect);
}

}  // namespace

Terrain::Terrain(Rectangle bounds) : bounds_(bounds) {}

Terrain::Terrain(const StageDef& stage) : bounds_(stage.bounds), obstacles_(stage.obstacles), bushes_(stage.bushes) {
    groupBushes();
}

bool Terrain::blocked(Vector2 pos, float radius) const {
    const float r = std::max(0.0f, radius - kTouchTolerance);
    if (pos.x - r < bounds_.x || pos.x + r > bounds_.x + bounds_.width || pos.y - r < bounds_.y ||
        pos.y + r > bounds_.y + bounds_.height) {
        return true;
    }
    return std::any_of(obstacles_.begin(), obstacles_.end(),
                       [&](const Obstacle& o) { return circleOverlapsShape(pos, radius, o.shape); });
}

Vector2 Terrain::pushOut(Vector2 pos, float radius) const {
    auto clampToBounds = [&](Vector2 p) {
        return Vector2{std::clamp(p.x, bounds_.x + radius, bounds_.x + bounds_.width - radius),
                       std::clamp(p.y, bounds_.y + radius, bounds_.y + bounds_.height - radius)};
    };
    pos = clampToBounds(pos);
    // 障害物が近くに並んでいると、1 つから押し出した先が別の障害物にかかることがあるので、何回か繰り返す
    for (int i = 0; i < 8; ++i) {
        bool moved = false;
        for (const Obstacle& obstacle : obstacles_) {
            if (!circleOverlapsShape(pos, radius, obstacle.shape)) continue;
            pos = clampToBounds(pushOutOfShape(pos, radius, obstacle.shape));
            moved = true;
        }
        if (!moved) return pos;
    }
    if (!blocked(pos, radius)) return pos;
    // 障害物の間に挟まって押し出せない時は、最も近い立てるマス目へ
    const Grid& grid = gridFor(radius);
    int col = 0;
    int row = 0;
    cellOf(pos, col, row);
    int freeCol = 0;
    int freeRow = 0;
    return nearestFreeCell(grid, col, row, freeCol, freeRow) ? cellCenter(freeCol, freeRow) : pos;
}

bool Terrain::segmentClear(Vector2 a, Vector2 b, float radius) const {
    const float length = Vector2Distance(a, b);
    const float step = std::clamp(radius * 0.5f, 4.0f, 20.0f);
    const int samples = std::max(1, static_cast<int>(std::ceil(length / step)));
    for (int i = 0; i <= samples; ++i) {
        if (blocked(Vector2Lerp(a, b, static_cast<float>(i) / static_cast<float>(samples)), radius)) return false;
    }
    return true;
}

std::vector<Vector2> Terrain::findPath(Vector2 from, Vector2 to, float radius) const {
    const Grid& grid = gridFor(radius);

    // 目的地に立てない時は、近くの立てる場所に変える（建物の中をクリックした時など）
    Vector2 goal = pushOut(to, radius);
    if (blocked(goal, radius)) return {};
    if (segmentClear(from, goal, radius)) return {goal};  // まっすぐ進める

    int startCol = 0;
    int startRow = 0;
    int goalCol = 0;
    int goalRow = 0;
    cellOf(from, startCol, startRow);
    cellOf(goal, goalCol, goalRow);
    if (!nearestFreeCell(grid, startCol, startRow, startCol, startRow)) return {};
    if (!nearestFreeCell(grid, goalCol, goalRow, goalCol, goalRow)) return {};

    // A*（8 方向。斜めは、両隣のマス目も立てる時だけ）
    const int cellCount = grid.cols * grid.rows;
    const auto indexOf = [&](int col, int row) { return row * grid.cols + col; };
    const auto heuristic = [&](int col, int row) {
        const float dx = static_cast<float>(std::abs(col - goalCol));
        const float dy = static_cast<float>(std::abs(row - goalRow));
        return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
    };
    std::vector<float> cost(static_cast<std::size_t>(cellCount), std::numeric_limits<float>::max());
    std::vector<int> cameFrom(static_cast<std::size_t>(cellCount), -1);
    std::vector<unsigned char> closed(static_cast<std::size_t>(cellCount), 0);
    using Entry = std::pair<float, int>;  // (見積もり, マス目の番号)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

    const int startIndex = indexOf(startCol, startRow);
    const int goalIndex = indexOf(goalCol, goalRow);
    cost[static_cast<std::size_t>(startIndex)] = 0.0f;
    open.push({heuristic(startCol, startRow), startIndex});
    bool found = false;
    while (!open.empty()) {
        const int current = open.top().second;
        open.pop();
        if (closed[static_cast<std::size_t>(current)]) continue;
        closed[static_cast<std::size_t>(current)] = 1;
        if (current == goalIndex) {
            found = true;
            break;
        }
        const int col = current % grid.cols;
        const int row = current / grid.cols;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nc = col + dx;
                const int nr = row + dy;
                if (!grid.isFree(nc, nr)) continue;
                if (dx != 0 && dy != 0 && (!grid.isFree(col + dx, row) || !grid.isFree(col, row + dy))) continue;
                const int next = indexOf(nc, nr);
                const float nextCost =
                    cost[static_cast<std::size_t>(current)] + (dx != 0 && dy != 0 ? 1.41421356f : 1.0f);
                if (nextCost >= cost[static_cast<std::size_t>(next)]) continue;
                cost[static_cast<std::size_t>(next)] = nextCost;
                cameFrom[static_cast<std::size_t>(next)] = current;
                open.push({nextCost + heuristic(nc, nr), next});
            }
        }
    }
    if (!found) return {};

    // マス目の並び（出発のマス目は除く）を点の並びにして、最後を本当の目的地にする
    std::vector<Vector2> cells;
    for (int i = goalIndex; i != startIndex && i >= 0; i = cameFrom[static_cast<std::size_t>(i)]) {
        cells.push_back(cellCenter(i % grid.cols, i / grid.cols));
    }
    std::reverse(cells.begin(), cells.end());
    if (cells.empty() || segmentClear(cells.back(), goal, radius)) {
        if (!cells.empty()) cells.back() = goal;
        else cells.push_back(goal);
    } else {
        cells.push_back(goal);
    }

    // まっすぐ進めるところは間引く（今いる点から、まっすぐ行ける最も先の点へ）
    std::vector<Vector2> path;
    Vector2 current = from;
    std::size_t i = 0;
    while (i < cells.size()) {
        std::size_t j = i;
        while (j + 1 < cells.size() && segmentClear(current, cells[j + 1], radius)) ++j;
        path.push_back(cells[j]);
        current = cells[j];
        i = j + 1;
    }
    return path;
}

int Terrain::bushAt(Vector2 pos) const {
    for (std::size_t i = 0; i < bushes_.size(); ++i) {
        if (pointInShape(pos, bushes_[i])) return bushGroups_[i];
    }
    return -1;
}

const Terrain::Grid& Terrain::gridFor(float radius) const {
    const int key = static_cast<int>(std::ceil(radius));
    if (const auto it = grids_.find(key); it != grids_.end()) return it->second;

    Grid grid;
    grid.cols = std::max(1, static_cast<int>(std::ceil(bounds_.width / kCellSize)));
    grid.rows = std::max(1, static_cast<int>(std::ceil(bounds_.height / kCellSize)));
    grid.free.resize(static_cast<std::size_t>(grid.cols * grid.rows));
    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.cols; ++col) {
            grid.free[static_cast<std::size_t>(row * grid.cols + col)] =
                blocked(cellCenter(col, row), static_cast<float>(key)) ? 0 : 1;
        }
    }
    return grids_.emplace(key, std::move(grid)).first->second;
}

Vector2 Terrain::cellCenter(int col, int row) const {
    return Vector2{bounds_.x + (static_cast<float>(col) + 0.5f) * kCellSize,
                   bounds_.y + (static_cast<float>(row) + 0.5f) * kCellSize};
}

void Terrain::cellOf(Vector2 pos, int& col, int& row) const {
    col = static_cast<int>(std::floor((pos.x - bounds_.x) / kCellSize));
    row = static_cast<int>(std::floor((pos.y - bounds_.y) / kCellSize));
}

bool Terrain::nearestFreeCell(const Grid& grid, int col, int row, int& outCol, int& outRow) const {
    col = std::clamp(col, 0, grid.cols - 1);
    row = std::clamp(row, 0, grid.rows - 1);
    const int maxRing = std::max(grid.cols, grid.rows);
    for (int ring = 0; ring <= maxRing; ++ring) {
        // 中心から ring マス離れた四角い輪の上で、最も近いマス目を探す
        int bestDistance = std::numeric_limits<int>::max();
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring || !grid.isFree(col + dx, row + dy)) continue;
                const int distance = dx * dx + dy * dy;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    outCol = col + dx;
                    outRow = row + dy;
                }
            }
        }
        if (bestDistance != std::numeric_limits<int>::max()) return true;
    }
    return false;
}

void Terrain::groupBushes() {
    // 重なり合うブッシュを 1 つにまとめる（素集合データ構造）
    std::vector<int> parent(bushes_.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](int i) {
        while (parent[static_cast<std::size_t>(i)] != i) i = parent[static_cast<std::size_t>(i)];
        return i;
    };
    for (std::size_t a = 0; a < bushes_.size(); ++a) {
        for (std::size_t b = a + 1; b < bushes_.size(); ++b) {
            if (shapesOverlap(bushes_[a], bushes_[b])) {
                parent[static_cast<std::size_t>(root(static_cast<int>(b)))] = root(static_cast<int>(a));
            }
        }
    }
    bushGroups_.resize(bushes_.size());
    for (std::size_t i = 0; i < bushes_.size(); ++i) bushGroups_[i] = root(static_cast<int>(i));
}

}  // namespace moba
