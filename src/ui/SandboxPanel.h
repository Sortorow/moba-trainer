#pragma once
// 練習場のダミーのプリセットを選ぶ UI（メニューと、練習場の画面右下のパネル）
// 選ぶとすぐに sandbox.jsonc の "preset" に保存する（ダミーの置き直しは Match::refreshDummiesIfChanged）

namespace moba {

class GameData;

/// プリセットを選ぶコンボボックスと、選んでいるプリセットの説明。showLabel が false なら「ダミー」の見出しを出さない
void drawSandboxPresetSelector(GameData& data, bool showLabel = true);

/// 練習場の画面右下に出すパネル
void drawSandboxPanel(GameData& data);

}  // namespace moba
