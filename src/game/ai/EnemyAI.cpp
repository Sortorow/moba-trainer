#include "game/ai/EnemyAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/CharacterDef.h"
#include "game/SkillCasting.h"
#include "game/World.h"

namespace moba::ai {
namespace {

constexpr float kStrafeDistance = 300.0f;       // 横移動の幅（strafe = 1 の時）
constexpr float kMinEngageRange = 350.0f;       // 自キャラに近づく距離の下限
constexpr float kFlankAngle = 70.0f * DEG2RAD;  // 他の敵とこの角度より近いと、離れようとする
constexpr float kMaxFlankShift = 35.0f * DEG2RAD;

// 角度の差を -π〜π に収める
float wrapAngle(float angle) {
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

Vector2 fromAngle(float angle) { return Vector2{std::cos(angle), std::sin(angle)}; }

}  // namespace

void EnemyDirector::setup(const AiConfig& config, int aiLevel, std::uint32_t seed) {
    config_ = config;
    level_ = config.level(aiLevel);
    rng_.seed(seed);
    observer_.clear();
    brains_.clear();
    log::info("ai",
              "敵 AI: 賢さ {}（{}）反応 {:.2f}s 間隔 {:.1f}〜{:.1f}s ぶれ {:.0f} 予測(留まる {:.2f} / 移動継続 {:.2f} "
              "/ 回避 {:.2f})",
              aiLevel, level_.name, level_.reactionTime, level_.castIntervalMin, level_.castIntervalMax,
              level_.aimError, level_.predictStay, level_.predictContinue, level_.predictDodge);
}

void EnemyDirector::addEnemy(EntityId unitId) {
    Brain brain;
    brain.unitId = unitId;
    brain.nextCastTime = config_.firstCastDelay + uniform(0.0f, 1.0f);
    brain.strafeSign = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
    brains_.push_back(brain);
}

float EnemyDirector::uniform(float min, float max) {
    if (max <= min) return min;
    return std::uniform_real_distribution<float>(min, max)(rng_);
}

void EnemyDirector::update(World& world, float dt) {
    const Unit* player = world.player();
    if (!player) return;
    const float now = world.time();
    observer_.record(now, player->pos);
    observer_.update(now);

    const Vector2 seenPlayer = observer_.positionAt(now - level_.reactionTime);
    for (Brain& brain : brains_) {
        Unit* enemy = world.findUnit(brain.unitId);
        if (!enemy || !enemy->alive) continue;
        brain.replanTimer -= dt;
        brain.strafeTimer -= dt;
        if (brain.replanTimer <= 0.0f) {
            planMovement(world, brain, *enemy, seenPlayer);
            brain.replanTimer = uniform(0.2f, 0.35f);
        }
        tryCast(world, brain, *enemy, *player);
    }
}

// スキルの射程に応じて、自キャラと保つ距離を決める
float EnemyDirector::engageRange(const World& world, const Unit& enemy) const {
    const CharacterDef* def = world.characterOf(enemy);
    float shortest = std::numeric_limits<float>::max();
    if (def) {
        for (const auto& skill : def->skills) {
            if (skill && skill->usedByEnemy) shortest = std::min(shortest, skill->range);
        }
    }
    if (shortest == std::numeric_limits<float>::max()) shortest = 800.0f;
    return std::clamp(shortest * level_.engageRangeRatio, kMinEngageRange, config_.maxEngageRange);
}

// 他の敵と同じ方向から攻めないように、自キャラから見た角度をずらす
Vector2 EnemyDirector::spreadDirection(World& world, const Brain& brain, Vector2 seenPlayer, Vector2 away) const {
    const float angle = std::atan2(away.y, away.x);
    float shift = 0.0f;
    for (const Brain& other : brains_) {
        if (other.unitId == brain.unitId) continue;
        const Unit* unit = world.findUnit(other.unitId);
        if (!unit || !unit->alive) continue;
        const Vector2 dir = unit->pos - seenPlayer;
        const float diff = wrapAngle(angle - std::atan2(dir.y, dir.x));
        if (std::fabs(diff) < kFlankAngle)
            shift += (diff >= 0.0f ? 1.0f : -1.0f) * (kFlankAngle - std::fabs(diff)) * 0.5f;
    }
    return fromAngle(angle + std::clamp(shift, -kMaxFlankShift, kMaxFlankShift));
}

void EnemyDirector::planMovement(World& world, Brain& brain, Unit& enemy, Vector2 seenPlayer) {
    const float range = engageRange(world, enemy);
    Vector2 away = geo::normalizedOr(enemy.pos - seenPlayer, fromAngle(uniform(-PI, PI)));
    if (level_.flank) away = spreadDirection(world, brain, seenPlayer, away);

    Vector2 desired = seenPlayer + away * range;
    if (level_.strafe > 0.0f) {
        if (brain.strafeTimer <= 0.0f) {
            brain.strafeSign = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
            brain.strafeTimer = uniform(0.6f, 1.6f);
        }
        desired = desired + geo::perpendicular(away) * (brain.strafeSign * kStrafeDistance * level_.strafe);
    }
    enemy.moveTarget = world.clampToBounds(desired, enemy.radius);
}

AimWeights EnemyDirector::aimWeights(bool playerStuck) const {
    if (playerStuck) return AimWeights{1.0f, 0.0f, 0.0f, 0.0f};  // 動けない相手は、今の位置を狙えば当たる
    AimWeights w;
    w.stay = level_.predictStay;
    w.keepMoving = level_.predictContinue;
    const float right = level_.adaptive ? observer_.rightBias() : 0.5f;
    w.dodgeRight = level_.predictDodge * right;
    w.dodgeLeft = level_.predictDodge * (1.0f - right);
    return w;
}

void EnemyDirector::tryCast(World& world, Brain& brain, Unit& enemy, const Unit& player) {
    if (enemy.cast.active || !enemy.status.canCast()) return;
    const float now = world.time();
    const bool playerStuck = !player.status.canMove();
    const bool punish = level_.punishCC && playerStuck;
    if (!punish && now < brain.nextCastTime) return;

    const CharacterDef* def = world.characterOf(enemy);
    if (!def) return;

    // AI が認識している自キャラ（反応の遅れの分だけ前の位置・速度）
    AimTarget target;
    target.pos = observer_.positionAt(now - level_.reactionTime);
    target.velocity = playerStuck ? Vector2{} : observer_.velocityAt(now - level_.reactionTime);
    target.moveSpeed = player.moveSpeed();
    target.radius = player.radius;
    const AimWeights weights = aimWeights(playerStuck);

    // 使えるスキルの中から、命中見込み × 優先度 が最も高いものを選ぶ
    const SkillDef* bestSkill = nullptr;
    std::optional<AimResult> bestAim;
    std::optional<AimModel> bestModel;
    float bestScore = -1.0f;
    for (const auto& skill : def->skills) {
        if (!skill || !skill->usedByEnemy) continue;
        if (enemy.cooldowns[static_cast<std::size_t>(index(skill->slot))] > 0.0f) continue;
        const std::optional<AimModel> model = aimModelFor(*skill);
        if (!model) continue;
        const std::optional<AimResult> aim = chooseAim(enemy.pos, *model, target, weights, config_.playerReactionTime);
        if (!aim) continue;
        const float score = aim->expectedHit * skill->aiPriority;
        if (score > bestScore) {
            bestScore = score;
            bestSkill = &*skill;
            bestAim = aim;
            bestModel = model;
        }
    }
    if (!bestSkill) return;  // 射程外か、すべてクールダウン中

    // 命中見込みが低い時は少し待つ（ただし待ちすぎない）
    if (!punish && bestAim->expectedHit < level_.minHitChance && now < brain.nextCastTime + level_.castIntervalMax)
        return;

    // 狙いのぶれ。直線の弾は横方向にだけぶらす
    Vector2 aimPoint = bestAim->aimPoint;
    const float error = uniform(0.0f, level_.aimError);
    if (bestModel->shape == AimShape::Line) {
        const Vector2 dir = geo::normalizedOr(aimPoint - enemy.pos, enemy.facing);
        aimPoint = aimPoint + geo::perpendicular(dir) * (uniform(0.0f, 1.0f) < 0.5f ? -error : error);
    } else {
        aimPoint = aimPoint + fromAngle(uniform(-PI, PI)) * error;
    }

    casting::requestCast(world, enemy, CastRequest{bestSkill->slot, aimPoint});
    if (!enemy.cast.active && enemy.cooldowns[static_cast<std::size_t>(index(bestSkill->slot))] <= 0.0f) return;

    brain.nextCastTime = now + uniform(level_.castIntervalMin, level_.castIntervalMax);
    if (bestModel->shape == AimShape::Line) {
        observer_.noteShot(now, player.pos, geo::normalizedOr(aimPoint - enemy.pos, enemy.facing));
    }
    log::debug("ai", "{} が [{}] {} を {} へ（命中見込み {:.0f}%{}）", enemy.name, toString(bestSkill->slot),
               bestSkill->name, geo::str(aimPoint), bestAim->expectedHit * 100.0f,
               punish ? "・動けない相手を狙う" : "");
}

}  // namespace moba::ai
