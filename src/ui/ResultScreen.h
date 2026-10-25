#pragma once
// リザルト画面（制限時間が来た後に表示する）

#include <string>
#include <vector>

#include "game/Match.h"
#include "game/ResultStore.h"

namespace moba {

/// リザルト画面で押されたボタン
enum class ResultAction { None, Retry, Menu };

/// リザルト画面（スキルドッジ: ドッジ率・被弾・ダウン、スキルショット: ヒット率と、プレイの設定、直近 10 回の順位表）を描く
/// @param recent そのモードの直近の記録（今回の結果を含む）
/// @param savedPlayedAt 今回の結果を保存した日時（順位表で強調する）。保存していなければ nullptr
/// @return 押されたボタン（もう一度・メニューへ）。押されていなければ None
ResultAction drawResultScreen(const MatchResult& result, const std::vector<SavedResult>& recent,
                              const std::string* savedPlayedAt);

}  // namespace moba
