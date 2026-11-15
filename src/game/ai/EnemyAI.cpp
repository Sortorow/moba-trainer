#include "game/ai/EnemyAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/CharacterDef.h"
#include "data/Settings.h"
#include "game/SkillCasting.h"
#include "game/World.h"

namespace moba::ai {
namespace {

constexpr float kStrafeDistance = 300.0f;       // 横移動の幅（strafe = 1 の時）
constexpr float kMinEngageRange = 350.0f;       // 自キャラに近づく距離の下限
constexpr float kFlankAngle = 70.0f * DEG2RAD;  // 他の敵とこの角度より近いと、離れようとする
constexpr float kMaxFlankShift = 35.0f * DEG2RAD;
constexpr float kChaseArrived = 30.0f;       // 追いかける先にここまで近づいたら、着いたとみなす
constexpr float kChaseTimeLimit = 8.0f;      // 追跡がこの秒数（先を読む時間は別）を超えたら、着かなくても探索に移る
constexpr float kMarkSeenInterval = 0.1f;    // 探索の記録（見た点）を更新する間隔（秒）
constexpr float kProgressDistance = 100.0f;  // 探索中の敵がこの距離より動いていれば、探索が進んでいるとみなす
constexpr float kBushInset = 40.0f;          // ブッシュに留まる敵は、ブッシュの縁からこれだけ内側に立つ

/// 角度の差を -π〜π に収める
float wrapAngle(float angle) {
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

/// 角度（ラジアン。+X 方向が 0）の向きの、長さ 1 のベクトル
Vector2 fromAngle(float angle) { return Vector2{std::cos(angle), std::sin(angle)}; }

/// group 番のブッシュ（重なり合うブッシュはまとめて 1 つ）の中で、point に最も近い位置。
/// 縁から kBushInset だけ内側にする（小さいブッシュなら、大きさの半分まで）
Vector2 nearestPointInBush(const Terrain& terrain, int group, Vector2 point) {
    Vector2 best = point;
    float bestDistance = std::numeric_limits<float>::max();
    const std::vector<Shape>& bushes = terrain.bushes();
    for (std::size_t i = 0; i < bushes.size(); ++i) {
        if (terrain.bushGroup(i) != group) continue;
        const Shape& bush = bushes[i];
        Vector2 inside = point;
        if (bush.kind == Shape::Kind::Circle) {
            const float reach = bush.radius - std::min(kBushInset, bush.radius * 0.5f);
            const Vector2 offset = point - bush.center;
            const float length = Vector2Length(offset);
            if (length > reach) inside = bush.center + offset * (reach / length);
        } else {
            const Rectangle& r = bush.rect;
            const float insetX = std::min(kBushInset, r.width * 0.5f);
            const float insetY = std::min(kBushInset, r.height * 0.5f);
            inside = Vector2{std::clamp(point.x, r.x + insetX, r.x + r.width - insetX),
                             std::clamp(point.y, r.y + insetY, r.y + r.height - insetY)};
        }
        const float distance = Vector2DistanceSqr(point, inside);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = inside;
        }
    }
    return best;
}

}  // namespace

void EnemyDirector::setup(const AiConfig& config, int aiLevel, std::uint32_t seed) {
    config_ = config;
    level_ = config.level(aiLevel);
    rng_.seed(seed);
    observer_.clear();
    brains_.clear();
    mode_ = Mode::Fight;
    clue_.reset();
    search_ = SearchMap{};
    markSeenTimer_ = 0.0f;
    searchRound_ = 0;
    log::info("ai",
              "敵 AI: 賢さ {}（{}）反応 {:.2f}s 間隔 {:.1f}〜{:.1f}s ぶれ {:.0f} 予測(留まる {:.2f} / 移動継続 {:.2f} "
              "/ 回避 {:.2f}) 追跡の読み {:.1f}s",
              aiLevel, level_.name, level_.reactionTime, level_.castIntervalMin, level_.castIntervalMax,
              level_.aimError, level_.predictStay, level_.predictContinue, level_.predictDodge, level_.chasePredict);
}

void EnemyDirector::addEnemy(EntityId unitId) {
    Brain brain;
    brain.unitId = unitId;
    brain.nextCastTime = config_.firstCastDelay + uniform(0.0f, 1.0f);
    brain.strafeSign = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
    brains_.push_back(brain);
}

float EnemyDirector::uniform(float min, float max) {
    if (max <= min) return min;
    return std::uniform_real_distribution<float>(min, max)(rng_);
}

void EnemyDirector::update(World& world, float dt) {
    const Unit* player = world.player();
    if (!player) return;
    const float now = world.time();
    // 自キャラが視界の外にいる間は、位置を記録しない（見失った時は、追跡・探索で探す）。
    // ただしプレイの始め（最初の update）だけは、見えていなくても開始位置を記録する（敵は自キャラの開始位置だけを
    // 知っている）。始めに見えていなければ、すぐに開始位置で見失ったことになり、そこへ追いかけてから探す。
    // この後は、見えていない自キャラの本当の位置を使わない
    const bool playerVisible = world.canSee(Team::Enemy, *player);
    if (!playerVisible && !observer_.hasSamples()) {
        log::debug("ai", "敵は自キャラの開始位置 {} だけを知っています（見えていないので、追いかけて探します）",
                   geo::str(player->pos));
    }
    if (playerVisible || !observer_.hasSamples()) observer_.record(now, player->pos);
    observer_.update(now);
    updateMode(world, *player, playerVisible, dt);

    const Vector2 seenPlayer = seenPlayerPosition(now);
    for (Brain& brain : brains_) {
        Unit* enemy = world.findUnit(brain.unitId);
        if (!enemy || !enemy->alive) continue;
        brain.replanTimer -= dt;
        brain.strafeTimer -= dt;
        if (brain.replanTimer <= 0.0f) {
            switch (mode_) {
                case Mode::Fight: planMovement(world, brain, *enemy, seenPlayer); break;
                case Mode::Chase: planChase(world, *enemy, now); break;
                case Mode::Search: planSearch(world, brain, *enemy); break;
            }
            // 確かめ撃ちの先を選ぶ計算は重いので、移動先を決め直す時だけ行う
            if (mode_ != Mode::Fight && level_.probe) tryProbe(world, brain, *enemy);
            brain.replanTimer = uniform(0.2f, 0.35f);
        }
        if (playerVisible) tryCast(world, brain, *enemy, *player);  // 見えない自キャラには撃たない
    }
}

// スキルの射程に応じて、自キャラと保つ距離を決める（見えない自キャラには撃たないので、視界の内側に収める）
float EnemyDirector::engageRange(const World& world, const Unit& enemy) const {
    const CharacterDef* def = world.characterOf(enemy);
    float shortest = std::numeric_limits<float>::max();
    if (def) {
        for (const auto& skill : def->skills) {
            if (skill && skill->usedByEnemy) shortest = std::min(shortest, skill->range);
        }
    }
    if (shortest == std::numeric_limits<float>::max()) shortest = 800.0f;
    const float maxRange = std::max(
        kMinEngageRange, std::min(config_.maxEngageRange, world.settings().visionRadius - config_.visionMargin));
    return std::clamp(shortest * level_.engageRangeRatio, kMinEngageRange, maxRange);
}

// 他の敵と同じ方向から攻めないように、自キャラから見た角度をずらす
Vector2 EnemyDirector::spreadDirection(World& world, const Brain& brain, Vector2 seenPlayer, Vector2 away) const {
    const float angle = std::atan2(away.y, away.x);
    float shift = 0.0f;
    for (const Brain& other : brains_) {
        if (other.unitId == brain.unitId) continue;
        const Unit* unit = world.findUnit(other.unitId);
        if (!unit || !unit->alive) continue;
        const Vector2 dir = unit->pos - seenPlayer;
        const float diff = wrapAngle(angle - std::atan2(dir.y, dir.x));
        if (std::fabs(diff) < kFlankAngle)
            shift += (diff >= 0.0f ? 1.0f : -1.0f) * (kFlankAngle - std::fabs(diff)) * 0.5f;
    }
    return fromAngle(angle + std::clamp(shift, -kMaxFlankShift, kMaxFlankShift));
}

void EnemyDirector::planMovement(World& world, Brain& brain, Unit& enemy, Vector2 seenPlayer) {
    const float range = engageRange(world, enemy);
    Vector2 away = geo::normalizedOr(enemy.pos - seenPlayer, fromAngle(uniform(-PI, PI)));
    if (level_.flank) away = spreadDirection(world, brain, seenPlayer, away);

    Vector2 desired = seenPlayer + away * range;
    if (level_.strafe > 0.0f) {
        if (brain.strafeTimer <= 0.0f) {
            brain.strafeSign = uniform(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
            brain.strafeTimer = uniform(0.6f, 1.6f);
        }
        desired = desired + geo::perpendicular(away) * (brain.strafeSign * kStrafeDistance * level_.strafe);
    }
    // 自キャラがブッシュの中にいる時は、自キャラに最も近い敵だけが、そのブッシュの中に留まって陣営の視界を保つ
    // （外へ間合いを取ると、見つける・見失うを繰り返すため。ほかの敵は、共有の視界でいつもの間合いから撃てる）
    const int bush = world.terrain().bushAt(seenPlayer);
    if (bush >= 0 && nearestToPlayer(world, brain, seenPlayer))
        desired = nearestPointInBush(world.terrain(), bush, desired);
    enemy.moveTarget = world.clampToBounds(desired, enemy.radius);
}

bool EnemyDirector::nearestToPlayer(World& world, const Brain& brain, Vector2 seenPlayer) const {
    const Unit* self = world.findUnit(brain.unitId);
    if (!self) return false;
    const float distance = Vector2DistanceSqr(self->pos, seenPlayer);
    for (const Brain& other : brains_) {
        if (other.unitId == brain.unitId) continue;
        const Unit* unit = world.findUnit(other.unitId);
        if (unit && unit->alive && Vector2DistanceSqr(unit->pos, seenPlayer) < distance) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 見失った時（追跡・探索・確かめ撃ち）
// ---------------------------------------------------------------------------
void EnemyDirector::updateMode(World& world, const Unit& player, bool playerVisible, float dt) {
    // 誰か 1 体でも見つけたら、全員が元の動きに戻る
    if (playerVisible) {
        if (mode_ != Mode::Fight) log::debug("ai", "敵が自キャラを見つけました: {}", geo::str(player.pos));
        mode_ = Mode::Fight;
        clue_.reset();
        return;
    }
    // 見えていなければ、見失ったことにする（プレイの始めに見えていない時も、update が開始位置を記録しているので同じ）
    const float now = world.time();
    if (mode_ == Mode::Fight) startChase(world);

    // 見えない自キャラに敵のスキルが当たった: そこにいると分かるので、当たった位置へ追いかける
    for (const HitRecord& hit : world.recentHits()) {
        if (hit.attacker != Team::Enemy || hit.target != player.id) continue;
        clue_ = hit.pos;
        lostAt_ = hit.pos;
        mode_ = Mode::Chase;
        chaseStartTime_ = now;
        log::debug("ai", "見えない自キャラにスキルが当たったので、当たった位置 {} へ向かいます", geo::str(hit.pos));
    }

    // 探索の記録（敵の陣営から見えた点）を更新する。追跡の間に見た所も、探索済みになる
    markSeenTimer_ -= dt;
    if (markSeenTimer_ <= 0.0f) {
        search_.markSeen(world);
        markSeenTimer_ = kMarkSeenInterval;
    }

    if (mode_ == Mode::Chase && chaseFinished(world, now)) {
        mode_ = Mode::Search;
        clue_.reset();
        log::debug("ai", "追いかけた先にいないので、探索します（探索済み {} / {} エリア）", search_.exploredCount(),
                   search_.areaCount());
    }
    if (mode_ == Mode::Search && search_.allExplored()) {
        search_.reset();
        search_.markSeen(world);  // 今見えている所は、探し直すまでもない
        ++searchRound_;
        for (Brain& brain : brains_) brain.searchArea = -1;
        log::debug("ai", "全エリアを探しても見つからないので、最初から探し直します（{} 周目）", searchRound_);
    }
}

void EnemyDirector::startChase(World& world) {
    if (!search_.built()) {
        // 確かめる点は、最も大きい敵が立てる所にだけ置く
        float radius = 0.0f;
        for (const Brain& brain : brains_) {
            if (const Unit* enemy = world.findUnit(brain.unitId)) radius = std::max(radius, enemy->radius);
        }
        search_.build(world.terrain(), config_.search, radius);
        log::debug("ai", "探索のエリア: {} 個（確かめる点 {} 個）", search_.areaCount(), search_.points().size());
    }
    search_.reset();
    searchRound_ = 1;
    mode_ = Mode::Chase;
    chaseStartTime_ = world.time();
    clue_.reset();
    lostAt_ = observer_.lastSeenPosition();
    markSeenTimer_ = 0.0f;
    for (Brain& brain : brains_) brain.searchArea = -1;
    log::debug("ai", "敵が自キャラを見失いました: 最後に見えた位置 {}", geo::str(lostAt_));
}

bool EnemyDirector::chaseFinished(World& world, float now) const {
    const float predict = clue_ ? 0.0f : level_.chasePredict;
    if (now - chaseStartTime_ > predict + kChaseTimeLimit) {
        log::debug("ai", "追いかける先にたどり着けないので、追跡をやめます");
        return true;
    }
    // 消えた方向の先を読んでいる間（chasePredict）は、追いかける先が動くので、まだ着いたことにしない
    if (!clue_ && now - observer_.lastSeenTime() < level_.chasePredict) return false;
    for (const Brain& brain : brains_) {
        const Unit* enemy = world.findUnit(brain.unitId);
        if (enemy && enemy->alive && Vector2Distance(enemy->pos, chaseGoal(world, *enemy, now)) <= kChaseArrived)
            return true;
    }
    return false;
}

void EnemyDirector::planChase(World& world, Unit& enemy, float now) {
    // 壁や建物の向こうなら、World が回り込む道を探す
    enemy.moveTarget = chaseGoal(world, enemy, now);
}

Vector2 EnemyDirector::chaseGoal(const World& world, const Unit& enemy, float now) const {
    const Vector2 point = clue_ ? *clue_ : chasePoint(now);
    // 木や建物の中なら、近くの立てる位置にする（着いたかを判定できるように）
    return world.placeable(world.clampToBounds(point, enemy.radius), enemy.radius);
}

Vector2 EnemyDirector::chasePoint(float now) const {
    const float lostAt = observer_.lastSeenTime();
    const float ahead = std::min(now - lostAt, level_.chasePredict);
    return observer_.lastSeenPosition() + observer_.velocityAt(lostAt) * ahead;
}

void EnemyDirector::planSearch(World& world, Brain& brain, Unit& enemy) {
    const float now = world.time();
    if (brain.searchArea >= 0 && !search_.explored(brain.searchArea)) {
        // 動いているか、エリアのまだ見ていない点が減っていれば、探索は進んでいる。
        // 進まないまま giveUpTime たったら、たどり着けないエリアとしてあきらめる
        const int unseen = search_.unseenCount(brain.searchArea);
        if (Vector2Distance(enemy.pos, brain.progressPos) > kProgressDistance || unseen < brain.progressUnseen) {
            brain.progressPos = enemy.pos;
            brain.progressUnseen = unseen;
            brain.progressTime = now;
        } else if (now - brain.progressTime > config_.search.giveUpTime) {
            log::debug("ai", "{} は {} にたどり着けないので、探索済みにします", enemy.name,
                       search_.areaName(brain.searchArea));
            search_.giveUp(brain.searchArea);
        }
    }
    if (brain.searchArea < 0 || search_.explored(brain.searchArea)) {
        // 他の敵が向かっているエリアは、なるべく選ばない（手分けして探す）
        std::vector<int> avoid;
        for (const Brain& other : brains_) {
            if (&other == &brain || other.searchArea < 0) continue;
            const Unit* unit = world.findUnit(other.unitId);
            if (unit && unit->alive) avoid.push_back(other.searchArea);
        }
        brain.searchArea = search_.chooseArea(enemy.pos, lostAt_, avoid);
        if (brain.searchArea < 0) {
            enemy.moveTarget.reset();  // 全エリアを探し終えた（次の刻みで、最初から探し直す）
            return;
        }
        brain.progressPos = enemy.pos;
        brain.progressUnseen = search_.unseenCount(brain.searchArea);
        brain.progressTime = now;
        log::debug("ai", "{} は {} を探しに行きます（探索済み {} / {} エリア）", enemy.name,
                   search_.areaName(brain.searchArea), search_.exploredCount(), search_.areaCount());
    }
    // 壁の向こうなら、World が回り込む道を探す
    enemy.moveTarget = search_.nearestUnseenPoint(brain.searchArea, enemy.pos);
}

void EnemyDirector::tryProbe(World& world, Brain& brain, Unit& enemy) {
    if (enemy.cast.active || !enemy.status.canCast()) return;
    const float now = world.time();
    if (now < brain.nextCastTime) return;
    const CharacterDef* def = world.characterOf(enemy);
    if (!def) return;

    // 方向指定で、当たると消える弾のスキル（途中で消えたかどうかで、いたかが分かる）のうち、優先度が最も高いもの
    const SkillDef* probeSkill = nullptr;
    for (const auto& skill : def->skills) {
        if (!skill || !skill->usedByEnemy || skill->targeting != Targeting::Direction) continue;
        if (skill->flagOr("pierce", false) || !aimModelFor(*skill)) continue;
        if (enemy.cooldowns[static_cast<std::size_t>(index(skill->slot))] > 0.0f) continue;
        if (!probeSkill || skill->aiPriority > probeSkill->aiPriority) probeSkill = &*skill;
    }
    if (!probeSkill) return;
    const std::optional<SearchMap::ProbeTarget> target =
        search_.chooseProbe(world, enemy.pos, probeSkill->range, lostAt_, rng_);
    if (!target) return;

    CastRequest request{probeSkill->slot, target->point};
    request.probe = true;  // 自キャラを狙った撃ち方ではないので、外れたらドッジ率に数えない（当たったら被弾）
    if (casting::requestCast(world, enemy, request) == casting::CastOutcome::Rejected) return;
    search_.markProbed(target->area);
    brain.lastCastTime = now;
    brain.nextCastTime = now + uniform(level_.castIntervalMin, level_.castIntervalMax);
    log::debug("ai", "{} が {} へ [{}] {} で確かめ撃ち（{} を狙う）", enemy.name, search_.areaName(target->area),
               toString(probeSkill->slot), probeSkill->name, geo::str(target->point));
}

Vector2 EnemyDirector::seenPlayerPosition(float now) const {
    // update の始めに必ず記録がある（最初の update で開始位置を記録する）ので、記録が無いことはない
    return observer_.positionAt(now - level_.reactionTime);
}

AimWeights EnemyDirector::aimWeights(bool playerStuck) const {
    if (playerStuck) return AimWeights{1.0f, 0.0f, 0.0f, 0.0f};  // 動けない相手は、今の位置を狙えば当たる
    AimWeights w;
    w.stay = level_.predictStay;
    w.keepMoving = level_.predictContinue;
    const float right = level_.adaptive ? observer_.rightBias() : 0.5f;
    w.dodgeRight = level_.predictDodge * right;
    w.dodgeLeft = level_.predictDodge * (1.0f - right);
    return w;
}

void EnemyDirector::tryCast(World& world, Brain& brain, Unit& enemy, const Unit& player) {
    if (enemy.cast.active || !enemy.status.canCast()) return;
    const float now = world.time();
    const bool playerStuck = !player.status.canMove();
    const bool punish = level_.punishCC && playerStuck;
    if (!punish && now < brain.nextCastTime) return;
    // 動けない相手にも、最低限の間隔は空ける（クールダウンの無いスキルで撃ち続けないように）
    if (punish && now < brain.lastCastTime + config_.punishMinInterval) return;

    const CharacterDef* def = world.characterOf(enemy);
    if (!def) return;

    // AI が認識している自キャラ（反応の遅れの分だけ前の位置・速度）
    AimTarget target;
    target.pos = seenPlayerPosition(now);
    target.velocity = playerStuck ? Vector2{} : observer_.velocityAt(now - level_.reactionTime);
    target.moveSpeed = player.moveSpeed();
    target.radius = player.radius;
    const AimWeights weights = aimWeights(playerStuck);

    // 使えるスキルの中から、命中見込み × 優先度 が最も高いものを選ぶ
    const SkillDef* bestSkill = nullptr;
    std::optional<AimResult> bestAim;
    std::optional<AimModel> bestModel;
    float bestScore = -1.0f;
    for (const auto& skill : def->skills) {
        if (!skill || !skill->usedByEnemy) continue;
        if (enemy.cooldowns[static_cast<std::size_t>(index(skill->slot))] > 0.0f) continue;
        const std::optional<AimModel> model = aimModelFor(*skill);
        if (!model) continue;
        const std::optional<AimResult> aim = chooseAim(enemy.pos, *model, target, weights, config_.playerReactionTime);
        if (!aim) continue;
        const float score = aim->expectedHit * skill->aiPriority;
        if (score > bestScore) {
            bestScore = score;
            bestSkill = &*skill;
            bestAim = aim;
            bestModel = model;
        }
    }
    if (!bestSkill) return;  // 射程外か、すべてクールダウン中

    // 命中見込みが低い時は少し待つ（ただし待ちすぎない）
    if (!punish && bestAim->expectedHit < level_.minHitChance && now < brain.nextCastTime + level_.castIntervalMax)
        return;

    // 狙いのぶれ。直線の弾は横方向にだけぶらす
    Vector2 aimPoint = bestAim->aimPoint;
    const float error = uniform(0.0f, level_.aimError);
    if (bestModel->shape == AimShape::Line) {
        const Vector2 dir = geo::normalizedOr(aimPoint - enemy.pos, enemy.facing);
        aimPoint = aimPoint + geo::perpendicular(dir) * (uniform(0.0f, 1.0f) < 0.5f ? -error : error);
    } else {
        aimPoint = aimPoint + fromAngle(uniform(-PI, PI)) * error;
    }

    // 使えなかった時だけ、次の刻みで撃ち直す（詠唱時間 0・クールダウン 0 のスキルも、撃てたら間隔を空ける）
    const casting::CastOutcome outcome = casting::requestCast(world, enemy, CastRequest{bestSkill->slot, aimPoint});
    if (outcome == casting::CastOutcome::Rejected) return;

    brain.lastCastTime = now;
    brain.nextCastTime = now + uniform(level_.castIntervalMin, level_.castIntervalMax);
    // 避けた方向を学習する（撃つのは見えている時だけなので、撃った時の位置は分かっている）
    if (bestModel->shape == AimShape::Line) {
        observer_.noteShot(now, player.pos, geo::normalizedOr(aimPoint - enemy.pos, enemy.facing));
    }
    log::debug("ai", "{} が [{}] {} を {} へ（命中見込み {:.0f}%{}）", enemy.name, toString(bestSkill->slot),
               bestSkill->name, geo::str(aimPoint), bestAim->expectedHit * 100.0f,
               punish ? "・動けない相手を狙う" : "");
}

}  // namespace moba::ai
