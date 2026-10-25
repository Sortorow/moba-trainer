#include "ui/PauseDialog.h"

#include "imgui.h"
#include "ui/UiScale.h"

namespace moba {

PauseAction drawPauseDialog() {
    PauseAction action = PauseAction::None;

    // ゲーム画面を少し暗くして、ポーズ中だと分かるようにする
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        viewport->Pos, ImVec2{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
        IM_COL32(0, 0, 0, 120));

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{ui::px(440.0f), 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::Begin("ポーズ", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::Spacing();
    ImGui::TextUnformatted("ゲームを中断して、メニューに戻りますか？");
    ImGui::Spacing();
    ImGui::TextDisabled("「いいえ」か Esc で、プレイに戻ります");
    ImGui::TextDisabled("「リスタート」は、同じ設定で最初からやり直します（結果も記録し直します）");
    ImGui::Separator();
    if (ImGui::Button("はい", ui::px(120.0f, 36.0f))) action = PauseAction::Menu;
    ImGui::SameLine();
    if (ImGui::Button("いいえ", ui::px(120.0f, 36.0f))) action = PauseAction::Resume;
    ImGui::SameLine();
    if (ImGui::Button("リスタート", ui::px(140.0f, 36.0f))) action = PauseAction::Restart;
    ImGui::End();
    return action;
}

}  // namespace moba
