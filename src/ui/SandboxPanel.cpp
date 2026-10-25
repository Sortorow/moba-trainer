#include "ui/SandboxPanel.h"

#include <algorithm>
#include <cfloat>
#include <format>
#include <string>

#include "data/GameData.h"
#include "imgui.h"
#include "ui/UiScale.h"

namespace moba {
namespace {

// 例: 「3 体（静止 2・往復 1）半径 65」
std::string summary(const SandboxPreset& preset) {
    if (preset.dummies.empty()) return "ダミーなし";
    int patrol = 0;
    float minRadius = preset.dummies.front().radius;
    float maxRadius = minRadius;
    for (const DummyDef& dummy : preset.dummies) {
        if (dummy.patrolTo) ++patrol;
        minRadius = std::min(minRadius, dummy.radius);
        maxRadius = std::max(maxRadius, dummy.radius);
    }
    const int stationary = static_cast<int>(preset.dummies.size()) - patrol;
    const std::string radius = minRadius == maxRadius ? std::format("半径 {:.0f}", minRadius)
                                                      : std::format("半径 {:.0f}〜{:.0f}", minRadius, maxRadius);
    return std::format("{} 体（静止 {}・往復 {}）{}", preset.dummies.size(), stationary, patrol, radius);
}

}  // namespace

void drawSandboxPresetSelector(GameData& data, bool showLabel) {
    const SandboxConfig& sandbox = data.sandbox();
    const SandboxPreset* current = sandbox.current();
    if (!current) {
        ImGui::TextDisabled("sandbox.jsonc にプリセットがありません");
        return;
    }
    if (!showLabel) ImGui::SetNextItemWidth(-FLT_MIN);  // 見出しを出さない時は、横幅いっぱいに広げる
    if (ImGui::BeginCombo(showLabel ? "ダミー" : "##sandboxPreset", current->name.c_str())) {
        for (const SandboxPreset& preset : sandbox.presets) {
            if (ImGui::Selectable(preset.name.c_str(), &preset == current) && &preset != current) {
                data.selectSandboxPreset(preset.name);
                current = data.sandbox().current();  // 選択が変わったので取り直す
                break;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("%s", summary(*current).c_str());
    if (!current->description.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", current->description.c_str());
        ImGui::PopTextWrapPos();
    }
}

void drawSandboxPanel(GameData& data) {
    // ダミーは自キャラの右側（画面の右上〜中央）に置くことが多いので、空いている右下に出す。
    // 小さいウィンドウでもスキルバー（画面下の中央。高さは Hud.cpp の drawSkillBar）に重ならないよう、その上にそろえる
    constexpr float kSkillBarHeight = 58.0f + 44.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 corner{viewport->WorkPos.x + viewport->WorkSize.x - ui::px(12.0f),
                        viewport->WorkPos.y + viewport->WorkSize.y - ui::px(kSkillBarHeight + 12.0f)};
    ImGui::SetNextWindowPos(corner, ImGuiCond_Always, ImVec2{1.0f, 1.0f});
    ImGui::SetNextWindowSize(ImVec2{ui::px(260.0f), 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("練習場のダミー", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoCollapse);
    drawSandboxPresetSelector(data, false);
    ImGui::End();
}

}  // namespace moba
