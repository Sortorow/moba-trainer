#pragma once
// 画面に出す文字・ボタン・余白の大きさの倍率（Windows の拡大率 × 設定ファイルの uiScale）
//
// このゲームは Windows に「拡大は自分で行う（DPI 対応）」と伝えている（描画ライブラリ GLFW の既定の動作）。
// そのため Windows はこのゲームの画面を拡大しない。代わりに、画面に出すピクセル単位の大きさには
// すべて px() で倍率を掛けてから描く。詳しい背景は docs/dpi-scaling.md を参照。
//
// 注意:
//   ・ImGui::PushFont(nullptr, size) の size には px() を掛けない（Dear ImGui が自動で倍率を掛ける）
//   ・ImDrawList::AddText(font, size, ...) の size や、座標・幅などのピクセル値には px() を掛ける
//   ・ゲームの世界（キャラ・弾など）はカメラがウィンドウの大きさに合わせて拡大するので、px() は不要

#include "imgui.h"

namespace moba::ui {

/// 今の倍率（Windows の拡大率 × 設定ファイルの uiScale）
float scale();
/// ピクセル単位の大きさに倍率を掛ける（拡大率 100% の時の大きさを渡す）
inline float px(float size) { return size * scale(); }
/// ピクセル単位の大きさ（幅・高さ）に倍率を掛ける
inline ImVec2 px(float x, float y) { return ImVec2{x * scale(), y * scale()}; }

/// 倍率を変え、Dear ImGui の文字と余白にも反映する（最初に呼んだ時の見た目を、倍率 1.0 の基準として覚える）
void setScale(float scale);

struct WindowSize {
    int width = 0;
    int height = 0;
};

/// 拡大率 100% の時のウィンドウの大きさ（logical）に拡大率を掛け、
/// 作業領域の maxFraction 倍に収まるように縮める（縦横比は保つ）
WindowSize fitWindowSize(int logicalWidth, int logicalHeight, float dpiScale, int areaWidth, int areaHeight,
                         float maxFraction = 0.85f);

}  // namespace moba::ui
