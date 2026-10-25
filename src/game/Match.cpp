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

    world_ = std::make_unique<World>(data_, kSandboxBounds);
    world_->setCooldownScale(Team::Player, config_.playerCooldownScale());
    world_->setCooldownScale(Team::Enemy, data_.ai().enemyCooldownScale);

    switch (config_.mode) {
        case GameMode::Sandbox: setupSandbox(*world_, dummies_, *character); break;
        case GameMode::SkillDodge: setupSkillDodge(*character); break;
    }
    log::info("match", "{}を開始: キャラ={} クールダウン={}（倍率 {:.2f}）{}", displayName(config_.mode), character->id,
              config_.cooldownMode == CooldownMode::None ? "なし" : "標準", config_.playerCooldownScale(),
              config_.mode == GameMode::SkillDodge
                  ? std::format(" 敵 {} 体 賢さ {} 制限時間 {:.0f} 秒 乱数の種 {}", config_.enemyCount, config_.aiLevel,
                                config_.timeLimitSeconds, config_.seed)
                  : std::string{});
}

void Match::setupSkillDodge(const CharacterDef& character) {
    // 自キャラはすべてのスキルを使える（エマの R のように、他のスキルで置いた物や敵に付けた状態へワープして回避に使うスキルがあるため）。
    // 攻撃スキルが敵に当たっても、敵には HP が無いのでダメージは無い（状態異常はかかる）
    Unit player = makePlayerUnit(character);
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
        enemy.pos = world_->clampToBounds(Vector2{std::cos(angle), std::sin(angle)} * distance, enemy.radius);
        enemy.facing = geo::normalizedOr(Vector2{} - enemy.pos, Vector2{1.0f, 0.0f});
        enemies_.addEnemy(world_->addUnit(enemy));
    }
}

void Match::update(float dt, const InputFrame& input) {
    if (!world_ || finished_) return;
    elapsed_ += dt;
    player_.update(*world_, input, dt);
    dummies_.update(*world_);
    enemies_.update(*world_, dt);
    world_->update(dt);

    if (config_.timed() && elapsed_ >= config_.timeLimitSeconds) {
        finished_ = true;
        const MatchResult r = result();
        log::info("match", "制限時間になりました: ドッジ率 {:.1f}%（{} / {}）被弾 {} 回 ダウン {} 回",
                  r.dodge.rate() * 100.0f, r.dodge.dodged, r.dodge.total, r.hitsTaken, r.deaths);
    }
}

float Match::remaining() const { return config_.timed() ? std::max(0.0f, config_.timeLimitSeconds - elapsed_) : 0.0f; }

MatchResult Match::result() const {
    MatchResult r;
    r.config = config_;
    r.playedSeconds = elapsed_;
    if (!world_) return r;
    if (const CharacterDef* def = data_.character(config_.characterId)) r.characterName = def->displayName;
    r.aiLevelName = enemies_.level().name;
    r.dodge = world_->dodge();
    r.hitsTaken = r.dodge.hit;
    if (const Unit* player = world_->player()) r.deaths = player->deaths;
    r.playerHits = world_->stats().total();
    return r;
}

}  // namespace moba
