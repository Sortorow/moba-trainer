#include "game/PlayerController.h"

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/SkillCasting.h"
#include "game/World.h"

namespace moba {

void PlayerController::update(World& world, const InputFrame& input, float dt) {
    Unit* player = world.player();
    if (!player) return;

    // 移動（右クリック。押しっぱなしの間は一定間隔で目的地をカーソル位置に更新する）
    if (input.rightPressed) {
        moveRepeatTimer_ = kMoveRepeatInterval;
        casting::orderMove(world, *player, input.cursorWorld);
        world.addVfx(makeClickMarkerVfx(input.cursorWorld));
        log::debug("input", "右クリック: 移動先{}", geo::str(input.cursorWorld));
    } else if (input.rightDown) {
        moveRepeatTimer_ -= dt;
        if (moveRepeatTimer_ <= 0.0f) {
            moveRepeatTimer_ = kMoveRepeatInterval;
            casting::orderMove(world, *player, input.cursorWorld);
            log::trace("input", "右クリック長押し: 移動先{}", geo::str(input.cursorWorld));
        }
    }

    // スキル（キーを押した瞬間にカーソル方向へ発動する）
    for (SkillSlot slot : kAllSkillSlots) {
        if (!input.skillPressed[static_cast<std::size_t>(index(slot))]) continue;
        log::debug("input", "{} キー: カーソル{}", toString(slot), geo::str(input.cursorWorld));
        casting::requestCast(world, *player, CastRequest{slot, input.cursorWorld});
    }
}

}  // namespace moba
