// like emma 固有のスキル（E は汎用の "homing_projectile"、D は "ground_zone" を使う）

#include <map>
#include <memory>

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {
namespace {

constexpr Color kDoveColor{245, 245, 255, 255};

void refundCooldown(SkillContext& ctx, std::string_view reason) {
    ctx.caster.cooldowns[static_cast<std::size_t>(index(ctx.def.slot))] = 0.0f;
    log::info("skill", "[{}] {} は不発（{}）。クールダウンを戻します", toString(ctx.def.slot), ctx.def.name, reason);
}

}  // namespace

// Q: カードを投げ、自分の位置に鳩を残す。
//    近くに鳩がいれば、その鳩もカードになって狙った地点へ飛ぶ。両方のカードが同じ敵に当たると追加効果。
// params: speed, width, visual, color（エマのカード）,
//         doveLifetime, doveLinkRange, doveCardSpeed, doveCardRange, doveCardColor, doubleHitEffects
void handleEmmaQ(SkillContext& ctx) {
    World& world = ctx.world;
    Unit& emma = ctx.caster;
    const SkillDef& def = ctx.def;
    const Vector2 dir = aimDirection(ctx);
    const Vector2 target = aimPoint(ctx);  // 鳩のカードが向かう地点

    // 敵ごとに「どのカードが当たったか」を記録する（bit 1: エマのカード, bit 2: 鳩のカード）
    auto hitMasks = std::make_shared<std::map<EntityId, int>>();
    const std::vector<EffectSpec> doubleHitEffects = def.effects("doubleHitEffects");
    auto makeOnHit = [hitMasks, doubleHitEffects](int bit) {
        return [hitMasks, doubleHitEffects, bit](World& w, Projectile& self, Unit& unit) {
            int& mask = (*hitMasks)[unit.id];
            const bool wasBoth = mask == 3;
            mask |= bit;
            if (!wasBoth && mask == 3) {
                log::info("skill", "{} にカードが 2 枚とも命中", unit.name);
                applyEffects(w, unit, doubleHitEffects, self.owner);
            }
        };
    };

    Projectile card = makeProjectile(ctx, emma.pos, dir, def.range);
    card.onHit = makeOnHit(1);
    world.spawnProjectile(std::move(card));

    // 鳩は 1 羽まで。近くの鳩はカードになり、遠い鳩はそのまま消える
    const float linkRange = def.num("doveLinkRange", 800.0f);
    for (Prop& dove : world.props()) {
        if (!dove.alive || dove.owner != emma.id || dove.kind != PropKind::Dove || dove.hiddenTime > 0.0f) continue;
        dove.alive = false;
        const float distance = Vector2Distance(dove.pos, emma.pos);
        if (distance > linkRange) {
            log::debug("skill", "鳩 #{} はエマから {:.0f} 離れているため、カードにならずに消えます", dove.id, distance);
            continue;
        }
        Projectile doveCard = makeProjectile(ctx, dove.pos, geo::normalizedOr(target - dove.pos, dir),
                                             def.num("doveCardRange", 800.0f));
        doveCard.speed = def.num("doveCardSpeed", doveCard.speed);
        doveCard.color = def.color("doveCardColor", kDoveColor);
        doveCard.label = def.name + "（鳩のカード）";
        doveCard.onHit = makeOnHit(2);
        world.spawnProjectile(std::move(doveCard));
        log::debug("skill", "鳩 #{} がカードになって {} へ飛びます", dove.id, geo::str(target));
    }

    Prop newDove;
    newDove.owner = emma.id;
    newDove.kind = PropKind::Dove;
    newDove.pos = emma.pos;
    newDove.lifetime = newDove.remaining = def.num("doveLifetime", 6.0f);
    world.spawnProp(newDove);
}

// W: 帽子を地点に投げ、着地で爆発する。帽子はしばらくその場に残る（R の対象になる）
// params: flightTime, radius, hatLifetime, color, onHit
void handleEmmaW(SkillContext& ctx) {
    AreaEffect a = makeArea(ctx, aimPoint(ctx));
    a.delay = ctx.def.num("flightTime", 0.5f);
    a.radius = ctx.def.num("radius", 175.0f);
    a.visual = AreaVisual::ThrownHat;

    const float hatLifetime = ctx.def.num("hatLifetime", 6.0f);
    a.onActivate = [hatLifetime](World& world, AreaEffect& self) {
        // 帽子は 1 つまで。古い帽子は消える
        for (Prop& prop : world.props()) {
            if (prop.owner == self.owner && prop.kind == PropKind::Hat) prop.alive = false;
        }
        Prop hat;
        hat.owner = self.owner;
        hat.kind = PropKind::Hat;
        hat.pos = self.center;
        hat.lifetime = hat.remaining = hatLifetime;
        world.spawnProp(hat);
    };
    const std::vector<EffectSpec> effects = ctx.def.effects("onHit");
    a.onUnitHit = [effects](World& world, AreaEffect& self, Unit& unit) { applyEffects(world, unit, effects, self.owner); };
    ctx.world.spawnArea(std::move(a));
}

// R: 鳩・帽子・兎（変身させた敵）のいずれかを指定して、その位置へ瞬間移動し、対象ごとの追加効果を出す
//   鳩: 鳩がエマの元の位置へ飛び、通り道の敵に効果（拘束）
//   帽子: 帽子がエマの元の位置へ移って爆発し、範囲の敵に効果（打ち上げ）
//   兎: 兎の周りの敵に効果（変身）
// params: pickRadius, targetKinds, dovePathWidth, doveSpeed, doveOnHit, hatFlightTime, hatRadius, hatOnHit,
//         rabbitRadius, rabbitOnHit, color
void handleEmmaR(SkillContext& ctx) {
    World& world = ctx.world;
    Unit& emma = ctx.caster;
    const SkillDef& def = ctx.def;
    const Vector2 oldPos = emma.pos;
    const Color color = def.color("color", Color{200, 170, 255, 255});

    switch (ctx.request.propKind) {
        case PropTargetKind::Dove: {
            Prop* dove = world.findProp(ctx.request.propId);
            if (!dove || !dove->alive) return refundCooldown(ctx, "鳩が消えていた");
            const Vector2 dovePos = dove->pos;
            teleport(world, emma, dovePos, color);

            const float distance = Vector2Distance(dovePos, oldPos);
            const float speed = def.num("doveSpeed", 3000.0f);
            if (distance > 1.0f) {
                Projectile path;
                path.owner = emma.id;
                path.team = emma.team;
                path.castId = ctx.castId;
                path.slot = def.slot;
                path.label = def.name + "（鳩）";
                path.pos = dovePos;
                path.dir = (oldPos - dovePos) / distance;
                path.speed = speed;
                path.radius = def.num("dovePathWidth", 120.0f) * 0.5f;
                path.maxDistance = distance;
                path.pierce = true;
                path.visual = ProjectileVisual::Dove;
                path.color = kDoveColor;
                const std::vector<EffectSpec> effects = def.effects("doveOnHit");
                path.onHit = [effects](World& w, Projectile& self, Unit& unit) { applyEffects(w, unit, effects, self.owner); };
                world.spawnProjectile(std::move(path));
            }
            // 鳩はエマの元の位置へ移る。飛んでいる間は表示しない
            dove->pos = oldPos;
            dove->hiddenTime = distance / speed;
            break;
        }
        case PropTargetKind::Hat: {
            Prop* hat = world.findProp(ctx.request.propId);
            if (!hat || !hat->alive) return refundCooldown(ctx, "帽子が消えていた");
            const Vector2 hatPos = hat->pos;
            hat->alive = false;
            teleport(world, emma, hatPos, color);

            AreaEffect a = makeArea(ctx, oldPos);
            a.origin = hatPos;
            a.label = def.name + "（帽子）";
            a.delay = def.numOr("hatFlightTime", 0.15f);
            a.radius = def.num("hatRadius", 250.0f);
            a.visual = AreaVisual::ThrownHat;
            const std::vector<EffectSpec> effects = def.effects("hatOnHit");
            a.onUnitHit = [effects](World& w, AreaEffect& self, Unit& unit) { applyEffects(w, unit, effects, self.owner); };
            world.spawnArea(std::move(a));
            break;
        }
        case PropTargetKind::Rabbit: {
            Unit* rabbit = world.findUnit(ctx.request.targetUnit);
            if (!rabbit || !rabbit->alive || !rabbit->status.has(EffectType::Polymorph)) {
                return refundCooldown(ctx, "兎の変身が解けていた");
            }
            // 兎のすぐ手前（エマ側）に移動する
            const Vector2 fromRabbit = geo::normalizedOr(oldPos - rabbit->pos, Vector2{-1.0f, 0.0f});
            teleport(world, emma, rabbit->pos + fromRabbit * (rabbit->radius + emma.radius) * 0.6f, color);

            AreaEffect a = makeArea(ctx, rabbit->pos);
            a.label = def.name + "（兎）";
            a.radius = def.num("rabbitRadius", 250.0f);
            a.visual = AreaVisual::Explosion;
            const std::vector<EffectSpec> effects = def.effects("rabbitOnHit");
            a.onUnitHit = [effects](World& w, AreaEffect& self, Unit& unit) { applyEffects(w, unit, effects, self.owner); };
            world.spawnArea(std::move(a));
            break;
        }
        case PropTargetKind::None:
            log::error("skill", "[R] 対象の種類が指定されていません");
            break;
    }
}

}  // namespace moba::skills
