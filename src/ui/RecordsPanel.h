#pragma once
// 記録（保存したリザルト）の表示: 直近 10 回の順位表（スコアアタックのような表示）と、メニューから開く記録の画面

#include <string>
#include <vector>

#include "game/MatchConfig.h"
#include "game/ResultStore.h"

namespace moba {

/// recent を順位の順（割合の高い順）に並べた時の、playedAt の結果の順位（1 始まり）。見つからなければ 0
int rankOf(const std::vector<SavedResult>& recent, const std::string& playedAt);

/// 直近の記録を、スコアアタックのような順位表で描く（割合の高い順。1〜3 位は金・銀・銅の色、今回の結果は強調）
/// @param recent 直近の結果（並びは問わない）
/// @param highlightPlayedAt 強調する結果（今回のプレイ）の日時。無ければ nullptr
/// @param maxHeight 表の高さの上限（画面の px）。表がこれより高くなる時はスクロールする。0 なら上限なし
void drawRecordTable(GameMode mode, const std::vector<SavedResult>& recent, const std::string* highlightPlayedAt,
                     float maxHeight = 0.0f);

/// 記録の画面（メニューから開く）。モードごとのタブに、直近 10 回の順位表を出す
/// @return 「閉じる」が押されたら true
bool drawRecordsWindow(const ResultStore& store);

}  // namespace moba
