#pragma once
// アプリ本体: ウィンドウ・メインループ・画面（メニュー / 練習場）の切り替え

#include <memory>
#include <optional>
#include <string>

#include "app/CommandLine.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/DummyController.h"
#include "game/PlayerController.h"
#include "game/World.h"
#include "input/InputSource.h"
#include "raylib.h"
#include "ui/MainMenu.h"

namespace moba {

class App {
public:
    App(AppPaths paths, CommandLineOptions options);
    int run();

private:
    enum class Screen { Menu, Sandbox };

    // ゲームの時間は 1/120 秒刻みで進める（描画のフレームレートに左右されないように）
    static constexpr float kFixedDt = 1.0f / 120.0f;

    void initWindow();
    void shutdownWindow();
    void frame();
    void handleGlobalKeys(const InputFrame& input);
    void reloadData(bool force);
    void validateData();
    void startSandbox(const std::string& characterId);
    void backToMenu();
    void updateSandbox(float frameDt, const InputFrame& input);
    void updateCamera();
    void draw();
    void saveScreenshot(const std::string& file);

    GameData data_;
    CommandLineOptions options_;
    Screen screen_ = Screen::Menu;

    std::unique_ptr<World> world_;
    PlayerController playerController_;
    DummyController dummies_;
    MainMenu menu_;
    std::unique_ptr<InputSource> input_;
    std::string characterId_;

    Camera2D camera_{};
    InputFrame pendingInput_;  // まだゲームの更新に渡していない入力
    float accumulator_ = 0.0f;
    float reloadCheckTimer_ = 0.0f;
    bool debugPanelVisible_ = true;
    bool quit_ = false;
    std::optional<std::string> screenshotRequest_;
};

}  // namespace moba
