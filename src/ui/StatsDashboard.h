#pragma once
// 統計の画面（ダッシュボード）。保存した結果を絞り込み、成長が分かるように数字とグラフで見せる
//
//   上段: 絞り込み（キャラ・ステージ・賢さ・敵の数 / 的の大きさ・速さ・期間）
//   数字: プレイ回数・平均・自己ベスト・直近 7 日とその前の 7 日の比較
//   グラフ: 成績の推移（プレイごと・直近 10 回の平均・自己ベストの更新）、日ごとのプレイ回数、
//           賢さごとの成績、キーごとのヒット率（スキルショット）
//   下段: 自己ベスト TOP 3（ハイライト）と、絞り込んだ結果の表
// 計算は game/ResultStats.h、グラフは ImPlot で描く。

#include <optional>

#include "game/ResultStats.h"

namespace moba {

class GameData;

/// グラフ（ImPlot）の見た目を、ゲームの暗い画面に合わせる（ImPlot のコンテキストを作った後に 1 回呼ぶ）
void applyPlotStyle();

/// 統計の画面。モード（スキルドッジ・スキルショット）ごとに、絞り込みの条件を覚えておく
class StatsDashboard {
public:
    /// 統計の画面を描く
    /// @return 「閉じる」が押されたら true
    bool draw(const ResultStore& store, const GameData& data);
    /// 次に開いた時に表示するモードのタブを選ぶ
    void selectMode(GameMode mode) { pendingMode_ = mode; }

private:
    /// 1 つのモードのダッシュボード（絞り込み・数字・グラフ・ハイライト・表）を描く
    void drawMode(StatsFilter& filter, const ResultStore& store, const GameData& data);

    StatsFilter dodgeFilter_{GameMode::SkillDodge};
    StatsFilter shotFilter_{GameMode::SkillShot};
    std::optional<GameMode> pendingMode_;  // 次に描く時に選ぶタブ
};

}  // namespace moba
