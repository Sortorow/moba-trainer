#pragma once
// デバッグパネル（F1 で表示切り替え）: 設定の一時変更・スキルの集計・ダミーの操作など

#include <optional>
#include <string>
#include <string_view>

namespace moba {

class World;
class GameData;
class DummyController;

struct DebugPanelResult {
    bool reloadData = false;
    std::optional<std::string> switchCharacter;  // キャラを切り替えて練習場をやり直す
};

DebugPanelResult drawDebugPanel(World& world, GameData& data, DummyController& dummies, std::string_view characterId);

}  // namespace moba
