#include "game/ai/SkillShotTarget.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/CharacterDef.h"
#include "game/World.h"
#include "raymath.h"

namespace moba::ai {
namespace {

constexpr float kWanderAngle = 75.0f * DEG2RAD;  // 移動先を、自キャラから見た今の方向からこの角度以内で選ぶ
constexpr float kMinWanderStep = 120.0f;         // これより近い移動先は選ばない（動いて見えないため）
constexpr float kInheritTime = 0.3f;  // 詠唱が終わってから、この時間内に出た弾・範囲に、詠唱を見た時の判断を引き継ぐ

/// 角度（ラジアン。+X 方向が 0）の向きの、長さ 1 のベクトル
Vector2 fromAngle(float angle) { return Vector2{std::cos(angle), std::sin(angle)}; }

}  // namespace

void SkillShotTarget::setup(const SkillShotConfig& config, int aiLevel, std::uint32_t seed) {
    config_ = config;
    level_ = config.level(aiLevel);
    rng_.seed(seed);
    unitId_ = kInvalidId;
    moveTimer_ = 0.0f;
    dodging_ = false;
    dodgeKey_ = 0;
    dodgeCount_ = 0;
    decisions_.clear();
    playerWasCasting_ = false;
    castSerial_ = 0;
    inherited_.reset();
    log::info("ai", "的の AI: 賢さ {}（{}）反応 {:.2f}s 避ける確率 {:.0f}% 詠唱を見る {} 移動の間隔 {:.1f}〜{:.1f}s",
              aiLevel, level_.name, level_.reactionTime, level_.dodgeChance * 100.0f, level_.readCast ? "あり" : "なし",
              level_.moveIntervalMin, level_.moveIntervalMax);
}

Vector2 SkillShotTarget::startPosition(Vector2 playerPos) {
    const float distance = (config_.distanceMin + config_.distanceMax) * 0.5f;
    // 上下に離れすぎない角度の範囲（左右どちらか）から選ぶ
    const float maxAngle = std::asin(std::min(1.0f, config_.maxVerticalOffset / distance));
    const float side = uniform(0.0f, 1.0f) < 0.5f ? 0.0f : PI;
    return playerPos + fromAngle(side + uniform(-maxAngle, maxAngle)) * distance;
}

float SkillShotTarget::uniform(float min, float max) {
    if (max <= min) return min;
    return std::uniform_real_distribution<float>(min, max)(rng_);
}

bool SkillShotTarget::decide(int key) {
    if (const auto it = decisions_.find(key); it != decisions_.end()) return it->second;
    const bool dodge = uniform(0.0f, 1.0f) < level_.dodgeChance;
    decisions_[key] = dodge;
    log::debug("ai", "的が{} {} に気づきました: {}", key < 0 ? "詠唱" : "発動", key < 0 ? -key : key,
               dodge ? "避ける" : "避けない");
    return dodge;
}

std::vector<SkillShotTarget::Threat> SkillShotTarget::collectThreats(World& world, const Unit& self) {
    std::vector<Threat> threats;
    std::unordered_set<int> aliveKeys;
    const float now = world.time();
    const Unit* player = world.player();

    // 詠唱の始まりと終わり。詠唱を見て決めた判断は、その詠唱から出た弾・範囲に引き継ぐ（同じスキルで 2 回迷わない）
    const bool casting = player && player->cast.active;
    if (casting && !playerWasCasting_) ++castSerial_;
    if (!casting && playerWasCasting_) {
        if (const auto it = decisions_.find(-castSerial_); it != decisions_.end()) {
            inherited_ = it->second;
            inheritedUntil_ = now + kInheritTime;
        }
    }
    playerWasCasting_ = casting;
    if (inherited_ && now > inheritedUntil_) inherited_.reset();
    if (inherited_) {
        // 引き継ぐのは、まだ判断していない発動のうち最も新しいもの（終わったばかりの詠唱から出た発動）
        int newest = 0;
        for (const Projectile& p : world.projectiles()) {
            if (p.alive && p.team == Team::Player && !decisions_.contains(p.castId))
                newest = std::max(newest, p.castId);
        }
        for (const AreaEffect& a : world.areas()) {
            if (a.alive && a.team == Team::Player && !decisions_.contains(a.castId))
                newest = std::max(newest, a.castId);
        }
        if (newest > 0) {
            decisions_[newest] = *inherited_;
            inherited_.reset();
        }
    }

    // 出てから reactionTime たった弾・範囲に気づく（判断済みの発動は、もう気づいている）
    auto noticed = [&](int key, float age) {
        aliveKeys.insert(key);
        if (!decisions_.contains(key) && age < level_.reactionTime) return false;
        return decide(key);
    };

    for (const Projectile& p : world.projectiles()) {
        // 追尾する弾（対象指定のスキル）は避けられないので、気にしない
        if (!p.alive || p.team != Team::Player || p.homingTarget != kInvalidId || p.hasHit(self.id)) continue;
        if (!noticed(p.castId, p.age)) continue;
        Threat t;
        t.key = p.castId;
        t.shape = AimShape::Line;
        t.start = p.pos;
        t.dir = p.dir;
        t.length = std::max(0.0f, p.maxDistance - p.traveled);
        t.speed = std::max(1.0f, p.speed);
        t.halfWidth = p.radius;
        threats.push_back(t);
    }
    for (const AreaEffect& a : world.areas()) {
        if (!a.alive || a.team != Team::Player || a.activated) continue;
        if (!noticed(a.castId, a.elapsed)) continue;
        Threat t;
        t.key = a.castId;
        t.shape = AimShape::Circle;
        t.center = a.center;
        t.radius = a.radius;
        t.wait = std::max(0.0f, a.delay - a.elapsed);
        threats.push_back(t);
    }

    // 詠唱を見て避ける: 詠唱中のスキルが、カーソルの方向・位置へ出ると考える
    if (casting && level_.readCast) {
        const CharacterDef* def = world.characterOf(*player);
        const SkillDef* skill = def ? def->skill(player->cast.request.slot) : nullptr;
        const std::optional<AimModel> model = skill && skill->countsForHitRate ? aimModelFor(*skill) : std::nullopt;
        if (model && noticed(-castSerial_, player->cast.total - player->cast.remaining)) {
            Threat t;
            t.key = -castSerial_;
            t.shape = model->shape;
            if (model->shape == AimShape::Line) {
                t.start = player->pos;
                t.dir = geo::normalizedOr(player->cast.request.cursor - player->pos, player->facing);
                t.length = model->range;
                t.speed = std::max(1.0f, model->speed);
                t.halfWidth = model->halfWidth;
                t.wait = player->cast.remaining;
            } else {
                t.center = geo::clampDistance(player->pos, player->cast.request.cursor, model->range);
                t.radius = model->radius;
                t.wait = player->cast.remaining + std::max(0.0f, model->delay - model->windup);
            }
            threats.push_back(t);
        }
    }

    // 消えた弾・範囲・詠唱の判断は捨てる
    std::erase_if(decisions_, [&](const auto& entry) { return !aliveKeys.contains(entry.first); });
    return threats;
}

bool SkillShotTarget::endangers(const Threat& threat, Vector2 pos, Vector2 destination, float speed, float radius,
                                float margin) const {
    // time 秒後の位置（移動先へまっすぐ進み、着いたら止まる）
    const Vector2 toDestination = destination - pos;
    const float travel = Vector2Length(toDestination);
    auto positionAfter = [&](float time) {
        if (travel < 1.0f) return pos;
        return pos + toDestination * (std::min(speed * time, travel) / travel);
    };

    if (threat.shape == AimShape::Circle) {
        const float reach = threat.radius + radius + margin;
        return Vector2DistanceSqr(positionAfter(threat.wait), threat.center) < reach * reach;
    }

    // 弾が届く時刻の位置で判定する（動いた先までの距離で、届く時刻をもう一度見積もる）
    const float reach = threat.halfWidth + radius + margin;
    Vector2 p = pos;
    for (int i = 0; i < 2; ++i) {
        const float along = Vector2DotProduct(p - threat.start, threat.dir);
        p = positionAfter(threat.wait + std::max(0.0f, along - radius) / threat.speed);
    }
    const Vector2 relative = p - threat.start;
    const float along = Vector2DotProduct(relative, threat.dir);
    if (along < -reach || along > threat.length + reach) return false;  // 弾の後ろ（通り過ぎた）か、射程の先
    return std::fabs(Vector2DotProduct(relative, geo::perpendicular(threat.dir))) < reach;
}

std::optional<Vector2> SkillShotTarget::chooseDodge(const World& world, const Unit& self,
                                                    const std::vector<Threat>& threats, const Threat& threat) {
    // 避ける先の候補。直線の弾は左右、円の範囲は中心から外へ向かう方向とその少し横
    std::vector<Vector2> candidates;
    if (threat.shape == AimShape::Line) {
        const Vector2 side = geo::perpendicular(threat.dir);
        const float lateral = Vector2DotProduct(self.pos - threat.start, side);
        const float clear = threat.halfWidth + self.radius + config_.dodgeMargin;
        candidates.push_back(self.pos + side * (clear - lateral));
        candidates.push_back(self.pos + side * (-clear - lateral));
        std::sort(candidates.begin(), candidates.end(), [&](Vector2 a, Vector2 b) {
            return Vector2DistanceSqr(a, self.pos) < Vector2DistanceSqr(b, self.pos);
        });
    } else {
        const Vector2 out = geo::normalizedOr(self.pos - threat.center, fromAngle(uniform(-PI, PI)));
        const float clear = threat.radius + self.radius + config_.dodgeMargin;
        for (const float degrees : {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f})
            candidates.push_back(threat.center + Vector2Rotate(out, degrees * DEG2RAD) * clear);
    }

    // 障害物で止まらず、着くまでに他の弾・範囲にも当たらない候補を、上の順に選ぶ
    std::vector<Vector2> safe;
    std::optional<Vector2> reachable;  // 安全な候補が無い時の、せめて障害物で止まらない候補
    for (const Vector2 candidate : candidates) {
        const Vector2 destination = world.clampToBounds(candidate, self.radius);
        if (!world.terrain().segmentClear(self.pos, destination, self.radius)) continue;
        if (!reachable) reachable = destination;
        const bool hit = std::any_of(threats.begin(), threats.end(), [&](const Threat& other) {
            return endangers(other, self.pos, destination, self.moveSpeed(), self.radius, 0.0f);
        });
        if (!hit) safe.push_back(destination);
    }
    if (safe.empty()) return reachable;
    // 直線の弾は、ときどき遠い側へ避けて、避ける向きを読まれにくくする
    if (threat.shape == AimShape::Line && safe.size() >= 2 && uniform(0.0f, 1.0f) < level_.sideRandom) return safe[1];
    return safe.front();
}

void SkillShotTarget::wander(const World& world, Unit& self, const Unit& player) {
    moveTimer_ = uniform(level_.moveIntervalMin, level_.moveIntervalMax);
    if (uniform(0.0f, 1.0f) < level_.stopChance) {
        self.moveTarget.reset();
        return;
    }
    // 自キャラから見た今の方向の近くで、距離を保てる位置を選ぶ（上下に離れすぎる位置・障害物・ブッシュの中は選ばない）
    const Vector2 away = geo::normalizedOr(self.pos - player.pos, fromAngle(uniform(-PI, PI)));
    const float baseAngle = std::atan2(away.y, away.x);
    for (int attempt = 0; attempt < 20; ++attempt) {
        const float angle = baseAngle + uniform(-kWanderAngle, kWanderAngle);
        const float distance = uniform(config_.distanceMin, config_.distanceMax);
        const Vector2 destination = world.clampToBounds(player.pos + fromAngle(angle) * distance, self.radius);
        if (std::fabs(destination.y - player.pos.y) > config_.maxVerticalOffset) continue;
        if (world.terrain().blocked(destination, self.radius) || world.terrain().bushAt(destination) >= 0) continue;
        if (Vector2Distance(destination, self.pos) < kMinWanderStep) continue;
        self.moveTarget = destination;
        return;
    }
    // 見つからなければ（壁際など）、自キャラの左右の、立てる位置へ
    const float sideX = self.pos.x >= player.pos.x ? 1.0f : -1.0f;
    self.moveTarget = world.placeable(player.pos + Vector2{sideX * config_.distanceMax, 0.0f}, self.radius);
}

void SkillShotTarget::update(World& world, float dt) {
    Unit* self = world.findUnit(unitId_);
    const Unit* player = world.player();
    if (!self || !self->alive || !player) return;
    moveTimer_ -= dt;
    // 動けない間も、弾・範囲に気づくかどうかは決めておく（動けるようになった時に、すぐ避けられるように）
    const std::vector<Threat> threats = collectThreats(world, *self);
    if (!self->status.canMove()) return;

    // 今の移動を続けると当たる弾・範囲のうち、最も早く届くもの
    const float speed = self->moveSpeed();
    const Vector2 destination = self->moveTarget.value_or(self->pos);
    const Threat* urgent = nullptr;
    float urgentTime = std::numeric_limits<float>::max();
    for (const Threat& threat : threats) {
        if (!endangers(threat, self->pos, destination, speed, self->radius, config_.dodgeMargin * 0.5f)) continue;
        const float time =
            threat.shape == AimShape::Circle
                ? threat.wait
                : threat.wait + std::max(0.0f, Vector2DotProduct(self->pos - threat.start, threat.dir)) / threat.speed;
        if (time < urgentTime) {
            urgentTime = time;
            urgent = &threat;
        }
    }

    if (urgent) {
        // 同じ弾・範囲を避けている途中なら、決めた向きのまま避け続ける（左右に迷わない）
        if (dodging_ && dodgeKey_ == urgent->key && self->moveTarget) return;
        if (const std::optional<Vector2> dodge = chooseDodge(world, *self, threats, *urgent)) {
            ++dodgeCount_;
            dodging_ = true;
            dodgeKey_ = urgent->key;
            self->moveTarget = *dodge;
            log::debug("ai", "的が{} {} を避けます: {} へ", urgent->key < 0 ? "詠唱" : "発動",
                       urgent->key < 0 ? -urgent->key : urgent->key, geo::str(*dodge));
        }
        return;
    }
    if (dodging_) {
        // 避けている途中は、避ける先に着くまで動き回らない。着いたら少し間を置いて、動き回るのに戻る
        if (self->moveTarget) return;
        dodging_ = false;
        moveTimer_ = uniform(0.15f, 0.4f);
        return;
    }
    // 自キャラに近づかれすぎたら、すぐに移動先を決め直す
    if (Vector2Distance(self->pos, player->pos) < config_.distanceMin * 0.6f) moveTimer_ = std::min(moveTimer_, 0.0f);
    if (moveTimer_ <= 0.0f) wander(world, *self, *player);
}

}  // namespace moba::ai
