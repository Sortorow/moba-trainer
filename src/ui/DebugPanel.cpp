#include "ui/DebugPanel.h"

#include <format>

#include "core/Geometry.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/DummyController.h"
#include "game/World.h"
#include "imgui.h"

namespace moba {
namespace {

void drawStatsTable(const World& world) {
    if (!ImGui::BeginTable("stats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn("キー");
    ImGui::TableSetupColumn("発動");
    ImGui::TableSetupColumn("命中");
    ImGui::TableSetupColumn("命中率");
    ImGui::TableHeadersRow();
    auto row = [](std::string_view label, const HitCount& count) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::TableNextColumn();
        ImGui::Text("%d", count.casts);
        ImGui::TableNextColumn();
        ImGui::Text("%d", count.hits);
        ImGui::TableNextColumn();
        if (count.casts > 0) ImGui::Text("%.1f%%", count.rate() * 100.0f);
        else ImGui::TextUnformatted("-");
    };
    for (SkillSlot slot : kAllSkillSlots) {
        if (world.stats().slot(slot).casts > 0) row(toString(slot), world.stats().slot(slot));
    }
    row("合計", world.stats().total());
    ImGui::EndTable();
}

}  // namespace

DebugPanelResult drawDebugPanel(World& world, GameData& data, DummyController& dummies, std::string_view characterId) {
    DebugPanelResult result;
    // 練習場ではダミーが自キャラの右側にいるので、パネルは左側に置く
    ImGui::SetNextWindowPos(ImVec2{12.0f, 40.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{340.0f, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("デバッグ（F1 で表示切替）")) {
        ImGui::End();
        return result;
    }

    ImGui::Text("FPS %d   経過 %.1f 秒", GetFPS(), world.time());
    const std::string logFile = toUtf8(log::currentFilePath().filename());
    ImGui::TextDisabled("ログ: %s", logFile.c_str());

    if (ImGui::CollapsingHeader("キャラクター・設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        const CharacterDef* current = data.character(characterId);
        if (ImGui::BeginCombo("キャラ", current ? current->displayName.c_str() : "?")) {
            for (const CharacterDef* def : data.characterList()) {
                if (ImGui::Selectable(def->displayName.c_str(), def->id == characterId)) result.switchCharacter = def->id;
            }
            ImGui::EndCombo();
        }
        Settings& settings = data.mutableSettings();
        ImGui::SliderFloat("クールダウン倍率", &settings.cooldownScale, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("視点の広さ", &settings.cameraViewHeight, 600.0f, 4000.0f, "%.0f");
        ImGui::Checkbox("当たり判定の円を表示", &settings.showHitboxes);
        ImGui::TextDisabled("※ここでの変更は一時的です（設定ファイルを保存すると上書きされます）");
        if (ImGui::Button("設定ファイルを再読み込み (F5)")) result.reloadData = true;
        ImGui::SameLine();
        if (ImGui::Button("クールダウンを解除")) {
            if (Unit* player = world.player()) player->cooldowns = {};
        }
    }

    if (ImGui::CollapsingHeader("スキルの命中集計", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawStatsTable(world);
        if (ImGui::Button("集計をリセット")) world.stats().reset();
    }

    if (ImGui::CollapsingHeader("ダミー")) {
        for (DummyBrain& brain : dummies.brains()) {
            Unit* unit = world.findUnit(brain.unitId);
            if (!unit) continue;
            ImGui::PushID(static_cast<int>(brain.unitId));
            ImGui::TextUnformatted(unit->name.c_str());
            int behavior = static_cast<int>(brain.behavior);
            if (ImGui::Combo("動き", &behavior, "静止\0往復\0")) {
                brain.behavior = static_cast<DummyBehavior>(behavior);
                unit->moveTarget.reset();
                if (brain.behavior == DummyBehavior::Patrol && Vector2Distance(brain.home, brain.patrolTo) < 1.0f) {
                    brain.patrolTo = brain.home + Vector2{0.0f, 800.0f};  // 往復先が無ければ下方向に作る
                }
            }
            ImGui::SliderFloat("移動速度", &unit->baseMoveSpeed, 0.0f, 600.0f, "%.0f");
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("自キャラの状態")) {
        if (const Unit* player = world.player()) {
            ImGui::Text("位置 %s", geo::str(player->pos).c_str());
            ImGui::Text("移動先 %s", player->moveTarget ? geo::str(*player->moveTarget).c_str() : "-");
            ImGui::Text("移動速度 %.0f", player->moveSpeed());
            ImGui::Text("詠唱中 %s", player->cast.active ? std::string(toString(player->cast.request.slot)).c_str() : "-");
        }
    }

    if (!data.errors().empty() && ImGui::CollapsingHeader("設定ファイルのエラー", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const std::string& error : data.errors()) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.5f, 1.0f}, "%s", error.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
    return result;
}

}  // namespace moba
