#include "ui/MainMenu.h"

#include <format>

#include "data/GameData.h"
#include "imgui.h"

namespace moba {

MenuAction MainMenu::draw(const GameData& data) {
    MenuAction action;
    const std::vector<const CharacterDef*> characters = data.characterList();
    if (data.character(selectedCharacter_) == nullptr && !characters.empty()) selectedCharacter_ = characters.front()->id;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{560.0f, 0.0f}, ImGuiCond_Always);
    ImGui::Begin("MOBA Trainer", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);

    ImGui::SeparatorText("キャラクター");
    for (const CharacterDef* def : characters) {
        const std::string label = std::format("{}（{} のキャラ風）", def->displayName, def->origin);
        if (ImGui::Selectable(label.c_str(), def->id == selectedCharacter_)) selectedCharacter_ = def->id;
    }
    if (characters.empty()) ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.5f, 1.0f}, "キャラクターの設定ファイルがありません");

    ImGui::SeparatorText("モード");
    ImGui::RadioButton("練習場（ダミー相手にスキルを試す）", true);
    ImGui::BeginDisabled();
    ImGui::RadioButton("スキルドッジ（準備中）", false);
    ImGui::RadioButton("スキルショット（準備中）", false);
    ImGui::EndDisabled();

    ImGui::SeparatorText("ステージ");
    ImGui::RadioButton("障害物なし", true);
    ImGui::BeginDisabled();
    ImGui::RadioButton("路地裏（準備中）", false);
    ImGui::RadioButton("森（準備中）", false);
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginDisabled(characters.empty());
    if (ImGui::Button("開始", ImVec2{140.0f, 36.0f})) {
        action.kind = MenuAction::Kind::Start;
        action.characterId = selectedCharacter_;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("終了", ImVec2{100.0f, 36.0f})) action.kind = MenuAction::Kind::Quit;

    if (!data.errors().empty()) {
        ImGui::SeparatorText("設定ファイルのエラー");
        for (const std::string& error : data.errors()) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.5f, 1.0f}, "%s", error.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
    return action;
}

}  // namespace moba
