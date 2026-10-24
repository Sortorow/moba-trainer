// 複数のキャラで使い回せる汎用のスキル

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {

// params: speed, width, pierce, visual, color, onHit（当たった時の効果）, detonateMark（起爆する印の名前）
void handleProjectile(SkillContext& ctx) {
    Projectile p = makeProjectile(ctx, ctx.caster.pos, aimDirection(ctx), ctx.def.range);
    const std::vector<EffectSpec> effects = ctx.def.effects("onHit");
    const std::string mark = ctx.def.text("detonateMark", "");
    p.onHit = [effects, mark](World& world, Projectile& self, Unit& target) {
        detonateMark(world, target, mark, self.color);
        applyEffects(world, target, effects, self.owner);
    };
    ctx.world.spawnProjectile(std::move(p));
}

// params: speed, width, visual, color, onHit, detonateMark
void handleHomingProjectile(SkillContext& ctx) {
    Unit* target = ctx.world.findUnit(ctx.request.targetUnit);
    if (!target || !target->alive) {
        log::debug("skill", "[{}] {}: 対象がいなくなったため不発", toString(ctx.def.slot), ctx.def.name);
        return;
    }
    const Vector2 dir = geo::normalizedOr(target->pos - ctx.caster.pos, ctx.caster.facing);
    Projectile p = makeProjectile(ctx, ctx.caster.pos, dir, 100000.0f);
    p.homingTarget = target->id;
    const std::vector<EffectSpec> effects = ctx.def.effects("onHit");
    const std::string mark = ctx.def.text("detonateMark", "");
    p.onHit = [effects, mark](World& world, Projectile& self, Unit& hit) {
        detonateMark(world, hit, mark, self.color);
        applyEffects(world, hit, effects, self.owner);
    };
    ctx.world.spawnProjectile(std::move(p));
}

// カーソルが射程内ならその位置へ、射程外なら射程の端へ瞬間移動する（params なし）
void handleBlink(SkillContext& ctx) {
    teleport(ctx.world, ctx.caster, aimPoint(ctx), ctx.def.color("color", Color{255, 240, 160, 255}));
}

// params: flightTime（着地までの時間）, radius, duration（残る時間）, visual, color,
//         slowAmount / lingerTime（範囲内にいる間かかるスロウと、出た後に残る時間）, onHit（最初に入った時の効果）
void handleGroundZone(SkillContext& ctx) {
    AreaEffect a = makeArea(ctx, aimPoint(ctx));
    a.delay = ctx.def.num("flightTime", 0.3f);
    a.duration = ctx.def.num("duration", 4.0f);
    a.radius = ctx.def.num("radius", 200.0f);
    a.visual = areaVisualFromString(ctx.def.text("visual", "caltrops"));

    const EffectSpec slow{EffectType::Slow, ctx.def.numOr("lingerTime", 0.5f), ctx.def.numOr("slowAmount", 0.0f), ""};
    if (slow.amount > 0.0f) {
        a.whileInside = [slow](World&, AreaEffect& self, Unit& unit) { unit.status.apply(slow, self.owner); };
    }
    const std::vector<EffectSpec> effects = ctx.def.effects("onHit");
    a.onUnitHit = [effects](World& world, AreaEffect& self, Unit& unit) {
        applyEffects(world, unit, effects, self.owner);
        if (self.whileInside) world.addVfx(makeTextVfx(unit.pos, "スロウ", Color{150, 210, 255, 255}));
    };
    ctx.world.spawnArea(std::move(a));
}

}  // namespace moba::skills
