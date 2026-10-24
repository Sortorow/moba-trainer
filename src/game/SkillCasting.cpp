#include "game/SkillCasting.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/CharacterDef.h"
#include "data/Settings.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::casting {
namespace {

// 対象指定スキルで、カーソルと敵の当たり判定の間にこの距離までの隙間があっても対象にする
constexpr float kUnitPickTolerance = 60.0f;

const SkillDef* skillOf(const World& world, const Unit& caster, SkillSlot slot) {
    const CharacterDef* def = world.characterOf(caster);
    return def ? def->skill(slot) : nullptr;
}

bool needsTarget(Targeting targeting) { return targeting == Targeting::Unit || targeting == Targeting::Prop; }

// 対象（敵・設置物）の現在位置。対象が消えていれば nullopt
std::optional<Vector2> targetPosition(World& world, const CastRequest& request) {
    if (request.propKind == PropTargetKind::Dove || request.propKind == PropTargetKind::Hat) {
        const Prop* prop = world.findProp(request.propId);
        if (prop && prop->alive) return prop->pos;
        return std::nullopt;
    }
    const Unit* unit = world.findUnit(request.targetUnit);
    if (unit && unit->alive) return unit->pos;
    return std::nullopt;
}

// 設置物などを指定するスキル（エマの R）の対象を、カーソルに最も近いものから選ぶ
bool resolvePropTarget(World& world, const Unit& caster, const SkillDef& def, CastRequest& request) {
    const float pickRadius = def.numOr("pickRadius", 150.0f);
    const std::vector<std::string> kinds = def.texts("targetKinds");
    auto allows = [&](std::string_view kind) { return std::find(kinds.begin(), kinds.end(), kind) != kinds.end(); };

    float bestDistance = std::numeric_limits<float>::max();
    for (const Prop& prop : world.props()) {
        if (!prop.alive || prop.owner != caster.id || prop.hiddenTime > 0.0f) continue;
        const PropTargetKind kind = prop.kind == PropKind::Dove ? PropTargetKind::Dove : PropTargetKind::Hat;
        if (!allows(toString(kind))) continue;
        const float distance = Vector2Distance(prop.pos, request.cursor);
        if (distance <= pickRadius && distance < bestDistance) {
            bestDistance = distance;
            request.propKind = kind;
            request.propId = prop.id;
            request.targetUnit = kInvalidId;
        }
    }
    if (allows(toString(PropTargetKind::Rabbit))) {
        for (const Unit& unit : world.units()) {
            if (!unit.alive || unit.team == caster.team || !unit.status.has(EffectType::Polymorph)) continue;
            const float distance = std::max(0.0f, Vector2Distance(unit.pos, request.cursor) - unit.radius);
            if (distance <= pickRadius && distance < bestDistance) {
                bestDistance = distance;
                request.propKind = PropTargetKind::Rabbit;
                request.targetUnit = unit.id;
                request.propId = kInvalidId;
            }
        }
    }
    return request.propKind != PropTargetKind::None;
}

void finishCast(World& world, Unit& caster) {
    const CastRequest request = caster.cast.request;
    caster.cast = CastState{};

    const SkillDef* def = skillOf(world, caster, request.slot);
    if (!def) return;  // 設定の再読み込みでスキルが無くなった
    const skills::SkillHandler handler = skills::findHandler(def->handler);
    if (!handler) {
        log::error("skill", "{} の {} の handler \"{}\" は存在しません", caster.characterId, toString(request.slot),
                   def->handler);
        return;
    }

    const int castId = world.newCastId();
    world.beginCast(castId, caster.team, request.slot);
    if (def->countsForHitRate && caster.team == Team::Player) world.stats().recordCast(castId, request.slot);
    log::info("skill", "{} が [{}] {} を発動 castId={} 位置{} カーソル{}", caster.name, toString(request.slot), def->name,
              castId, geo::str(caster.pos), geo::str(request.cursor));

    skills::SkillContext ctx{world, caster, *def, request, castId};
    handler(ctx);
    world.endCastIfIdle(castId);

    // 詠唱中に押されていたスキルを続けて発動する
    if (caster.bufferedCast) {
        const CastRequest next = *caster.bufferedCast;
        caster.bufferedCast.reset();
        requestCast(world, caster, next);
    }
}

void beginCast(World& world, Unit& caster, const SkillDef& def, const CastRequest& request) {
    caster.cooldowns[static_cast<std::size_t>(index(def.slot))] = def.cooldown * world.cooldownScaleFor(caster);
    caster.cast = CastState{true, request, def.castTime, def.castTime};

    const Vector2 aim = needsTarget(def.targeting) ? targetPosition(world, request).value_or(request.cursor)
                                                   : request.cursor;
    caster.facing = geo::normalizedOr(aim - caster.pos, caster.facing);
    if (def.castTime > 0.0f) {
        log::debug("skill", "{} が [{}] {} の詠唱開始（{:.2f}s）", caster.name, toString(def.slot), def.name, def.castTime);
    } else {
        finishCast(world, caster);
    }
}

}  // namespace

void orderMove(World& world, Unit& unit, Vector2 destination) {
    if (unit.pendingCast) {
        log::debug("skill", "移動の指示により、射程外の対象への接近を中止しました");
        unit.pendingCast.reset();
    }
    unit.moveTarget = world.clampToBounds(destination, unit.radius);
}

void requestCast(World& world, Unit& caster, const CastRequest& request) {
    const SkillDef* def = skillOf(world, caster, request.slot);
    if (!def) {
        log::debug("skill", "[{}] にはスキルがありません", toString(request.slot));
        return;
    }
    if (caster.team == Team::Player && world.rules().movementSkillsOnly && !def->movementSkill) {
        log::debug("skill", "[{}] {}: このモードでは移動系のスキルだけ使えます", toString(request.slot), def->name);
        return;
    }
    const float cooldown = caster.cooldowns[static_cast<std::size_t>(index(request.slot))];
    if (cooldown > 0.0f) {
        log::debug("skill", "[{}] {} はクールダウン中（残り {:.1f}s）", toString(request.slot), def->name, cooldown);
        return;
    }
    if (!caster.status.canCast()) {
        log::debug("skill", "[{}] {} は行動不能のため使えません", toString(request.slot), def->name);
        return;
    }
    if (caster.cast.active) {
        caster.bufferedCast = request;
        log::debug("skill", "詠唱中のため [{}] {} を予約しました", toString(request.slot), def->name);
        return;
    }

    CastRequest resolved = request;
    if (def->targeting == Targeting::Unit) {
        const Unit* target = world.enemyAt(request.cursor, caster.team, kUnitPickTolerance);
        if (!target) {
            log::debug("skill", "[{}] {}: カーソルの位置に敵がいません", toString(request.slot), def->name);
            return;
        }
        resolved.targetUnit = target->id;
    } else if (def->targeting == Targeting::Prop) {
        if (!resolvePropTarget(world, caster, *def, resolved)) {
            log::debug("skill", "[{}] {}: カーソルの近くに対象がありません", toString(request.slot), def->name);
            return;
        }
    }

    if (needsTarget(def->targeting)) {
        const std::optional<Vector2> targetPos = targetPosition(world, resolved);
        if (targetPos && Vector2Distance(caster.pos, *targetPos) > def->range) {
            caster.pendingCast = resolved;
            caster.moveTarget = *targetPos;
            log::debug("skill", "[{}] {}: 射程外のため、対象に近づいてから発動します", toString(request.slot), def->name);
            return;
        }
    }
    caster.pendingCast.reset();
    beginCast(world, caster, *def, resolved);
}

void update(World& world, Unit& caster, float dt) {
    if (caster.cast.active && !caster.status.canCast()) cancelCast(world, caster, "行動不能");

    if (caster.cast.active) {
        caster.cast.remaining -= dt;
        if (caster.cast.remaining <= 0.0f) finishCast(world, caster);
        return;
    }

    if (caster.pendingCast) {
        const CastRequest request = *caster.pendingCast;
        const SkillDef* def = skillOf(world, caster, request.slot);
        const std::optional<Vector2> targetPos = targetPosition(world, request);
        if (!def || !targetPos) {
            caster.pendingCast.reset();
            caster.moveTarget.reset();
            log::debug("skill", "対象がいなくなったため、[{}] の発動を中止しました", toString(request.slot));
            return;
        }
        if (Vector2Distance(caster.pos, *targetPos) <= def->range) {
            caster.pendingCast.reset();
            caster.moveTarget.reset();  // 発動のために近づいていただけなので、ここで止まる
            if (!caster.status.canCast()) return;
            beginCast(world, caster, *def, request);
        } else {
            caster.moveTarget = *targetPos;  // 動く対象を追いかける
        }
    }
}

void cancelCast(World&, Unit& caster, std::string_view reason) {
    if (!caster.cast.active) return;
    log::debug("skill", "{} の [{}] の詠唱を中断しました（{}）", caster.name, toString(caster.cast.request.slot), reason);
    caster.cast = CastState{};
    caster.bufferedCast.reset();
}

}  // namespace moba::casting
