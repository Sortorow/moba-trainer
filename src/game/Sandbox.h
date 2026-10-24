#pragma once
// 練習場: 障害物のないステージに、自キャラと操作確認用のダミーを置く

#include "raylib.h"

namespace moba {

class World;
class DummyController;
struct CharacterDef;

// ステージの広さ（ゲーム内単位）
inline constexpr Rectangle kSandboxBounds{-2500.0f, -1600.0f, 5000.0f, 3200.0f};

void setupSandbox(World& world, DummyController& dummies, const CharacterDef& character);

}  // namespace moba
