#pragma once
// カメラの動かし方（自キャラに固定 / 固定しない）
//
// 固定しない時は、自キャラが動いてもカメラは動かない。カーソルを画面の端に置くと、その方向へカメラが動く
// （カーソルはプレイ中、ウィンドウの外に出ないように閉じ込めている。App.cpp を参照）。
// Space を押している間は、どちらのカメラでも自キャラを画面の中央に映す。

#include "data/Settings.h"
#include "raylib.h"

namespace moba {

struct CameraControlInput {
    Vector2 mouse{};  // カーソルの位置（ウィンドウ内の実ピクセル）
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    float edgeMargin = 0.0f;  // 画面の端からこの幅（実ピクセル）まで近づくとカメラが動く
    // 画面の端でカメラを動かしてよいか（ウィンドウが選ばれていて、カーソルが UI の上に無い時）
    bool edgeScrollEnabled = false;
    bool centerHeld = false;  // Space を押している間
};

// 画面の端によってカメラが動く方向（長さ 1。端に無ければ 0）。角では斜めに動く
Vector2 edgeScrollDirection(const CameraControlInput& input);

// 次のフレームのカメラの注視点（画面の中央に映すワールド座標）。注視点はステージの範囲 bounds の中に収める
Vector2 nextCameraTarget(CameraMode mode, Vector2 current, Vector2 playerPos, const CameraControlInput& input,
                         float scrollSpeed, Rectangle bounds, float dt);

}  // namespace moba
