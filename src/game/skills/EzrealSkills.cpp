// like ezreal 固有のスキル（Q / W / R / D は汎用の "projectile" と "blink" を使う）

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {

// E: ワープした後、近くの敵に追尾弾を撃つ
// params: boltEnabled, boltSearchRange, boltSpeed, boltWidth, boltPreferMark（優先して狙う印）,
//         boltDetonateMark（追尾弾で起爆する印。空なら起爆しない）, color
void handleEzrealE(SkillContext& ctx) {
    handleBlink(ctx);
    if (!ctx.def.flagOr("boltEnabled", true)) return;

    Unit& caster = ctx.caster;
    const std::string preferMark = ctx.def.text("boltPreferMark", "");
    Unit* target = ctx.world.nearestEnemy(caster.pos, caster.team, ctx.def.num("boltSearchRange", 750.0f), preferMark);
    if (!target) {
        log::debug("skill", "[E] 追尾弾: 範囲内に敵がいないため撃たない");
        return;
    }

    Projectile bolt;
    bolt.owner = caster.id;
    bolt.team = caster.team;
    bolt.castId = ctx.castId;
    bolt.slot = ctx.def.slot;
    bolt.label = ctx.def.name + "（追尾弾）";
    bolt.pos = caster.pos;
    bolt.dir = geo::normalizedOr(target->pos - caster.pos, caster.facing);
    bolt.speed = ctx.def.num("boltSpeed", 2000.0f);
    bolt.radius = ctx.def.num("boltWidth", 60.0f) * 0.5f;
    bolt.maxDistance = 100000.0f;
    bolt.homingTarget = target->id;
    bolt.visual = ProjectileVisual::Bolt;
    bolt.color = ctx.def.color("color", Color{255, 220, 120, 255});

    const std::string detonate = ctx.def.text("boltDetonateMark", "");
    bolt.onHit = [detonate](World& world, Projectile& self, Unit& hit) { detonateMark(world, hit, detonate, self.color); };
    ctx.world.spawnProjectile(std::move(bolt));
}

}  // namespace moba::skills
