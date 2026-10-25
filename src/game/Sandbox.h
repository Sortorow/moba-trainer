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

// 自キャラを作る（ステージの中央 (0, 0) に置く）
Unit makePlayerUnit(const CharacterDef& character);

// 自キャラと、プリセットのダミーを置く。preset が nullptr ならダミーは置かない
void setupSandbox(World& world, DummyController& dummies, const CharacterDef& character, const SandboxPreset* preset);

// 今のダミーを取り除き、プリセットのダミーを置き直す（プレイの途中でプリセットを切り替えた時）
void placeDummies(World& world, DummyController& dummies, const SandboxPreset* preset);

}  // namespace moba
