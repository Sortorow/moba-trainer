#pragma once
// 入力を自キャラへの命令（移動・スキル）に変える

#include "input/InputFrame.h"

namespace moba {

class World;

class PlayerController {
public:
    /// 入力を自キャラへの命令に変える。右クリックは移動（押しっぱなしならカーソルを追いかける）、
    /// Q〜F はスキルの発動（カーソルの位置を狙う）。固定の時間刻みごとに呼ぶ
    void update(World& world, const InputFrame& input, float dt);

private:
    // 右クリックを押しっぱなしにした時、目的地を更新する間隔（秒）
    static constexpr float kMoveRepeatInterval = 0.1f;
    float moveRepeatTimer_ = 0.0f;
};

}  // namespace moba
