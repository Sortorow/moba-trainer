#pragma once
// 入力を自キャラへの命令（移動・スキル）に変える

#include "input/InputFrame.h"

namespace moba {

class World;

class PlayerController {
public:
    void update(World& world, const InputFrame& input, float dt);

private:
    // 右クリックを押しっぱなしにした時、目的地を更新する間隔（秒）
    static constexpr float kMoveRepeatInterval = 0.1f;
    float moveRepeatTimer_ = 0.0f;
};

}  // namespace moba
