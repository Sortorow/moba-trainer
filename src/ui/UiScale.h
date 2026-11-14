#pragma once
// 画面に出す文字・ボタン・余白の大きさの倍率（ウィンドウの高さ ÷ 900 × 設定ファイルの uiScale）
//
// このゲームは Windows に「拡大は自分で行う（DPI 対応）」と伝えている（描画ライブラリ GLFW の既定の動作）。
// そのため Windows はこのゲームの画面を拡大しない。代わりに、画面に出すピクセル単位の大きさには
// すべて px() で倍率を掛けてから描く。倍率はウィンドウの高さに合わせて決めるので（LoL・ER などのゲームと同じ）、
// 解像度を変えても、画面に対する文字・ボタンの大きさの割合は変わらない。Windows の拡大率は使わない。
// 詳しい背景は docs/dpi-scaling.md を参照。
//
// 注意:
//   ・ImGui::PushFont(nullptr, size) の size には px() を掛けない（Dear ImGui が自動で倍率を掛ける）
//   ・ImDrawList::AddText(font, size, ...) の size や、座標・幅などのピクセル値には px() を掛ける
//   ・ゲームの世界（キャラ・弾など）はカメラがウィンドウの大きさに合わせて拡大するので、px() は不要

#include "imgui.h"

namespace moba::ui {

/// 倍率が 1.0 になるウィンドウの高さ（実際のピクセル数）。px() に渡す大きさは、この高さの時の大きさで書く
constexpr float kBaseWindowHeight = 900.0f;

/// 今の倍率（ウィンドウの高さ ÷ 900 × 設定ファイルの uiScale）
float scale();
/// ピクセル単位の大きさに倍率を掛ける（ウィンドウの高さが 900 の時の大きさを渡す）
inline float px(float size) { return size * scale(); }
/// ピクセル単位の大きさ（幅・高さ）に倍率を掛ける
inline ImVec2 px(float x, float y) { return ImVec2{x * scale(), y * scale()}; }

/// 倍率を変え、Dear ImGui の文字と余白にも反映する（最初に呼んだ時の見た目を、倍率 1.0 の基準として覚える）
void setScale(float scale);

/// ウィンドウの高さに合わせた倍率（高さ ÷ 900 × uiScale）。高さが 0 以下なら uiScale をそのまま返す
float scaleForWindowHeight(int windowHeight, float uiScale);

}  // namespace moba::ui
