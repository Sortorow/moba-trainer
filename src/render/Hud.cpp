#include "render/Hud.h"

#include <cfloat>
#include <format>
#include <string>

#include "data/CharacterDef.h"
#include "data/Settings.h"
#include "game/World.h"
#include "imgui.h"
#include "raymath.h"

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

// キャラ名と状態異常の残り時間
void drawUnitLabels(ImDrawList* dl, const World& world, const Camera2D& camera) {
    for (const Unit& unit : world.units()) {
        const ImVec2 top = toScreen(unit.pos - Vector2{0.0f, unit.radius + 30.0f}, camera);
        drawCenteredText(dl, top, 16.0f, IM_COL32(255, 255, 255, 230), unit.name);

        std::string status;
        for (const StatusEffect& effect : unit.status.list()) {
            if (!status.empty()) status += "  ";
            const std::string_view name = effect.type == EffectType::Mark ? std::string_view("マーク") : displayName(effect.type);
            status += std::format("{} {:.1f}", name, effect.remaining);
        }
        if (!status.empty()) {
            const ImVec2 below = toScreen(unit.pos + Vector2{0.0f, unit.radius + 28.0f}, camera);
            drawCenteredText(dl, below, 14.0f, IM_COL32(255, 230, 140, 255), status);
        }
    }
    // 状態異常などの文字演出（少しずつ上へ浮かぶ）
    for (const Vfx& vfx : world.vfx()) {
        if (vfx.kind != VfxKind::Text) continue;
        const float t = 1.0f - vfx.remaining / vfx.duration;
        const ImVec2 pos = toScreen(vfx.pos - Vector2{0.0f, 110.0f + t * 60.0f}, camera);
        drawCenteredText(dl, pos, 18.0f, toImColor(vfx.color, vfx.remaining / vfx.duration), vfx.text);
    }
}

// 画面下中央のスキルバー（クールダウン表示付き）と、詠唱バー
void drawSkillBar(ImDrawList* dl, const World& world) {
    const Unit* player = world.player();
    if (!player) return;
    const CharacterDef* def = world.characterOf(*player);
    const float cooldownScale = world.cooldownScaleFor(*player);

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
        const SkillDef* skill = def ? def->skill(slot) : nullptr;

        const ImVec2 boxCenter{x + kBox * 0.5f, top + kBox * 0.5f};
        const std::string key(toString(slot));
        dl->AddRectFilled(p0, p1, skill ? IM_COL32(30, 40, 55, 230) : IM_COL32(25, 25, 30, 160), 6.0f);
        dl->AddRect(p0, p1, skill ? IM_COL32(150, 180, 220, 255) : IM_COL32(70, 70, 80, 255), 6.0f, 0, 2.0f);
        if (!skill) {
            drawCenteredText(dl, boxCenter, 24.0f, IM_COL32(110, 110, 120, 255), key);
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
        drawCenteredText(dl, ImVec2{x + kBox * 0.5f, top + kBox + 14.0f}, 13.0f, IM_COL32(220, 225, 235, 255),
                         skill->name);
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

void drawHelp(ImDrawList* dl) {
    const char* help = "右クリック: 移動    Q/W/E/R/D/F: スキル    F1: デバッグ表示    F5: 設定の再読み込み    Esc: メニュー";
    dl->AddText(ImGui::GetFont(), 15.0f, ImVec2{13.0f, 11.0f}, IM_COL32(0, 0, 0, 200), help);
    dl->AddText(ImGui::GetFont(), 15.0f, ImVec2{12.0f, 10.0f}, IM_COL32(230, 235, 240, 255), help);
}

}  // namespace

void drawHud(const World& world, const Camera2D& camera) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    drawUnitLabels(dl, world, camera);
    drawSkillBar(dl, world);
    drawHelp(dl);
}

}  // namespace moba
