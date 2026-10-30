#pragma once
// 画面下中央のスキルバーの大きさ。render/Hud.cpp がスキルバーを描き、ui/SandboxPanel.cpp がその上にパネルをそろえる。
// どれも拡大率 100% の時のピクセルなので、使う時に ui::px() を掛ける

namespace moba::ui {

inline constexpr float kSkillBarBoxSize = 58.0f;       // キー 1 つ分の四角の大きさ
inline constexpr float kSkillBarBottomMargin = 44.0f;  // 四角の下端から画面の下端まで（スキル名を出す分）
/// 画面の下端からスキルバーの上端までの高さ
inline constexpr float kSkillBarHeight = kSkillBarBoxSize + kSkillBarBottomMargin;

}  // namespace moba::ui
