#pragma once
// ゲームの設定（data/settings.jsonc）

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/Log.h"

namespace moba {

/// スキルの発動方式
enum class CastMode {
    Quick,  // キーを押した瞬間にカーソル方向へ発動
};

/// カメラの動かし方
enum class CameraMode {
    Locked,  // 自キャラに固定（自キャラが常に画面の中央）
    Free,    // 固定しない（自キャラが動いてもカメラは動かず、画面の端にカーソルを置くとその方向へ動く）
};

/// カメラの、設定ファイルでの表記（"locked" / "free"）
constexpr std::string_view toString(CameraMode mode) {
    switch (mode) {
        case CameraMode::Locked: return "locked";
        case CameraMode::Free: return "free";
    }
    return "?";
}

/// カメラの、画面に出す名前（「自キャラに固定」/「固定しない」）
constexpr std::string_view displayName(CameraMode mode) {
    switch (mode) {
        case CameraMode::Locked: return "自キャラに固定";
        case CameraMode::Free: return "固定しない";
    }
    return "?";
}

/// ゲーム全体の設定（data/settings.jsonc）
struct Settings {
    CastMode castMode = CastMode::Quick;
    CameraMode cameraMode = CameraMode::Locked;  // 起動時のカメラ（メニューと Y キーでも切り替えられる）
    float cameraViewHeight = 1500.0f;            // 画面の縦方向に映るゲーム内の距離
    float cameraEdgeScrollSpeed = 2500.0f;       // 固定しない時に、画面の端でカメラが動く速さ（ゲーム内の距離 / 秒）
    // 固定しない時に、画面の端からこの幅まで近づくとカメラが動く（拡大率 100% の時のピクセル）
    float cameraEdgeMargin = 12.0f;
    float visionRadius = 1150.0f;  // 視界の半径（全キャラ共通）
    // 相手から見えていない時に、対象指定のスキルでその相手を撃つと、相手の陣営から見えるようになる時間（秒）
    float attackRevealTime = 3.0f;
    float fogDarkness = 0.6f;  // 視界の外を暗くする強さ（0 = 暗くしない、1 = 真っ黒）
    // モードごとの、視界の初期値（true = 全視界。敵も味方もステージのすべてが見える）。メニューで変えられる
    bool fullVisionSandbox = true;
    bool fullVisionDodge = false;
    bool fullVisionShot = false;
    float cooldownScale = 0.25f;  // クールダウン「標準」モードの倍率の初期値（メニューで変更できる）
    bool showHitboxes = true;     // 弾・範囲の当たり判定の円を表示する
    float uiScale = 1.0f;         // 文字・ボタンの大きさの倍率（Windows の拡大率に、さらに掛ける）
    int windowWidth = 1600;       // 起動時のウィンドウの大きさ（拡大率 100% の時の大きさ）
    int windowHeight = 900;
    log::Level logLevel = log::Level::Debug;
};

/// settings.jsonc の読み込み結果（設定とおかしかった項目、または読めなかった理由）
struct SettingsLoadResult {
    Settings settings;
    std::string error;                  // 空なら成功
    std::vector<std::string> warnings;  // 読み込めたが、値がおかしかった項目
};

/// data/settings.jsonc を読み込む。値は決められた範囲に収め、知らない値は既定値にして warnings に書く
/// @return 読み込んだ設定。ファイルが読めない時は error に理由が入る
SettingsLoadResult loadSettings(const std::filesystem::path& file);

}  // namespace moba
