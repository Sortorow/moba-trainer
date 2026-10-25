#pragma once
// 画面に重ねて表示するもの（キャラ名・状態異常・HP・スキルバー・残り時間・ドッジ率・画面外の敵と自キャラの位置・操作説明）
// 日本語を表示するため Dear ImGui の描画機能を使う。rlImGuiBegin / rlImGuiEnd の間で呼ぶ

#include "data/Settings.h"
#include "raylib.h"

namespace moba {

class Match;

void drawHud(const Match& match, const Camera2D& camera, CameraMode cameraMode);

}  // namespace moba
