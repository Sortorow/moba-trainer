#pragma once
// スキルヒット率の集計
// 1 回の発動（castId）につき、1 体以上に当たれば「命中」と数える（多段ヒット・複数の弾も 1 回扱い）

#include <array>
#include <unordered_map>
#include <unordered_set>

#include "core/GameTypes.h"

namespace moba {

/// スキルヒット率の数（発動した数と、当たった数）
struct HitCount {
    int casts = 0;
    int hits = 0;
    /// 命中率（0〜1）。1 回も発動していなければ 0
    float rate() const { return casts > 0 ? static_cast<float>(hits) / static_cast<float>(casts) : 0.0f; }
};

/// スキルドッジ率の集計（敵の 1 回の発動ごとに、自キャラに当たったかどうか）
/// 発動で出た弾・範囲がすべて消えた時点で結果が確定する。制限時間の終了時に飛んでいる途中の発動は数えない
struct DodgeCount {
    int total = 0;   // 結果が確定した発動の数
    int dodged = 0;  // 1 つも当たらなかった発動
    int hit = 0;     // 当たった発動
    /// ドッジ率（0〜1）。確定した発動が無ければ 0
    float rate() const { return total > 0 ? static_cast<float>(dodged) / static_cast<float>(total) : 0.0f; }
};

/// 自キャラのスキルヒット率の集計（キーごと）。1 回の発動で何体に当たっても、命中は 1 回と数える
class CastStats {
public:
    /// 集計対象のスキル（countsForHitRate が true）を発動したことを記録する
    void recordCast(int castId, SkillSlot slot);
    /// その発動が当たったことを記録する。同じ発動の 2 回目以降の命中と、集計対象外の castId は無視する
    void recordHit(int castId);
    /// 集計を 0 に戻す（デバッグパネルの「集計をリセット」）
    void reset();

    /// キーごとの発動数と命中数
    const HitCount& slot(SkillSlot s) const { return slots_[static_cast<std::size_t>(index(s))]; }
    /// すべてのキーを合わせた発動数と命中数
    HitCount total() const;

private:
    std::array<HitCount, kSkillSlotCount> slots_{};
    std::unordered_map<int, SkillSlot> castSlots_;
    std::unordered_set<int> hitCasts_;
};

}  // namespace moba
