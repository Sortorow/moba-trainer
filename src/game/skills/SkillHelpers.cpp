#include <format>

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {

Vector2 aimDirection(const SkillContext& ctx) {
    return geo::normalizedOr(ctx.request.cursor - ctx.caster.pos, ctx.caster.facing);
}

Vector2 aimPoint(const SkillContext& ctx) {
    return geo::clampDistance(ctx.caster.pos, ctx.request.cursor, ctx.def.range);
}

Projectile makeProjectile(const SkillContext& ctx, Vector2 origin, Vector2 dir, float range) {
    Projectile p;
    p.owner = ctx.caster.id;
    p.team = ctx.caster.team;
    p.castId = ctx.castId;
    p.slot = ctx.def.slot;
    p.label = ctx.def.name;
    p.pos = origin;
    p.dir = dir;
    p.maxDistance = range;
    p.speed = ctx.def.num("speed", 1500.0f);
    p.radius = ctx.def.num("width", 80.0f) * 0.5f;
    p.pierce = ctx.def.flagOr("pierce", false);
    p.visual = projectileVisualFromString(ctx.def.text("visual", "orb"));
    p.color = ctx.def.color("color", ctx.caster.color);
    return p;
}

AreaEffect makeArea(const SkillContext& ctx, Vector2 center) {
    AreaEffect a;
    a.owner = ctx.caster.id;
    a.team = ctx.caster.team;
    a.castId = ctx.castId;
    a.slot = ctx.def.slot;
    a.label = ctx.def.name;
    a.origin = ctx.caster.pos;
    a.center = center;
    a.color = ctx.def.color("color", ctx.caster.color);
    return a;
}

void applyEffects(World& world, Unit& target, const std::vector<EffectSpec>& effects, EntityId source) {
    for (const EffectSpec& effect : effects) {
        target.status.apply(effect, source);
        if (effect.type == EffectType::Mark) {
            log::debug("status", "{} に印「{}」（{:.1f}s）", target.name, effect.tag, effect.duration);
            continue;
        }
        log::debug("status", "{} に{}（{:.2f}s, 強さ{:.0f}%）", target.name, displayName(effect.type), effect.duration,
                   effect.amount * 100.0f);
        world.addVfx(makeTextVfx(target.pos, std::string(displayName(effect.type)), Color{255, 230, 140, 255}));
    }
}

bool detonateMark(World& world, Unit& target, std::string_view markTag, Color color) {
    if (markTag.empty() || !target.status.consumeMark(markTag)) return false;
    world.addVfx(makeBurstVfx(target.pos, target.radius * 2.2f, color, 0.5f));
    world.addVfx(makeRingVfx(target.pos, target.radius * 2.8f, color, 0.5f));
    world.addVfx(makeTextVfx(target.pos, "起爆!", color));
    log::info("skill", "{} の印「{}」を起爆", target.name, markTag);
    return true;
}

void teleport(World& world, Unit& unit, Vector2 destination, Color color) {
    const Vector2 from = unit.pos;
    unit.pos = world.clampToBounds(destination, unit.radius);
    unit.facing = geo::normalizedOr(unit.pos - from, unit.facing);
    world.addVfx(makeTeleportVfx(from, unit.pos, color));
    log::debug("skill", "{} が {} から {} へ移動", unit.name, geo::str(from), geo::str(unit.pos));
}

}  // namespace moba::skills
