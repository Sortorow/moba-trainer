#include "render/CameraControl.h"

#include <algorithm>

#include "core/Geometry.h"
#include "raymath.h"

namespace moba {

Vector2 edgeScrollDirection(const CameraControlInput& input) {
    Vector2 dir{};
    if (input.mouse.x <= input.edgeMargin) dir.x = -1.0f;
    else if (input.mouse.x >= input.screenWidth - 1.0f - input.edgeMargin) dir.x = 1.0f;
    if (input.mouse.y <= input.edgeMargin) dir.y = -1.0f;
    else if (input.mouse.y >= input.screenHeight - 1.0f - input.edgeMargin) dir.y = 1.0f;
    return geo::normalizedOr(dir, Vector2{});
}

Vector2 nextCameraTarget(CameraMode mode, Vector2 current, Vector2 playerPos, const CameraControlInput& input,
                         float scrollSpeed, Rectangle bounds, float dt) {
    if (mode == CameraMode::Locked || input.centerHeld) return playerPos;

    Vector2 target = current;
    if (input.edgeScrollEnabled) target += edgeScrollDirection(input) * scrollSpeed * dt;
    target.x = std::clamp(target.x, bounds.x, bounds.x + bounds.width);
    target.y = std::clamp(target.y, bounds.y, bounds.y + bounds.height);
    return target;
}

}  // namespace moba
