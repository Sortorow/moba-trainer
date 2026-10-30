#pragma once
// 記録（プレイの結果）の表示に共通の書き方。記録の表・リザルト画面・統計の画面で、同じ内容を同じ形で出すため

#include <string>

#include "game/MatchConfig.h"
#include "game/ResultStore.h"
#include "imgui.h"

namespace moba::ui {

/// 難易度の表示（スキルドッジ:「敵 3 体・ふつう」、スキルショット:「ふつう・標準・340」）
std::string difficultyLabel(const MatchResult& result);

/// 保存した日時の短い表示（「2026-10-25 22:40:12」→「10-25 22:40」。形が違えばそのまま）
std::string shortTime(const std::string& playedAt);

/// 割合の表示（「52.0%」。総数が 0 なら「-」）
std::string percentText(const ResultScore& score);

/// 順位の文字の色（1〜3 位は金・銀・銅、それ以外は白）
ImVec4 rankColor(int rank);

/// クールダウンの設定の表示の書き方（「なし」の時はどちらも「なし」）
enum class CooldownLabelStyle {
    Short,     // 「×0.25」（記録の表など、狭い所）
    WithMode,  // 「標準（×0.25）」（リザルト画面）
};

/// クールダウンの設定の表示。「なし」か倍率かの判断は共通で、標準の時の書き方を style で選ぶ
std::string cooldownLabel(const MatchConfig& config, CooldownLabelStyle style);

}  // namespace moba::ui
