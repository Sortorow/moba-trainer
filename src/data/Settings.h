#pragma once
// ゲームの設定（data/settings.jsonc）

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/Log.h"

namespace moba {

enum class CastMode {
    Quick,  // キーを押した瞬間にカーソル方向へ発動
};

enum class CameraMode {
    Locked,  // 自キャラに固定（自キャラが常に画面の中央）
    Free,    // 固定しない（自キャラが動いてもカメラは動かず、画面の端にカーソルを置くとその方向へ動く）
};

constexpr std::string_view toString(CameraMode mode) {
    switch (mode) {
        case CameraMode::Locked: return "locked";
        case CameraMode::Free: return "free";
    }
    return "?";
}

constexpr std::string_view displayName(CameraMode mode) {
    switch (mode) {
        case CameraMode::Locked: return "自キャラに固定";
        case CameraMode::Free: return "固定しない";
    }
    return "?";
}

struct Settings {
    CastMode castMode = CastMode::Quick;
    CameraMode cameraMode = CameraMode::Locked;  // 起動時のカメラ（メニューと Y キーでも切り替えられる）
    float cameraViewHeight = 1500.0f;            // 画面の縦方向に映るゲーム内の距離
    float cameraEdgeScrollSpeed = 2500.0f;       // 固定しない時に、画面の端でカメラが動く速さ（ゲーム内の距離 / 秒）
    // 固定しない時に、画面の端からこの幅まで近づくとカメラが動く（拡大率 100% の時のピクセル）
    float cameraEdgeMargin = 12.0f;
    float cooldownScale = 0.25f;  // クールダウン「標準」モードの倍率の初期値（メニューで変更できる）
    bool showHitboxes = true;     // 弾・範囲の当たり判定の円を表示する
    float uiScale = 1.0f;         // 文字・ボタンの大きさの倍率（Windows の拡大率に、さらに掛ける）
    int windowWidth = 1600;       // 起動時のウィンドウの大きさ（拡大率 100% の時の大きさ）
    int windowHeight = 900;
    log::Level logLevel = log::Level::Debug;
};

struct SettingsLoadResult {
    Settings settings;
    std::string error;                  // 空なら成功
    std::vector<std::string> warnings;  // 読み込めたが、値がおかしかった項目
};

SettingsLoadResult loadSettings(const std::filesystem::path& file);

}  // namespace moba
