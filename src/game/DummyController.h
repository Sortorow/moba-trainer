#pragma once
// 練習場のダミー（動作確認用の的）の動き

#include <optional>
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
    Vector2 home{};                 // 静止する位置 / 往復の一方
    Vector2 patrolTo{};             // 往復のもう一方
    bool headingToPatrolTo = true;  // 次に向かう先が patrolTo か（目的地に着いた時・折り返す時に、こちらへ向かう）

    // 往復の途中で進めなくなったか（押し合いで挟まった時など）を見分けるための記録。
    // 目的地までの直線距離では見ない（壁を回り込む道では、いったん目的地から遠ざかることがあるため）
    std::optional<Vector2> progressAnchor;  // 最後に「動けた」と数えた時の位置
    float progressTime = 0.0f;              // その時刻（World::time）
};

/// 練習場のダミーを動かす（静止・往復）
class DummyController {
public:
    /// すべてのダミーの動きを消す（ダミーのキャラ自体は World に残るので、先に取り除いておく）
    void clear() { brains_.clear(); }
    /// ダミー 1 体の動き方を足す
    void add(const DummyBrain& brain) { brains_.push_back(brain); }
    /// ダミーを動かす（静止のダミーは元の位置へ戻り、往復のダミーは 2 点の間を行き来する）。
    /// 往復のダミーが、動ける状態なのに kPatrolStuckTime 秒のあいだ、ほとんど動けなければ（kPatrolProgress 未満）、
    /// 折り返す（押し合いで、障害物と別のキャラの間に挟まって進めない時。練習のために往復を止めない）
    void update(World& world);

    /// すべてのダミーの動き方（デバッグパネルで一時的に変えられる）
    std::vector<DummyBrain>& brains() { return brains_; }

private:
    std::vector<DummyBrain> brains_;
};

}  // namespace moba
