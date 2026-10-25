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

class App {
public:
    App(AppPaths paths, CommandLineOptions options);
    int run();

private:
    enum class Screen { Menu, Playing, Result };

    // ゲームの時間は 1/120 秒刻みで進める（描画のフレームレートに左右されないように）
    static constexpr float kFixedDt = 1.0f / 120.0f;

    void initWindow();
    void shutdownWindow();
    void fitWindowToScreen(int logicalWidth, int logicalHeight);
    void applyUiScale();    // 文字・ボタンの倍率（Windows の拡大率 × 設定の uiScale）を反映する
    void checkDpiChange();  // 別のモニターへの移動などで Windows の拡大率が変わったら追従する
    void frame();
    void handleGlobalKeys(const InputFrame& input);
    void reloadData(bool force);
    void validateData();
    MatchConfig configFromCommandLine() const;
    void startMatch(const MatchConfig& config);
    void backToMenu();
    void updateMatch(float frameDt, const InputFrame& input);
    void updateCamera(float frameDt, const InputFrame& input);
    void toggleCameraMode();
    void updateCursorConfinement();  // プレイ中は、カーソルをウィンドウの外に出さない
    void draw();
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

    // 処理時間の計測（一定間隔でログに出す）
    struct PerfCounter {
        double windowStart = 0.0;
        int frames = 0;
        double updateSeconds = 0.0;
        double drawSeconds = 0.0;
        double worstFrame = 0.0;
    };
    PerfCounter perf_;
    void recordPerf(double frameStart, double updateEnd, double drawEnd);
    bool debugPanelVisible_ = true;
    bool quit_ = false;
    std::optional<std::string> screenshotRequest_;
};

}  // namespace moba
