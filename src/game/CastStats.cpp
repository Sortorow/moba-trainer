#include "game/CastStats.h"

namespace moba {

void CastStats::recordCast(int castId, SkillSlot slot) {
    castSlots_[castId] = slot;
    ++slots_[static_cast<std::size_t>(index(slot))].casts;
}

void CastStats::recordHit(int castId) {
    const auto it = castSlots_.find(castId);
    if (it == castSlots_.end()) return;
    if (!hitCasts_.insert(castId).second) return;  // この発動は既に命中として数えた
    ++slots_[static_cast<std::size_t>(index(it->second))].hits;
}

void CastStats::reset() {
    slots_ = {};
    castSlots_.clear();
    hitCasts_.clear();
}

HitCount CastStats::total() const {
    HitCount sum;
    for (const HitCount& count : slots_) {
        sum.casts += count.casts;
        sum.hits += count.hits;
    }
    return sum;
}

}  // namespace moba
