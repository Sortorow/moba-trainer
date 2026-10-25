#pragma once
// メインメニュー（キャラ・モード・ステージ・クールダウン・カメラの選択）

#include "data/Settings.h"
#include "game/MatchConfig.h"

namespace moba {

class GameData;

/// メニューで押された操作（開始・終了）と、開始する時の設定
struct MenuAction {
    /// 押された操作の種類
    enum class Kind { None, Start, Quit };
    Kind kind = Kind::None;
    MatchConfig config;  // Start の時に、選ばれた設定
};

/// メインメニュー（キャラ・モード・ステージ・ダミー・クールダウン・カメラの選択）
class MainMenu {
public:
    /// cameraMode はプレイ中に Y キーでも変わるので、メニューでは持たずに App の値をそのまま変える
    /// data は、練習場のダミーのプリセットを選んだ時に書き換える（sandbox.jsonc に保存する）
    MenuAction draw(GameData& data, CameraMode& cameraMode);
    /// 前回のプレイの設定を、次にメニューを開いた時の初期値にする
    void setConfig(const MatchConfig& config);

private:
    MatchConfig config_;
    bool initialized_ = false;  // 設定ファイルの初期値を読み込んだか
};

}  // namespace moba
