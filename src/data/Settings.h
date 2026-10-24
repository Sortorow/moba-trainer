#pragma once
// ゲームの設定（data/settings.jsonc）

#include <filesystem>
#include <string>
#include <vector>

#include "core/Log.h"

namespace moba {

enum class CastMode {
    Quick,  // キーを押した瞬間にカーソル方向へ発動
};

enum class CameraMode {
    Locked,  // 自キャラに固定
};

struct Settings {
    CastMode castMode = CastMode::Quick;
    CameraMode cameraMode = CameraMode::Locked;
    float cameraViewHeight = 1500.0f;  // 画面の縦方向に映るゲーム内の距離
    float cooldownScale = 1.0f;        // クールダウンの倍率（0 でクールダウンなし）
    bool showHitboxes = true;          // 弾・範囲の当たり判定の円を表示する
    int windowWidth = 1600;
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
