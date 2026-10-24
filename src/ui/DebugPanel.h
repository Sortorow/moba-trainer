#pragma once
// デバッグパネル（F1 で表示切り替え）: 設定の一時変更・スキルの集計・ダミーの操作・敵 AI の状態など

#include <optional>
#include <string>

namespace moba {

class Match;
class GameData;

struct DebugPanelResult {
    bool reloadData = false;
    std::optional<std::string> switchCharacter;  // キャラを切り替えて、同じモードをやり直す
};

DebugPanelResult drawDebugPanel(Match& match, GameData& data);

}  // namespace moba
