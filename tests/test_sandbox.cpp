// 練習場のダミーの設定（data/sandbox.jsonc）のテスト

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

#include "core/Paths.h"
#include "data/GameData.h"
#include "data/SandboxConfig.h"
#include "game/Match.h"

using namespace moba;

namespace {

// data フォルダを一時フォルダへ写して使う（プリセットを選ぶと sandbox.jsonc に書き込むため、本物を書き換えないように）
class TempDataCopy {
public:
    /// data フォルダを、一時フォルダの中の新しいフォルダへ丸ごと写す
    TempDataCopy() {
        root_ = std::filesystem::temp_directory_path() / std::format("moba_tests_sandbox_{}", std::random_device{}());
        std::filesystem::create_directories(root_);
        std::filesystem::copy(std::filesystem::path(MOBA_SOURCE_DIR) / "data", root_ / "data",
                              std::filesystem::copy_options::recursive);
    }
    /// 写した一時フォルダを消す
    ~TempDataCopy() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    /// 写した data フォルダの各ファイルの場所
    AppPaths paths() const { return makeAppPaths(root_); }

private:
    std::filesystem::path root_;
};

/// ファイルの中身をそのまま（文字コードや改行を変えずに）読む
std::string readText(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// 練習場のダミー（自キャラ以外）
std::vector<const Unit*> dummyUnits(const Match& match) {
    std::vector<const Unit*> list;
    for (const Unit& unit : match.world().units()) {
        if (unit.team == Team::Enemy) list.push_back(&unit);
    }
    return list;
}

}  // namespace

TEST_CASE("sandbox.jsonc: プリセットが読み込める") {
    const SandboxConfigLoadResult result =
        loadSandboxConfig(std::filesystem::path(MOBA_SOURCE_DIR) / "data" / "sandbox.jsonc");
    for (const std::string& warning : result.warnings) MESSAGE(warning);
    REQUIRE(result.error.empty());
    CHECK(result.warnings.empty());
    REQUIRE(result.config.current() != nullptr);

    const SandboxPreset* standard = result.config.preset("標準");
    REQUIRE(standard != nullptr);
    REQUIRE(standard->dummies.size() == 3);
    CHECK_FALSE(standard->dummies[0].patrolTo.has_value());  // A は静止
    CHECK(standard->dummies[1].patrolTo.has_value());        // B は往復
    CHECK(standard->dummies[0].radius == doctest::Approx(65.0f));
}

TEST_CASE("sandbox.jsonc: プリセットを選ぶと \"preset\" だけが書き換わり、コメントは残る") {
    TempDataCopy copy;
    const AppPaths paths = copy.paths();
    const std::string before = readText(paths.sandboxFile);
    // 今選ばれているプリセット（ゲームで選び直すと変わる）とは別のプリセットを選ぶ
    const std::string current = loadSandboxConfig(paths.sandboxFile).config.selectedPreset;
    const std::string next = current == "標準" ? "小さい的" : "標準";

    REQUIRE(saveSelectedSandboxPreset(paths.sandboxFile, next).empty());
    const std::string after = readText(paths.sandboxFile);
    CHECK(after.find("\"preset\": \"" + next + "\"") != std::string::npos);
    CHECK(after.find("■ 書き方") != std::string::npos);                   // 先頭のコメントは残っている
    CHECK(after.size() == before.size() + next.size() - current.size());  // 変わったのは名前の部分だけ

    const SandboxConfigLoadResult reloaded = loadSandboxConfig(paths.sandboxFile);
    REQUIRE(reloaded.error.empty());
    CHECK(reloaded.config.selectedPreset == next);
}

TEST_CASE("練習場: 選んだプリセットのダミーが置かれ、プレイの途中で切り替えると置き直される") {
    TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    REQUIRE(data.selectSandboxPreset("標準"));

    MatchConfig config;
    config.mode = GameMode::Sandbox;
    config.characterId = "like_ezreal";
    Match match(data, config);
    REQUIRE(match.valid());
    REQUIRE(dummyUnits(match).size() == 3);
    CHECK(dummyUnits(match)[0]->radius == doctest::Approx(65.0f));

    // プリセットを選び直すと、ダミーが置き直され、ファイルにも保存される
    REQUIRE(data.selectSandboxPreset("距離の練習"));
    match.refreshDummiesIfChanged();
    CHECK(dummyUnits(match).size() == 5);
    CHECK(match.dummies().brains().size() == 5);
    CHECK(loadSandboxConfig(copy.paths().sandboxFile).config.selectedPreset == "距離の練習");

    // 存在しない名前は選べない
    CHECK_FALSE(data.selectSandboxPreset("ないプリセット"));

    // ダミーの設定が変わっていなければ、置き直さない（ダミーの ID が変わらない）
    const EntityId firstId = dummyUnits(match)[0]->id;
    data.loadAll();
    match.refreshDummiesIfChanged();
    CHECK(dummyUnits(match)[0]->id == firstId);
}
