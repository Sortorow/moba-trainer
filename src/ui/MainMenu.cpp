#include "ui/MainMenu.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "data/GameData.h"
#include "imgui.h"
#include "ui/UiScale.h"

namespace moba {
namespace {

struct ScalePreset {
    float value;
    const char* label;
};

constexpr ScalePreset kCooldownPresets[] = {
    {1.0f, "実戦どおり（×1）"},
    {0.5f, "2 分の 1（×0.5）"},
    {0.25f, "4 分の 1（×0.25）"},
};

bool nearlyEqual(float a, float b) { return std::fabs(a - b) < 0.001f; }

// 標準モードの倍率を選ぶ。設定ファイルの値がプリセットに無ければ、それも選択肢に加える
void drawCooldownScaleCombo(float& scale, float settingsScale) {
    std::string current = std::format("×{:.2f}", scale);
    for (const ScalePreset& preset : kCooldownPresets) {
        if (nearlyEqual(preset.value, scale)) current = preset.label;
    }
    if (!ImGui::BeginCombo("倍率", current.c_str())) return;
    for (const ScalePreset& preset : kCooldownPresets) {
        if (ImGui::Selectable(preset.label, nearlyEqual(preset.value, scale))) scale = preset.value;
    }
    bool settingsIsPreset = false;
    for (const ScalePreset& preset : kCooldownPresets)
        settingsIsPreset = settingsIsPreset || nearlyEqual(preset.value, settingsScale);
    if (!settingsIsPreset) {
        const std::string label = std::format("設定ファイルの値（×{:.2f}）", settingsScale);
        if (ImGui::Selectable(label.c_str(), nearlyEqual(settingsScale, scale))) scale = settingsScale;
    }
    ImGui::EndCombo();
}

void drawSkillDodgeOptions(MatchConfig& config, const GameData& data) {
    ImGui::SeparatorText("スキルドッジの設定");
    ImGui::SliderInt("敵の数", &config.enemyCount, 1, 5, "%d 体");
    const AiConfig& ai = data.ai();
    const std::string levelLabel = std::format("%d（{}）", ai.level(config.aiLevel).name);
    ImGui::SliderInt("敵の賢さ", &config.aiLevel, 1, std::max(1, static_cast<int>(ai.levels.size())),
                     levelLabel.c_str());
    int minutes = std::clamp(static_cast<int>(std::lround(config.timeLimitSeconds / 60.0f)), 1, 5);
    ImGui::SliderInt("制限時間", &minutes, 1, 5, "%d 分");
    config.timeLimitSeconds = static_cast<float>(minutes) * 60.0f;

    std::string enemies;
    for (const CharacterDef* def : data.enemyCandidates()) enemies += (enemies.empty() ? "" : "、") + def->displayName;
    ImGui::TextDisabled("登場する敵: %s（ランダム）", enemies.empty() ? "なし" : enemies.c_str());
    ImGui::TextDisabled("自キャラはすべてのスキルを使えます（敵に当たってもダメージはありません）");
}

}  // namespace

void MainMenu::setConfig(const MatchConfig& config) {
    config_ = config;
    config_.seed = 0;  // 次のプレイでは新しい乱数の種を使う
    initialized_ = true;
}

MenuAction MainMenu::draw(const GameData& data) {
    if (!initialized_) {
        config_.cooldownScale = data.settings().cooldownScale;
        initialized_ = true;
    }
    MenuAction action;
    const std::vector<const CharacterDef*> characters = data.characterList();
    if (data.character(config_.characterId) == nullptr && !characters.empty())
        config_.characterId = characters.front()->id;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{ui::px(580.0f), 0.0f}, ImGuiCond_Always);
    ImGui::Begin("MOBA Trainer", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);

    ImGui::SeparatorText("キャラクター");
    for (const CharacterDef* def : characters) {
        const std::string label = std::format("{}（{} のキャラ風）", def->displayName, def->origin);
        if (ImGui::Selectable(label.c_str(), def->id == config_.characterId)) config_.characterId = def->id;
    }
    if (characters.empty())
        ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.5f, 1.0f}, "キャラクターの設定ファイルがありません");

    ImGui::SeparatorText("モード");
    if (ImGui::RadioButton("練習場（ダミー相手にスキルを試す）", config_.mode == GameMode::Sandbox)) {
        config_.mode = GameMode::Sandbox;
    }
    if (ImGui::RadioButton("スキルドッジ（敵のスキルを避ける）", config_.mode == GameMode::SkillDodge)) {
        config_.mode = GameMode::SkillDodge;
    }
    ImGui::BeginDisabled();
    ImGui::RadioButton("スキルショット（準備中）", false);
    ImGui::EndDisabled();

    ImGui::SeparatorText("ステージ");
    ImGui::RadioButton("障害物なし", true);
    ImGui::BeginDisabled();
    ImGui::RadioButton("路地裏（準備中）", false);
    ImGui::RadioButton("森（準備中）", false);
    ImGui::EndDisabled();

    if (config_.mode == GameMode::SkillDodge) drawSkillDodgeOptions(config_, data);

    ImGui::SeparatorText("スキルのクールダウン");
    if (ImGui::RadioButton("標準", config_.cooldownMode == CooldownMode::Standard)) {
        config_.cooldownMode = CooldownMode::Standard;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("なし（すぐに使える）", config_.cooldownMode == CooldownMode::None)) {
        config_.cooldownMode = CooldownMode::None;
    }
    ImGui::BeginDisabled(config_.cooldownMode == CooldownMode::None);
    drawCooldownScaleCombo(config_.cooldownScale, data.settings().cooldownScale);
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginDisabled(characters.empty());
    if (ImGui::Button("開始", ui::px(140.0f, 36.0f))) {
        action.kind = MenuAction::Kind::Start;
        action.config = config_;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("終了", ui::px(100.0f, 36.0f))) action.kind = MenuAction::Kind::Quit;

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
