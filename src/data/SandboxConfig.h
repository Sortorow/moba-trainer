#pragma once
// 練習場のダミーの設定（data/sandbox.jsonc）
//
// ダミーの組み合わせ（大きさ・移動速度・数・位置・動き方）を「プリセット」として複数書いておき、
// メニューや練習場の画面で選ぶ。選んだプリセットの名前は sandbox.jsonc の "preset" に書き戻す

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

namespace moba {

/// 練習場のダミー 1 体の設定
struct DummyDef {
    std::string name;
    Vector2 position{};               // 置く位置（自キャラは (0, 0) から始まる）
    std::optional<Vector2> patrolTo;  // あれば position とこの位置の間を往復する。無ければ静止
    float radius = 65.0f;             // 当たり判定の半径
    float moveSpeed = 300.0f;         // 往復する速さ（静止のダミーも、押し出された時に元の位置へ戻る速さに使う）
    Color color{215, 75, 75, 255};
};

/// ダミーの組み合わせ 1 つ（プリセット）
struct SandboxPreset {
    std::string name;
    std::string description;  // メニューなどに出す説明
    std::vector<DummyDef> dummies;
};

/// sandbox.jsonc の内容（今選んでいるプリセットと、すべてのプリセット）
struct SandboxConfig {
    std::string selectedPreset;  // 今選んでいるプリセットの名前
    std::vector<SandboxPreset> presets;

    /// 名前でプリセットを探す。見つからなければ nullptr
    const SandboxPreset* preset(std::string_view name) const;
    /// 今選んでいるプリセット。見つからなければ最初のプリセット。1 つも無ければ nullptr
    const SandboxPreset* current() const;
};

/// 中身（プリセットの選択と、すべてのプリセットの値）が同じか（再読み込みでダミーを置き直すかの判断に使う）
bool sameSandboxConfig(const SandboxConfig& a, const SandboxConfig& b);

/// sandbox.jsonc の読み込み結果（設定とおかしかった項目、または読めなかった理由）
struct SandboxConfigLoadResult {
    SandboxConfig config;
    std::string error;                  // 空なら成功
    std::vector<std::string> warnings;  // 読み込めたが、値がおかしかった項目
};

/// data/sandbox.jsonc を読み込む。値は決められた範囲に収める
/// @return 読み込んだ設定。読めない・"presets" が空の時は error に理由が入る
SandboxConfigLoadResult loadSandboxConfig(const std::filesystem::path& file);

/// sandbox.jsonc の "preset" の値だけを書き換える（コメントや他の行はそのまま残す）
/// @return エラーの説明（空なら成功）
std::string saveSelectedSandboxPreset(const std::filesystem::path& file, std::string_view presetName);

}  // namespace moba
