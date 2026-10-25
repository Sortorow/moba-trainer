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

/// 1 回のプレイの結果（リザルト画面に出す内容）
struct MatchResult {
    MatchConfig config;
    std::string characterName;
    std::string stageName;
    std::string aiLevelName;
    float playedSeconds = 0.0f;
    DodgeCount dodge;     // スキルドッジ: 敵のスキルを避けた割合
    int hitsTaken = 0;    // スキルドッジ: 被弾した回数
    int deaths = 0;       // スキルドッジ: HP が 0 になった回数
    HitCount playerHits;  // 自キャラのスキルの命中
};

/// 1 回のプレイ（練習場・スキルドッジ）。World と、自キャラ・ダミー・敵 AI の操作、制限時間、結果をまとめて持つ
class Match {
public:
    /// プレイを始める。ステージの地形で World を作り、モードに合わせて自キャラ・ダミー・敵を置く。
    /// config.characterId のキャラが見つからなければ最初のキャラを、ステージが見つからなければ最初のステージを使う
    Match(const GameData& data, MatchConfig config);

    /// 開始できたか（キャラの設定が 1 つも無い時は false）
    bool valid() const { return world_ != nullptr; }
    /// ゲームを dt 秒進める（自キャラ・ダミー・敵 AI・World の順）。制限時間になったら終了する。固定の時間刻みで呼ぶ
    void update(float dt, const InputFrame& input);

    /// 制限時間になって終わったか
    bool finished() const { return finished_; }
    /// 始めてからの経過時間（秒）
    float elapsed() const { return elapsed_; }
    /// 制限時間の残り（秒。制限なしなら 0）
    float remaining() const;
    /// 今の結果（ドッジ率・被弾・ダウン・命中など。リザルト画面に渡す）
    MatchResult result() const;

    /// 始めた時の設定（キャラやステージが見つからなかった場合は、代わりに使ったものに直してある）
    const MatchConfig& config() const { return config_; }
    /// ゲームの中身（キャラ・弾・地形など）
    World& world() { return *world_; }
    /// ゲームの中身（キャラ・弾・地形など）
    const World& world() const { return *world_; }
    /// 練習場のダミーの動き
    DummyController& dummies() { return dummies_; }
    /// スキルドッジの敵 AI
    const ai::EnemyDirector& enemies() const { return enemies_; }

    /// 練習場: ダミーの設定（プリセットの選択や sandbox.jsonc の中身）が変わっていれば、ダミーを置き直す
    void refreshDummiesIfChanged();

private:
    /// スキルドッジ: 自キャラ（HP 付き）と、自キャラを囲むように敵を置く
    void setupSkillDodge(const CharacterDef& character);

    const GameData& data_;
    MatchConfig config_;
    std::unique_ptr<World> world_;
    PlayerController player_;
    DummyController dummies_;
    ai::EnemyDirector enemies_;
    int sandboxRevision_ = 0;  // 今のダミーを置いた時の GameData::sandboxRevision()
    float elapsed_ = 0.0f;
    bool finished_ = false;
};

}  // namespace moba
