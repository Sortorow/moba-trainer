#pragma once
// ウィンドウの大きさ（解像度）の選択肢と、画面に収まるかの判定
//
// 解像度は実際のピクセル数（Windows の拡大率は掛けない）。ウィンドウの縁をドラッグして大きさを変えることはできず、
// メニューの「画面」で一覧から選ぶ。全画面（Alt + Enter）の間は、モニターの解像度で描く

#include <vector>

namespace moba::ui {

/// ウィンドウの中の大きさ（実際のピクセル数。タイトルバーと枠は含まない）
struct WindowSize {
    int width = 0;
    int height = 0;
    bool operator==(const WindowSize&) const = default;
};

/// メニューで選べる解像度（16:9 と 16:10 の代表的なものと、ユーザーの指定した 2160x1440（3:2）・2400x1350（16:9）。
/// ピクセル数の少ない順）
const std::vector<WindowSize>& resolutionList();

/// 中の大きさ size のウィンドウが、モニターの作業領域（タスクバーを除いた範囲）area に収まるか。
/// 作業領域が分からない時（area の幅か高さが 0）は、収まるとみなす
/// @param frame タイトルバーと枠の分、ウィンドウ全体が中の大きさより大きくなる量（幅・高さ）
bool fitsInArea(WindowSize size, WindowSize frame, WindowSize area);

/// 起動時のウィンドウの大きさ。wanted が収まればそのまま使う。収まらなければ、一覧の中で収まる最も大きいもの
/// （wanted と同じ縦横比のものを優先する）。どれも収まらなければ、一覧の最も小さいもの
WindowSize startupWindowSize(WindowSize wanted, WindowSize frame, WindowSize area);

}  // namespace moba::ui
