#include "ui/Fonts.h"

#include "core/Log.h"
#include "imgui.h"
#include "raylib.h"

namespace moba::ui {

ImFont* loadJapaneseFont(float size) {
    constexpr const char* kCandidates[] = {
        "C:/Windows/Fonts/meiryo.ttc",
        "C:/Windows/Fonts/YuGothM.ttc",
        "C:/Windows/Fonts/msgothic.ttc",
    };
    ImGuiIO& io = ImGui::GetIO();
    for (const char* path : kCandidates) {
        if (!FileExists(path)) continue;
        ImFontConfig config;
        config.FontNo = 0;  // .ttc（複数のフォントをまとめたファイル）の 1 つ目を使う
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, &config)) {
            io.FontDefault = font;
            log::info("ui", "日本語フォントを読み込みました: {}", path);
            return font;
        }
    }
    log::warn("ui", "日本語フォントが見つかりませんでした。日本語は表示されません");
    return nullptr;
}

}  // namespace moba::ui
