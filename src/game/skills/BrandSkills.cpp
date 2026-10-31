// like brand 固有のスキル（Q は "projectile"、W は "ground_blast"、E は "targeted_blast"、D は "blink" を使う）

#include <limits>
#include <map>
#include <memory>

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "game/skills/Skill.h"

namespace moba::skills {
namespace {

/// 跳ね返る火炎弾の設定（1 回の発動で共有する）
struct BounceParams {
    EntityId caster = kInvalidId;
    Team team = Team::Player;
    int castId = 0;
    SkillSlot slot = SkillSlot::R;
    std::string label;
    float speed = 1000.0f;
    float radius = 30.0f;
    float bounceRange = 600.0f;  // 次の対象を探す距離
    float recentWindow = 1.5f;   // この秒数以内に当たった敵は、跳ね返り先として後回しにする
    int maxHitsPerTarget = 3;    // 同じ敵に当たる回数の上限
    Color color = WHITE;
    std::vector<EffectSpec> effects;
};

/// 跳ね返りの途中経過（1 回の発動で共有する）
struct BounceState {
    int bouncesLeft = 0;
    std::map<EntityId, int> hitCount;
    std::map<EntityId, float> lastHitTime;
};

/// 次の跳ね返り先を選ぶ（current から bounceRange 以内）。優先順位（数字が小さいほど優先。同じ順位なら近い方）:
///   1. まだ一度も当たっていない敵
///   2. 直近 recentWindow 秒以内に当たっていない敵
///   3. 自分
///   4. 当たった回数が上限に達していない敵（他に跳ね返り先が無い時の最後の手段）
/// @param current 今、火炎弾が当たった相手
/// @return 跳ね返り先の通し番号。どこにも跳ね返れなければ kInvalidId
EntityId chooseNextTarget(World& world, const BounceParams& params, const BounceState& state, const Unit& current) {
    const Unit* best = nullptr;
    int bestRank = std::numeric_limits<int>::max();
    float bestDistance = std::numeric_limits<float>::max();
    auto consider = [&](const Unit& unit, int rank) {
        const float distance = Vector2Distance(current.pos, unit.pos);
        if (distance > params.bounceRange) return;
        if (rank < bestRank || (rank == bestRank && distance < bestDistance)) {
            best = &unit;
            bestRank = rank;
            bestDistance = distance;
        }
    };

    for (const Unit& unit : world.units()) {
        if (!unit.alive || unit.team == params.team || unit.id == current.id) continue;
        const auto hits = state.hitCount.find(unit.id);
        const int hitCount = hits == state.hitCount.end() ? 0 : hits->second;
        if (hitCount >= params.maxHitsPerTarget) continue;
        if (hitCount == 0) {
            consider(unit, 1);
        } else if (world.time() - state.lastHitTime.at(unit.id) >= params.recentWindow) {
            consider(unit, 2);
        } else {
            consider(unit, 4);
        }
    }
    if (const Unit* caster = world.findUnit(params.caster); caster && caster->alive && caster->id != current.id) {
        consider(*caster, 3);
    }
    return best ? best->id : kInvalidId;
}

/// from から対象 targetId へ向かう火炎弾（追尾弾）を出す。当たったら、残りの跳ね返り回数があれば次の対象へまた出す
/// @param params 1 回の発動で共有する設定
/// @param state 1 回の発動で共有する途中経過（残りの跳ね返り回数・当たった回数）
void spawnBounce(World& world, std::shared_ptr<const BounceParams> params, std::shared_ptr<BounceState> state,
                 Vector2 from, EntityId targetId) {
    Projectile p;
    p.owner = params->caster;
    p.team = params->team;
    p.castId = params->castId;
    p.slot = params->slot;
    p.label = params->label;
    p.pos = from;
    p.speed = params->speed;
    p.radius = params->radius;
    p.maxDistance = 100000.0f;
    p.homingTarget = targetId;
    p.visual = ProjectileVisual::Orb;
    p.color = params->color;
    p.onHit = [params, state](World& w, Projectile&, Unit& hit) {
        if (hit.team != params->team) {
            ++state->hitCount[hit.id];
            state->lastHitTime[hit.id] = w.time();
            applyEffects(w, hit, params->effects, params->caster);
        }
        if (state->bouncesLeft <= 0) return;
        const EntityId next = chooseNextTarget(w, *params, *state, hit);
        if (next == kInvalidId) {
            log::debug("skill", "castId={} 跳ね返り先が無いため終了（残り {} 回）", params->castId, state->bouncesLeft);
            return;
        }
        --state->bouncesLeft;
        spawnBounce(w, params, state, hit.pos, next);
    };
    world.spawnProjectile(std::move(p));
}

}  // namespace

/// R: 対象の敵へ火炎弾を放つ。火炎弾は敵と自分の間を跳ね返る
/// params: speed, width, color, bounces（跳ね返る回数）, maxHitsPerTarget, bounceRange, recentWindow, onHit
void handleBrandR(SkillContext& ctx) {
    Unit* target = ctx.world.findUnit(ctx.request.targetUnit);
    if (!target || !target->alive) {
        ctx.caster.cooldowns[static_cast<std::size_t>(index(ctx.def.slot))] = 0.0f;
        log::debug("skill", "[{}] {}: 対象がいなくなったため不発", toString(ctx.def.slot), ctx.def.name);
        return;
    }

    auto params = std::make_shared<BounceParams>();
    params->caster = ctx.caster.id;
    params->team = ctx.caster.team;
    params->castId = ctx.castId;
    params->slot = ctx.def.slot;
    params->label = ctx.def.name;
    params->speed = ctx.def.num("speed", 1000.0f);
    params->radius = ctx.def.num("width", 60.0f) * 0.5f;
    params->bounceRange = ctx.def.num("bounceRange", 600.0f);
    params->recentWindow = ctx.def.num("recentWindow", 1.5f);
    params->maxHitsPerTarget = static_cast<int>(ctx.def.num("maxHitsPerTarget", 3.0f));
    params->color = ctx.def.color("color", Color{255, 140, 60, 255});
    params->effects = ctx.def.effects("onHit");

    auto state = std::make_shared<BounceState>();
    state->bouncesLeft = static_cast<int>(ctx.def.num("bounces", 4.0f));
    spawnBounce(ctx.world, params, state, ctx.caster.pos, target->id);
}

ParamRules brandRRules() {
    return {
        requiredParam("speed", ParamKind::Positive),
        requiredParam("width", ParamKind::Positive),
        requiredParam("bounces", ParamKind::NonNegativeInteger),  // 0 = 跳ね返らない
        requiredParam("maxHitsPerTarget", ParamKind::PositiveInteger),
        requiredParam("bounceRange", ParamKind::Positive),
        requiredParam("recentWindow", ParamKind::NonNegative),  // 0 = 直前に当たった敵も後回しにしない
        optionalParam("color", ParamKind::Color),
        optionalParam("onHit", ParamKind::Effects),
    };
}

}  // namespace moba::skills
