#pragma once
// ステージの準備: 練習場（障害物のないステージに、自キャラと操作確認用のダミーを置く）

#include "game/Unit.h"
#include "raylib.h"

namespace moba {

class World;
class DummyController;
struct CharacterDef;
struct SandboxPreset;

// ステージの広さ（ゲーム内単位）
inline constexpr Rectangle kSandboxBounds{-2500.0f, -1600.0f, 5000.0f, 3200.0f};

/// 自キャラを作る（キャラの設定から大きさ・移動速度・色を写し、ステージの中央 (0, 0) に置く。World にはまだ加えない）
Unit makePlayerUnit(const CharacterDef& character);

/// 練習場を準備する: 自キャラと、プリセットのダミーを置く。preset が nullptr ならダミーは置かない
void setupSandbox(World& world, DummyController& dummies, const CharacterDef& character, const SandboxPreset* preset);

/// 今のダミーを取り除き、プリセットのダミーを置き直す（プレイの途中でプリセットを切り替えた時）。
/// 建物などと重なる位置は、近くの立てる位置へずらす
void placeDummies(World& world, DummyController& dummies, const SandboxPreset* preset);

}  // namespace moba
