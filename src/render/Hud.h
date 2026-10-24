#pragma once
// 画面に重ねて表示するもの（キャラ名・状態異常・スキルバー・操作説明）
// 日本語を表示するため Dear ImGui の描画機能を使う。rlImGuiBegin / rlImGuiEnd の間で呼ぶ

#include "raylib.h"

namespace moba {

class World;

void drawHud(const World& world, const Camera2D& camera);

}  // namespace moba
