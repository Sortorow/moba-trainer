#include "ui/DebugPanel.h"

#include <format>

#include "core/Geometry.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "imgui.h"
#include "ui/UiScale.h"

namespace moba {
namespace {

/// スキルの命中集計の表（キーごとの発動・命中・命中率と、合計）
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

/// ダミーごとの動き方（静止・往復）と移動速度を、一時的に変える操作（ファイルには保存しない）
void drawDummyControls(Match& match) {
    World& world = match.world();
    for (DummyBrain& brain : match.dummies().brains()) {
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

/// スキルドッジの敵 AI の設定（賢さ・反応・撃つ間隔・予測の重み）と、ドッジの集計を表示する
void drawSkillDodgeInfo(const Match& match) {
    const AiLevel& level = match.enemies().level();
    const DodgeCount& dodge = match.world().dodge();
    ImGui::Text("賢さ %d（%s）  乱数の種 %u", match.config().aiLevel, level.name.c_str(), match.config().seed);
    ImGui::Text("反応 %.2fs  間隔 %.1f〜%.1fs  ぶれ %.0f", level.reactionTime, level.castIntervalMin,
                level.castIntervalMax, level.aimError);
    ImGui::Text("予測: 留まる %.2f / 移動継続 %.2f / 回避 %.2f%s", level.predictStay, level.predictContinue,
                level.predictDodge, level.adaptive ? "（学習あり）" : "");
    ImGui::Text("ドッジ %d / %d（被弾 %d）", dodge.dodged, dodge.total, dodge.hit);
}

/// スキルショットの的の AI の設定（賢さ・反応・避ける確率・パターンの重み）と、的の状態を表示する
void drawSkillShotInfo(const Match& match) {
    const ai::SkillShotTarget& target = match.target();
    const SkillShotLevel& level = target.level();
    ImGui::Text("賢さ %d（%s）  乱数の種 %u", match.config().targetLevel, level.name.c_str(), match.config().seed);
    ImGui::Text("反応 %.2fs  避ける確率 %.0f%%  詠唱を見る %s", level.reactionTime, level.dodgeChance * 100.0f,
                level.readCast ? "あり" : "なし");
    ImGui::Text("パターン 往復 %.2f / 接近 %.2f / ランダム %.2f / 停止 %.2f", level.weightStrafe, level.weightApproach,
                level.weightWander, level.weightStop);
    ImGui::Text("往復 %.0f〜%.0f 回・幅 %.0f〜%.0f  リズムを崩す %.0f%%  遠い側へ避ける %.0f%%", level.strafeSwingsMin,
                level.strafeSwingsMax, level.strafeWidthMin, level.strafeWidthMax, level.rhythmBreak * 100.0f,
                level.sideRandom * 100.0f);
    const std::string pattern = target.patternLabel();
    ImGui::Text("今の動き %s%s", pattern.c_str(), target.dodging() ? "・避けている" : "");
    ImGui::Text("避けた回数 %d", target.dodgeCount());
}

}  // namespace

DebugPanelResult drawDebugPanel(Match& match, GameData& data) {
    DebugPanelResult result;
    World& world = match.world();
    // 練習場ではダミーが自キャラの右側にいるので、パネルは左側に置く
    ImGui::SetNextWindowPos(ui::px(12.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ui::px(340.0f), 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("デバッグ（F1 で表示切替）")) {
        ImGui::End();
        return result;
    }

    ImGui::Text("FPS %d   経過 %.1f 秒", GetFPS(), world.time());
    const std::string logFile = toUtf8(log::currentFilePath().filename());
    ImGui::TextDisabled("ログ: %s", logFile.c_str());

    if (ImGui::CollapsingHeader("キャラクター・設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        const CharacterDef* current = data.character(match.config().characterId);
        if (ImGui::BeginCombo("キャラ", current ? current->displayName.c_str() : "?")) {
            for (const CharacterDef* def : data.characterList()) {
                if (ImGui::Selectable(def->displayName.c_str(), def->id == match.config().characterId)) {
                    result.switchCharacter = def->id;
                }
            }
            ImGui::EndCombo();
        }
        float cooldownScale = world.cooldownScale(Team::Player);
        if (ImGui::SliderFloat("クールダウン倍率", &cooldownScale, 0.0f, 1.0f, "%.2f")) {
            world.setCooldownScale(Team::Player, cooldownScale);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            log::info("ui", "デバッグパネル: 自キャラのクールダウン倍率を {:.2f} に変更", cooldownScale);
        }
        Settings& settings = data.mutableSettings();
        ImGui::SliderFloat("視点の広さ", &settings.cameraViewHeight, 600.0f, 4000.0f, "%.0f");
        ImGui::Checkbox("当たり判定の円を表示", &settings.showHitboxes);
        ImGui::TextDisabled("※ここでの変更は一時的です（設定ファイルを保存すると上書きされます）");
        if (ImGui::Button("設定ファイルを再読み込み (F5)")) result.reloadData = true;
        ImGui::SameLine();
        if (ImGui::Button("クールダウンを解除")) {
            if (Unit* player = world.player()) player->cooldowns = {};
        }
    }

    if (match.config().mode == GameMode::SkillDodge) {
        if (ImGui::CollapsingHeader("スキルドッジ・敵 AI", ImGuiTreeNodeFlags_DefaultOpen)) drawSkillDodgeInfo(match);
    } else if (match.config().mode == GameMode::SkillShot) {
        if (ImGui::CollapsingHeader("スキルショット・的の AI", ImGuiTreeNodeFlags_DefaultOpen))
            drawSkillShotInfo(match);
        if (ImGui::CollapsingHeader("スキルの命中集計", ImGuiTreeNodeFlags_DefaultOpen)) drawStatsTable(world);
    } else {
        if (ImGui::CollapsingHeader("スキルの命中集計", ImGuiTreeNodeFlags_DefaultOpen)) {
            drawStatsTable(world);
            if (ImGui::Button("集計をリセット")) world.stats().reset();
        }
        if (ImGui::CollapsingHeader("ダミー")) drawDummyControls(match);
    }

    if (ImGui::CollapsingHeader("自キャラの状態")) {
        if (const Unit* player = world.player()) {
            ImGui::Text("位置 %s", geo::str(player->pos).c_str());
            ImGui::Text("移動先 %s", player->moveTarget ? geo::str(*player->moveTarget).c_str() : "-");
            ImGui::Text("移動速度 %.0f", player->moveSpeed());
            ImGui::Text("詠唱中 %s",
                        player->cast.active ? std::string(toString(player->cast.request.slot)).c_str() : "-");
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
