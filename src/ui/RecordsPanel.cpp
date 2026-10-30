#include "ui/RecordsPanel.h"

#include <algorithm>

#include "imgui.h"
#include "ui/RecordFormat.h"
#include "ui/UiScale.h"

namespace moba {

int rankOf(const std::vector<SavedResult>& recent, const std::string& playedAt) {
    std::vector<SavedResult> ranked = recent;
    std::stable_sort(ranked.begin(), ranked.end(), rankedHigher);
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        if (ranked[i].playedAt == playedAt) return static_cast<int>(i) + 1;
    }
    return 0;
}

void drawRecordTable(GameMode mode, const std::vector<SavedResult>& recent, const std::string* highlightPlayedAt,
                     float maxHeight) {
    if (recent.empty()) {
        ImGui::TextDisabled("まだ記録がありません（制限時間まで遊ぶと記録されます）");
        return;
    }
    std::vector<SavedResult> ranked = recent;
    std::stable_sort(ranked.begin(), ranked.end(), rankedHigher);

    const char* scoreTitle = mode == GameMode::SkillDodge ? "ドッジ率" : "ヒット率";
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    // 行がすべて入らない時は、見出しを固定してスクロールする
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().CellPadding.y * 2.0f;
    const float fullHeight = rowHeight * static_cast<float>(ranked.size() + 1);
    ImVec2 outerSize{0.0f, 0.0f};
    if (maxHeight > 0.0f && fullHeight > maxHeight) {
        flags |= ImGuiTableFlags_ScrollY;
        outerSize.y = maxHeight;
    }
    if (!ImGui::BeginTable("records", 8, flags, outerSize)) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("順位");
    ImGui::TableSetupColumn(scoreTitle);
    ImGui::TableSetupColumn("内訳");
    ImGui::TableSetupColumn("キャラ");
    ImGui::TableSetupColumn("ステージ");
    ImGui::TableSetupColumn(mode == GameMode::SkillDodge ? "敵" : "的（賢さ・大きさ・速さ）");
    ImGui::TableSetupColumn("CD");
    ImGui::TableSetupColumn("日時");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < ranked.size(); ++i) {
        const SavedResult& saved = ranked[i];
        const MatchResult& r = saved.result;
        const ResultScore score = scoreOf(r);
        const int rank = static_cast<int>(i) + 1;
        const bool highlight = highlightPlayedAt && saved.playedAt == *highlightPlayedAt;

        ImGui::TableNextRow();
        if (highlight) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(60, 110, 170, 140));
        ImGui::TableNextColumn();
        ImGui::TextColored(ui::rankColor(rank), "%d 位", rank);
        ImGui::TableNextColumn();
        ImGui::TextColored(ui::rankColor(rank), "%s", ui::percentText(score).c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d / %d", score.count, score.total);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.characterName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.stageName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ui::difficultyLabel(r).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ui::cooldownLabel(r.config, ui::CooldownLabelStyle::Short).c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s%s", ui::shortTime(saved.playedAt).c_str(), highlight ? "  今回" : "");
    }
    ImGui::EndTable();
}

bool drawRecordsWindow(const ResultStore& store) {
    bool close = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{ui::px(860.0f), 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::Begin("記録（直近 10 回）", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextDisabled("モードごとに、直近 10 回のプレイを成績の順に並べます（同じ割合なら、総数が多い順）");
    if (ImGui::BeginTabBar("modes")) {
        for (const GameMode mode : {GameMode::SkillDodge, GameMode::SkillShot}) {
            if (!ImGui::BeginTabItem(std::string(displayName(mode)).c_str())) continue;
            const std::vector<SavedResult> recent = store.recent(mode, kRecentResultCount);
            drawRecordTable(mode, recent, recent.empty() ? nullptr : &recent.front().playedAt, viewport->Size.y * 0.6f);
            if (!recent.empty()) ImGui::TextDisabled("強調している行が、最も新しいプレイです");
            ImGui::TextDisabled("保存している記録: %zu 件（1 日 %zu 件まで・%d 日分）", store.all(mode).size(),
                                kMaxResultsPerDay, kResultKeepDays);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    for (const std::string& error : store.errors()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.5f, 1.0f}, "%s", error.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::Separator();
    if (ImGui::Button("閉じる", ui::px(120.0f, 36.0f))) close = true;
    ImGui::End();
    return close;
}

}  // namespace moba
