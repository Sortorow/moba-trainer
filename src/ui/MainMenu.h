#pragma once
// メインメニュー（キャラ・モード・ステージの選択）

#include <string>

namespace moba {

class GameData;

struct MenuAction {
    enum class Kind { None, Start, Quit };
    Kind kind = Kind::None;
    std::string characterId;
};

class MainMenu {
public:
    MenuAction draw(const GameData& data);
    void select(std::string characterId) { selectedCharacter_ = std::move(characterId); }

private:
    std::string selectedCharacter_;
};

}  // namespace moba
