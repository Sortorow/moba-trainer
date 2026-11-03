#pragma once
// ゲーム画面（ワールド座標の図形）の描画
// 文字は日本語を出すため、Hud.cpp で Dear ImGui を使って描く

#include "raylib.h"

namespace moba {

class World;

/// ゲーム画面の描き方の設定
struct RenderOptions {
    bool showHitboxes = true;  // 弾・範囲の当たり判定の円を表示する
    float fogDarkness = 0.6f;  // 視界の外を暗くする強さ（0〜1）
};

/// 相手の陣営の物（範囲・設置物・弾・演出）を、自キャラの視界の中だけ描くための描画先（画面と同じ大きさの 2 枚）。
/// ウィンドウを開いてから使い、ウィンドウを閉じる前に release() する（閉じた後は解放できないため、デストラクタでは解放しない）
class VisionLayers {
public:
    VisionLayers() = default;
    VisionLayers(const VisionLayers&) = delete;
    VisionLayers& operator=(const VisionLayers&) = delete;

    /// width × height の描画先を用意する（大きさが変わっていれば作り直す）
    void ensureSize(int width, int height);
    /// 描画先を解放する
    void release();
    /// キャラより下に重ねる層（範囲・設置物）
    const RenderTexture2D& below() const { return below_; }
    /// ブッシュより上に重ねる層（弾・演出）
    const RenderTexture2D& above() const { return above_; }

private:
    RenderTexture2D below_{};
    RenderTexture2D above_{};
    int width_ = 0;
    int height_ = 0;
};

/// ゲーム画面を描く（BeginDrawing の中、BeginMode2D の外で呼ぶ。中で camera を使って BeginMode2D する）。
/// 床 → 障害物 → 範囲 → 設置物 → キャラ → ブッシュ → 弾 → 演出 → 視界の外の暗さ の順。
/// 自キャラの陣営から見えない敵は描かない。相手の陣営の範囲・設置物・弾・演出は、視界の中の部分だけ描く
void drawWorld(const World& world, const Camera2D& camera, const RenderOptions& options, VisionLayers& layers);

}  // namespace moba
