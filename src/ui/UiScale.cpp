#include "ui/UiScale.h"

#include <algorithm>
#include <cmath>
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

WindowSize fitWindowSize(int logicalWidth, int logicalHeight, float dpiScale, int areaWidth, int areaHeight,
                         float maxFraction) {
    float width = static_cast<float>(logicalWidth) * dpiScale;
    float height = static_cast<float>(logicalHeight) * dpiScale;
    if (areaWidth > 0 && areaHeight > 0) {
        const float fit = std::min({1.0f, static_cast<float>(areaWidth) * maxFraction / width,
                                    static_cast<float>(areaHeight) * maxFraction / height});
        width *= fit;
        height *= fit;
    }
    return WindowSize{static_cast<int>(std::lround(width)), static_cast<int>(std::lround(height))};
}

}  // namespace moba::ui
