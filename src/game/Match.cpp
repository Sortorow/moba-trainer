#include "game/Match.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <random>

#include "core/Geometry.h"
#include "core/Log.h"
#include "data/GameData.h"
#include "game/Sandbox.h"

namespace moba {
namespace {

constexpr int kPlayerMaxHp = 5;
constexpr float kEnemySpawnMin = 1000.0f;  // 敵が現れる距離（自キャラから）
constexpr float kEnemySpawnMax = 1250.0f;
constexpr Color kTargetColor{255, 170, 70, 255};  // スキルショットの的の色

}  // namespace

Match::Match(const GameData& data, MatchConfig config) : data_(data), config_(std::move(config)) {
    const CharacterDef* character = data_.character(config_.characterId);
    if (!character) {
        const std::vector<const CharacterDef*> list = data_.characterList();
        if (list.empty()) {
            log::error("match", "キャラクターの設定が 1 つもないため、開始できません");
            return;
        }
        log::warn("match", "キャラ \"{}\" が見つからないため {} を使います", config_.characterId, list.front()->id);
        character = list.front();
        config_.characterId = character->id;
    }
    if (config_.seed == 0) config_.seed = std::random_device{}() | 1u;
    config_.enemyCount = std::clamp(config_.enemyCount, 1, 5);
    config_.aiLevel = std::clamp(config_.aiLevel, 1, 5);
    config_.targetLevel = std::clamp(config_.targetLevel, 1, 5);

    // ステージ: 見つからなければ一覧の最初のステージ。ステージのファイルが 1 つも無ければ、障害物のない広場
    const StageDef* stage = data_.stage(config_.stageId);
    if (!stage) {
        const std::vector<const StageDef*> stages = data_.stageList();
        if (!stages.empty()) {
            if (!config_.stageId.empty())
                log::warn("match", "ステージ \"{}\" が見つからないため {} を使います", config_.stageId,
                          stages.front()->id);
            stage = stages.front();
        }
    }
    if (stage) {
        config_.stageId = stage->id;
        world_ = std::make_unique<World>(data_, Terrain(*stage));
    } else {
        log::warn("match", "ステージの設定が無いため、障害物のない広場で始めます");
        world_ = std::make_unique<World>(data_, kSandboxBounds);
    }
    world_->setCooldownScale(Team::Player, config_.playerCooldownScale());
    world_->setCooldownScale(Team::Enemy, data_.ai().enemyCooldownScale);

    switch (config_.mode) {
        case GameMode::Sandbox:
            setupSandbox(*world_, dummies_, *character, data_.sandbox().current());
            sandboxRevision_ = data_.sandboxRevision();
            break;
        case GameMode::SkillDodge: setupSkillDodge(*character); break;
        case GameMode::SkillShot: setupSkillShot(*character); break;
    }
    std::string modeSettings;
    if (config_.mode == GameMode::SkillDodge) {
        modeSettings = std::format(" 敵 {} 体 賢さ {} 制限時間 {:.0f} 秒 乱数の種 {}", config_.enemyCount,
                                   config_.aiLevel, config_.timeLimitSeconds, config_.seed);
    } else if (config_.mode == GameMode::SkillShot) {
        modeSettings = std::format(" 的の賢さ {} 制限時間 {:.0f} 秒 乱数の種 {}", config_.targetLevel,
                                   config_.timeLimitSeconds, config_.seed);
    }
    log::info("match", "{}を開始: キャラ={} ステージ={} クールダウン={}（倍率 {:.2f}）{}", displayName(config_.mode),
              character->id, config_.stageId.empty() ? "（なし）" : config_.stageId,
              config_.cooldownMode == CooldownMode::None ? "なし" : "標準", config_.playerCooldownScale(),
              modeSettings);
}

void Match::setupSkillDodge(const CharacterDef& character) {
    // 自キャラはすべてのスキルを使える（エマの R のように、他のスキルで置いた物や敵に付けた状態へワープして回避に使うスキルがあるため）。
    // 攻撃スキルが敵に当たっても、敵には HP が無いのでダメージは無い（状態異常はかかる）
    Unit player = makePlayerUnit(character);
    player.pos = world_->placeable(player.pos, player.radius);
    player.maxHp = player.hp = kPlayerMaxHp;
    world_->addUnit(player);

    enemies_.setup(data_.ai(), config_.aiLevel, config_.seed);
    const std::vector<const CharacterDef*> candidates = data_.enemyCandidates();
    if (candidates.empty()) {
        log::error("match", "敵として出られるキャラがいません（usedByEnemy のスキルを持つキャラがありません）");
        return;
    }

    // 自キャラを囲むように、均等な角度 + ばらつきで配置する
    std::mt19937 rng(config_.seed);
    std::uniform_real_distribution<float> unit01(0.0f, 1.0f);
    const float baseAngle = unit01(rng) * 2.0f * PI;
    for (int i = 0; i < config_.enemyCount; ++i) {
        const CharacterDef& def = *candidates[static_cast<std::size_t>(rng() % candidates.size())];
        const float angle = baseAngle + 2.0f * PI * static_cast<float>(i) / static_cast<float>(config_.enemyCount) +
                            (unit01(rng) - 0.5f) * 0.5f;
        const float distance = kEnemySpawnMin + (kEnemySpawnMax - kEnemySpawnMin) * unit01(rng);

        Unit enemy;
        enemy.team = Team::Enemy;
        enemy.name = std::format("敵{}: {}", i + 1, def.displayName);
        enemy.characterId = def.id;
        enemy.radius = def.hitboxRadius;
        enemy.baseMoveSpeed = def.moveSpeed;
        enemy.color = def.color;
        // 建物や木の中に決まった時は、近くの立てる位置へ
        enemy.pos = world_->placeable(Vector2{std::cos(angle), std::sin(angle)} * distance, enemy.radius);
        enemy.facing = geo::normalizedOr(Vector2{} - enemy.pos, Vector2{1.0f, 0.0f});
        enemies_.addEnemy(world_->addUnit(enemy));
    }
}

void Match::setupSkillShot(const CharacterDef& character) {
    // 自キャラはすべてのスキルを使える（ヒット率に数えるのは countsForHitRate のスキルだけ）。HP は使わない
    Unit player = makePlayerUnit(character);
    player.pos = world_->placeable(player.pos, player.radius);
    world_->addUnit(player);

    // 的は、自キャラから「保つ距離」の真ん中の、ランダムな方向に置く。大きさ・移動速度は ai.jsonc で決める
    const SkillShotConfig& shot = data_.ai().skillShot;
    target_.setup(shot, config_.targetLevel, config_.seed);

    Unit target;
    target.team = Team::Enemy;
    target.name = "的";
    target.radius = shot.targetRadius;
    target.baseMoveSpeed = shot.targetMoveSpeed;
    target.color = kTargetColor;
    target.pos = world_->placeable(target_.startPosition(player.pos), target.radius);
    target.facing = geo::normalizedOr(player.pos - target.pos, Vector2{1.0f, 0.0f});
    target_.setTarget(world_->addUnit(target));
}

void Match::update(float dt, const InputFrame& input) {
    if (!world_ || finished_) return;
    elapsed_ += dt;
    player_.update(*world_, input, dt);
    dummies_.update(*world_);
    enemies_.update(*world_, dt);
    target_.update(*world_, dt);
    world_->update(dt);

    if (config_.timed() && elapsed_ >= config_.timeLimitSeconds) {
        finished_ = true;
        const MatchResult r = result();
        if (config_.mode == GameMode::SkillShot) {
            log::info("match", "制限時間になりました: ヒット率 {:.1f}%（{} / {}）的が避けた回数 {}",
                      r.playerHits.rate() * 100.0f, r.playerHits.hits, r.playerHits.casts, target_.dodgeCount());
        } else {
            log::info("match", "制限時間になりました: ドッジ率 {:.1f}%（{} / {}）被弾 {} 回 ダウン {} 回",
                      r.dodge.rate() * 100.0f, r.dodge.dodged, r.dodge.total, r.hitsTaken, r.deaths);
        }
    }
}

void Match::refreshDummiesIfChanged() {
    if (!world_ || config_.mode != GameMode::Sandbox || sandboxRevision_ == data_.sandboxRevision()) return;
    placeDummies(*world_, dummies_, data_.sandbox().current());
    sandboxRevision_ = data_.sandboxRevision();
}

float Match::remaining() const { return config_.timed() ? std::max(0.0f, config_.timeLimitSeconds - elapsed_) : 0.0f; }

MatchResult Match::result() const {
    MatchResult r;
    r.config = config_;
    r.playedSeconds = elapsed_;
    if (!world_) return r;
    if (const CharacterDef* def = data_.character(config_.characterId)) r.characterName = def->displayName;
    if (const StageDef* stage = data_.stage(config_.stageId)) r.stageName = stage->displayName;
    r.aiLevelName = config_.mode == GameMode::SkillShot ? target_.level().name : enemies_.level().name;
    r.dodge = world_->dodge();
    r.hitsTaken = r.dodge.hit;
    if (const Unit* player = world_->player()) r.deaths = player->deaths;
    r.playerHits = world_->stats().settledTotal();
    for (const SkillSlot slot : kAllSkillSlots) {
        r.playerHitsBySlot[static_cast<std::size_t>(index(slot))] = world_->stats().settledSlot(slot);
    }
    return r;
}

}  // namespace moba
