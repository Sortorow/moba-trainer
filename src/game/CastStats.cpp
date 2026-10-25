#include "game/CastStats.h"

namespace moba {

void CastStats::recordCast(int castId, SkillSlot slot) {
    castSlots_[castId] = slot;
    unsettled_.insert(castId);
    ++slots_[static_cast<std::size_t>(index(slot))].casts;
}

void CastStats::recordHit(int castId) {
    const auto it = castSlots_.find(castId);
    if (it == castSlots_.end()) return;
    if (!hitCasts_.insert(castId).second) return;  // この発動は既に命中として数えた
    unsettled_.erase(castId);                      // 当たった時点で結果が出る
    ++slots_[static_cast<std::size_t>(index(it->second))].hits;
}

void CastStats::reset() {
    slots_ = {};
    castSlots_.clear();
    hitCasts_.clear();
    unsettled_.clear();
}

HitCount CastStats::total() const {
    HitCount sum;
    for (const HitCount& count : slots_) {
        sum.casts += count.casts;
        sum.hits += count.hits;
    }
    return sum;
}

HitCount CastStats::settledSlot(SkillSlot s) const {
    HitCount count = slot(s);
    for (const int castId : unsettled_) {
        const auto it = castSlots_.find(castId);
        if (it != castSlots_.end() && it->second == s) --count.casts;
    }
    return count;
}

HitCount CastStats::settledTotal() const {
    HitCount sum = total();
    sum.casts -= static_cast<int>(unsettled_.size());
    return sum;
}

}  // namespace moba
