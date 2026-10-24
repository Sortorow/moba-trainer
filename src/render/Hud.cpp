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

namespace moba {
namespace {

ImU32 toImColor(Color c, float alpha = 1.0f) {
    return IM_COL32(c.r, c.g, c.b, static_cast<int>(static_cast<float>(c.a) * alpha));
}

// 中央揃えで文字を描く（影付き）
void drawCenteredText(ImDrawList* dl, ImVec2 center, float size, ImU32 color, const std::string& text) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
    const ImVec2 pos{center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f};
    dl->AddText(font, size, ImVec2{pos.x + 1.0f, pos.y + 1.0f}, IM_COL32(0, 0, 0, 200), text.c_str());
    dl->AddText(font, size, pos, color, text.c_str());
}

ImVec2 toScreen(Vector2 world, const Camera2D& camera) {
    const Vector2 s = GetWorldToScreen2D(world, camera);
    return ImVec2{s.x, s.y};
}

// HP ゲージ（maxHp 個の区切り）。戻り値はゲージの下端の y 座標
float drawHpBar(ImDrawList* dl, const Unit& unit, ImVec2 topCenter) {
    constexpr float kSegment = 16.0f;
    constexpr float kHeight = 7.0f;
    constexpr float kGap = 3.0f;
    const float total = kSegment * static_cast<float>(unit.maxHp) + kGap * static_cast<float>(unit.maxHp - 1);
    float x = topCenter.x - total * 0.5f;
    for (int i = 0; i < unit.maxHp; ++i) {
        const ImVec2 p0{x, topCenter.y};
        const ImVec2 p1{x + kSegment, topCenter.y + kHeight};
        dl->AddRectFilled(p0, p1, i < unit.hp ? IM_COL32(90, 220, 110, 255) : IM_COL32(50, 30, 30, 220), 2.0f);
        dl->AddRect(p0, p1, IM_COL32(0, 0, 0, 200), 2.0f);
        x += kSegment + kGap;
    }
    return topCenter.y + kHeight;
}

// キャラ名・HP・状態異常の残り時間
void drawUnitLabels(ImDrawList* dl, const World& world, const Camera2D& camera) {
    for (const Unit& unit : world.units()) {
        const bool enemy = unit.team == Team::Enemy && !unit.characterId.empty();
        const ImVec2 top = toScreen(unit.pos - Vector2{0.0f, unit.radius + 30.0f}, camera);
        drawCenteredText(dl, top, 16.0f, enemy ? IM_COL32(255, 160, 150, 240) : IM_COL32(255, 255, 255, 230), unit.name);

        ImVec2 below = toScreen(unit.pos + Vector2{0.0f, unit.radius + 20.0f}, camera);
        if (unit.maxHp > 0) below.y = drawHpBar(dl, unit, below) + 10.0f;

        std::string status;
        for (const StatusEffect& effect : unit.status.list()) {
            if (!status.empty()) status += "  ";
            const std::string_view name = effect.type == EffectType::Mark ? std::string_view("マーク") : displayName(effect.type);
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

// 画面の外にいる敵の方向を、画面の端に矢印で示す
void drawOffscreenIndicators(ImDrawList* dl, const World& world, const Camera2D& camera) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Vector2 center{display.x * 0.5f, display.y * 0.5f};
    constexpr float kMargin = 30.0f;
    for (const Unit& unit : world.units()) {
        if (unit.team != Team::Enemy || unit.characterId.empty()) continue;
        const Vector2 s = GetWorldToScreen2D(unit.pos, camera);
        const float r = unit.radius * camera.zoom;
        if (s.x + r >= 0.0f && s.x - r <= display.x && s.y + r >= 0.0f && s.y - r <= display.y) continue;

        const Vector2 dir = geo::normalizedOr(s - center, Vector2{1.0f, 0.0f});
        const float t = std::min((center.x - kMargin) / std::max(std::fabs(dir.x), 1e-4f),
                                 (center.y - kMargin) / std::max(std::fabs(dir.y), 1e-4f));
        const Vector2 p = center + dir * t;
        const Vector2 side = geo::perpendicular(dir);
        const Vector2 tip = p + dir * 14.0f;
        const Vector2 b1 = p - dir * 10.0f + side * 11.0f;
        const Vector2 b2 = p - dir * 10.0f - side * 11.0f;
        dl->AddTriangleFilled(ImVec2{tip.x, tip.y}, ImVec2{b1.x, b1.y}, ImVec2{b2.x, b2.y}, IM_COL32(255, 80, 80, 230));
        const Vector2 label = p - dir * 30.0f;
        drawCenteredText(dl, ImVec2{label.x, label.y}, 13.0f, IM_COL32(255, 170, 160, 230), unit.name);
    }
}

// 画面下中央のスキルバー（クールダウン表示付き）と、詠唱バー
void drawSkillBar(ImDrawList* dl, const World& world) {
    const Unit* player = world.player();
    if (!player) return;
    const CharacterDef* def = world.characterOf(*player);
    const float cooldownScale = world.cooldownScaleFor(*player);
    const bool movementOnly = world.rules().movementSkillsOnly;

    constexpr float kBox = 58.0f;
    constexpr float kGap = 10.0f;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float totalWidth = kBox * kSkillSlotCount + kGap * (kSkillSlotCount - 1);
    const float left = (display.x - totalWidth) * 0.5f;
    const float top = display.y - kBox - 44.0f;

    for (SkillSlot slot : kAllSkillSlots) {
        const float x = left + static_cast<float>(index(slot)) * (kBox + kGap);
        const ImVec2 p0{x, top};
        const ImVec2 p1{x + kBox, top + kBox};
        const ImVec2 boxCenter{x + kBox * 0.5f, top + kBox * 0.5f};
        const std::string key(toString(slot));
        const SkillDef* skill = def ? def->skill(slot) : nullptr;
        const bool usable = skill && (!movementOnly || skill->movementSkill);

        dl->AddRectFilled(p0, p1, usable ? IM_COL32(30, 40, 55, 230) : IM_COL32(25, 25, 30, 160), 6.0f);
        dl->AddRect(p0, p1, usable ? IM_COL32(150, 180, 220, 255) : IM_COL32(70, 70, 80, 255), 6.0f, 0, 2.0f);
        if (!usable) {
            drawCenteredText(dl, boxCenter, 24.0f, IM_COL32(110, 110, 120, 255), key);
            if (skill) {
                // このモードでは使えないスキル
                dl->AddLine(ImVec2{p0.x + 8.0f, p1.y - 8.0f}, ImVec2{p1.x - 8.0f, p0.y + 8.0f}, IM_COL32(200, 80, 80, 200), 2.0f);
                drawCenteredText(dl, ImVec2{boxCenter.x, top + kBox + 14.0f}, 13.0f, IM_COL32(120, 120, 130, 255), skill->name);
            }
            continue;
        }

        // 準備完了ならキーを大きく、クールダウン中はキーを左上に小さく出して残り秒数を中央に出す
        const float remaining = player->cooldowns[static_cast<std::size_t>(index(slot))];
        const float total = skill->cooldown * cooldownScale;
        if (remaining > 0.0f && total > 0.0f) {
            const float ratio = remaining / total;
            dl->AddRectFilled(p0, ImVec2{p1.x, p0.y + kBox * ratio}, IM_COL32(0, 0, 0, 170), 6.0f);
            dl->AddText(ImGui::GetFont(), 13.0f, ImVec2{x + 5.0f, top + 3.0f}, IM_COL32(190, 200, 215, 255), key.c_str());
            drawCenteredText(dl, boxCenter, 20.0f, IM_COL32(255, 220, 120, 255),
                             remaining < 10.0f ? std::format("{:.1f}", remaining) : std::format("{:.0f}", remaining));
        } else {
            drawCenteredText(dl, boxCenter, 24.0f, IM_COL32(255, 255, 255, 255), key);
        }
        drawCenteredText(dl, ImVec2{boxCenter.x, top + kBox + 14.0f}, 13.0f, IM_COL32(220, 225, 235, 255), skill->name);
    }

    // 詠唱バー
    if (player->cast.active && player->cast.total > 0.05f) {
        const float width = 260.0f;
        const ImVec2 p0{(display.x - width) * 0.5f, top - 34.0f};
        const ImVec2 p1{p0.x + width, p0.y + 14.0f};
        const float progress = 1.0f - player->cast.remaining / player->cast.total;
        dl->AddRectFilled(p0, p1, IM_COL32(20, 20, 30, 220), 4.0f);
        dl->AddRectFilled(p0, ImVec2{p0.x + width * progress, p1.y}, IM_COL32(120, 190, 255, 255), 4.0f);
        const SkillDef* skill = def ? def->skill(player->cast.request.slot) : nullptr;
        if (skill) drawCenteredText(dl, ImVec2{display.x * 0.5f, p0.y - 10.0f}, 14.0f, IM_COL32(255, 255, 255, 230), skill->name);
    }
}

// スキルドッジ: 画面上部に残り時間・ドッジ率・被弾・ダウン
void drawDodgePanel(ImDrawList* dl, const Match& match) {
    const World& world = match.world();
    const DodgeCount& dodge = world.dodge();
    const Unit* player = world.player();
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    const ImVec2 p0{display.x * 0.5f - 260.0f, 34.0f};
    const ImVec2 p1{display.x * 0.5f + 260.0f, 104.0f};
    dl->AddRectFilled(p0, p1, IM_COL32(10, 14, 20, 190), 8.0f);

    const int seconds = static_cast<int>(std::ceil(match.remaining()));
    drawCenteredText(dl, ImVec2{display.x * 0.5f, 56.0f}, 28.0f,
                     seconds <= 10 ? IM_COL32(255, 140, 120, 255) : IM_COL32(255, 255, 255, 255),
                     std::format("残り {}:{:02}", seconds / 60, seconds % 60));
    const std::string rate = dodge.total > 0 ? std::format("{:.1f}%（{} / {}）", dodge.rate() * 100.0f, dodge.dodged, dodge.total)
                                             : std::string("-");
    drawCenteredText(dl, ImVec2{display.x * 0.5f, 88.0f}, 16.0f, IM_COL32(220, 230, 240, 255),
                     std::format("ドッジ率 {}　　被弾 {}　　ダウン {}", rate, dodge.hit, player ? player->deaths : 0));
}

void drawHelp(ImDrawList* dl, GameMode mode) {
    const char* help = mode == GameMode::SkillDodge
                           ? "右クリック: 移動    移動系スキルのみ使用可    F1: デバッグ表示    Esc: メニュー"
                           : "右クリック: 移動    Q/W/E/R/D/F: スキル    F1: デバッグ表示    F5: 設定の再読み込み    Esc: メニュー";
    dl->AddText(ImGui::GetFont(), 15.0f, ImVec2{13.0f, 11.0f}, IM_COL32(0, 0, 0, 200), help);
    dl->AddText(ImGui::GetFont(), 15.0f, ImVec2{12.0f, 10.0f}, IM_COL32(230, 235, 240, 255), help);
}

}  // namespace

void drawHud(const Match& match, const Camera2D& camera) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const World& world = match.world();
    drawUnitLabels(dl, world, camera);
    drawOffscreenIndicators(dl, world, camera);
    drawSkillBar(dl, world);
    if (match.config().mode == GameMode::SkillDodge) drawDodgePanel(dl, match);
    drawHelp(dl, match.config().mode);
}

}  // namespace moba
