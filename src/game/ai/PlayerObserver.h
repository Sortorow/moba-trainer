#pragma once
// 敵 AI が見ている自キャラの動き
// ・反応の遅れを再現するため、少し前の位置・速度を取り出せるように位置の履歴を持つ
// ・賢さが最大の AI のために、弾を撃たれた時に自キャラが左右どちらへ避けやすいかを記録する

#include <deque>

#include "raylib.h"

namespace moba::ai {

/// 敵 AI が見ている自キャラの動き（位置の履歴と、弾を避けた向きの記録）
class PlayerObserver {
public:
    /// 記録をすべて消す（新しいプレイの始め）
    void clear();
    /// 自キャラの位置を記録する（自キャラが見えている間、毎刻み呼ぶ。2 秒より古い記録は捨てる）
    void record(float time, Vector2 pos);
    /// 位置の記録が 1 つでもあるか（自キャラが一度も見えていなければ false）
    bool hasSamples() const { return !samples_.empty(); }
    /// ある時刻の位置（記録の間は補間する。記録より前・後なら最初・最後の位置）。
    /// 記録が無ければ (0, 0) なので、先に hasSamples() で確かめる
    Vector2 positionAt(float time) const;
    /// ある時刻の速度（0.1 秒前の位置との差から）
    Vector2 velocityAt(float time) const;

    /// 敵が直線の弾を撃った時に呼ぶ。少し後に、自キャラが弾の左右どちらへ動いたかを数える
    void noteShot(float time, Vector2 playerPos, Vector2 shotDir);
    /// 撃ってから一定時間たった弾について、自キャラが左右どちらへ避けたかを数える（毎刻み呼ぶ）
    void update(float time);
    /// 弾の進行方向に対して右へ避ける割合（0〜1。最初は 0.5）
    float rightBias() const;

private:
    /// ある時刻の自キャラの位置
    struct Sample {
        float time;
        Vector2 pos;
    };
    /// 敵が撃った直線の弾 1 つ（撃った時刻・その時の自キャラの位置・弾の向き）
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
