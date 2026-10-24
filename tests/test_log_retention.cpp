#include <doctest/doctest.h>

#include <algorithm>
#include <format>

#include "core/LogRetention.h"

using namespace moba::log;

namespace {

std::vector<LogFileEntry> makeFiles(const std::vector<int>& dates) {
    std::vector<LogFileEntry> files;
    for (std::size_t i = 0; i < dates.size(); ++i) {
        files.push_back({std::format("moba_trainer_{}_{:06}.log", dates[i], i), dates[i]});
    }
    return files;
}

bool contains(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& path) {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

}  // namespace

TEST_CASE("ファイル名から日付を読む") {
    CHECK(dateFromLogFileName("moba_trainer_20261024_173001.log", "moba_trainer") == 20261024);
    CHECK(dateFromLogFileName("moba_trainer_20261024_173001_2.log", "moba_trainer") == 20261024);
    CHECK_FALSE(dateFromLogFileName("other_20261024_173001.log", "moba_trainer").has_value());
    CHECK_FALSE(dateFromLogFileName("moba_trainer_2026_10.log", "moba_trainer").has_value());
    CHECK_FALSE(dateFromLogFileName("moba_trainer_20261024_173001.txt", "moba_trainer").has_value());
}

TEST_CASE("10 日分が溜まった状態で 11 日目のファイルを作ると、最も古い日のファイルをすべて消す") {
    // 1 日目だけ 3 ファイル（20MB を超えて分割された想定）
    std::vector<int> dates = {20261001, 20261001, 20261001};
    for (int day = 2; day <= 10; ++day) dates.push_back(20261000 + day);
    const std::vector<LogFileEntry> files = makeFiles(dates);

    const auto toDelete = selectLogFilesToDelete(files, 20261011, 10);
    CHECK(toDelete.size() == 3);
    for (int i = 0; i < 3; ++i) CHECK(contains(toDelete, files[static_cast<std::size_t>(i)].path));
}

TEST_CASE("既にある日付のファイルを追加しても、何も消さない") {
    std::vector<int> dates;
    for (int day = 1; day <= 10; ++day) dates.push_back(20261000 + day);
    CHECK(selectLogFilesToDelete(makeFiles(dates), 20261010, 10).empty());
}

TEST_CASE("日付は暦ではなく「ログがある日」で数える（間が空いていても 10 日分残す）") {
    // 毎日ではなく、とびとびの 10 日
    const std::vector<int> dates = {20260101, 20260205, 20260310, 20260401, 20260502,
                                    20260603, 20260704, 20260805, 20260906, 20261007};
    const std::vector<LogFileEntry> files = makeFiles(dates);
    CHECK(selectLogFilesToDelete(files, 20261007, 10).empty());

    const auto toDelete = selectLogFilesToDelete(files, 20261024, 10);
    REQUIRE(toDelete.size() == 1);
    CHECK(toDelete.front() == files.front().path);
}
