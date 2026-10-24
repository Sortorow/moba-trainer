#pragma once
// スキルヒット率の集計
// 1 回の発動（castId）につき、1 体以上に当たれば「命中」と数える（多段ヒット・複数の弾も 1 回扱い）

#include <array>
#include <unordered_map>
#include <unordered_set>

#include "core/GameTypes.h"

namespace moba {

struct HitCount {
    int casts = 0;
    int hits = 0;
    float rate() const { return casts > 0 ? static_cast<float>(hits) / static_cast<float>(casts) : 0.0f; }
};

// スキルドッジ率の集計（敵の 1 回の発動ごとに、自キャラに当たったかどうか）
// 発動で出た弾・範囲がすべて消えた時点で結果が確定する。制限時間の終了時に飛んでいる途中の発動は数えない
struct DodgeCount {
    int total = 0;   // 結果が確定した発動の数
    int dodged = 0;  // 1 つも当たらなかった発動
    int hit = 0;     // 当たった発動
    float rate() const { return total > 0 ? static_cast<float>(dodged) / static_cast<float>(total) : 0.0f; }
};

class CastStats {
public:
    void recordCast(int castId, SkillSlot slot);  // 集計対象のスキルを発動した
    void recordHit(int castId);                   // その発動が当たった（集計対象外の castId は無視する）
    void reset();

    const HitCount& slot(SkillSlot s) const { return slots_[static_cast<std::size_t>(index(s))]; }
    HitCount total() const;

private:
    std::array<HitCount, kSkillSlotCount> slots_{};
    std::unordered_map<int, SkillSlot> castSlots_;
    std::unordered_set<int> hitCasts_;
};

}  // namespace moba
