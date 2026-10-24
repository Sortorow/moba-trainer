#pragma once
// リザルト画面（制限時間が来た後に表示する）

#include "game/Match.h"

namespace moba {

enum class ResultAction { None, Retry, Menu };

ResultAction drawResultScreen(const MatchResult& result);

}  // namespace moba
