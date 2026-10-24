#include "input/InputSource.h"

namespace moba {

InputFrame LiveInput::poll(const InputPollContext& ctx) {
    InputFrame frame;
    frame.cursorWorld = GetScreenToWorld2D(GetMousePosition(), ctx.camera);

    if (!ctx.uiWantsMouse) {
        frame.rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        frame.rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    }
    if (!ctx.uiWantsText) {
        constexpr int kSkillKeys[kSkillSlotCount] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_D, KEY_F};
        for (int i = 0; i < kSkillSlotCount; ++i) {
            frame.skillPressed[static_cast<std::size_t>(i)] = IsKeyPressed(kSkillKeys[i]);
        }
        frame.escapePressed = IsKeyPressed(KEY_ESCAPE);
        frame.toggleDebugPressed = IsKeyPressed(KEY_F1);
        frame.reloadPressed = IsKeyPressed(KEY_F5);
        frame.toggleFullscreenPressed =
            IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
    }
    return frame;
}

}  // namespace moba
