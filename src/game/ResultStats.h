#pragma once
// 統計（手順 6）の計算。保存した結果（ResultStore）を条件で絞り込み、集計する。
// 画面に依存しない計算だけなので、テストで直接確かめられる。
//
// 成績（スコア）は、スキルドッジがドッジ率、スキルショットがヒット率（ResultStore.h の scoreOf）。
// 平均は、プレイごとの割合の平均ではなく、数を合計してから割る（総数の少ないプレイに引っぱられないように）。

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "game/ResultStore.h"

namespace moba {

inline constexpr int kMinTotalForBest = 10;  // 自己ベストに数えるのに必要な総数（1 / 1 = 100% のようなプレイを除く）
inline constexpr std::size_t kMovingAverageWindow = 10;  // 成績の推移の移動平均の回数
inline constexpr int kCompareDays = 7;                   // 「直近 7 日」と「その前の 7 日」を比べる

/// 統計の絞り込み条件（0 や空の項目は「すべて」）
struct StatsFilter {
    GameMode mode = GameMode::SkillDodge;
    std::string characterId;       // 自キャラ
    std::string stageId;           // ステージ
    int level = 0;                 // 賢さ（スキルドッジは敵、スキルショットは的）
    int enemyCount = 0;            // スキルドッジ: 敵の数
    float targetRadius = 0.0f;     // スキルショット: 的の大きさ
    float targetMoveSpeed = 0.0f;  // スキルショット: 的の移動速度
    int periodDays = 0;            // 今日を含めて何日分か（0 なら保存しているすべて）
};

/// saved が条件に合うか
/// @param today 今日（YYYYMMDD。期間の絞り込みに使う）
bool matchesFilter(const SavedResult& saved, const StatsFilter& filter, int today);
/// 条件に合う結果（古い順）
std::vector<const SavedResult*> filterResults(const std::vector<SavedResult>& all, const StatsFilter& filter,
                                              int today);

/// 絞り込んだ結果の要約（ダッシュボードの数字の欄）
struct StatsSummary {
    int plays = 0;
    float playedSeconds = 0.0f;
    ResultScore total;     // すべてのプレイの合計
    ResultScore recent;    // 直近 7 日（今日を含む）の合計
    ResultScore previous;  // その前の 7 日の合計
    int recentPlays = 0;
    int previousPlays = 0;
    const SavedResult* best = nullptr;  // 自己ベスト（総数が kMinTotalForBest 以上のプレイの中で最も成績が高いもの）
};

/// 絞り込んだ結果を要約する
/// @param results 古い順
StatsSummary summarize(const std::vector<const SavedResult*>& results, int today);

/// values の移動平均（それまでの最大 window 回の平均。最初の方は、ある分だけで平均する）
std::vector<float> movingAverage(const std::vector<float>& values, std::size_t window);

/// 自己ベストを更新したプレイの番号（results の中の位置）。総数が kMinTotalForBest 未満のプレイは数えない
/// @param results 古い順
std::vector<std::size_t> personalBestUpdates(const std::vector<const SavedResult*>& results);

/// 成績の高い順に n 件（同じ割合なら総数が多い順、さらに同じなら新しい順）。総数が kMinTotalForBest 未満のプレイは除く
std::vector<const SavedResult*> topResults(const std::vector<const SavedResult*>& results, std::size_t n);

/// 1 日分の集計
struct DayStat {
    int date = 0;  // YYYYMMDD
    int plays = 0;
    ResultScore score;
};
/// 日ごとの集計（古い順。プレイした日だけ）
std::vector<DayStat> dailyStats(const std::vector<const SavedResult*>& results);

/// 賢さ 1 段階分の集計
struct LevelStat {
    int level = 0;
    std::string name;  // 賢さの名前（保存した時の名前）
    int plays = 0;
    ResultScore score;
};
/// 賢さごとの集計（賢さの低い順。プレイした賢さだけ）
std::vector<LevelStat> levelStats(const std::vector<const SavedResult*>& results);

/// キーごとの、自キャラのスキルのヒット率の合計
std::array<HitCount, kSkillSlotCount> hitsBySlot(const std::vector<const SavedResult*>& results);

}  // namespace moba
