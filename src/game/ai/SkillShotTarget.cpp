#include "game/ai/SkillShotTarget.h"

#include <algorithm>
#include <cmath>
#include <format>
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
constexpr float kInheritTime = 0.3f;     // 詠唱が終わってから、この時間内に出た弾・範囲に、詠唱を見た時の判断を引き継ぐ
constexpr float kPatternTimeout = 6.0f;  // これより長く続いたパターンは打ち切る（壁に引っかかった時など）

/// 角度（ラジアン。+X 方向が 0）の向きの、長さ 1 のベクトル
Vector2 fromAngle(float angle) { return Vector2{std::cos(angle), std::sin(angle)}; }

}  // namespace

void SkillShotTarget::setup(const SkillShotConfig& config, int aiLevel, std::uint32_t seed) {
    config_ = config;
    level_ = config.level(aiLevel);
    rng_.seed(seed);
    unitId_ = kInvalidId;
    pattern_ = PatternState{};
    patternActive_ = false;
    playerStillTime_ = 0.0f;
    dodging_ = false;
    dodgeKey_ = 0;
    dodgeCount_ = 0;
    decisions_.clear();
    playerWasCasting_ = false;
    castSerial_ = 0;
    inherited_.reset();
    log::info(
        "ai",
        "的の AI: 賢さ {}（{}）反応 {:.2f}s 避ける確率 {:.0f}% 詠唱を見る {} パターン 往復 {:.2f} / 接近 {:.2f} / "
        "ランダム {:.2f} / 停止 {:.2f} リズムを崩す {:.0f}%",
        aiLevel, level_.name, level_.reactionTime, level_.dodgeChance * 100.0f, level_.readCast ? "あり" : "なし",
        level_.weightStrafe, level_.weightApproach, level_.weightWander, level_.weightStop,
        level_.rhythmBreak * 100.0f);
}

Vector2 SkillShotTarget::startPosition(Vector2 playerPos) {
    const float distance = (config_.minDistance + config_.maxDistance) * 0.5f;
    return playerPos + fromAngle(uniform(-PI, PI)) * distance;
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

    // 障害物で止まらず、自キャラから離れすぎず、着くまでに他の弾・範囲にも当たらない候補を、上の順に選ぶ
    const Unit* player = world.player();
    std::vector<Vector2> safe;
    std::optional<Vector2> reachable;  // 安全な候補が無い時の、せめて障害物で止まらない候補
    for (const Vector2 candidate : candidates) {
        const Vector2 destination = world.clampToBounds(candidate, self.radius);
        if (!world.terrain().segmentClear(self.pos, destination, self.radius)) continue;
        if (player && Vector2Distance(destination, player->pos) > config_.maxDistance) continue;
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

// ---------------------------------------------------------------------------
// 動き方のパターン
// ---------------------------------------------------------------------------
std::string_view displayName(TargetPattern pattern) {
    switch (pattern) {
        case TargetPattern::Strafe: return "往復";
        case TargetPattern::Approach: return "接近";
        case TargetPattern::Wander: return "ランダム";
        case TargetPattern::Stop: return "停止";
    }
    return "?";
}

std::string SkillShotTarget::patternLabel() const {
    if (!patternActive_) return "-";
    const std::string name(displayName(pattern_.kind));
    switch (pattern_.kind) {
        case TargetPattern::Strafe:
            return std::format("{}（残り {} 回・幅 {:.0f}）", name, pattern_.swingsLeft, pattern_.width);
        case TargetPattern::Approach:
            return std::format("{}（{:.0f} まで{}）", name, pattern_.stopDistance,
                               pattern_.juking ? "・切り返し中" : (pattern_.jukeDelay >= 0.0f ? "・切り返す直前" : ""));
        case TargetPattern::Wander: return name;
        case TargetPattern::Stop: return std::format("{}（残り {:.1f} 秒）", name, pattern_.stopTime);
    }
    return name;
}

float SkillShotTarget::playerSkillRange(const World& world, const Unit& player) const {
    float range = 0.0f;
    if (const CharacterDef* def = world.characterOf(player)) {
        for (const auto& skill : def->skills) {
            if (skill && skill->countsForHitRate && aimModelFor(*skill)) range = std::max(range, skill->range);
        }
    }
    return range > 0.0f ? std::min(range, config_.maxDistance) : config_.maxDistance;
}

Vector2 SkillShotTarget::keepInRange(const World& world, const Unit& self, Vector2 playerPos,
                                     Vector2 destination) const {
    const Vector2 offset = destination - playerPos;
    const float distance = Vector2Length(offset);
    const Vector2 dir = geo::normalizedOr(offset, geo::normalizedOr(self.pos - playerPos, Vector2{1.0f, 0.0f}));
    if (distance > config_.maxDistance) destination = playerPos + dir * config_.maxDistance;
    if (distance < config_.minDistance) destination = playerPos + dir * config_.minDistance;
    return world.placeable(world.clampToBounds(destination, self.radius), self.radius);
}

Vector2 SkillShotTarget::sideStep(const World& world, const Unit& self, const Unit& player, float& side,
                                  float distance) const {
    // 自キャラから見て右 = 自キャラの方を向いた時の右手側（画面は y が下向きなので、向きを 90 度回したもの）
    const Vector2 toPlayer = geo::normalizedOr(player.pos - self.pos, Vector2{1.0f, 0.0f});
    const Vector2 right = geo::perpendicular(toPlayer);
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Vector2 destination = keepInRange(world, self, player.pos, self.pos + right * (side * distance));
        // 壁際などで、ほとんど動けない時は反対側へ
        if (Vector2Distance(destination, self.pos) >= distance * 0.5f || attempt == 1) return destination;
        side = -side;
    }
    return self.pos;
}

void SkillShotTarget::startPattern(const World& world, Unit& self, const Unit& player) {
    pattern_ = PatternState{};
    patternActive_ = true;
    const float distance = Vector2Distance(self.pos, player.pos);

    // 離れすぎていれば接近する。それ以外は重みで選ぶ
    TargetPattern kind = TargetPattern::Approach;
    if (distance <= config_.maxDistance) {
        const float weights[] = {level_.weightStrafe, level_.weightApproach, level_.weightWander, level_.weightStop};
        const float total = weights[0] + weights[1] + weights[2] + weights[3];
        kind = TargetPattern::Wander;
        if (total > 0.0f) {
            float pick = uniform(0.0f, total);
            for (int i = 0; i < 4; ++i) {
                if (pick < weights[i] || i == 3) {
                    kind = static_cast<TargetPattern>(i);
                    break;
                }
                pick -= weights[i];
            }
        }
    }

    // 接近: 近づける余地が無ければ（もう十分に近ければ）、往復にする
    if (kind == TargetPattern::Approach) {
        pattern_.stopDistance = uniform(config_.minDistance + 50.0f, std::max(config_.minDistance + 100.0f, 650.0f));
        if (distance < pattern_.stopDistance + 150.0f) kind = TargetPattern::Strafe;
    }
    pattern_.kind = kind;

    switch (kind) {
        case TargetPattern::Strafe:
            pattern_.swingsLeft = static_cast<int>(std::min(
                std::floor(uniform(level_.strafeSwingsMin, level_.strafeSwingsMax + 1.0f)), level_.strafeSwingsMax));
            pattern_.width = uniform(level_.strafeWidthMin, level_.strafeWidthMax);
            pattern_.side = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
            self.moveTarget.reset();
            break;
        case TargetPattern::Approach:
            pattern_.prevDistance = distance;
            pattern_.playerWasMoving = playerStillTime_ <= 0.0f;
            break;
        case TargetPattern::Wander: wander(world, self, player); break;
        case TargetPattern::Stop:
            pattern_.stopTime = uniform(level_.stopTimeMin, level_.stopTimeMax);
            self.moveTarget.reset();
            break;
    }
    log::debug("ai", "的の動き: {}", patternLabel());
}

void SkillShotTarget::nextSwing(const World& world, Unit& self, const Unit& player) {
    float width = pattern_.width;
    // リズムを崩す: いつもより大きく動く / 少しだけ動いてすぐ切り返す
    if (pattern_.swingIndex >= 2 && uniform(0.0f, 1.0f) < level_.rhythmBreak) {
        const bool longSwing = uniform(0.0f, 1.0f) < 0.5f;
        width *= longSwing ? 2.5f : 0.25f;
        log::debug("ai", "的がリズムを崩します: {}", longSwing ? "大きく動く" : "少しだけ動いてすぐ切り返す");
    }
    self.moveTarget = sideStep(world, self, player, pattern_.side, width);
    pattern_.side = -pattern_.side;
    ++pattern_.swingIndex;
    --pattern_.swingsLeft;
}

void SkillShotTarget::updateApproach(const World& world, Unit& self, const Unit& player, float dt) {
    if (pattern_.juking) {
        if (!self.moveTarget) startPattern(world, self, player);
        return;
    }
    const float distance = Vector2Distance(self.pos, player.pos);
    const float range = playerSkillRange(world, player);
    if (playerStillTime_ <= 0.0f) pattern_.playerWasMoving = true;

    // 撃たれそう: 近づききった・射程に入った・（射程の中で）自キャラが止まった・構えた
    if (pattern_.jukeDelay < 0.0f) {
        const bool reached = distance <= pattern_.stopDistance + 10.0f;
        const bool enteredRange = pattern_.prevDistance > range && distance <= range;
        const bool suspicious =
            distance <= range && ((pattern_.playerWasMoving && playerStillTime_ >= 0.1f) || player.cast.active);
        if (reached || enteredRange || suspicious) {
            pattern_.jukeDelay = level_.reactionTime;  // 気づいてから動くまでの遅れ
            log::debug("ai", "的が切り返しを決めました（{}）",
                       reached ? "近づききった" : (enteredRange ? "射程に入った" : "自キャラが止まった・構えた"));
        }
    }
    pattern_.prevDistance = distance;

    if (pattern_.jukeDelay >= 0.0f) {
        pattern_.jukeDelay -= dt;
        if (pattern_.jukeDelay <= 0.0f) {
            float side = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
            self.moveTarget = sideStep(world, self, player, side, uniform(250.0f, 400.0f));
            pattern_.juking = true;
            return;
        }
    }
    // 自キャラへまっすぐ近づく（自キャラが動くので、毎刻み向け直す）
    const Vector2 fromPlayer = geo::normalizedOr(self.pos - player.pos, Vector2{1.0f, 0.0f});
    self.moveTarget = keepInRange(world, self, player.pos, player.pos + fromPlayer * pattern_.stopDistance);
}

void SkillShotTarget::wander(const World& world, Unit& self, const Unit& player) {
    // 自キャラから見た今の方向の近くで、距離の範囲に収まる位置を選ぶ（障害物・ブッシュの中は選ばない）
    const Vector2 away = geo::normalizedOr(self.pos - player.pos, fromAngle(uniform(-PI, PI)));
    const float baseAngle = std::atan2(away.y, away.x);
    for (int attempt = 0; attempt < 20; ++attempt) {
        const float angle = baseAngle + uniform(-kWanderAngle, kWanderAngle);
        const float distance = uniform(config_.minDistance + 150.0f, config_.maxDistance - 150.0f);
        const Vector2 destination = world.clampToBounds(player.pos + fromAngle(angle) * distance, self.radius);
        if (world.terrain().blocked(destination, self.radius) || world.terrain().bushAt(destination) >= 0) continue;
        if (Vector2Distance(destination, self.pos) < kMinWanderStep) continue;
        self.moveTarget = destination;
        return;
    }
    // 見つからなければ（壁際など）、自キャラから離れる向きの、立てる位置へ
    self.moveTarget = keepInRange(world, self, player.pos, player.pos + away * config_.maxDistance * 0.7f);
}

void SkillShotTarget::updatePattern(const World& world, Unit& self, const Unit& player, float dt) {
    pattern_.elapsed += dt;
    // 壁に引っかかるなどして長く終わらないパターンは打ち切る
    if (pattern_.elapsed > kPatternTimeout) {
        startPattern(world, self, player);
        return;
    }
    switch (pattern_.kind) {
        case TargetPattern::Strafe:
            if (self.moveTarget) return;  // 切り返しの途中
            if (pattern_.swingsLeft <= 0) startPattern(world, self, player);
            else nextSwing(world, self, player);
            return;
        case TargetPattern::Approach: updateApproach(world, self, player, dt); return;
        case TargetPattern::Wander:
            if (!self.moveTarget) startPattern(world, self, player);
            return;
        case TargetPattern::Stop:
            pattern_.stopTime -= dt;
            if (pattern_.stopTime <= 0.0f) startPattern(world, self, player);
            return;
    }
}

// ---------------------------------------------------------------------------
// 毎刻みの処理
// ---------------------------------------------------------------------------
void SkillShotTarget::update(World& world, float dt) {
    Unit* self = world.findUnit(unitId_);
    const Unit* player = world.player();
    if (!self || !self->alive || !player) return;
    playerStillTime_ = (!player->moveTarget && !player->cast.active) ? playerStillTime_ + dt : 0.0f;
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
        // 避けている途中は、避ける先に着くまでパターンを止める。着いたら、新しいパターンを始める
        if (self->moveTarget) return;
        dodging_ = false;
        startPattern(world, *self, *player);
        return;
    }
    if (!patternActive_) {
        startPattern(world, *self, *player);
        return;
    }
    updatePattern(world, *self, *player, dt);
}

}  // namespace moba::ai
