#pragma once
// 敵 AI が見ている自キャラの動き
// ・反応の遅れを再現するため、少し前の位置・速度を取り出せるように位置の履歴を持つ
// ・賢さが最大の AI のために、弾を撃たれた時に自キャラが左右どちらへ避けやすいかを記録する

#include <deque>

#include "raylib.h"

namespace moba::ai {

class PlayerObserver {
public:
    void clear();
    void record(float time, Vector2 pos);  // 毎刻み、自キャラの実際の位置を記録する
    Vector2 positionAt(float time) const;  // ある時刻の位置（記録の間は補間する）
    Vector2 velocityAt(float time) const;  // ある時刻の速度（0.1 秒前との差から）

    // 敵が直線の弾を撃った時に呼ぶ。少し後に、自キャラが弾の左右どちらへ動いたかを数える
    void noteShot(float time, Vector2 playerPos, Vector2 shotDir);
    void update(float time);
    float rightBias() const;  // 弾の進行方向に対して右へ避ける割合（0〜1）

private:
    struct Sample {
        float time;
        Vector2 pos;
    };
    struct Shot {
        float time;
        Vector2 playerPos;
        Vector2 dir;
    };

    std::deque<Sample> samples_;
    std::deque<Shot> shots_;
    int rightCount_ = 1;  // 最初は五分五分（0 で割らないように 1 から数える）
    int leftCount_ = 1;
};

}  // namespace moba::ai
