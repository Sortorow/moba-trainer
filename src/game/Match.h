#pragma once
// 1 回のプレイ（練習場・スキルドッジ）。
// World と、自キャラ・ダミー・敵 AI の操作、制限時間、結果をまとめて持つ

#include <memory>
#include <string>

#include "game/CastStats.h"
#include "game/DummyController.h"
#include "game/MatchConfig.h"
#include "game/PlayerController.h"
#include "game/World.h"
#include "game/ai/EnemyAI.h"
#include "input/InputFrame.h"

namespace moba {

class GameData;

struct MatchResult {
    MatchConfig config;
    std::string characterName;
    std::string aiLevelName;
    float playedSeconds = 0.0f;
    DodgeCount dodge;     // スキルドッジ: 敵のスキルを避けた割合
    int hitsTaken = 0;    // スキルドッジ: 被弾した回数
    int deaths = 0;       // スキルドッジ: HP が 0 になった回数
    HitCount playerHits;  // 自キャラのスキルの命中
};

class Match {
public:
    // config.characterId のキャラが見つからなければ、最初のキャラを使う
    Match(const GameData& data, MatchConfig config);

    bool valid() const { return world_ != nullptr; }
    void update(float dt, const InputFrame& input);  // 固定の時間刻みで呼ぶ

    bool finished() const { return finished_; }
    float elapsed() const { return elapsed_; }
    float remaining() const;  // 制限時間の残り（制限なしなら 0）
    MatchResult result() const;

    const MatchConfig& config() const { return config_; }
    World& world() { return *world_; }
    const World& world() const { return *world_; }
    DummyController& dummies() { return dummies_; }
    const ai::EnemyDirector& enemies() const { return enemies_; }

private:
    void setupSkillDodge(const CharacterDef& character);

    const GameData& data_;
    MatchConfig config_;
    std::unique_ptr<World> world_;
    PlayerController player_;
    DummyController dummies_;
    ai::EnemyDirector enemies_;
    float elapsed_ = 0.0f;
    bool finished_ = false;
};

}  // namespace moba
