#pragma once
// 画面に重ねて表示するもの（キャラ名・状態異常・HP・スキルバー・残り時間・ドッジ率・画面外の敵と自キャラの位置・操作説明）
// 日本語を表示するため Dear ImGui の描画機能を使う。rlImGuiBegin / rlImGuiEnd の間で呼ぶ

#include "data/Settings.h"
#include "raylib.h"

namespace moba {

class Match;

/// ゲーム画面に重ねる表示をすべて描く（キャラ名・HP・状態異常・画面外の矢印・スキルバー・詠唱バー・
/// スキルドッジの残り時間とドッジ率・操作説明）
/// @param camera ワールド座標を画面の位置に直すためのカメラ
/// @param cameraMode 操作説明に出す今のカメラ
void drawHud(const Match& match, const Camera2D& camera, CameraMode cameraMode);

}  // namespace moba
