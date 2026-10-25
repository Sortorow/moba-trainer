#include "game/ai/PlayerObserver.h"

#include "core/Geometry.h"
#include "core/Log.h"

namespace moba::ai {
namespace {

constexpr float kHistorySeconds = 2.0f;   // 位置の履歴を残す長さ
constexpr float kDodgeCheckDelay = 0.4f;  // 弾を撃ってから、避けた方向を判定するまでの時間
constexpr float kDodgeThreshold = 40.0f;  // これ以上横に動いたら「避けた」とみなす距離

}  // namespace

void PlayerObserver::clear() {
    samples_.clear();
    shots_.clear();
    rightCount_ = 1;
    leftCount_ = 1;
}

void PlayerObserver::record(float time, Vector2 pos) {
    samples_.push_back({time, pos});
    while (!samples_.empty() && samples_.front().time < time - kHistorySeconds) samples_.pop_front();
}

Vector2 PlayerObserver::positionAt(float time) const {
    if (samples_.empty()) return Vector2{};
    if (time <= samples_.front().time) return samples_.front().pos;
    if (time >= samples_.back().time) return samples_.back().pos;
    for (std::size_t i = 1; i < samples_.size(); ++i) {
        const Sample& b = samples_[i];
        if (b.time < time) continue;
        const Sample& a = samples_[i - 1];
        const float span = b.time - a.time;
        const float t = span > 1e-6f ? (time - a.time) / span : 1.0f;
        return Vector2Lerp(a.pos, b.pos, t);
    }
    return samples_.back().pos;
}

Vector2 PlayerObserver::velocityAt(float time) const {
    constexpr float kWindow = 0.1f;
    return (positionAt(time) - positionAt(time - kWindow)) / kWindow;
}

void PlayerObserver::noteShot(float time, Vector2 playerPos, Vector2 shotDir) {
    shots_.push_back({time, playerPos, shotDir});
}

void PlayerObserver::update(float time) {
    while (!shots_.empty() && shots_.front().time + kDodgeCheckDelay <= time) {
        const Shot& shot = shots_.front();
        const Vector2 moved = positionAt(shot.time + kDodgeCheckDelay) - shot.playerPos;
        const float sideways = Vector2DotProduct(moved, geo::perpendicular(shot.dir));
        if (sideways > kDodgeThreshold) ++rightCount_;
        else if (sideways < -kDodgeThreshold) ++leftCount_;
        shots_.pop_front();
    }
}

float PlayerObserver::rightBias() const {
    return static_cast<float>(rightCount_) / static_cast<float>(rightCount_ + leftCount_);
}

}  // namespace moba::ai
