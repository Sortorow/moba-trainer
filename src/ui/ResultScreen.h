#pragma once
// リザルト画面（制限時間が来た後に表示する）

#include "game/Match.h"

namespace moba {

enum class ResultAction { None, Retry, Menu };

/// リザルト画面（ドッジ率・被弾・ダウン・プレイの設定）を描く
/// @return 押されたボタン（もう一度・メニューへ）。押されていなければ None
ResultAction drawResultScreen(const MatchResult& result);

}  // namespace moba
