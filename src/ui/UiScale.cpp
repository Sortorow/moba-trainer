#include "ui/UiScale.h"

#include <optional>

namespace moba::ui {
namespace {

float g_scale = 1.0f;
std::optional<ImGuiStyle> g_baseStyle;  // 倍率 1.0 の時の Dear ImGui の見た目

}  // namespace

float scale() { return g_scale; }

void setScale(float scale) {
    if (!g_baseStyle) g_baseStyle = ImGui::GetStyle();
    g_scale = scale;
    // 倍率を掛けた結果にさらに掛けないよう、毎回 1.0 の見た目から作り直す
    ImGuiStyle& style = ImGui::GetStyle();
    style = *g_baseStyle;
    style.ScaleAllSizes(scale);  // 余白・枠の太さなど
    style.FontScaleDpi = scale;  // 文字の大きさ
}

float scaleForWindowHeight(int windowHeight, float uiScale) {
    if (windowHeight <= 0) return uiScale;
    return static_cast<float>(windowHeight) / kBaseWindowHeight * uiScale;
}

}  // namespace moba::ui
