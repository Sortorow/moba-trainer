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

/// 「52.0%」のような割合の大きな表示と、その下の「13 / 25」の表示
void drawRate(const char* title, float rate, int count, int total) {
    textCentered(title);
    ImGui::PushFont(nullptr, 52.0f);
    textCentered(total > 0 ? std::format("{:.1f}%", rate * 100.0f) : std::string("-"));
    ImGui::PopFont();
    ImGui::PushFont(nullptr, 24.0f);
    textCentered(std::format("{} / {}", count, total));
    ImGui::PopFont();
}

/// スキルドッジの結果（ドッジ率・被弾・ダウン）
void drawDodgeResult(const MatchResult& result) {
    drawRate("スキルドッジ率", result.dodge.rate(), result.dodge.dodged, result.dodge.total);
    ImGui::Spacing();
    textCentered(std::format("被弾 {} 回　　ダウン {} 回", result.hitsTaken, result.deaths));
}

/// スキルショットの結果（ヒット率と、キーごとの命中）
void drawShotResult(const MatchResult& result) {
    drawRate("スキルヒット率", result.playerHits.rate(), result.playerHits.hits, result.playerHits.casts);
    ImGui::Spacing();
    std::string bySlot;
    for (const SkillSlot slot : kAllSkillSlots) {
        const HitCount& count = result.playerHitsBySlot[static_cast<std::size_t>(index(slot))];
        if (count.casts <= 0) continue;
        bySlot += std::format("{}{} {:.0f}%（{} / {}）", bySlot.empty() ? "" : "　", toString(slot),
                              count.rate() * 100.0f, count.hits, count.casts);
    }
    textCentered(bySlot.empty() ? std::string("ヒット率に数えるスキルを使っていません") : bySlot);
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
    if (config.mode == GameMode::SkillShot) {
        ImGui::TextDisabled("的の賢さ %s ／ 制限時間 %.0f 秒 ／ クールダウン %s", result.aiLevelName.c_str(),
                            config.timeLimitSeconds, cooldownLabel(config).c_str());
    } else {
        ImGui::TextDisabled("敵 %d 体 ／ 賢さ %s ／ 制限時間 %.0f 秒 ／ クールダウン %s", config.enemyCount,
                            result.aiLevelName.c_str(), config.timeLimitSeconds, cooldownLabel(config).c_str());
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (config.mode == GameMode::SkillShot) drawShotResult(result);
    else drawDodgeResult(result);
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
