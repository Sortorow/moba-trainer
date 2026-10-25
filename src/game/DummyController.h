#pragma once
// 練習場のダミー（動作確認用の的）の動き

#include <vector>

#include "core/GameTypes.h"
#include "raylib.h"

namespace moba {

class World;

enum class DummyBehavior {
    Stationary,  // その場で動かない
    Patrol,      // 2 点の間を往復する
};

struct DummyBrain {
    EntityId unitId = kInvalidId;
    DummyBehavior behavior = DummyBehavior::Stationary;
    Vector2 home{};      // 静止する位置 / 往復の一方
    Vector2 patrolTo{};  // 往復のもう一方
    bool headingToPatrolTo = true;
};

class DummyController {
public:
    void clear() { brains_.clear(); }
    void add(const DummyBrain& brain) { brains_.push_back(brain); }
    void update(World& world);

    std::vector<DummyBrain>& brains() { return brains_; }

private:
    std::vector<DummyBrain> brains_;
};

}  // namespace moba
