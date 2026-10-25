#include "app/App.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <format>

#include "core/Log.h"
#include "core/Platform.h"
#include "game/skills/Skill.h"
#include "imgui.h"
#include "render/CameraControl.h"
#include "render/Hud.h"
#include "render/Renderer.h"
#include "rlImGui.h"
#include "ui/DebugPanel.h"
#include "ui/Fonts.h"
#include "ui/ResultScreen.h"
#include "ui/SandboxPanel.h"
#include "ui/UiScale.h"

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
    cameraMode_ = settingsCameraMode_ = data_.settings().cameraMode;

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
    if (options_.characterId || options_.scriptFile || options_.mode || options_.stageId) {
        startMatch(configFromCommandLine());
    }
    while (!quit_ && !WindowShouldClose()) frame();
    shutdownWindow();
    return 0;
}

void App::initWindow() {
    SetTraceLogCallback(raylibLogCallback);
    SetTraceLogLevel(LOG_INFO);
    // 大きさを決めるまではウィンドウを隠しておく（Windows の拡大率に合わせて大きさを変える時に、ちらつかないように）
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIDDEN);
    const Settings& settings = data_.settings();
    InitWindow(settings.windowWidth, settings.windowHeight, "MOBA Trainer");
    SetExitKey(KEY_NULL);  // Esc はメニューに戻るキーとして使う
    platform::disableIme(GetWindowHandle());

    dpiScale_ = platform::windowDpiScale(GetWindowHandle());
    if (options_.windowWidth && options_.windowHeight) {
        // コマンドラインで指定された大きさは、拡大率を掛けずに実ピクセルとしてそのまま使う（自動での確認用）
        SetWindowSize(*options_.windowWidth, *options_.windowHeight);
    } else {
        fitWindowToScreen(settings.windowWidth, settings.windowHeight);
    }
    ClearWindowState(FLAG_WINDOW_HIDDEN);

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                               // ウィンドウ配置を imgui.ini に保存しない
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;  // Q/W/E/R が UI の操作に取られないように
    ui::loadJapaneseFont(18.0f);
    applyUiScale();

    log::info("app", "ウィンドウを開きました: {}x{}（Windows の拡大率 {:.0f}%）", GetScreenWidth(), GetScreenHeight(),
              dpiScale_ * 100.0f);
}

// 設定の大きさ（拡大率 100% の時の大きさ）に Windows の拡大率を掛け、画面に収まるようにして中央に置く
void App::fitWindowToScreen(int logicalWidth, int logicalHeight) {
    const platform::ScreenRect area = platform::monitorWorkArea(GetWindowHandle());
    const ui::WindowSize size = ui::fitWindowSize(logicalWidth, logicalHeight, dpiScale_, area.width, area.height);
    SetWindowSize(size.width, size.height);
    if (area.width > 0 && area.height > 0) {
        SetWindowPosition(area.x + (area.width - size.width) / 2, area.y + (area.height - size.height) / 2);
    }
}

void App::applyUiScale() {
    const float scale = dpiScale_ * data_.settings().uiScale;
    ui::setScale(scale);
    log::info("ui", "文字・ボタンの倍率を {:.2f} にしました（Windows の拡大率 {:.0f}% × 設定の uiScale {:.2f}）", scale,
              dpiScale_ * 100.0f, data_.settings().uiScale);
}

void App::checkDpiChange() {
    const float dpi = platform::windowDpiScale(GetWindowHandle());
    if (std::fabs(dpi - dpiScale_) < 0.001f) return;
    log::info("app", "Windows の拡大率が {:.0f}% から {:.0f}% に変わりました（別のモニターへの移動など）",
              dpiScale_ * 100.0f, dpi * 100.0f);
    dpiScale_ = dpi;
    applyUiScale();
}

void App::shutdownWindow() {
    if (cursorConfined_) platform::releaseCursor();
    rlImGuiShutdown();
    CloseWindow();
    log::info("app", "ウィンドウを閉じました");
}

void App::frame() {
    // 入力スクリプトの時は描画速度に関係なく 1/60 秒ずつ進める（毎回同じ結果になるように）
    const bool scripted = input_->isScripted();
    const float frameDt = scripted ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.25f);

    reloadCheckTimer_ += frameDt;
    if (reloadCheckTimer_ >= 1.0f) {
        reloadCheckTimer_ = 0.0f;
        reloadData(false);
    }
    checkDpiChange();

    const double frameStart = GetTime();
    const ImGuiIO& io = ImGui::GetIO();
    const InputPollContext pollContext{camera_, scriptClock_, io.WantCaptureMouse, io.WantTextInput};
    const InputFrame input = input_->poll(pollContext);
    handleGlobalKeys(input);
    updateCursorConfinement();
    if (input.screenshotRequest) screenshotRequest_ = input.screenshotRequest;

    if (screen_ == Screen::Playing && match_) updateMatch(frameDt, input);
    const double updateEnd = GetTime();
    draw();
    recordPerf(frameStart, updateEnd, GetTime());
    if (scripted) scriptClock_ += frameDt;
    if (input.exitRequested) {
        log::info("app", "入力スクリプトの exit で終了します");
        quit_ = true;
    }
}

// 5 秒ごとに、FPS と 1 フレームあたりの処理時間（ゲームの更新 / 描画。描画には垂直同期の待ち時間も含む）をログに出す
void App::recordPerf(double frameStart, double updateEnd, double drawEnd) {
    constexpr double kInterval = 5.0;
    if (perf_.frames == 0 && perf_.windowStart == 0.0) perf_.windowStart = frameStart;
    ++perf_.frames;
    perf_.updateSeconds += updateEnd - frameStart;
    perf_.drawSeconds += drawEnd - updateEnd;
    perf_.worstFrame = std::max(perf_.worstFrame, drawEnd - frameStart);

    const double elapsed = drawEnd - perf_.windowStart;
    if (elapsed < kInterval) return;
    log::debug("perf", "FPS {:.1f}  更新 {:.2f}ms  描画 {:.2f}ms  最も遅いフレーム {:.1f}ms", perf_.frames / elapsed,
               perf_.updateSeconds * 1000.0 / perf_.frames, perf_.drawSeconds * 1000.0 / perf_.frames,
               perf_.worstFrame * 1000.0);
    perf_ = PerfCounter{drawEnd};
}

void App::handleGlobalKeys(const InputFrame& input) {
    if (input.toggleDebugPressed) debugPanelVisible_ = !debugPanelVisible_;
    if (input.reloadPressed) reloadData(true);
    if (input.toggleFullscreenPressed) ToggleBorderlessWindowed();
    if (input.escapePressed && screen_ != Screen::Menu) backToMenu();
    if (input.toggleCameraPressed && screen_ == Screen::Playing) toggleCameraMode();
    if (input.resetCooldownsRequested && match_) {
        if (Unit* player = match_->world().player()) player->cooldowns = {};
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
    applyUiScale();  // uiScale が変わっているかもしれないので反映し直す
    // カメラは、設定ファイルの cameraMode が書き換えられた時だけ反映する（Y キーで切り替えた状態を、別の項目の保存で戻さないように）
    if (data_.settings().cameraMode != settingsCameraMode_) {
        settingsCameraMode_ = cameraMode_ = data_.settings().cameraMode;
        log::info("camera", "設定ファイルにより、カメラを「{}」にしました", displayName(cameraMode_));
    }
    if (match_) {
        match_->world().refreshUnitsFromData();
        match_->refreshDummiesIfChanged();  // sandbox.jsonc が書き換えられていれば、ダミーを置き直す
    }
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

MatchConfig App::configFromCommandLine() const {
    MatchConfig config;
    const std::vector<const CharacterDef*> list = data_.characterList();
    config.characterId = options_.characterId.value_or(list.empty() ? std::string{} : list.front()->id);
    config.cooldownScale = data_.settings().cooldownScale;
    if (options_.noCooldown) config.cooldownMode = CooldownMode::None;
    if (options_.mode == "dodge") config.mode = GameMode::SkillDodge;
    if (options_.stageId) config.stageId = *options_.stageId;
    if (options_.enemyCount) config.enemyCount = *options_.enemyCount;
    if (options_.aiLevel) config.aiLevel = *options_.aiLevel;
    if (options_.timeSeconds) config.timeLimitSeconds = *options_.timeSeconds;
    if (options_.seed) config.seed = *options_.seed;
    return config;
}

void App::startMatch(const MatchConfig& config) {
    auto match = std::make_unique<Match>(data_, config);
    if (!match->valid()) {
        log::error("app", "プレイを開始できませんでした");
        return;
    }
    match_ = std::move(match);
    result_.reset();
    pendingInput_ = InputFrame{};
    accumulator_ = 0.0f;
    menu_.setConfig(match_->config());
    screen_ = Screen::Playing;
    // どちらのカメラでも、始めは自キャラを画面の中央に映す
    if (const Unit* player = match_->world().player()) camera_.target = player->pos;
    updateCamera(0.0f, InputFrame{});
    log::info("camera", "カメラ: {}", displayName(cameraMode_));
}

void App::backToMenu() {
    log::info("app", "メニューに戻ります");
    match_.reset();
    result_.reset();
    screen_ = Screen::Menu;
}

void App::updateMatch(float frameDt, const InputFrame& input) {
    pendingInput_.mergeNewer(input);
    accumulator_ += frameDt;
    while (accumulator_ >= kFixedDt && !match_->finished()) {
        match_->update(kFixedDt, pendingInput_);
        pendingInput_.clearPressed();
        accumulator_ -= kFixedDt;
    }
    updateCamera(frameDt, input);
    if (match_->finished()) {
        result_ = match_->result();
        screen_ = Screen::Result;
    }
}

void App::updateCamera(float frameDt, const InputFrame& input) {
    const Settings& settings = data_.settings();
    camera_.offset = Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
    camera_.rotation = 0.0f;
    camera_.zoom = static_cast<float>(GetScreenHeight()) / settings.cameraViewHeight;
    if (!match_) return;
    const Unit* player = match_->world().player();
    if (!player) return;

    CameraControlInput control;
    control.mouse = GetMousePosition();
    control.screenWidth = static_cast<float>(GetScreenWidth());
    control.screenHeight = static_cast<float>(GetScreenHeight());
    control.edgeMargin = ui::px(settings.cameraEdgeMargin);
    // 入力スクリプトの時は実際のマウスを使わない。UI（デバッグパネルなど）の上にカーソルがある時も動かさない
    control.edgeScrollEnabled = !input_->isScripted() && IsWindowFocused() && !ImGui::GetIO().WantCaptureMouse;
    control.centerHeld = input.centerCameraDown;
    camera_.target = nextCameraTarget(cameraMode_, camera_.target, player->pos, control, settings.cameraEdgeScrollSpeed,
                                      match_->world().bounds(), frameDt);
}

void App::toggleCameraMode() {
    cameraMode_ = cameraMode_ == CameraMode::Locked ? CameraMode::Free : CameraMode::Locked;
    log::info("camera", "Y: カメラを「{}」にしました", displayName(cameraMode_));
}

// プレイ中（ウィンドウが選ばれている間）は、カーソルをウィンドウの外に出さない。カメラを固定しない時に、
// 画面の端にカーソルを置いてカメラを動かせるように。メニュー・リザルト・別のウィンドウへ切り替えた時は解除する
void App::updateCursorConfinement() {
    const bool confine = screen_ == Screen::Playing && !input_->isScripted() && IsWindowFocused();
    if (confine)
        platform::confineCursorToWindow(GetWindowHandle());  // ウィンドウの移動・大きさの変更に追従するため毎回
    else if (cursorConfined_) platform::releaseCursor();
    if (confine != cursorConfined_) {
        log::debug("camera", "{}",
                   confine ? "カーソルをウィンドウの中に閉じ込めました" : "カーソルの閉じ込めを解除しました");
    }
    cursorConfined_ = confine;
}

void App::draw() {
    BeginDrawing();
    ClearBackground(Color{14, 17, 22, 255});

    if (screen_ != Screen::Menu && match_) {
        BeginMode2D(camera_);
        drawWorld(match_->world(), RenderOptions{data_.settings().showHitboxes});
        EndMode2D();
    }

    rlImGuiBegin();
    switch (screen_) {
        case Screen::Menu: {
            const MenuAction action = menu_.draw(data_, cameraMode_);
            if (action.kind == MenuAction::Kind::Start) startMatch(action.config);
            if (action.kind == MenuAction::Kind::Quit) quit_ = true;
            break;
        }
        case Screen::Playing: {
            drawHud(*match_, camera_, cameraMode_);
            if (match_->config().mode == GameMode::Sandbox) {
                drawSandboxPanel(data_);
                match_->refreshDummiesIfChanged();  // プリセットを選び直していれば、ダミーを置き直す
            }
            if (!debugPanelVisible_) break;
            const DebugPanelResult result = drawDebugPanel(*match_, data_);
            if (result.reloadData) reloadData(true);
            if (result.switchCharacter) {
                MatchConfig next = match_->config();
                next.characterId = *result.switchCharacter;
                next.seed = 0;
                startMatch(next);
            }
            break;
        }
        case Screen::Result: {
            drawHud(*match_, camera_, cameraMode_);
            const ResultAction action = drawResultScreen(*result_);
            if (action == ResultAction::Retry) {
                MatchConfig next = result_->config;
                next.seed = 0;  // もう一度遊ぶ時は、新しい乱数の種で
                startMatch(next);
            } else if (action == ResultAction::Menu) {
                backToMenu();
            }
            break;
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
