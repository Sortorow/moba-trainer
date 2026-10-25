#include "ui/ResultScreen.h"

#include <format>
#include <string>

#include "imgui.h"
#include "ui/UiScale.h"

namespace moba {
namespace {

/// 今のウィンドウの横幅の中央に、文字を書く
void textCentered(const std::string& text) {
    const float width = ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - width) * 0.5f);
    ImGui::TextUnformatted(text.c_str());
}

/// プレイのクールダウンの設定の表示（「なし」や「標準（×0.25）」など）
std::string cooldownLabel(const MatchConfig& config) {
    if (config.cooldownMode == CooldownMode::None) return "なし";
    return std::format("標準（×{:.2f}）", config.cooldownScale);
}

}  // namespace

ResultAction drawResultScreen(const MatchResult& result) {
    ResultAction action = ResultAction::None;
    const MatchConfig& config = result.config;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{ui::px(520.0f), 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::Begin("リザルト", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("%s ／ %s ／ %s", std::string(displayName(config.mode)).c_str(), result.characterName.c_str(),
                result.stageName.empty() ? "-" : result.stageName.c_str());
    ImGui::TextDisabled("敵 %d 体 ／ 賢さ %s ／ 制限時間 %.0f 秒 ／ クールダウン %s", config.enemyCount,
                        result.aiLevelName.c_str(), config.timeLimitSeconds, cooldownLabel(config).c_str());
    ImGui::Separator();
    ImGui::Spacing();

    textCentered("スキルドッジ率");
    ImGui::PushFont(nullptr, 52.0f);
    textCentered(result.dodge.total > 0 ? std::format("{:.1f}%", result.dodge.rate() * 100.0f) : std::string("-"));
    ImGui::PopFont();
    ImGui::PushFont(nullptr, 24.0f);
    textCentered(std::format("{} / {}", result.dodge.dodged, result.dodge.total));
    ImGui::PopFont();
    ImGui::Spacing();
    textCentered(std::format("被弾 {} 回　　ダウン {} 回", result.hitsTaken, result.deaths));
    ImGui::Spacing();
    ImGui::TextDisabled("※ 制限時間の終了時に飛んでいる途中だったスキルは、集計に含めません");

    ImGui::Separator();
    if (ImGui::Button("もう一度", ui::px(140.0f, 36.0f))) action = ResultAction::Retry;
    ImGui::SameLine();
    if (ImGui::Button("メニューへ", ui::px(140.0f, 36.0f))) action = ResultAction::Menu;
    ImGui::End();
    return action;
}

}  // namespace moba
