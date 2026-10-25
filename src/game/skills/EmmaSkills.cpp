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

/// スキルが不発だった時（R の対象が消えていた時など）に、クールダウンを 0 に戻してログを出す
/// @param reason ログに出す不発の理由
void cancelAndRefund(SkillContext& ctx, std::string_view reason) {
    ctx.caster.cooldowns[static_cast<std::size_t>(index(ctx.def.slot))] = 0.0f;
    log::info("skill", "[{}] {} は不発（{}）。クールダウンを戻します", toString(ctx.def.slot), ctx.def.name, reason);
}

}  // namespace

/// Q: カードを投げ、自分の位置に鳩を残す。
///    鳩がいれば（どれだけ離れていても）、その鳩もカードになって狙った地点へ飛ぶ。
///    エマ本体のカードが当たると、Q のクールダウンが短くなる。両方のカードが同じ敵に当たると追加効果。
/// params: speed, width, visual, color, cooldownRefundOnHit（エマのカード）,
///         doveLifetime, doveCardSpeed, doveCardRange, doveCardColor（鳩のカード）, doubleHitEffects,
///         doveCardDrawOffsetForward, doveCardDrawOffsetSide（溜めの間に R で鳩と入れ替わった時の、鳩のカードの見た目のずれ）
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
    const float refund = def.numOr("cooldownRefundOnHit", 0.0f);
    const SkillSlot slot = def.slot;
    card.onHit = [recordHit = makeOnHit(1), refund, slot](World& w, Projectile& self, Unit& unit) {
        recordHit(w, self, unit);
        if (Unit* owner = w.findUnit(self.owner)) refundCooldown(w, *owner, slot, refund);
    };
    world.spawnProjectile(std::move(card));

    // 鳩は 1 羽まで。今いる鳩は、距離に関係なくカードになって狙った地点へ飛ぶ
    for (Prop& dove : world.props()) {
        if (!dove.alive || dove.owner != emma.id || dove.kind != PropKind::Dove) continue;
        dove.alive = false;
        // 溜めの間に R で鳩と入れ替わっていれば、鳩のカードは入れ替わる前の鳩の位置（＝今のエマの位置）から出る
        const Vector2 from = ctx.request.doveCardOrigin.value_or(dove.pos);
        // 鳩のカードの飛距離・弾速は、書かれていなければエマ本体のカード（range / speed）と同じにする
        Projectile doveCard =
            makeProjectile(ctx, from, geo::normalizedOr(target - from, dir), def.numOr("doveCardRange", def.range));
        doveCard.speed = def.numOr("doveCardSpeed", doveCard.speed);
        doveCard.color = def.color("doveCardColor", kDoveColor);
        doveCard.label = def.name + "（鳩のカード）";
        doveCard.onHit = makeOnHit(2);
        if (ctx.request.doveCardOrigin) {
            // エマのカードとぴったり重なって 1 枚に見えないよう、見た目だけずらす（当たり判定は重なったまま）
            doveCard.drawOffsetForward = def.numOr("doveCardDrawOffsetForward", 0.0f);
            doveCard.drawOffsetSide = def.numOr("doveCardDrawOffsetSide", 0.0f);
        }
        world.spawnProjectile(std::move(doveCard));
        log::debug("skill", "鳩 #{} が {} からカードになって {} へ飛びます", dove.id, geo::str(from), geo::str(target));
    }

    Prop newDove;
    newDove.owner = emma.id;
    newDove.kind = PropKind::Dove;
    newDove.pos = emma.pos;
    newDove.lifetime = newDove.remaining = def.num("doveLifetime", 6.0f);
    world.spawnProp(newDove);
}

/// W: 指定した地点が少し遅れて爆発し（ブランドの W のように予告円が出る）、その場に帽子が残る（R の対象）。
///    敵に当たると、W のクールダウンが短くなる。
/// params: delay（爆発までの時間）, radius, hatLifetime, color, onHit, cooldownRefundOnHit
void handleEmmaW(SkillContext& ctx) {
    AreaEffect a = makeGroundBlast(ctx, aimPoint(ctx));
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
    ctx.world.spawnArea(std::move(a));
}

/// R: 鳩・帽子・兎（変身させた敵）のいずれかを指定して、その位置へ瞬間移動し、対象ごとの追加効果を出す
///   鳩: エマと鳩の位置が入れ替わる。鳩はエマの元の位置へ飛び、通り道の敵に効果（拘束）。鳩は残り時間を戻して残る
///   帽子: エマと帽子の位置が入れ替わる。帽子はエマの元の位置で爆発し、範囲の敵に効果（打ち上げ）。帽子は残り時間を戻して残る
///   兎: 兎の位置へ移動し、兎の周りの敵に効果（変身）
/// R の前の右クリックの移動指示は捨てる（R の後は、新しく右クリックするまで移動先にとどまる）
/// params: pickRadius, targetKinds, dovePathWidth, doveSpeed, doveOnHit, hatFlightTime, hatRadius, hatOnHit,
///         rabbitRadius, rabbitOnHit, color
void handleEmmaR(SkillContext& ctx) {
    World& world = ctx.world;
    Unit& emma = ctx.caster;
    const SkillDef& def = ctx.def;
    const Vector2 oldPos = emma.pos;
    const Color color = def.color("color", Color{200, 170, 255, 255});

    switch (ctx.request.propKind) {
        case PropTargetKind::Dove: {
            Prop* dove = world.findProp(ctx.request.propId);
            if (!dove || !dove->alive) return cancelAndRefund(ctx, "鳩が消えていた");
            const Vector2 dovePos = dove->pos;
            teleport(world, emma, dovePos, color);
            // Q の溜めの間なら、この後に出る鳩のカードは入れ替わる前の鳩の位置から出す
            if (emma.cast.active) emma.cast.request.doveCardOrigin = dovePos;

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
                path.onHit = [effects](World& w, Projectile& self, Unit& unit) {
                    applyEffects(w, unit, effects, self.owner);
                };
                world.spawnProjectile(std::move(path));
            }
            // 鳩はエマの元の位置へ移り、残り時間を最初からにして残る（飛んでいる間は表示しない）
            dove->pos = oldPos;
            dove->hiddenTime = distance / speed;
            dove->remaining = dove->lifetime;
            break;
        }
        case PropTargetKind::Hat: {
            Prop* hat = world.findProp(ctx.request.propId);
            if (!hat || !hat->alive) return cancelAndRefund(ctx, "帽子が消えていた");
            const Vector2 hatPos = hat->pos;
            teleport(world, emma, hatPos, color);

            // 帽子はエマの元の位置へ移って爆発し、残り時間を最初からにして残る（飛んでいる間は表示しない）
            const float flightTime = def.numOr("hatFlightTime", 0.15f);
            hat->pos = oldPos;
            hat->hiddenTime = flightTime;
            hat->remaining = hat->lifetime;

            AreaEffect a = makeArea(ctx, oldPos);
            a.origin = hatPos;
            a.label = def.name + "（帽子）";
            a.delay = flightTime;
            a.radius = def.num("hatRadius", 250.0f);
            a.visual = AreaVisual::ThrownHat;
            const std::vector<EffectSpec> effects = def.effects("hatOnHit");
            a.onUnitHit = [effects](World& w, AreaEffect& self, Unit& unit) {
                applyEffects(w, unit, effects, self.owner);
            };
            world.spawnArea(std::move(a));
            break;
        }
        case PropTargetKind::Rabbit: {
            Unit* rabbit = world.findUnit(ctx.request.targetUnit);
            if (!rabbit || !rabbit->alive || !rabbit->status.has(EffectType::Polymorph)) {
                return cancelAndRefund(ctx, "兎の変身が解けていた");
            }
            // 兎のすぐ手前（エマ側）に移動する
            const Vector2 fromRabbit = geo::normalizedOr(oldPos - rabbit->pos, Vector2{-1.0f, 0.0f});
            teleport(world, emma, rabbit->pos + fromRabbit * (rabbit->radius + emma.radius) * 0.6f, color);

            AreaEffect a = makeArea(ctx, rabbit->pos);
            a.label = def.name + "（兎）";
            a.radius = def.num("rabbitRadius", 250.0f);
            a.visual = AreaVisual::Explosion;
            const std::vector<EffectSpec> effects = def.effects("rabbitOnHit");
            a.onUnitHit = [effects](World& w, AreaEffect& self, Unit& unit) {
                applyEffects(w, unit, effects, self.owner);
            };
            world.spawnArea(std::move(a));
            break;
        }
        case PropTargetKind::None: log::error("skill", "[R] 対象の種類が指定されていません"); return;
    }
    // R の前の移動指示は捨てる（R の後に右クリックしなければ、移動先にとどまる）
    emma.moveTarget.reset();
}

}  // namespace moba::skills
