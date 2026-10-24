#include "game/ai/Aim.h"

#include <algorithm>
#include <array>
#include <vector>

#include "core/Geometry.h"
#include "data/CharacterDef.h"

namespace moba::ai {

std::optional<AimModel> aimModelFor(const SkillDef& def) {
    AimModel model;
    model.range = def.range;
    model.windup = def.castTime;
    if (def.targeting == Targeting::Direction) {
        const float speed = def.numOr("speed", 0.0f);
        const float width = def.numOr("width", 0.0f);
        if (speed <= 0.0f || width <= 0.0f) return std::nullopt;
        model.shape = AimShape::Line;
        model.speed = speed;
        model.halfWidth = width * 0.5f;
        return model;
    }
    if (def.targeting == Targeting::Point) {
        const float radius = def.numOr("radius", 0.0f);
        if (radius <= 0.0f) return std::nullopt;
        model.shape = AimShape::Circle;
        model.radius = radius;
        // 発動までの時間は "delay"（遅れて爆発）か "flightTime"（投げて着地）のどちらか
        model.delay = def.castTime + def.numOr("delay", def.numOr("flightTime", 0.0f));
        return model;
    }
    return std::nullopt;
}

bool wouldHit(Vector2 shooter, const AimModel& model, Vector2 aimPoint, Vector2 p, float targetRadius) {
    if (model.shape == AimShape::Line) {
        const Vector2 dir = geo::normalizedOr(aimPoint - shooter, Vector2{1.0f, 0.0f});
        const float reach = model.halfWidth + targetRadius;
        return geo::distanceSqToSegment(p, shooter, shooter + dir * model.range) <= reach * reach;
    }
    const Vector2 center = geo::clampDistance(shooter, aimPoint, model.range);
    const float reach = model.radius + targetRadius;
    return Vector2DistanceSqr(p, center) <= reach * reach;
}

std::optional<AimResult> chooseAim(Vector2 shooter, const AimModel& model, const AimTarget& target,
                                   const AimWeights& weights, float playerReactionTime) {
    const float distance = Vector2Distance(shooter, target.pos);
    if (model.shape == AimShape::Line && distance > model.range + model.halfWidth + target.radius) return std::nullopt;
    if (model.shape == AimShape::Circle && distance > model.range + model.radius * 0.5f) return std::nullopt;

    // 着弾までの時間。直線の弾は、移動を続けた先までの距離でもう一度見積もる
    float t = model.delay;
    if (model.shape == AimShape::Line) {
        t = model.windup + distance / model.speed;
        const Vector2 moved = target.pos + target.velocity * t;
        t = model.windup + std::max(0.0f, Vector2Distance(shooter, moved) - target.radius) / model.speed;
    }

    // 着弾時に自キャラがいそうな位置の候補。避ける距離は「反応してから動ける距離」と「当たり判定から出るのに十分な距離」の小さい方
    const Vector2 toTarget = geo::normalizedOr(target.pos - shooter, Vector2{1.0f, 0.0f});
    const Vector2 right = geo::perpendicular(toTarget);
    const float hitReach = (model.shape == AimShape::Line ? model.halfWidth : model.radius) + target.radius;
    const float dodgeDistance = std::min(target.moveSpeed * std::max(0.0f, t - playerReactionTime), hitReach * 1.6f);

    struct Candidate {
        Vector2 pos;
        float weight;
    };
    const std::array<Candidate, 4> candidates = {{
        {target.pos, weights.stay},
        {target.pos + target.velocity * t, weights.keepMoving},
        {target.pos + right * dodgeDistance, weights.dodgeRight},
        {target.pos - right * dodgeDistance, weights.dodgeLeft},
    }};
    float totalWeight = 0.0f;
    for (const Candidate& c : candidates) totalWeight += c.weight;

    // 狙い先の候補: 重みのある候補位置と、その 2 つずつの中間
    std::vector<Vector2> aims;
    for (const Candidate& c : candidates) {
        if (c.weight > 0.0f) aims.push_back(c.pos);
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        for (std::size_t j = i + 1; j < candidates.size(); ++j) {
            if (candidates[i].weight > 0.0f && candidates[j].weight > 0.0f) {
                aims.push_back((candidates[i].pos + candidates[j].pos) * 0.5f);
            }
        }
    }
    if (aims.empty() || totalWeight <= 0.0f) {
        return AimResult{model.shape == AimShape::Circle ? geo::clampDistance(shooter, target.pos, model.range) : target.pos,
                         1.0f, t};
    }

    AimResult best{aims.front(), -1.0f, t};
    for (const Vector2& aim : aims) {
        float expected = 0.0f;
        for (const Candidate& c : candidates) {
            if (c.weight > 0.0f && wouldHit(shooter, model, aim, c.pos, target.radius)) expected += c.weight;
        }
        expected /= totalWeight;
        if (expected > best.expectedHit + 1e-4f) best = AimResult{aim, expected, t};
    }
    if (model.shape == AimShape::Circle) best.aimPoint = geo::clampDistance(shooter, best.aimPoint, model.range);
    return best;
}

}  // namespace moba::ai
