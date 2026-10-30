#include "game/ResultStats.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace moba {
namespace {

/// 結果の賢さ（スキルドッジは敵、スキルショットは的）
int levelOf(const MatchResult& r) {
    return r.config.mode == GameMode::SkillShot ? r.config.targetLevel : r.config.aiLevel;
}

/// a に b の数を足す
void add(ResultScore& a, const ResultScore& b) {
    a.count += b.count;
    a.total += b.total;
}

/// 自己ベストに数えられるプレイか（総数が kMinTotalForBest 以上）
bool countsForBest(const SavedResult& saved) { return scoreOf(saved.result).total >= kMinTotalForBest; }

}  // namespace

bool matchesFilter(const SavedResult& saved, const StatsFilter& filter, int today) {
    const MatchResult& r = saved.result;
    const MatchConfig& c = r.config;
    if (c.mode != filter.mode) return false;
    if (!filter.characterId.empty() && c.characterId != filter.characterId) return false;
    if (!filter.stageId.empty() && c.stageId != filter.stageId) return false;
    if (filter.level > 0 && levelOf(r) != filter.level) return false;
    if (filter.enemyCount > 0 && c.enemyCount != filter.enemyCount) return false;
    if (filter.targetRadius > 0.0f && std::fabs(c.targetRadius - filter.targetRadius) > 0.5f) return false;
    if (filter.targetMoveSpeed > 0.0f && std::fabs(c.targetMoveSpeed - filter.targetMoveSpeed) > 0.5f) return false;
    if (filter.periodDays > 0 && daysBetween(saved.date, today) >= filter.periodDays) return false;
    return true;
}

std::vector<const SavedResult*> filterResults(const std::vector<SavedResult>& all, const StatsFilter& filter,
                                              int today) {
    std::vector<const SavedResult*> results;
    for (const SavedResult& saved : all) {
        if (matchesFilter(saved, filter, today)) results.push_back(&saved);
    }
    return results;
}

StatsSummary summarize(const std::vector<const SavedResult*>& results, int today) {
    StatsSummary s;
    for (const SavedResult* saved : results) {
        const ResultScore score = scoreOf(saved->result);
        ++s.plays;
        s.playedSeconds += saved->result.playedSeconds;
        add(s.total, score);
        const int age = daysBetween(saved->date, today);
        if (age >= 0 && age < kCompareDays) {
            add(s.recent, score);
            ++s.recentPlays;
        } else if (age >= kCompareDays && age < kCompareDays * 2) {
            add(s.previous, score);
            ++s.previousPlays;
        }
        if (countsForBest(*saved) && (!s.best || rankedHigher(*saved, *s.best))) s.best = saved;
    }
    return s;
}

std::vector<float> movingAverage(const std::vector<float>& values, std::size_t window) {
    std::vector<float> averages;
    averages.reserve(values.size());
    float sum = 0.0f;
    for (std::size_t i = 0; i < values.size(); ++i) {
        sum += values[i];
        if (window > 0 && i >= window) sum -= values[i - window];
        const std::size_t count = window > 0 ? std::min(i + 1, window) : i + 1;
        averages.push_back(sum / static_cast<float>(count));
    }
    return averages;
}

std::vector<std::size_t> personalBestUpdates(const std::vector<const SavedResult*>& results) {
    std::vector<std::size_t> updates;
    const SavedResult* best = nullptr;
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (!countsForBest(*results[i])) continue;
        // 自己ベストと同じ決まり（rankedHigher）で、それまでの自己ベストより上なら更新
        if (!best || rankedHigher(*results[i], *best)) {
            best = results[i];
            updates.push_back(i);
        }
    }
    return updates;
}

std::vector<const SavedResult*> topResults(const std::vector<const SavedResult*>& results, std::size_t n) {
    std::vector<const SavedResult*> ranked;
    for (const SavedResult* saved : results) {
        if (countsForBest(*saved)) ranked.push_back(saved);
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const SavedResult* a, const SavedResult* b) { return rankedHigher(*a, *b); });
    if (ranked.size() > n) ranked.resize(n);
    return ranked;
}

std::vector<DayStat> dailyStats(const std::vector<const SavedResult*>& results) {
    std::map<int, DayStat> days;
    for (const SavedResult* saved : results) {
        DayStat& day = days[saved->date];
        day.date = saved->date;
        ++day.plays;
        add(day.score, scoreOf(saved->result));
    }
    std::vector<DayStat> list;
    for (const auto& [date, day] : days) list.push_back(day);
    return list;
}

std::vector<LevelStat> levelStats(const std::vector<const SavedResult*>& results) {
    std::map<int, LevelStat> levels;
    for (const SavedResult* saved : results) {
        const int level = levelOf(saved->result);
        LevelStat& stat = levels[level];
        stat.level = level;
        stat.name = saved->result.aiLevelName;  // 新しい結果の名前で上書き（名前を変えた時は新しい方）
        ++stat.plays;
        add(stat.score, scoreOf(saved->result));
    }
    std::vector<LevelStat> list;
    for (const auto& [level, stat] : levels) list.push_back(stat);
    return list;
}

std::array<HitCount, kSkillSlotCount> hitsBySlot(const std::vector<const SavedResult*>& results) {
    std::array<HitCount, kSkillSlotCount> sums{};
    for (const SavedResult* saved : results) {
        for (std::size_t i = 0; i < sums.size(); ++i) {
            sums[i].casts += saved->result.playerHitsBySlot[i].casts;
            sums[i].hits += saved->result.playerHitsBySlot[i].hits;
        }
    }
    return sums;
}

}  // namespace moba
