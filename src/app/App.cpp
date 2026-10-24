#include "app/App.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <format>

#include "core/Log.h"
#include "core/Platform.h"
#include "game/Sandbox.h"
#include "game/skills/Skill.h"
#include "imgui.h"
#include "render/Hud.h"
#include "render/Renderer.h"
#include "rlImGui.h"
#include "ui/DebugPanel.h"
#include "ui/Fonts.h"

namespace moba {
namespace {

// raylib 自身のログも、ゲームのログファイルに書き込む
void raylibLogCallback(int logLevel, const char* text, va_list args) {
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), text, args);
    log::Level level = log::Level::Debug;
    if (logLevel >= LOG_ERROR) level = log::Level::Error;
    else if (logLevel == LOG_WARNING) level = log::Level::Warn;
    else if (logLevel == LOG_INFO) level = log::Level::Info;
    log::write(level, "raylib", buffer);
}

}  // namespace

App::App(AppPaths paths, CommandLineOptions options) : data_(std::move(paths)), options_(std::move(options)) {
    debugPanelVisible_ = !options_.hideDebugPanel;
}

int App::run() {
    data_.loadAll();
    validateData();
    log::setMinLevel(data_.settings().logLevel);

    if (options_.scriptFile) {
        std::string error;
        input_ = ScriptInput::load(*options_.scriptFile, error);
        if (!input_) {
            log::error("app", "{}", error);
            return 2;
        }
    } else {
        input_ = std::make_unique<LiveInput>();
    }

    initWindow();
    if (options_.characterId || options_.scriptFile) {
        const std::vector<const CharacterDef*> list = data_.characterList();
        const std::string id = options_.characterId.value_or(list.empty() ? std::string{} : list.front()->id);
        startSandbox(id);
    }
    while (!quit_ && !WindowShouldClose()) frame();
    shutdownWindow();
    return 0;
}

void App::initWindow() {
    SetTraceLogCallback(raylibLogCallback);
    SetTraceLogLevel(LOG_INFO);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    const int width = options_.windowWidth.value_or(data_.settings().windowWidth);
    const int height = options_.windowHeight.value_or(data_.settings().windowHeight);
    InitWindow(width, height, "MOBA Trainer");
    SetExitKey(KEY_NULL);  // Esc はメニューに戻るキーとして使う
    platform::disableIme(GetWindowHandle());

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                                 // ウィンドウ配置を imgui.ini に保存しない
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;    // Q/W/E/R が UI の操作に取られないように
    ui::loadJapaneseFont(18.0f);

    log::info("app", "ウィンドウを開きました: {}x{}", width, height);
}

void App::shutdownWindow() {
    rlImGuiShutdown();
    CloseWindow();
    log::info("app", "ウィンドウを閉じました");
}

void App::frame() {
    // 入力スクリプトの時は描画速度に関係なく 1/60 秒ずつ進める（毎回同じ結果になるように）
    const float frameDt = input_->isScripted() ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.25f);

    reloadCheckTimer_ += frameDt;
    if (reloadCheckTimer_ >= 1.0f) {
        reloadCheckTimer_ = 0.0f;
        reloadData(false);
    }

    const ImGuiIO& io = ImGui::GetIO();
    const InputPollContext pollContext{camera_, world_ ? world_->time() : 0.0f, io.WantCaptureMouse, io.WantTextInput};
    const InputFrame input = input_->poll(pollContext);
    handleGlobalKeys(input);
    if (input.screenshotRequest) screenshotRequest_ = input.screenshotRequest;

    if (screen_ == Screen::Sandbox && world_) updateSandbox(frameDt, input);
    draw();
    if (input.exitRequested) {
        log::info("app", "入力スクリプトの exit で終了します");
        quit_ = true;
    }
}

void App::handleGlobalKeys(const InputFrame& input) {
    if (input.toggleDebugPressed) debugPanelVisible_ = !debugPanelVisible_;
    if (input.reloadPressed) reloadData(true);
    if (input.toggleFullscreenPressed) ToggleBorderlessWindowed();
    if (input.escapePressed && screen_ == Screen::Sandbox) backToMenu();
    if (input.resetCooldownsRequested && world_) {
        if (Unit* player = world_->player()) player->cooldowns = {};
        log::debug("app", "入力スクリプト: クールダウンを解除しました");
    }
}

void App::reloadData(bool force) {
    if (force) {
        log::info("app", "F5: 設定ファイルを再読み込みします");
        data_.loadAll();
    } else if (!data_.reloadIfChanged()) {
        return;
    }
    validateData();
    log::setMinLevel(data_.settings().logLevel);
    if (world_) world_->refreshUnitsFromData();
}

// 設定ファイルの handler 名が実在するかを確認する
void App::validateData() {
    for (const CharacterDef* def : data_.characterList()) {
        for (const auto& skill : def->skills) {
            if (skill && !skills::handlerExists(skill->handler)) {
                const std::string message = std::format("{} の {}: handler \"{}\" は存在しません", def->id,
                                                        toString(skill->slot), skill->handler);
                log::error("data", "{}", message);
                data_.addError(message);
            }
        }
    }
}

void App::startSandbox(const std::string& characterId) {
    const CharacterDef* def = data_.character(characterId);
    if (!def) {
        const std::vector<const CharacterDef*> list = data_.characterList();
        if (list.empty()) {
            log::error("app", "キャラクターの設定が 1 つもないため、練習場を始められません");
            return;
        }
        log::warn("app", "キャラ \"{}\" が見つからないため {} を使います", characterId, list.front()->id);
        def = list.front();
    }

    world_ = std::make_unique<World>(data_, kSandboxBounds);
    setupSandbox(*world_, dummies_, *def);
    playerController_ = PlayerController{};
    pendingInput_ = InputFrame{};
    accumulator_ = 0.0f;
    characterId_ = def->id;
    menu_.select(def->id);
    screen_ = Screen::Sandbox;
    updateCamera();
    log::info("app", "練習場を開始しました: キャラ={}", def->id);
}

void App::backToMenu() {
    log::info("app", "メニューに戻ります");
    world_.reset();
    screen_ = Screen::Menu;
}

void App::updateSandbox(float frameDt, const InputFrame& input) {
    pendingInput_.mergeNewer(input);
    accumulator_ += frameDt;
    while (accumulator_ >= kFixedDt) {
        playerController_.update(*world_, pendingInput_, kFixedDt);
        dummies_.update(*world_);
        world_->update(kFixedDt);
        pendingInput_.clearPressed();
        accumulator_ -= kFixedDt;
    }
    updateCamera();
}

void App::updateCamera() {
    camera_.offset = Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
    camera_.rotation = 0.0f;
    camera_.zoom = static_cast<float>(GetScreenHeight()) / data_.settings().cameraViewHeight;
    if (world_) {
        if (const Unit* player = world_->player()) camera_.target = player->pos;  // カメラは自キャラに固定
    }
}

void App::draw() {
    BeginDrawing();
    ClearBackground(Color{14, 17, 22, 255});

    if (screen_ == Screen::Sandbox && world_) {
        BeginMode2D(camera_);
        drawWorld(*world_, RenderOptions{data_.settings().showHitboxes});
        EndMode2D();
    }

    rlImGuiBegin();
    if (screen_ == Screen::Menu) {
        const MenuAction action = menu_.draw(data_);
        if (action.kind == MenuAction::Kind::Start) startSandbox(action.characterId);
        if (action.kind == MenuAction::Kind::Quit) quit_ = true;
    } else if (world_) {
        drawHud(*world_, camera_);
        if (debugPanelVisible_) {
            const DebugPanelResult result = drawDebugPanel(*world_, data_, dummies_, characterId_);
            if (result.reloadData) reloadData(true);
            if (result.switchCharacter) startSandbox(*result.switchCharacter);
        }
    }
    rlImGuiEnd();

    // 画面を描き終えた後、表示に切り替える前に保存する
    if (screenshotRequest_) {
        saveScreenshot(*screenshotRequest_);
        screenshotRequest_.reset();
    }
    EndDrawing();
}

void App::saveScreenshot(const std::string& file) {
    Image image = LoadImageFromScreen();
    const bool ok = ExportImage(image, file.c_str());
    UnloadImage(image);
    if (ok) log::info("app", "スクリーンショットを保存しました: {}", file);
    else log::error("app", "スクリーンショットを保存できませんでした: {}", file);
}

}  // namespace moba
