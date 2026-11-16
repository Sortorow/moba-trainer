#pragma once
// メインメニュー（キャラ・モード・ステージ・モードごとの設定・クールダウン・視界・カメラ・解像度の選択と、
// 記録・統計の画面を開くボタン。モードごとの設定は、練習場のダミー / 敵の数・賢さ・制限時間 / 的の賢さ・大きさ・移動速度・制限時間）

#include "data/Settings.h"
#include "game/MatchConfig.h"
#include "game/ResultStore.h"
#include "ui/StatsDashboard.h"
#include "ui/WindowResolution.h"

namespace moba {

class GameData;

/// メニューで押された操作（開始・終了・解像度の変更）と、その内容
struct MenuAction {
    /// 押された操作の種類
    enum class Kind { None, Start, Quit, ChangeResolution };
    Kind kind = Kind::None;
    MatchConfig config;         // Start の時に、選ばれた設定
    ui::WindowSize resolution;  // ChangeResolution の時に、選ばれた解像度
};

/// メニューの「画面」に出す、今のウィンドウの状態（App が毎フレーム作る）
struct DisplayState {
    ui::WindowSize windowSize;  // ウィンドウの時の大きさ（全画面の間は、ウィンドウに戻した時の大きさ）
    ui::WindowSize screenSize;  // 今描いている大きさ（全画面の間はモニターの解像度）
    ui::WindowSize frame;       // タイトルバーと枠の分、ウィンドウ全体が中の大きさより大きくなる量
    ui::WindowSize workArea;    // モニターの作業領域（タスクバーを除いた範囲）。分からなければ 0
    bool fullscreen = false;    // 全画面（Alt + Enter）の間は、解像度を選べない
};

/// メインメニュー（キャラ・モード・ステージ・モードごとの設定・クールダウン・視界・カメラ・解像度の選択と、
/// 記録・統計の画面を開くボタン）
class MainMenu {
public:
    /// cameraMode はプレイ中に Y キーでも変わるので、メニューでは持たずに App の値をそのまま変える
    /// data は、練習場のダミーのプリセットを選んだ時に書き換える（sandbox.jsonc に保存する）
    /// results は「記録を見る」で開く記録の画面と、「統計を見る」で開く統計の画面に使う
    /// display は「画面」の欄（解像度の一覧と、画面に入らない解像度の判定）に使う
    MenuAction draw(GameData& data, CameraMode& cameraMode, const ResultStore& results, const DisplayState& display);
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
