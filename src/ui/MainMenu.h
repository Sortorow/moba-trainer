#pragma once
// メインメニュー（キャラ・モード・ステージ・クールダウン・カメラの選択）

#include "data/Settings.h"
#include "game/MatchConfig.h"
#include "game/ResultStore.h"
#include "ui/StatsDashboard.h"

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
    /// results は「記録を見る」で開く記録の画面と、「統計を見る」で開く統計の画面に使う
    MenuAction draw(GameData& data, CameraMode& cameraMode, const ResultStore& results);
    /// 前回のプレイの設定を、次にメニューを開いた時の初期値にする
    void setConfig(const MatchConfig& config);
    /// 記録の画面を開く
    void openRecords() { showRecords_ = true; }
    /// 統計の画面を、mode のタブを選んで開く
    void openStats(GameMode mode) {
        showStats_ = true;
        stats_.selectMode(mode);
    }

private:
    MatchConfig config_;
    bool initialized_ = false;  // 設定ファイルの初期値を読み込んだか
    bool showRecords_ = false;  // 記録の画面を開いているか
    bool showStats_ = false;    // 統計の画面を開いているか
    StatsDashboard stats_;      // 統計の画面（絞り込みの条件を覚えておく）
};

}  // namespace moba
