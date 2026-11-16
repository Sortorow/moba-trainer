// 複数のキャラで使い回せる汎用のスキル

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {

/// params: speed, width, pierce, visual, color, onHit（当たった時の効果）, detonateMark（起爆する印の名前）
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

ParamRules projectileRules() {
    return joinRules(projectileBaseRules(), {
                                                optionalParam("onHit", ParamKind::Effects),
                                                optionalParam("detonateMark", ParamKind::Text),
                                            });
}

/// params: speed, width, visual, color, onHit, detonateMark
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

ParamRules homingProjectileRules() {
    // 追尾する弾は必ず当たって消えるので、pierce は使わない（書くと「使わない項目」の警告になる）
    return {
        requiredParam("speed", ParamKind::Positive),
        requiredParam("width", ParamKind::Positive),
        choiceParam("visual", ParamKind::Choice, projectileVisualNames()),
        optionalParam("color", ParamKind::Color),
        optionalParam("onHit", ParamKind::Effects),
        optionalParam("detonateMark", ParamKind::Text),
    };
}

/// カーソルが射程内ならその位置へ、射程外なら射程の端へ瞬間移動する（params は演出の color だけ）
void handleBlink(SkillContext& ctx) {
    teleport(ctx.world, ctx.caster, aimPoint(ctx), ctx.def.color("color", Color{255, 240, 160, 255}));
}

ParamRules blinkRules() { return {optionalParam("color", ParamKind::Color)}; }

/// params: flightTime（着地までの時間）, radius（鉄菱をまく円の半径）, duration（残る時間）, color,
///         pieceRadius（鉄菱 1 つの当たり判定の半径）, rings（鉄菱の並び）, onStep（踏むたびに付く効果）
void handleCaltrops(SkillContext& ctx) {
    AreaEffect a = makeArea(ctx, aimPoint(ctx));
    a.delay = ctx.def.num("flightTime", 0.35f);
    a.duration = ctx.def.num("duration", 6.0f);
    a.radius = ctx.def.num("radius", 220.0f);
    a.visual = AreaVisual::Caltrops;
    a.onActivate = [](World& world, AreaEffect& self) {
        world.addVfx(makeBurstVfx(self.team, self.center, self.radius, Color{235, 245, 255, 255}));  // 着地の白い光
    };

    // 鉄菱そのもの（踏むと発動する物）。踏んだ鉄菱を消すのと命中を数えるのは World（World::stepOnTriggers）
    StepTriggers caltrops;
    caltrops.radius = ctx.def.num("pieceRadius", 20.0f);
    for (const Vector2 offset : ringLayout(ctx.def.params.value("rings", nlohmann::json::array()), a.radius)) {
        caltrops.positions.push_back(a.center + offset);
    }
    const std::vector<EffectSpec> effects = ctx.def.effects("onStep");
    caltrops.onStep = [effects, owner = a.owner](World& world, Unit& unit, bool firstStep) {
        if (firstStep) {
            applyEffects(world, unit, effects, owner);  // 効果の名前の文字も出す
            return;
        }
        // 2 回目に踏んだ時からは、文字を出さずに付け直す（踏むたびに出すと文字が重なる）。同じ強さのスロウは 1 つにまとまり、
        // 残り時間が長い方になるので、最後に踏んだ時から効果の長さだけ続く
        for (const EffectSpec& effect : effects) unit.status.apply(effect, owner);
    };
    log::debug("skill", "[{}] {}: 鉄菱 {} 個を {} にまく", toString(ctx.def.slot), ctx.def.name,
               caltrops.positions.size(), geo::str(a.center));
    a.stepTriggers = std::move(caltrops);
    ctx.world.spawnArea(std::move(a));
}

ParamRules caltropsRules() {
    return {
        requiredParam("flightTime", ParamKind::NonNegative),  // 0 = 投げた瞬間に着地
        requiredParam("duration", ParamKind::Positive),
        requiredParam("radius", ParamKind::Positive),
        requiredParam("pieceRadius", ParamKind::Positive),
        requiredParam("rings", ParamKind::Rings),
        optionalParam("color", ParamKind::Color),
        optionalParam("onStep", ParamKind::Effects),
    };
}

/// params: delay（爆発までの時間）, radius, color, onHit, cooldownRefundOnHit
void handleGroundBlast(SkillContext& ctx) { ctx.world.spawnArea(makeGroundBlast(ctx, aimPoint(ctx))); }

ParamRules groundBlastRules() { return groundBlastBaseRules(); }

/// params: radius（対象の周りの範囲）, color, onHit
void handleTargetedBlast(SkillContext& ctx) {
    Unit* target = ctx.world.findUnit(ctx.request.targetUnit);
    if (!target || !target->alive) {
        ctx.caster.cooldowns[static_cast<std::size_t>(index(ctx.def.slot))] = 0.0f;
        log::debug("skill", "[{}] {}: 対象がいなくなったため不発", toString(ctx.def.slot), ctx.def.name);
        return;
    }
    AreaEffect a = makeArea(ctx, target->pos);
    a.radius = ctx.def.num("radius", 300.0f);
    a.visual = AreaVisual::Explosion;
    const std::vector<EffectSpec> effects = ctx.def.effects("onHit");
    a.onUnitHit = [effects](World& world, AreaEffect& self, Unit& unit) {
        applyEffects(world, unit, effects, self.owner);
    };
    ctx.world.addVfx(makeTeleportVfx(ctx.caster.team, ctx.caster.pos, target->pos, a.color));  // 爆風が飛んでいく演出
    ctx.world.spawnArea(std::move(a));
}

ParamRules targetedBlastRules() {
    return {
        requiredParam("radius", ParamKind::Positive),
        optionalParam("color", ParamKind::Color),
        optionalParam("onHit", ParamKind::Effects),
    };
}

}  // namespace moba::skills
