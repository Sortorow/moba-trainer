#pragma once
// Dear ImGui で日本語を表示するためのフォント読み込み

struct ImFont;

namespace moba::ui {

// Windows に入っている日本語フォント（メイリオ → 游ゴシック → MS ゴシック の順）を読み込み、既定のフォントにする。
// 見つからなければ nullptr（英数字だけ表示できる既定フォントのまま）
ImFont* loadJapaneseFont(float size);

}  // namespace moba::ui
