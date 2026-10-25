#pragma once
// アプリ本体: ウィンドウ・メインループ・画面（メニュー / プレイ中 / リザルト）の切り替え

#include <memory>
#include <optional>
#include <string>

#include "app/CommandLine.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "game/MatchConfig.h"
#include "input/InputSource.h"
#include "raylib.h"
#include "ui/MainMenu.h"

namespace moba {

/// アプリ本体。ウィンドウとメインループを持ち、画面（メニュー・プレイ中・リザルト）を切り替える
class App {
public:
    /// 設定ファイルの場所とコマンドライン引数を覚える（ウィンドウはまだ開かない）
    App(AppPaths paths, CommandLineOptions options);
    /// 設定ファイルを読み、ウィンドウを開いて、閉じられるまでメインループを回す
    /// @return 終了コード（0 = 正常。入力スクリプトが読めない時は 2）
    int run();

private:
    /// 今の画面（メニュー・プレイ中・ポーズ中・リザルト）
    enum class Screen { Menu, Playing, Paused, Result };

    // ゲームの時間は 1/120 秒刻みで進める（描画のフレームレートに左右されないように）
    static constexpr float kFixedDt = 1.0f / 120.0f;

    /// ウィンドウを開き、Windows の拡大率に合わせて大きさと文字の倍率を決め、日本語フォントを読み込む
    void initWindow();
    /// カーソルの閉じ込めを解除し、ウィンドウを閉じる
    void shutdownWindow();
    /// 設定の大きさ（拡大率 100% の時の大きさ）に Windows の拡大率を掛け、画面に収まるようにして中央に置く
    void fitWindowToScreen(int logicalWidth, int logicalHeight);
    /// 文字・ボタンの倍率（Windows の拡大率 × 設定の uiScale）を反映する
    void applyUiScale();
    /// 別のモニターへの移動などで Windows の拡大率が変わったら追従する
    void checkDpiChange();
    /// 1 フレームの処理（設定ファイルの確認 → 入力 → ゲームの更新 → 描画 → 処理時間の記録）
    void frame();
    /// 画面に関係なく効くキー（F1・F5・Alt + Enter・Esc・Y）と、入力スクリプトの指示を処理する
    void handleGlobalKeys(const InputFrame& input);
    /// 設定ファイルを読み直す。読み直したら、ログの詳しさ・文字の倍率・カメラ・キャラ・ダミーにも反映する
    /// @param force true なら変更が無くても読み直す（F5）。false なら変更があった時だけ
    void reloadData(bool force);
    /// 読み込んだ設定ファイルの誤り（存在しない handler の名前など）を確かめ、エラーとして残す
    void validateData();
    /// コマンドライン引数から、メニューを飛ばして始める時のプレイの設定を作る
    MatchConfig configFromCommandLine() const;
    /// プレイを始める（始められなければメニューのまま）。カメラは自キャラを映す
    void startMatch(const MatchConfig& config);
    /// プレイを終えてメニューに戻る
    void backToMenu();
    /// プレイ中に Esc を押した時の処理。制限時間のあるモード（スキルドッジ・スキルショット）はポーズし、
    /// 練習場はメニューに戻る
    void pauseOrLeave();
    /// ポーズを解いてプレイに戻る（ポーズ中の入力は捨てる。進行状況・キャラ・弾の状態はそのまま）
    void resume();
    /// たまった時間の分だけ、固定の時間刻みでゲームを進める。終わったらリザルト画面へ
    /// @param frameDt 前のフレームからの経過時間（秒）
    void updateMatch(float frameDt, const InputFrame& input);
    /// カメラの拡大率と注視点を決める（自キャラに固定・固定しない・Space）
    void updateCamera(float frameDt, const InputFrame& input);
    /// カメラを「自キャラに固定」と「固定しない」で切り替える（Y キー）
    void toggleCameraMode();
    /// プレイ中（ウィンドウが選ばれている間）は、カーソルをウィンドウの外に出さない。それ以外の時は解除する
    void updateCursorConfinement();
    /// 画面を描く（ゲーム画面 → メニュー・表示・デバッグパネル・リザルト）。スクリーンショットの指示があれば保存する
    void draw();
    /// 今の画面を PNG で保存する（入力スクリプトの screenshot）
    void saveScreenshot(const std::string& file);

    GameData data_;
    CommandLineOptions options_;
    Screen screen_ = Screen::Menu;

    std::unique_ptr<Match> match_;
    std::optional<MatchResult> result_;
    MainMenu menu_;
    std::unique_ptr<InputSource> input_;

    Camera2D camera_{};
    CameraMode cameraMode_ = CameraMode::Locked;          // 今のカメラ（メニューと Y キーで変わる）
    CameraMode settingsCameraMode_ = CameraMode::Locked;  // 設定ファイルの cameraMode（変わった時だけ反映するため）
    bool cursorConfined_ = false;                         // カーソルをウィンドウの中に閉じ込めているか
    InputFrame pendingInput_;                             // まだゲームの更新に渡していない入力
    float accumulator_ = 0.0f;
    float reloadCheckTimer_ = 0.0f;
    float scriptClock_ = 0.0f;  // 入力スクリプトの時刻（1 フレームごとに 1/60 秒進む）
    float dpiScale_ = 1.0f;     // ウィンドウがあるモニターの Windows の拡大率（150% なら 1.5）

    /// 処理時間の計測（一定間隔でログに出す）
    struct PerfCounter {
        double windowStart = 0.0;
        int frames = 0;
        double updateSeconds = 0.0;
        double drawSeconds = 0.0;
        double worstFrame = 0.0;
    };
    PerfCounter perf_;
    /// 1 フレームの処理時間を数え、5 秒ごとに FPS と処理時間をログに出す
    /// @param frameStart / updateEnd / drawEnd フレームの始め・更新の終わり・描画の終わりの時刻（秒）
    void recordPerf(double frameStart, double updateEnd, double drawEnd);
    bool debugPanelVisible_ = true;
    bool quit_ = false;
    std::optional<std::string> screenshotRequest_;
};

}  // namespace moba
