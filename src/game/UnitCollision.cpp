#include "game/UnitCollision.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "raymath.h"

namespace moba::collision {
namespace {

// この距離より動いていれば「動いた」とみなす（計算の誤差で動いたことにならないように）
constexpr float kMovedEpsilon = 1e-3f;
// 押し出しを繰り返す回数の上限（3 体以上が重なった時や、障害物から押し出し直した後に、もう一度直すため）
constexpr int kMaxIterations = 4;
// 真正面に近い向きでぶつかったとみなす角度（度）。進む向きと「相手の中心への向き」の差がこれより小さいと、
// 押し出すだけでは横にほとんど進まない（相手の前で止まる）ので、この角度でぶつかった時と同じだけ横へずらす
constexpr float kHeadOnDegrees = 20.0f;
// 差がこれより小さければ「真正面」とみなし、進む向きの右側へ回り込む（これ以上なら、ずれている側へ回り込む）
constexpr float kExactHeadOnDegrees = 2.0f;

/// キャラ 1 体の、この刻みの動き
struct Motion {
    bool mover = false;  // 押し出される側か（この刻みで自分で動いた、またはすぐ押し出すの印がある）
    Vector2 step{};      // この刻みで自分で動いた量（前の刻みの終わりからの差）
};

/// 真正面に近い向きでぶつかった時に、横へずらす量（相手の円に沿って回り込ませる）。斜めにぶつかった時は 0。
/// 回り込む側は、真正面なら進む向きの右側、少しずれていればずれている側。ただし、その側が障害物でふさがっていて
/// 反対側が空いていれば、反対側へ回り込む（壁沿いを歩いて、同じ壁沿いに立つ相手にぶつかった時など）
/// @param unit 押し出される側（位置は、相手の外へ押し出した後）
/// @param step この刻みで自分で動いた量
/// @param pushDir 押し出す向き（相手の中心から自分への向き。長さ 1）
/// @param minDistance 2 体の押し合いの半径の合計（相手の横を抜けるのに、横へずれる必要がある量）
Vector2 headOnSlide(const World& world, const Unit& unit, Vector2 step, Vector2 pushDir, float minDistance) {
    const float length = Vector2Length(step);
    if (length <= kMovedEpsilon) return {};
    const Vector2 forward = step / length;
    // 進む向きと「相手の中心への向き（-pushDir）」の差。cos が大きいほど真正面
    if (Vector2DotProduct(forward, pushDir * -1.0f) < std::cos(kHeadOnDegrees * DEG2RAD)) return {};
    const Vector2 right = geo::perpendicular(forward);
    // 相手から見て、自分が進む向きの右側にいれば正（大きさは、差の角度の sin）
    const float lateral = Vector2DotProduct(pushDir, right);
    const bool exactlyHeadOn = std::fabs(lateral) < std::sin(kExactHeadOnDegrees * DEG2RAD);
    float side = (exactlyHeadOn || lateral > 0.0f) ? 1.0f : -1.0f;
    // 回り込む側へ、相手の横を抜けるだけずらした位置に立てなければ（障害物・ステージの外）、空いている反対側へ。
    // 両側ともふさがっていれば、そのまま（挟まって動けなくなる。練習場の往復するダミーは DummyController が折り返す）
    const auto blockedSide = [&](float s) {
        return world.terrain().blocked(unit.pos + right * (s * minDistance), unit.radius);
    };
    if (blockedSide(side) && !blockedSide(-side)) side = -side;
    // 押し出すだけでも、歩いた量 × lateral だけ右へ進む。回り込む側へ、kHeadOnDegrees でぶつかった時と同じだけ
    // 進むように足りない分を足す（ずれている側と反対へ回り込む時は、押し出しで進む分も打ち消す）
    const float extra = length * (side * std::sin(kHeadOnDegrees * DEG2RAD) - lateral);
    return side * extra > 0.0f ? right * extra : Vector2{};
}

/// 2 体が重なっていれば押し出す（動いた側だけ。両方動いていたら半分ずつ）
/// @param slide 真正面に近い時の横へのずらしも行うか（1 刻みに 1 回だけ行う）
/// @return 押し出したか
bool separate(const World& world, Unit& a, const Motion& ma, Unit& b, const Motion& mb, bool slide) {
    if (!a.alive || !b.alive || (!ma.mover && !mb.mover) || passes(a, b)) return false;
    const float minDistance = a.collisionRadius + b.collisionRadius;
    const Vector2 delta = a.pos - b.pos;
    const float distance = Vector2Length(delta);
    if (distance >= minDistance) return false;

    // 押し出す向き（b から a へ）。2 体がぴったり同じ位置なら、押し出される側が向いている方向の逆へ
    Vector2 normal{};
    if (distance > 1e-4f) {
        normal = delta / distance;
    } else {
        normal = ma.mover ? a.facing * -1.0f : b.facing;
    }
    const float overlap = minDistance - distance;
    const float shareA = ma.mover ? (mb.mover ? 0.5f : 1.0f) : 0.0f;
    const float shareB = mb.mover ? (ma.mover ? 0.5f : 1.0f) : 0.0f;
    a.pos = a.pos + normal * (overlap * shareA);
    b.pos = b.pos - normal * (overlap * shareB);
    if (slide) {
        if (shareA > 0.0f) a.pos = a.pos + headOnSlide(world, a, ma.step, normal, minDistance) * shareA;
        if (shareB > 0.0f) b.pos = b.pos + headOnSlide(world, b, mb.step, normal * -1.0f, minDistance) * shareB;
    }
    return true;
}

/// 許可の残り時間を減らし、時間切れの許可を外す。離れたら終わる許可からは、離れた相手を外す（誰も残らなければ終わる）
void updatePasses(World& world, Unit& unit, float dt) {
    if (unit.overlapPasses.empty()) return;
    std::erase_if(unit.overlapPasses, [&](OverlapPass& pass) {
        if (pass.remaining) {
            *pass.remaining -= dt;
            if (*pass.remaining <= 0.0f) {
                log::debug("collision", "{} の押し合いの許可「{}」は時間切れで終わり", unit.name, pass.reason);
                return true;
            }
        }
        if (!pass.endsWhenSeparated) return false;
        if (pass.allUnits) {
            const bool touching = std::any_of(world.units().begin(), world.units().end(), [&](const Unit& other) {
                return other.id != unit.id && other.alive && overlapping(unit, other);
            });
            if (!touching)
                log::debug("collision", "{} の押し合いの許可「{}」は、誰とも重ならなくなって終わり", unit.name,
                           pass.reason);
            return !touching;
        }
        std::erase_if(pass.targets, [&](EntityId id) {
            const Unit* other = world.findUnit(id);
            return !other || !other->alive || !overlapping(unit, *other);
        });
        if (pass.targets.empty()) {
            log::debug("collision", "{} の押し合いの許可「{}」は、相手と離れて終わり", unit.name, pass.reason);
            return true;
        }
        return false;
    });
}

}  // namespace

bool passes(const Unit& a, const Unit& b) {
    const auto covers = [](const Unit& holder, const Unit& other) {
        return std::any_of(holder.overlapPasses.begin(), holder.overlapPasses.end(),
                           [&](const OverlapPass& pass) { return pass.covers(other.id); });
    };
    return covers(a, b) || covers(b, a);
}

bool overlapping(const Unit& a, const Unit& b) {
    const float minDistance = a.collisionRadius + b.collisionRadius;
    return Vector2DistanceSqr(a.pos, b.pos) < minDistance * minDistance;
}

void resolve(World& world, float dt) {
    std::vector<Unit>& units = world.units();
    for (Unit& unit : units) updatePasses(world, unit, dt);

    std::vector<Motion> motions(units.size());
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& unit = units[i];
        motions[i].step = unit.pos - unit.prevPos;
        motions[i].mover = unit.alive && (Vector2Length(motions[i].step) > kMovedEpsilon || unit.pushOutNow);
    }

    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        bool pushed = false;
        for (std::size_t i = 0; i < units.size(); ++i) {
            for (std::size_t j = i + 1; j < units.size(); ++j) {
                if (separate(world, units[i], motions[i], units[j], motions[j], iteration == 0)) pushed = true;
            }
        }
        if (!pushed) break;
        // 押し出した先が障害物にかかっていたら、障害物から押し出し直す（障害物が優先。押し出されるのは動いた側だけ）
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (motions[i].mover) units[i].pos = world.placeable(units[i].pos, units[i].radius);
        }
    }

    for (Unit& unit : units) {
        unit.prevPos = unit.pos;
        unit.pushOutNow = false;
    }
}

void allowLandingOverlap(World& world, Unit& unit, std::string reason) {
    OverlapPass pass;
    pass.reason = std::move(reason);
    pass.endsWhenSeparated = true;
    pass.endsOnCastWhileMoving = true;
    for (const Unit& other : world.units()) {
        if (other.id != unit.id && other.alive && overlapping(unit, other)) pass.targets.push_back(other.id);
    }
    if (pass.targets.empty()) return;
    log::debug("collision", "{} は「{}」で {} 体と重なってよい（離れるか、歩きながら詠唱を始めるまで）", unit.name,
               pass.reason, pass.targets.size());
    unit.overlapPasses.push_back(std::move(pass));
}

void onCastStarted(World& world, Unit& unit) {
    if (!unit.walking) return;  // 止まったまま使った時（ワープの直後を含む）は、重なったまま
    bool ended = false;
    // 自分の許可
    std::erase_if(unit.overlapPasses, [&](const OverlapPass& pass) {
        if (!pass.endsOnCastWhileMoving) return false;
        ended = true;
        return true;
    });
    // 相手が持つ、自分を対象にした許可（兎の側が歩きながら詠唱を始めた時など）
    for (Unit& other : world.units()) {
        if (other.id == unit.id) continue;
        for (OverlapPass& pass : other.overlapPasses) {
            if (pass.endsOnCastWhileMoving && !pass.allUnits && std::erase(pass.targets, unit.id) > 0) ended = true;
        }
        std::erase_if(other.overlapPasses,
                      [](const OverlapPass& pass) { return !pass.allUnits && pass.targets.empty(); });
    }
    if (!ended) return;
    unit.pushOutNow = true;
    log::debug("collision", "{} が歩きながら詠唱を始めたので、押し合いの許可を終えて押し出します", unit.name);
}

}  // namespace moba::collision
