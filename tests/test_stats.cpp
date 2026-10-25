// 統計の計算のテスト: 絞り込み・要約（平均・自己ベスト・直近 7 日の比較）・移動平均・自己ベストの更新・日ごと・賢さごと・キーごと

#include <doctest/doctest.h>

#include <format>
#include <vector>

#include "game/ResultStats.h"

using namespace moba;

namespace {

/// テスト用の結果を作る
/// @param date 日付（YYYYMMDD）
/// @param count 避けた数（スキルドッジ）/ 当たった数（スキルショット）
/// @param total 総数
SavedResult makeResult(GameMode mode, int date, int count, int total, int level = 3,
                       std::string character = "like_ezreal") {
    SavedResult saved;
    saved.date = date;
    saved.playedAt = std::format("{:04}-{:02}-{:02} 12:00:00", date / 10000, date / 100 % 100, date % 100);
    MatchResult& r = saved.result;
    r.config.mode = mode;
    r.config.characterId = std::move(character);
    r.config.stageId = "open";
    r.playedSeconds = 60.0f;
    if (mode == GameMode::SkillDodge) {
        r.config.aiLevel = level;
        r.dodge = DodgeCount{total, count, total - count};
    } else {
        r.config.targetLevel = level;
        r.playerHits = HitCount{total, count};
        r.playerHitsBySlot[static_cast<std::size_t>(index(SkillSlot::Q))] = HitCount{total, count};
    }
    r.aiLevelName = std::format("賢さ{}", level);
    return saved;
}

/// all の要素をすべて指すポインターの一覧（filterResults を通さずに計算を試す時用）
std::vector<const SavedResult*> pointers(const std::vector<SavedResult>& all) {
    std::vector<const SavedResult*> list;
    for (const SavedResult& saved : all) list.push_back(&saved);
    return list;
}

}  // namespace

TEST_CASE("統計の絞り込み: モード・キャラ・賢さ・期間で絞り込める") {
    const std::vector<SavedResult> all{
        makeResult(GameMode::SkillShot, 20261001, 5, 10, 3),
        makeResult(GameMode::SkillShot, 20261020, 6, 10, 5),
        makeResult(GameMode::SkillShot, 20261025, 7, 10, 3, "like_emma"),
        makeResult(GameMode::SkillDodge, 20261025, 8, 10, 3),
    };
    StatsFilter filter;
    filter.mode = GameMode::SkillShot;
    CHECK(filterResults(all, filter, 20261025).size() == 3);
    filter.characterId = "like_ezreal";
    CHECK(filterResults(all, filter, 20261025).size() == 2);
    filter.level = 3;
    CHECK(filterResults(all, filter, 20261025).size() == 1);
    filter = StatsFilter{GameMode::SkillShot};
    filter.periodDays = 7;  // 10-19〜10-25
    CHECK(filterResults(all, filter, 20261025).size() == 2);
}

TEST_CASE("統計の要約: 平均は数を合計してから割り、自己ベストは総数 10 以上のプレイから選ぶ") {
    const std::vector<SavedResult> all{
        makeResult(GameMode::SkillDodge, 20261001, 1, 1),    // 100% だが総数が少ないので自己ベストにしない
        makeResult(GameMode::SkillDodge, 20261002, 6, 10),   // 60%
        makeResult(GameMode::SkillDodge, 20261003, 16, 20),  // 80%（自己ベスト）
    };
    const StatsSummary s = summarize(pointers(all), 20261025);
    CHECK(s.plays == 3);
    CHECK(s.playedSeconds == doctest::Approx(180.0f));
    CHECK(s.total.count == 23);
    CHECK(s.total.total == 31);
    REQUIRE(s.best != nullptr);
    CHECK(s.best->date == 20261003);
}

TEST_CASE("統計の要約: 直近 7 日（今日を含む）と、その前の 7 日を分けて数える") {
    const std::vector<SavedResult> all{
        makeResult(GameMode::SkillShot, 20261011, 1, 10),  // 14 日前（どちらにも入らない）
        makeResult(GameMode::SkillShot, 20261012, 4, 10),  // 13 日前（前の 7 日）
        makeResult(GameMode::SkillShot, 20261018, 5, 10),  // 7 日前（前の 7 日）
        makeResult(GameMode::SkillShot, 20261019, 7, 10),  // 6 日前（直近 7 日）
        makeResult(GameMode::SkillShot, 20261025, 9, 10),  // 今日（直近 7 日）
    };
    const StatsSummary s = summarize(pointers(all), 20261025);
    CHECK(s.recentPlays == 2);
    CHECK(s.recent.count == 16);
    CHECK(s.previousPlays == 2);
    CHECK(s.previous.count == 9);
}

TEST_CASE("移動平均: それまでの最大 window 回の平均") {
    const std::vector<float> averages = movingAverage({10, 20, 30, 40}, 2);
    REQUIRE(averages.size() == 4);
    CHECK(averages[0] == doctest::Approx(10.0f));
    CHECK(averages[1] == doctest::Approx(15.0f));
    CHECK(averages[2] == doctest::Approx(25.0f));
    CHECK(averages[3] == doctest::Approx(35.0f));
}

TEST_CASE("自己ベストの更新と TOP 3: 総数の少ないプレイは数えず、成績の高い順に並べる") {
    const std::vector<SavedResult> all{
        makeResult(GameMode::SkillShot, 20261001, 5, 10),   // 50%（最初の自己ベスト）
        makeResult(GameMode::SkillShot, 20261002, 4, 10),   // 40%
        makeResult(GameMode::SkillShot, 20261003, 3, 3),    // 100% だが総数が少ない
        makeResult(GameMode::SkillShot, 20261004, 7, 10),   // 70%（更新）
        makeResult(GameMode::SkillShot, 20261005, 14, 20),  // 70%（同じ割合なので更新ではない）
        makeResult(GameMode::SkillShot, 20261006, 9, 10),   // 90%（更新）
    };
    CHECK(personalBestUpdates(pointers(all)) == std::vector<std::size_t>{0, 3, 5});
    const std::vector<const SavedResult*> top = topResults(pointers(all), 3);
    REQUIRE(top.size() == 3);
    CHECK(top[0]->date == 20261006);
    CHECK(top[1]->date == 20261005);  // 同じ 70% なら総数が多い方が上
    CHECK(top[2]->date == 20261004);
}

TEST_CASE("日ごと・賢さごと・キーごとの集計") {
    const std::vector<SavedResult> all{
        makeResult(GameMode::SkillShot, 20261024, 2, 10, 1),
        makeResult(GameMode::SkillShot, 20261025, 4, 10, 3),
        makeResult(GameMode::SkillShot, 20261025, 6, 10, 1),
    };
    const std::vector<DayStat> days = dailyStats(pointers(all));
    REQUIRE(days.size() == 2);
    CHECK(days[0].date == 20261024);
    CHECK(days[1].plays == 2);
    CHECK(days[1].score.count == 10);

    const std::vector<LevelStat> levels = levelStats(pointers(all));
    REQUIRE(levels.size() == 2);
    CHECK(levels[0].level == 1);
    CHECK(levels[0].plays == 2);
    CHECK(levels[0].score.count == 8);
    CHECK(levels[1].name == "賢さ3");

    const auto slots = hitsBySlot(pointers(all));
    CHECK(slots[static_cast<std::size_t>(index(SkillSlot::Q))].hits == 12);
    CHECK(slots[static_cast<std::size_t>(index(SkillSlot::Q))].casts == 30);
    CHECK(slots[static_cast<std::size_t>(index(SkillSlot::W))].casts == 0);
}
