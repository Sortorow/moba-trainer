#include "game/World.h"

#include <algorithm>
#include <limits>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/GameData.h"
#include "game/SkillCasting.h"
#include "game/UnitCollision.h"

namespace moba {

World::World(const GameData& data, Rectangle bounds) : data_(&data), terrain_(bounds) {}

World::World(const GameData& data, Terrain terrain) : data_(&data), terrain_(std::move(terrain)) {}

const Settings& World::settings() const { return data_->settings(); }

void World::update(float dt) {
    time_ += dt;
    recentHits_.clear();
    for (Unit& unit : units_) updateUnit(unit, dt);
    // 押し合いは、全キャラが動いた後にまとめて直す（キャラを処理する順番で結果が変わらないように）
    collision::resolve(*this, dt);
    flushSpawns();
    updateProjectiles(dt);
    updateAreas(dt);
    updateProps(dt);
    updateVfx(dt);
    flushSpawns();
    removeDead();
}

// ---------------------------------------------------------------------------
// 追加
// ---------------------------------------------------------------------------
EntityId World::addUnit(Unit unit) {
    unit.id = ++lastEntityId_;
    unit.prevPos = unit.pos;  // 置いた位置からは、まだ動いていない（押し合いで「動いた側」にしない）
    log::info("world", "キャラを配置: #{} {} 位置{}", unit.id, unit.name, geo::str(unit.pos));
    units_.push_back(std::move(unit));
    return units_.back().id;
}

void World::removeUnit(EntityId id) {
    std::erase_if(units_, [id](const Unit& unit) { return unit.id == id; });
}

EntityId World::spawnProjectile(Projectile projectile) {
    projectile.id = ++lastEntityId_;
    projectile.origin = projectile.pos;  // 出た位置（これより後ろには当たらない）
    if (const auto it = casts_.find(projectile.castId); it != casts_.end()) ++it->second.liveEntities;
    pendingProjectiles_.push_back(std::move(projectile));
    return pendingProjectiles_.back().id;
}

EntityId World::spawnArea(AreaEffect area) {
    area.id = ++lastEntityId_;
    if (const auto it = casts_.find(area.castId); it != casts_.end()) ++it->second.liveEntities;
    pendingAreas_.push_back(std::move(area));
    return pendingAreas_.back().id;
}

EntityId World::spawnProp(Prop prop) {
    prop.id = ++lastEntityId_;
    log::debug("world", "{}を設置: #{} 位置{} 持続{:.1f}s", displayName(prop.kind), prop.id, geo::str(prop.pos),
               prop.lifetime);
    pendingProps_.push_back(prop);
    return prop.id;
}

void World::addVfx(Vfx vfx) { vfx_.push_back(std::move(vfx)); }

void World::flushSpawns() {
    for (Projectile& p : pendingProjectiles_) projectiles_.push_back(std::move(p));
    pendingProjectiles_.clear();
    for (AreaEffect& a : pendingAreas_) areas_.push_back(std::move(a));
    pendingAreas_.clear();
    for (Prop& p : pendingProps_) props_.push_back(p);
    pendingProps_.clear();
}

void World::removeDead() {
    for (const Projectile& p : projectiles_) {
        if (!p.alive) releaseCastEntity(p.castId);
    }
    for (const AreaEffect& a : areas_) {
        if (!a.alive) releaseCastEntity(a.castId);
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return !p.alive; });
    std::erase_if(areas_, [](const AreaEffect& a) { return !a.alive; });
    std::erase_if(props_, [](const Prop& p) { return !p.alive; });
    std::erase_if(vfx_, [](const Vfx& v) { return v.remaining <= 0.0f; });
}

// ---------------------------------------------------------------------------
// 検索
// ---------------------------------------------------------------------------
Unit* World::findUnit(EntityId id) {
    for (Unit& unit : units_) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

const Unit* World::findUnit(EntityId id) const {
    for (const Unit& unit : units_) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

Prop* World::findProp(EntityId id) {
    for (Prop& prop : props_) {
        if (prop.id == id) return &prop;
    }
    return nullptr;
}

Unit* World::player() {
    for (Unit& unit : units_) {
        if (unit.team == Team::Player) return &unit;
    }
    return nullptr;
}

const Unit* World::player() const {
    for (const Unit& unit : units_) {
        if (unit.team == Team::Player) return &unit;
    }
    return nullptr;
}

const CharacterDef* World::characterOf(const Unit& unit) const {
    return unit.characterId.empty() ? nullptr : data_->character(unit.characterId);
}

Unit* World::enemyAt(Vector2 point, Team myTeam, float tolerance) {
    Unit* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit& unit : units_) {
        if (!unit.alive || unit.team == myTeam || !canSee(myTeam, unit)) continue;
        const float distance = Vector2Distance(point, unit.pos) - unit.radius;
        if (distance <= tolerance && distance < bestDistance) {
            best = &unit;
            bestDistance = distance;
        }
    }
    return best;
}

Unit* World::nearestEnemy(Vector2 from, Team myTeam, float maxDistance, std::string_view preferMark) {
    Unit* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (Unit& unit : units_) {
        if (!unit.alive || unit.team == myTeam || !canSee(myTeam, unit)) continue;
        const float distance = Vector2Distance(from, unit.pos);
        if (distance > maxDistance) continue;
        // 印が付いている敵を優先する（距離に関係なく、印の付いていない敵より先に選ぶ）
        const bool marked = !preferMark.empty() && unit.status.hasMark(preferMark);
        const float score = marked ? distance - 1e6f : distance;
        if (score < bestScore) {
            best = &unit;
            bestScore = score;
        }
    }
    return best;
}

std::vector<Unit*> World::enemiesInCircle(Vector2 center, float radius, Team myTeam) {
    std::vector<Unit*> result;
    for (Unit& unit : units_) {
        if (unit.alive && unit.team != myTeam && geo::circlesOverlap(center, radius, unit.pos, unit.radius)) {
            result.push_back(&unit);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// 視界
// ---------------------------------------------------------------------------
bool World::canSee(Team observer, const Unit& target) const {
    if (target.team == observer || target.revealTime > 0.0f) return true;
    return canSeePoint(observer, target.pos);
}

bool World::canSeePoint(Team observer, Vector2 point) const {
    if (rules_.fullVision) return true;
    const float radius = settings().visionRadius;
    return std::any_of(units_.begin(), units_.end(), [&](const Unit& unit) {
        // 見る側が入っているブッシュは、視線を遮らない
        return unit.alive && unit.team == observer && Vector2Distance(unit.pos, point) <= radius &&
               vision::lineOfSight(terrain_, unit.pos, point, terrain_.bushAt(unit.pos));
    });
}

std::vector<vision::VisionRay> World::visionPolygon(const Unit& viewer) const {
    return vision::visionPolygon(terrain_, viewer.pos, settings().visionRadius, terrain_.bushAt(viewer.pos));
}

void World::revealAttacker(Unit& caster, const Unit& target) {
    if (target.team == caster.team || canSee(target.team, caster)) return;
    caster.revealTime = settings().attackRevealTime;
    log::debug("vision", "{} が見えていない所から {} を対象指定のスキルで撃ったため、{:.1f} 秒間見えます", caster.name,
               target.name, caster.revealTime);
}

// ---------------------------------------------------------------------------
// 共通処理
// ---------------------------------------------------------------------------
void World::registerHit(int castId, Team attacker, SkillSlot slot, std::string_view label, Unit& target) {
    stats_.recordHit(castId);
    target.hitFlash = 0.25f;
    recentHits_.push_back(HitRecord{attacker, target.id, target.pos});
    log::info("hit", "castId={} [{}] {} が {} に命中 位置{}", castId, toString(slot), label, target.name,
              geo::str(target.pos));
    if (hitObserver_) hitObserver_(castId, slot, target);

    // 敵の発動が自キャラに当たった: 1 回の発動につき HP を 1 減らす（弾が複数当たっても 1 回）
    if (target.team != Team::Player) return;
    const auto it = casts_.find(castId);
    if (it == casts_.end() || it->second.team != Team::Enemy || it->second.hitPlayer) return;
    it->second.hitPlayer = true;
    damagePlayer(target);
}

void World::damagePlayer(Unit& player) {
    if (player.maxHp <= 0) return;
    --player.hp;
    log::info("dodge", "被弾: HP {} / {}", player.hp, player.maxHp);
    if (player.hp > 0) return;
    ++player.deaths;
    player.hp = player.maxHp;
    addVfx(makeRingVfx(Team::Player, player.pos, player.radius * 2.5f, Color{255, 90, 90, 255}, 0.6f));
    addVfx(makeTextVfx(Team::Player, player.pos, "ダウン!", Color{255, 120, 120, 255}));
    log::info("dodge", "HP が 0 になりました（{} 回目）。全回復します", player.deaths);
}

// ---------------------------------------------------------------------------
// 発動の記録（ドッジ率の集計）
// ---------------------------------------------------------------------------
void World::beginCast(int castId, Team team, SkillSlot slot, bool probe) {
    casts_[castId] = CastRecord{team, slot, 0, false, probe};
}

void World::endCastIfIdle(int castId) {
    const auto it = casts_.find(castId);
    if (it != casts_.end() && it->second.liveEntities <= 0) finishCastRecord(castId);
}

void World::releaseCastEntity(int castId) {
    const auto it = casts_.find(castId);
    if (it == casts_.end()) return;
    if (--it->second.liveEntities <= 0) finishCastRecord(castId);
}

void World::finishCastRecord(int castId) {
    const auto it = casts_.find(castId);
    if (it == casts_.end()) return;
    const CastRecord record = it->second;
    casts_.erase(it);
    stats_.settle(castId);  // 自キャラの発動なら、ヒット率の「外れ」もここで確定する
    if (record.team != Team::Enemy) return;
    // 確かめ撃ちは、自キャラを狙った撃ち方ではない。外れたものは数えない（いない所へ撃った弾を「回避」にしない）。
    // 当たったものは HP が減るので、敵の発動 1・被弾 1 として数える（被弾の数が HP・ダウンの数と合うように）
    if (record.probe && !record.hitPlayer) {
        log::debug("dodge", "castId={} [{}] の結果: 当たらなかった（確かめ撃ちなので、ドッジ率に数えない）", castId,
                   toString(record.slot));
        return;
    }

    ++dodge_.total;
    if (record.hitPlayer) {
        ++dodge_.hit;
    } else {
        ++dodge_.dodged;
    }
    log::debug("dodge", "castId={} [{}] の結果: {}{}（ドッジ {} / {}）", castId, toString(record.slot),
               record.hitPlayer ? "被弾" : "回避", record.probe ? "（確かめ撃ち）" : "", dodge_.dodged, dodge_.total);
}

Vector2 World::clampToBounds(Vector2 point, float radius) const {
    const Rectangle b = terrain_.bounds();
    return Vector2{std::clamp(point.x, b.x + radius, b.x + b.width - radius),
                   std::clamp(point.y, b.y + radius, b.y + b.height - radius)};
}

void World::setCooldownScale(Team team, float scale) {
    (team == Team::Player ? playerCooldownScale_ : enemyCooldownScale_) = std::max(0.0f, scale);
}

// ---------------------------------------------------------------------------
// キャラ
// ---------------------------------------------------------------------------
void World::updateUnit(Unit& unit, float dt) {
    for (float& cooldown : unit.cooldowns) cooldown = std::max(0.0f, cooldown - dt);
    unit.hitFlash = std::max(0.0f, unit.hitFlash - dt);
    unit.revealTime = std::max(0.0f, unit.revealTime - dt);

    casting::update(*this, unit, dt);

    // 硬直のある詠唱中と、拘束などで動けない間は移動しない（目的地は覚えておき、動けるようになったら再開する）
    const bool castLocked = unit.cast.active && unit.cast.locksMovement;
    const Vector2 beforeWalk = unit.pos;
    if (unit.moveTarget && !castLocked && unit.status.canMove()) moveAlongPath(unit, dt);
    // 歩いて動いたか（押し合いの許可が、詠唱を始めた時に終わるかの判定に使う。ワープは含まない）
    unit.walking = Vector2DistanceSqr(unit.pos, beforeWalk) > 1e-6f;
    if (!unit.moveTarget) {
        unit.path.clear();
        unit.pathGoal.reset();
    }
    // ワープなどで障害物やステージの外にかかっていたら押し出す
    unit.pos = terrain_.pushOut(unit.pos, unit.radius);

    // 状態異常の残り時間を減らすのは、詠唱と移動の後にする。状態異常を付けるのは、この後の弾と範囲の処理だけなので、
    // 付いた効果は次の刻みの詠唱・移動に必ず効く。先に減らすと、長さ 1 刻み以下の効果が移動の前に消えてしまう。
    // AI・入力・HUD から見た状態異常とも、同じ刻みで効くようになる
    unit.status.update(dt);
}

// 目的地（moveTarget）へ、壁を回り込む道のり（path）に沿って進む
void World::moveAlongPath(Unit& unit, float dt) {
    const Vector2 goal = *unit.moveTarget;
    // 目的地が変わった・道のりが無い・ワープなどで次の点へまっすぐ行けなくなった時は、道のりを作り直す
    const bool goalChanged = !unit.pathGoal || Vector2DistanceSqr(*unit.pathGoal, goal) > 0.25f;
    if (goalChanged || unit.path.empty() || !terrain_.segmentClear(unit.pos, unit.path.front(), unit.radius * 0.9f)) {
        unit.path = terrain_.findPath(unit.pos, goal, unit.radius);
        unit.pathGoal = goal;
        if (unit.path.empty()) {
            log::debug("move", "{}: {} へはたどり着けないため、移動をやめます", unit.name, geo::str(goal));
            unit.moveTarget.reset();
            return;
        }
    }

    float step = unit.moveSpeed() * dt;
    while (step > 0.0f && !unit.path.empty()) {
        const Vector2 toNext = unit.path.front() - unit.pos;
        const float distance = Vector2Length(toNext);
        if (distance <= step) {
            unit.pos = unit.path.front();
            unit.path.erase(unit.path.begin());
            step -= distance;
        } else {
            const Vector2 dir = toNext / distance;
            unit.pos = unit.pos + dir * step;
            unit.facing = dir;
            step = 0.0f;
        }
    }
    if (unit.path.empty()) unit.moveTarget.reset();  // 着いた
}

// ---------------------------------------------------------------------------
// 弾
// ---------------------------------------------------------------------------
void World::updateProjectiles(float dt) {
    for (Projectile& p : projectiles_) {
        if (!p.alive) continue;
        p.age += dt;
        if (p.homingTarget != kInvalidId) {
            updateHomingProjectile(p, dt);
            continue;
        }

        const float step = std::min(p.speed * dt, p.maxDistance - p.traveled);
        const Vector2 from = p.pos;
        const Vector2 to = p.pos + p.dir * step;

        // この刻みで通過した範囲にいる敵を、弾の進行方向に近い順に並べる

        /// 弾がこの刻みで触れた敵（進行方向に沿った距離と一緒に持つ）
        struct Candidate {
            Unit* unit;
            float along;  // 弾の進行方向に沿った距離
        };
        std::vector<Candidate> candidates;
        for (Unit& unit : units_) {
            if (!unit.alive || unit.team == p.team || p.hasHit(unit.id)) continue;
            // 出た位置より後ろ（発射した向きの反対側）には当たらない
            if (!geo::sweptCircleHitsCircleAhead(p.origin, p.dir, from, to, p.radius, unit.pos, unit.radius)) continue;
            candidates.push_back({&unit, Vector2DotProduct(unit.pos - from, p.dir)});
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) { return a.along < b.along; });

        p.pos = to;
        p.traveled += step;
        for (const Candidate& candidate : candidates) {
            hitWithProjectile(p, *candidate.unit);
            if (!p.pierce) {
                p.pos = from + p.dir * std::clamp(candidate.along, 0.0f, step);
                p.alive = false;
                break;
            }
        }
        if (p.alive && p.traveled >= p.maxDistance - 1e-3f) expireProjectile(p);
    }
}

void World::updateHomingProjectile(Projectile& p, float dt) {
    Unit* target = findUnit(p.homingTarget);
    if (!target || !target->alive) {
        p.alive = false;
        log::debug("skill", "castId={} {} は追尾対象がいなくなったため消滅", p.castId, p.label);
        return;
    }
    const Vector2 toTarget = target->pos - p.pos;
    const float distance = Vector2Length(toTarget);
    const float step = p.speed * dt;
    if (distance <= step + p.radius + target->radius) {
        p.pos = target->pos;
        hitWithProjectile(p, *target);
        p.alive = false;
        return;
    }
    p.dir = toTarget / distance;
    p.pos = p.pos + p.dir * step;
    p.traveled += step;
}

void World::hitWithProjectile(Projectile& p, Unit& target) {
    p.hitUnits.push_back(target.id);
    // 味方（自分自身）への到達は命中として数えない（ブランドの R が自分に跳ね返る時など）
    if (target.team != p.team) registerHit(p.castId, p.team, p.slot, p.label, target);
    if (p.onHit) p.onHit(*this, p, target);
}

void World::expireProjectile(Projectile& p) {
    if (p.onExpire) p.onExpire(*this, p);
    p.alive = false;
    if (p.hitUnits.empty())
        log::debug("skill", "castId={} [{}] {} は命中せずに射程の端で消滅", p.castId, toString(p.slot), p.label);
}

// ---------------------------------------------------------------------------
// 範囲効果
// ---------------------------------------------------------------------------
void World::updateAreas(float dt) {
    for (AreaEffect& a : areas_) {
        if (!a.alive) continue;
        a.elapsed += dt;
        if (!a.activated) {
            if (a.elapsed < a.delay) continue;
            a.activated = true;
            if (a.onActivate) a.onActivate(*this, a);
            if (a.duration <= 0.0f) {
                // 発動の瞬間だけ判定する範囲
                for (Unit* unit : enemiesInCircle(a.center, a.radius, a.team)) hitWithArea(a, *unit);
                addVfx(makeBurstVfx(a.team, a.center, a.radius, a.color));
                if (a.hitUnits.empty())
                    log::debug("skill", "castId={} [{}] {} は誰にも当たらなかった", a.castId, toString(a.slot),
                               a.label);
                a.alive = false;
                continue;
            }
        }
        // 一定時間残る範囲: 踏むと発動する物を持っていれば、発動した刻みから、残っている物に触れた敵が踏む
        // （着地した時に既に上に立っている敵も、ワープで上に来た敵も、触れていれば踏む）。
        // 物は円の中に置くので、円より物 1 つ分広い範囲の敵だけを調べればよい
        if (a.stepTriggers) {
            for (Unit* unit : enemiesInCircle(a.center, a.radius + a.stepTriggers->radius, a.team)) {
                if (a.stepTriggers->positions.empty()) break;
                stepOnTriggers(a, *unit);
            }
        }
        if (a.elapsed >= a.delay + a.duration) a.alive = false;
    }
}

void World::hitWithArea(AreaEffect& a, Unit& target) {
    a.hitUnits.push_back(target.id);
    registerHit(a.castId, a.team, a.slot, a.label, target);
    if (a.onUnitHit) a.onUnitHit(*this, a, target);
}

void World::stepOnTriggers(AreaEffect& a, Unit& unit) {
    StepTriggers& triggers = *a.stepTriggers;
    const std::size_t stepped = triggers.consumeTouching(unit.pos, unit.radius);
    if (stepped == 0) return;
    const bool firstStep = !a.hasHit(unit.id);
    log::debug("skill", "castId={} [{}] {}: {} が {} 個踏んだ（残り {} 個）", a.castId, toString(a.slot), a.label,
               unit.name, stepped, triggers.positions.size());
    if (firstStep) hitWithArea(a, unit);  // 命中として数えるのは、1 回の発動につき初めて踏んだ時だけ
    if (triggers.onStep) triggers.onStep(*this, unit, firstStep);
}

// ---------------------------------------------------------------------------
// 設置物・演出
// ---------------------------------------------------------------------------
void World::updateProps(float dt) {
    for (Prop& prop : props_) {
        if (!prop.alive) continue;
        prop.hiddenTime = std::max(0.0f, prop.hiddenTime - dt);
        prop.remaining -= dt;
        if (prop.remaining <= 0.0f) {
            prop.alive = false;
            log::debug("world", "{} #{} が時間切れで消滅", displayName(prop.kind), prop.id);
        }
    }
}

void World::updateVfx(float dt) {
    for (Vfx& vfx : vfx_) vfx.remaining -= dt;
}

}  // namespace moba
