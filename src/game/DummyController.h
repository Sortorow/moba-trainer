#pragma once
// 練習場のダミー（動作確認用の的）の動き

#include <vector>

#include "core/GameTypes.h"
#include "raylib.h"

namespace moba {

class World;

/// ダミーの動き方
enum class DummyBehavior {
    Stationary,  // その場で動かない
    Patrol,      // 2 点の間を往復する
};

/// ダミー 1 体の動き方と、静止する位置・往復する 2 点
struct DummyBrain {
    EntityId unitId = kInvalidId;
    DummyBehavior behavior = DummyBehavior::Stationary;
    Vector2 home{};      // 静止する位置 / 往復の一方
    Vector2 patrolTo{};  // 往復のもう一方
    bool headingToPatrolTo = true;
};

/// 練習場のダミーを動かす（静止・往復）
class DummyController {
public:
    /// すべてのダミーの動きを消す（ダミーのキャラ自体は World に残るので、先に取り除いておく）
    void clear() { brains_.clear(); }
    /// ダミー 1 体の動き方を足す
    void add(const DummyBrain& brain) { brains_.push_back(brain); }
    /// ダミーを動かす（静止のダミーは元の位置へ戻り、往復のダミーは 2 点の間を行き来する）
    void update(World& world);

    /// すべてのダミーの動き方（デバッグパネルで一時的に変えられる）
    std::vector<DummyBrain>& brains() { return brains_; }

private:
    std::vector<DummyBrain> brains_;
};

}  // namespace moba
