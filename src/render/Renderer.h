#pragma once
// ゲーム画面（ワールド座標の図形）の描画。BeginMode2D の中で呼ぶ
// 文字は日本語を出すため、Hud.cpp で Dear ImGui を使って描く

namespace moba {

class World;

/// ゲーム画面の描き方の設定
struct RenderOptions {
    bool showHitboxes = true;  // 弾・範囲の当たり判定の円を表示する
};

/// ゲーム画面を描く（床 → 障害物 → 範囲 → 設置物 → キャラ → ブッシュ → 弾 → 演出の順）。
/// ブッシュに隠れていて自キャラの陣営から見えない敵は描かない
void drawWorld(const World& world, const RenderOptions& options);

}  // namespace moba
