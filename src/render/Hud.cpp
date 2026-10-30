#include "render/Hud.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <format>
#include <string>

#include "core/Geometry.h"
#include "data/CharacterDef.h"
#include "game/Match.h"
#include "imgui.h"
#include "ui/HudLayout.h"
#include "ui/UiScale.h"

// 画面に出すピクセル単位の大きさ（文字の大きさ・枠・余白など）は、すべて ui::px() で倍率を掛ける。
// キャラ名などの表示位置のうち、ゲームの世界の距離（unit.radius など）で決まる部分は、カメラが拡大するので掛けない。

namespace moba {
namespace {

using ui::px;

/// raylib の色を Dear ImGui の色にする
/// @param alpha 不透明度に掛ける倍率（0〜1）
ImU32 toImColor(Color c, float alpha = 1.0f) {
    return IM_COL32(c.r, c.g, c.b, static_cast<int>(static_cast<float>(c.a) * alpha));
}

/// 中央揃えで文字を描く（影付き）。size は倍率を掛ける前の大きさ
void drawCenteredText(ImDrawList* dl, ImVec2 center, float size, ImU32 color, const std::string& text) {
    ImFont* font = ImGui::GetFont();
    const float scaled = px(size);
    const ImVec2 textSize = font->CalcTextSizeA(scaled, FLT_MAX, 0.0f, text.c_str());
    const ImVec2 pos{center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f};
    const float shadow = std::max(1.0f, px(1.0f));
    dl->AddText(font, scaled, ImVec2{pos.x + shadow, pos.y + shadow}, IM_COL32(0, 0, 0, 200), text.c_str());
    dl->AddText(font, scaled, pos, color, text.c_str());
}

/// ワールド座標を、画面上の位置（ピクセル）にする
ImVec2 toScreen(Vector2 world, const Camera2D& camera) {
    const Vector2 s = GetWorldToScreen2D(world, camera);
    return ImVec2{s.x, s.y};
}

/// HP ゲージ（maxHp 個の区切り）。戻り値はゲージの下端の y 座標
float drawHpBar(ImDrawList* dl, const Unit& unit, ImVec2 topCenter) {
    const float segment = px(16.0f);
    const float height = px(7.0f);
    const float gap = px(3.0f);
    const float total = segment * static_cast<float>(unit.maxHp) + gap * static_cast<float>(unit.maxHp - 1);
    float x = topCenter.x - total * 0.5f;
    for (int i = 0; i < unit.maxHp; ++i) {
        const ImVec2 p0{x, topCenter.y};
        const ImVec2 p1{x + segment, topCenter.y + height};
        dl->AddRectFilled(p0, p1, i < unit.hp ? IM_COL32(90, 220, 110, 255) : IM_COL32(50, 30, 30, 220), px(2.0f));
        dl->AddRect(p0, p1, IM_COL32(0, 0, 0, 200), px(2.0f));
        x += segment + gap;
    }
    return topCenter.y + height;
}

/// キャラ名・HP・状態異常の残り時間
void drawUnitLabels(ImDrawList* dl, const World& world, const Camera2D& camera) {
    for (const Unit& unit : world.units()) {
        if (!world.canSee(Team::Player, unit)) continue;  // ブッシュに隠れている敵
        const bool enemy = unit.team == Team::Enemy && !unit.characterId.empty();
        const ImVec2 top = toScreen(unit.pos - Vector2{0.0f, unit.radius + 30.0f}, camera);
        drawCenteredText(dl, top, 16.0f, enemy ? IM_COL32(255, 160, 150, 240) : IM_COL32(255, 255, 255, 230),
                         unit.name);

        ImVec2 below = toScreen(unit.pos + Vector2{0.0f, unit.radius + 20.0f}, camera);
        if (unit.maxHp > 0) below.y = drawHpBar(dl, unit, below) + px(10.0f);

        std::string status;
        // 自キャラがブッシュの中にいる時: 敵から見えているかどうか
        if (unit.team == Team::Player && world.terrain().bushAt(unit.pos) >= 0) {
            status = world.canSee(Team::Enemy, unit)
                         ? (unit.revealTime > 0.0f ? std::format("ブッシュ: 見えている {:.1f}", unit.revealTime)
                                                   : std::string("ブッシュ: 見えている"))
                         : std::string("ブッシュ: 隠れている");
        }
        for (const StatusEffect& effect : unit.status.list()) {
            if (!status.empty()) status += "  ";
            const std::string_view name =
                effect.type == EffectType::Mark ? std::string_view("マーク") : displayName(effect.type);
            status += std::format("{} {:.1f}", name, effect.remaining);
        }
        if (!status.empty()) drawCenteredText(dl, below, 14.0f, IM_COL32(255, 230, 140, 255), status);
    }
    // 状態異常などの文字演出（少しずつ上へ浮かぶ）
    for (const Vfx& vfx : world.vfx()) {
        if (vfx.kind != VfxKind::Text) continue;
        const float t = 1.0f - vfx.remaining / vfx.duration;
        const ImVec2 pos = toScreen(vfx.pos - Vector2{0.0f, 110.0f + t * 60.0f}, camera);
        drawCenteredText(dl, pos, 18.0f, toImColor(vfx.color, vfx.remaining / vfx.duration), vfx.text);
    }
}

/// 画面の外にいる敵の方向を、画面の端に矢印で示す。
/// カメラを固定しない時は自キャラも画面の外に出ることがあるので、自キャラも別の色で示す
void drawOffscreenIndicators(ImDrawList* dl, const World& world, const Camera2D& camera) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Vector2 center{display.x * 0.5f, display.y * 0.5f};
    const float margin = px(30.0f);
    for (const Unit& unit : world.units()) {
        const bool isPlayer = unit.team == Team::Player;
        const bool isEnemyCharacter = unit.team == Team::Enemy && !unit.characterId.empty();
        if (!isPlayer && !isEnemyCharacter) continue;
        if (!world.canSee(Team::Player, unit)) continue;  // ブッシュに隠れている敵の方向は示さない
        const Vector2 s = GetWorldToScreen2D(unit.pos, camera);
        const float r = unit.radius * camera.zoom;
        if (s.x + r >= 0.0f && s.x - r <= display.x && s.y + r >= 0.0f && s.y - r <= display.y) continue;

        const Vector2 dir = geo::normalizedOr(s - center, Vector2{1.0f, 0.0f});
        const float t = std::min((center.x - margin) / std::max(std::fabs(dir.x), 1e-4f),
                                 (center.y - margin) / std::max(std::fabs(dir.y), 1e-4f));
        const Vector2 p = center + dir * t;
        const Vector2 side = geo::perpendicular(dir);
        const Vector2 tip = p + dir * px(14.0f);
        const Vector2 b1 = p - dir * px(10.0f) + side * px(11.0f);
        const Vector2 b2 = p - dir * px(10.0f) - side * px(11.0f);
        const ImU32 arrowColor = isPlayer ? IM_COL32(120, 230, 140, 240) : IM_COL32(255, 80, 80, 230);
        const ImU32 labelColor = isPlayer ? IM_COL32(180, 245, 190, 240) : IM_COL32(255, 170, 160, 230);
        dl->AddTriangleFilled(ImVec2{tip.x, tip.y}, ImVec2{b1.x, b1.y}, ImVec2{b2.x, b2.y}, arrowColor);
        const Vector2 label = p - dir * px(30.0f);
        drawCenteredText(dl, ImVec2{label.x, label.y}, 13.0f, labelColor, unit.name);
    }
}

/// 画面下中央のスキルバー（クールダウン表示付き）と、詠唱バー
void drawSkillBar(ImDrawList* dl, const World& world) {
    const Unit* player = world.player();
    if (!player) return;
    const CharacterDef* def = world.characterOf(*player);
    const float cooldownScale = world.cooldownScaleFor(*player);
    const bool movementOnly = world.rules().movementSkillsOnly;

    const float box = px(ui::kSkillBarBoxSize);
    const float gap = px(10.0f);
    const float rounding = px(6.0f);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float totalWidth = box * kSkillSlotCount + gap * (kSkillSlotCount - 1);
    const float left = (display.x - totalWidth) * 0.5f;
    const float top = display.y - box - px(ui::kSkillBarBottomMargin);
    const float nameY = top + box + px(14.0f);

    for (SkillSlot slot : kAllSkillSlots) {
        const float x = left + static_cast<float>(index(slot)) * (box + gap);
        const ImVec2 p0{x, top};
        const ImVec2 p1{x + box, top + box};
        const ImVec2 boxCenter{x + box * 0.5f, top + box * 0.5f};
        const std::string key(toString(slot));
        const SkillDef* skill = def ? def->skill(slot) : nullptr;
        const bool usable = skill && (!movementOnly || skill->movementSkill);

        dl->AddRectFilled(p0, p1, usable ? IM_COL32(30, 40, 55, 230) : IM_COL32(25, 25, 30, 160), rounding);
        dl->AddRect(p0, p1, usable ? IM_COL32(150, 180, 220, 255) : IM_COL32(70, 70, 80, 255), rounding, 0, px(2.0f));
        if (!usable) {
            drawCenteredText(dl, boxCenter, 24.0f, IM_COL32(110, 110, 120, 255), key);
            if (skill) {
                // このモードでは使えないスキル
                const float inset = px(8.0f);
                dl->AddLine(ImVec2{p0.x + inset, p1.y - inset}, ImVec2{p1.x - inset, p0.y + inset},
                            IM_COL32(200, 80, 80, 200), px(2.0f));
                drawCenteredText(dl, ImVec2{boxCenter.x, nameY}, 13.0f, IM_COL32(120, 120, 130, 255), skill->name);
            }
            continue;
        }

        // 準備完了ならキーを大きく、クールダウン中はキーを左上に小さく出して残り秒数を中央に出す
        const float remaining = player->cooldowns[static_cast<std::size_t>(index(slot))];
        const float total = skill->cooldown * cooldownScale;
        if (remaining > 0.0f && total > 0.0f) {
            const float ratio = remaining / total;
            dl->AddRectFilled(p0, ImVec2{p1.x, p0.y + box * ratio}, IM_COL32(0, 0, 0, 170), rounding);
            dl->AddText(ImGui::GetFont(), px(13.0f), ImVec2{x + px(5.0f), top + px(3.0f)}, IM_COL32(190, 200, 215, 255),
                        key.c_str());
            drawCenteredText(dl, boxCenter, 20.0f, IM_COL32(255, 220, 120, 255),
                             remaining < 10.0f ? std::format("{:.1f}", remaining) : std::format("{:.0f}", remaining));
        } else {
            drawCenteredText(dl, boxCenter, 24.0f, IM_COL32(255, 255, 255, 255), key);
        }
        drawCenteredText(dl, ImVec2{boxCenter.x, nameY}, 13.0f, IM_COL32(220, 225, 235, 255), skill->name);
    }

    // 詠唱バー
    if (player->cast.active && player->cast.total > 0.05f) {
        const float width = px(260.0f);
        const ImVec2 p0{(display.x - width) * 0.5f, top - px(34.0f)};
        const ImVec2 p1{p0.x + width, p0.y + px(14.0f)};
        const float progress = 1.0f - player->cast.remaining / player->cast.total;
        dl->AddRectFilled(p0, p1, IM_COL32(20, 20, 30, 220), px(4.0f));
        dl->AddRectFilled(p0, ImVec2{p0.x + width * progress, p1.y}, IM_COL32(120, 190, 255, 255), px(4.0f));
        const SkillDef* skill = def ? def->skill(player->cast.request.slot) : nullptr;
        if (skill) {
            drawCenteredText(dl, ImVec2{display.x * 0.5f, p0.y - px(10.0f)}, 14.0f, IM_COL32(255, 255, 255, 230),
                             skill->name);
        }
    }
}

/// スキルドッジ: 画面上部に残り時間・ドッジ率・被弾・ダウン
void drawDodgePanel(ImDrawList* dl, const Match& match) {
    const World& world = match.world();
    const DodgeCount& dodge = world.dodge();
    const Unit* player = world.player();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float centerX = display.x * 0.5f;

    dl->AddRectFilled(ImVec2{centerX - px(260.0f), px(34.0f)}, ImVec2{centerX + px(260.0f), px(104.0f)},
                      IM_COL32(10, 14, 20, 190), px(8.0f));

    const int seconds = static_cast<int>(std::ceil(match.remaining()));
    drawCenteredText(dl, ImVec2{centerX, px(56.0f)}, 28.0f,
                     seconds <= 10 ? IM_COL32(255, 140, 120, 255) : IM_COL32(255, 255, 255, 255),
                     std::format("残り {}:{:02}", seconds / 60, seconds % 60));
    const std::string rate = dodge.total > 0
                                 ? std::format("{:.1f}%（{} / {}）", dodge.rate() * 100.0f, dodge.dodged, dodge.total)
                                 : std::string("-");
    drawCenteredText(dl, ImVec2{centerX, px(88.0f)}, 16.0f, IM_COL32(220, 230, 240, 255),
                     std::format("ドッジ率 {}　　被弾 {}　　ダウン {}", rate, dodge.hit, player ? player->deaths : 0));
}

/// スキルショット: 画面上部に残り時間・ヒット率
void drawShotPanel(ImDrawList* dl, const Match& match) {
    const HitCount hits = match.world().stats().total();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float centerX = display.x * 0.5f;

    dl->AddRectFilled(ImVec2{centerX - px(260.0f), px(34.0f)}, ImVec2{centerX + px(260.0f), px(104.0f)},
                      IM_COL32(10, 14, 20, 190), px(8.0f));

    const int seconds = static_cast<int>(std::ceil(match.remaining()));
    drawCenteredText(dl, ImVec2{centerX, px(56.0f)}, 28.0f,
                     seconds <= 10 ? IM_COL32(255, 140, 120, 255) : IM_COL32(255, 255, 255, 255),
                     std::format("残り {}:{:02}", seconds / 60, seconds % 60));
    const std::string rate = hits.casts > 0
                                 ? std::format("{:.1f}%（{} / {}）", hits.rate() * 100.0f, hits.hits, hits.casts)
                                 : std::string("-");
    drawCenteredText(dl, ImVec2{centerX, px(88.0f)}, 16.0f, IM_COL32(220, 230, 240, 255),
                     std::format("ヒット率 {}", rate));
}

/// 画面左上の操作説明（今のカメラも出す）
/// @param escPauses Esc でポーズするモードか（スキルドッジ・スキルショット）。false なら Esc でメニューに戻る
void drawHelp(ImDrawList* dl, CameraMode cameraMode, bool escPauses) {
    const std::string help = std::format(
        "右クリック: 移動    Q/W/E/R/D/F: スキル    Y: カメラ（今: {}）    Space: 自キャラを映す    "
        "F1: デバッグ表示    F5: 設定の再読み込み    Esc: {}",
        displayName(cameraMode), escPauses ? "ポーズ" : "メニュー");
    const float size = px(15.0f);
    const ImVec2 pos = px(12.0f, 10.0f);
    const float shadow = std::max(1.0f, px(1.0f));
    dl->AddText(ImGui::GetFont(), size, ImVec2{pos.x + shadow, pos.y + shadow}, IM_COL32(0, 0, 0, 200), help.c_str());
    dl->AddText(ImGui::GetFont(), size, pos, IM_COL32(230, 235, 240, 255), help.c_str());
}

}  // namespace

void drawHud(const Match& match, const Camera2D& camera, CameraMode cameraMode) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const World& world = match.world();
    drawUnitLabels(dl, world, camera);
    drawOffscreenIndicators(dl, world, camera);
    drawSkillBar(dl, world);
    if (match.config().mode == GameMode::SkillDodge) drawDodgePanel(dl, match);
    if (match.config().mode == GameMode::SkillShot) drawShotPanel(dl, match);
    drawHelp(dl, cameraMode, match.config().timed());
}

}  // namespace moba
