// リザルトの保存のテスト: 1 日の上限・残す期間・直近の結果・保存と読み込み（一時フォルダを使う）

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>

#include "game/ResultStore.h"
#include "ui/RecordsPanel.h"

using namespace moba;

namespace {

/// テスト用の一時フォルダ（作った時に空にし、テストが終わったら消す）
class TempDir {
public:
    /// 一時フォルダの下に name のフォルダを作る（既にあれば中身を消す）
    explicit TempDir(std::string_view name) : path_(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /// フォルダの場所
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// スキルドッジの結果（dodged / total）
MatchResult dodgeResult(int dodged, int total) {
    MatchResult r;
    r.config.mode = GameMode::SkillDodge;
    r.config.characterId = "like_ezreal";
    r.config.enemyCount = 3;
    r.config.aiLevel = 2;
    r.characterName = "like ezreal";
    r.stageName = "障害物なし";
    r.aiLevelName = "ややかんたん";
    r.dodge.total = total;
    r.dodge.dodged = dodged;
    r.dodge.hit = total - dodged;
    return r;
}

/// スキルショットの結果（hits / casts。すべて Q で撃ったものとする）
MatchResult shotResult(int hits, int casts) {
    MatchResult r;
    r.config.mode = GameMode::SkillShot;
    r.config.characterId = "like_emma";
    r.config.targetLevel = 4;
    r.config.targetRadius = 40.0f;
    r.config.targetMoveSpeed = 385.0f;
    r.characterName = "like emma";
    r.aiLevelName = "むずかしい";
    r.targetSizeName = "小さい";
    r.playerHits = HitCount{casts, hits};
    r.playerHitsBySlot[static_cast<std::size_t>(index(SkillSlot::Q))] = HitCount{casts, hits};
    return r;
}

/// date の日の、seconds 秒目の日時
LocalTimestamp at(int date, int seconds) {
    return LocalTimestamp{date, std::format("{:04}-{:02}-{:02} 12:{:02}:{:02}", date / 10000, date / 100 % 100,
                                            date % 100, seconds / 60, seconds % 60)};
}

}  // namespace

TEST_CASE("リザルトの保存: 1 日 20 件を超えると、その日の最も古い結果から消える") {
    TempDir dir("moba_results_test_daily");
    ResultStore store(dir.path());
    store.load(20261025);
    for (int i = 0; i < 22; ++i) REQUIRE(store.save(dodgeResult(i, 30), at(20261025, i)));
    // 別の日の結果は数に含めない
    REQUIRE(store.save(dodgeResult(1, 30), at(20261024, 0)));

    CHECK(store.all(GameMode::SkillDodge).size() == kMaxResultsPerDay + 1);
    // 読み込み直しても同じ（ファイルにも 20 件だけ残っている）
    ResultStore reloaded(dir.path());
    reloaded.load(20261025);
    const std::vector<SavedResult>& all = reloaded.all(GameMode::SkillDodge);
    REQUIRE(all.size() == kMaxResultsPerDay + 1);
    CHECK(all.front().date == 20261024);          // 古い日が先
    CHECK(all[1].result.dodge.dodged == 2);       // 0・1 件目（最も古い 2 件）は消えた
    CHECK(all.back().result.dodge.dodged == 21);  // 最も新しい結果
    CHECK(std::filesystem::exists(dir.path() / "dodge" / "2026-10-25.json"));
}

TEST_CASE("リザルトの保存: 今日を含めて 180 日より前の日の結果は消える") {
    CHECK(selectExpiredDates({20261025, 20260429, 20260428, 20251231}, 20261025, kResultKeepDays) ==
          std::vector<int>{20260428, 20251231});  // 2026-04-29 は 179 日前（残す）、04-28 は 180 日前（消す）

    TempDir dir("moba_results_test_expire");
    {
        ResultStore store(dir.path());
        store.load(20260428);
        REQUIRE(store.save(dodgeResult(5, 10), at(20260428, 0)));
        REQUIRE(store.save(dodgeResult(6, 10), at(20260429, 0)));
    }
    ResultStore store(dir.path());
    store.load(20261025);
    REQUIRE(store.all(GameMode::SkillDodge).size() == 1);
    CHECK(store.all(GameMode::SkillDodge).front().date == 20260429);
    CHECK_FALSE(std::filesystem::exists(dir.path() / "dodge" / "2026-04-28.json"));
}

TEST_CASE("リザルトの保存: モードごとに分けて保存し、直近の結果は新しい順に取り出せる") {
    TempDir dir("moba_results_test_recent");
    ResultStore store(dir.path());
    store.load(20261025);
    for (int i = 0; i < 12; ++i) REQUIRE(store.save(shotResult(i, 20), at(20261024 + i / 6, i)));
    REQUIRE(store.save(dodgeResult(3, 4), at(20261025, 59)));
    CHECK_FALSE(store.save(MatchResult{}, at(20261025, 58)));  // 練習場は保存しない

    const std::vector<SavedResult> recent = store.recent(GameMode::SkillShot, kRecentResultCount);
    REQUIRE(recent.size() == kRecentResultCount);
    CHECK(recent.front().result.playerHits.hits == 11);  // 最も新しい結果が先頭
    CHECK(recent.back().result.playerHits.hits == 2);
    CHECK(store.recent(GameMode::SkillDodge, kRecentResultCount).size() == 1);
    CHECK(std::filesystem::exists(dir.path() / "shot"));
    CHECK(std::filesystem::exists(dir.path() / "dodge"));
}

TEST_CASE("リザルトの保存: 保存した内容（設定・キーごとの命中など）を、読み込み直しても同じに戻せる") {
    TempDir dir("moba_results_test_roundtrip");
    MatchResult original = shotResult(7, 12);
    original.config.stageId = "forest";
    original.stageName = "森";
    original.config.cooldownMode = CooldownMode::None;
    original.config.timeLimitSeconds = 180.0f;
    original.config.seed = 12345;
    original.playedSeconds = 180.0f;
    {
        ResultStore store(dir.path());
        store.load(20261025);
        REQUIRE(store.save(original, at(20261025, 3)));
    }
    ResultStore store(dir.path());
    store.load(20261025);
    REQUIRE(store.all(GameMode::SkillShot).size() == 1);
    const SavedResult& saved = store.all(GameMode::SkillShot).front();
    const MatchResult& r = saved.result;
    CHECK(saved.playedAt == "2026-10-25 12:00:03");
    CHECK(r.config.mode == GameMode::SkillShot);
    CHECK(r.config.characterId == "like_emma");
    CHECK(r.characterName == "like emma");
    CHECK(r.config.stageId == "forest");
    CHECK(r.stageName == "森");
    CHECK(r.config.cooldownMode == CooldownMode::None);
    CHECK(r.config.timeLimitSeconds == doctest::Approx(180.0f));
    CHECK(r.config.seed == 12345u);
    CHECK(r.config.targetLevel == 4);
    CHECK(r.aiLevelName == "むずかしい");
    CHECK(r.config.targetRadius == doctest::Approx(40.0f));
    CHECK(r.targetSizeName == "小さい");
    CHECK(r.config.targetMoveSpeed == doctest::Approx(385.0f));
    CHECK(r.playerHits.hits == 7);
    CHECK(r.playerHits.casts == 12);
    CHECK(r.playerHitsBySlot[static_cast<std::size_t>(index(SkillSlot::Q))].hits == 7);
    CHECK(r.playerHitsBySlot[static_cast<std::size_t>(index(SkillSlot::W))].casts == 0);
}

TEST_CASE("リザルトの保存: 読めないファイルは名前を変えて残し、次の保存で上書きしない") {
    TempDir dir("moba_results_test_broken");
    std::filesystem::create_directories(dir.path() / "dodge");
    std::ofstream(dir.path() / "dodge" / "2026-10-25.json") << "{ broken";
    ResultStore store(dir.path());
    store.load(20261025);
    CHECK(store.all(GameMode::SkillDodge).empty());
    CHECK_FALSE(store.errors().empty());
    CHECK(std::filesystem::exists(dir.path() / "dodge" / "2026-10-25.json.broken"));
    REQUIRE(store.save(dodgeResult(1, 2), at(20261025, 0)));
    CHECK(std::filesystem::exists(dir.path() / "dodge" / "2026-10-25.json.broken"));
}

TEST_CASE("記録の順位: 割合の高い順。同じ割合なら総数が多い順、さらに同じなら新しい順") {
    std::vector<SavedResult> recent;
    auto add = [&](int dodged, int total, int second) {
        SavedResult saved;
        saved.playedAt = at(20261025, second).text;
        saved.result = dodgeResult(dodged, total);
        recent.push_back(saved);
    };
    add(5, 10, 0);   // 50%
    add(9, 10, 1);   // 90%
    add(10, 20, 2);  // 50%（総数が多い）
    add(5, 10, 3);   // 50%（新しい）
    CHECK(rankOf(recent, at(20261025, 1).text) == 1);
    CHECK(rankOf(recent, at(20261025, 2).text) == 2);
    CHECK(rankOf(recent, at(20261025, 3).text) == 3);
    CHECK(rankOf(recent, at(20261025, 0).text) == 4);
    CHECK(rankOf(recent, "none") == 0);
}
