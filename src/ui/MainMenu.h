#pragma once
// メインメニュー（キャラ・モード・ステージ・クールダウン・カメラの選択）

#include "data/Settings.h"
#include "game/MatchConfig.h"

namespace moba {

class GameData;

struct MenuAction {
    enum class Kind { None, Start, Quit };
    Kind kind = Kind::None;
    MatchConfig config;  // Start の時に、選ばれた設定
};

class MainMenu {
public:
    // cameraMode はプレイ中に Y キーでも変わるので、メニューでは持たずに App の値をそのまま変える
    MenuAction draw(const GameData& data, CameraMode& cameraMode);
    // 前回のプレイの設定を、次にメニューを開いた時の初期値にする
    void setConfig(const MatchConfig& config);

private:
    MatchConfig config_;
    bool initialized_ = false;  // 設定ファイルの初期値を読み込んだか
};

}  // namespace moba
