#pragma once
// 1 フレーム分の入力（実際のマウス・キーボードからでも、入力スクリプトからでも同じ形で渡す）

#include <array>
#include <optional>
#include <string>

#include "core/GameTypes.h"
#include "raylib.h"

namespace moba {

struct InputFrame {
    Vector2 cursorWorld{};  // カーソルのワールド座標
    bool rightPressed = false;  // 右クリックした瞬間
    bool rightDown = false;     // 右クリックを押している間
    std::array<bool, kSkillSlotCount> skillPressed{};  // Q/W/E/R/D/F を押した瞬間

    bool escapePressed = false;
    bool toggleDebugPressed = false;       // F1
    bool reloadPressed = false;            // F5
    bool toggleFullscreenPressed = false;  // Alt + Enter

    // 入力スクリプトからの指示
    std::optional<std::string> screenshotRequest;
    bool resetCooldownsRequested = false;
    bool exitRequested = false;

    // 「押した瞬間」の入力だけを、後から来た入力 newer の分も含めてまとめる
    // （ゲームの更新が走らなかったフレームの入力を取りこぼさないため）
    void mergeNewer(const InputFrame& newer) {
        cursorWorld = newer.cursorWorld;
        rightDown = newer.rightDown;
        rightPressed = rightPressed || newer.rightPressed;
        for (std::size_t i = 0; i < skillPressed.size(); ++i) skillPressed[i] = skillPressed[i] || newer.skillPressed[i];
    }

    // 「押した瞬間」の入力を消す（1 回の更新で使い終わった後）
    void clearPressed() {
        rightPressed = false;
        skillPressed = {};
    }
};

}  // namespace moba
