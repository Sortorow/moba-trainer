// 設定ファイル（data/settings.jsonc）と、コメントを残したままの値の書き換え（core/JsonFile）のテスト

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "TempDataCopy.h"
#include "core/JsonFile.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "data/Settings.h"

using namespace moba;
// 解像度を選ぶと settings.jsonc に書き込むので、data フォルダを一時フォルダへ写して使う
using test::markModified;
using test::readText;
using test::TempDataCopy;

namespace {

/// text の中の range の部分
std::string slice(const std::string& text, const TextRange& range) {
    return text.substr(range.begin, range.end - range.begin);
}

}  // namespace

TEST_CASE("settings.jsonc: 既定の値（縦の見える範囲 2000、ウィンドウ 1920x1080）") {
    // window はメニューで解像度を選ぶと書き換わるので、ファイルの値ではなく、書かなかった時の既定値で確かめる
    const SettingsLoadResult loaded = loadSettings(makeAppPaths(MOBA_SOURCE_DIR).settingsFile);
    REQUIRE(loaded.error.empty());
    CHECK(loaded.settings.cameraViewHeight == doctest::Approx(2000.0f));
    CHECK(Settings{}.cameraViewHeight == doctest::Approx(2000.0f));
    CHECK(Settings{}.windowWidth == 1920);
    CHECK(Settings{}.windowHeight == 1080);
}

TEST_CASE("findJsoncValue: 入れ子のキーの値（数・文字列・true）を、コメントや文字列の中と区別して探す") {
    const std::string text =
        "/* \"window\": { \"width\": 1 } */\n"
        "{\n"
        "  \"width\": 10,  // 一番外側の width（window の中ではない）\n"
        "  \"text\": \"文字列の中の \\\"width\\\": 20\",\n"
        "  \"other\": { \"width\": 30 },\n"
        "  \"window\": {\n"
        "    // \"width\": 40\n"
        "    \"inner\": { \"width\": 50 },\n"
        "    \"width\" : 1920,\n"
        "    \"height\": 1080 /* 高さ */, \"name\": \"大きい\", \"on\": true\n"
        "  }\n"
        "}\n";
    const auto find = [&](std::vector<std::string> path) {
        const std::optional<TextRange> range = findJsoncValue(text, path);
        return range ? slice(text, *range) : std::string("（なし）");
    };
    CHECK(find({"width"}) == "10");
    CHECK(find({"window", "width"}) == "1920");
    CHECK(find({"window", "height"}) == "1080");
    CHECK(find({"window", "name"}) == "\"大きい\"");
    CHECK(find({"window", "on"}) == "true");
    CHECK(find({"window", "inner", "width"}) == "50");
    CHECK(find({"other", "width"}) == "30");
    // 無いキー・オブジェクトの値は見つからない
    CHECK(find({"window", "depth"}) == "（なし）");
    CHECK(find({"other", "height"}) == "（なし）");  // other の外にある height は、other の中のものではない
    CHECK(find({"window"}) == "（なし）");           // 値がオブジェクト
    CHECK(find({}) == "（なし）");
}

TEST_CASE("settings.jsonc: 解像度の保存は window の width・height だけを書き換え、コメントは残る") {
    TempDataCopy copy;
    const std::filesystem::path file = copy.paths().settingsFile;
    const std::string before = readText(file);
    // 今の値（メニューで選ぶと変わる）とは別の大きさを保存する
    const Settings current = loadSettings(file).settings;
    const int width = current.windowWidth == 1280 ? 1600 : 1280;
    const int height = current.windowHeight == 720 ? 900 : 720;

    REQUIRE(saveWindowSize(file, width, height).empty());
    const std::string after = readText(file);
    CHECK(after.find("// MOBA Trainer の設定ファイル") != std::string::npos);  // 先頭のコメントは残っている
    // 変わったのは数字の部分だけ
    const auto digits = [](int value) { return std::to_string(value).size(); };
    CHECK(after.size() + digits(current.windowWidth) + digits(current.windowHeight) ==
          before.size() + digits(width) + digits(height));

    const SettingsLoadResult reloaded = loadSettings(file);
    REQUIRE(reloaded.error.empty());
    CHECK(reloaded.settings.windowWidth == width);
    CHECK(reloaded.settings.windowHeight == height);
    CHECK(reloaded.settings.cameraViewHeight == doctest::Approx(current.cameraViewHeight));  // 他の値は変わらない
}

TEST_CASE("settings.jsonc: window の width か height が無ければ、ファイルを変えずに保存できない") {
    TempDataCopy copy;
    const std::filesystem::path file = copy.paths().settingsFile;
    const std::string text =
        "{\n  // \"window\": { \"width\": 1, \"height\": 2 }\n  \"window\": { \"width\": 1600 }\n}\n";
    std::ofstream(file, std::ios::binary | std::ios::trunc) << text;

    const std::string error = saveWindowSize(file, 1920, 1080);
    CHECK(error.find("window.height") != std::string::npos);
    CHECK(readText(file) == text);  // width も書き換えていない
}

TEST_CASE("GameData: 解像度を保存すると、読み込んだ設定にも反映し、自分の書き込みでは読み直さない") {
    TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    const int generation = data.generation();
    const std::size_t errorCount = data.errors().size();

    data.saveWindowSize(1600, 900);
    CHECK(data.settings().windowWidth == 1600);
    CHECK(data.settings().windowHeight == 900);
    CHECK(data.errors().size() == errorCount);  // 保存に失敗していない
    CHECK_FALSE(data.reloadIfChanged());
    CHECK(data.generation() == generation);
    CHECK(loadSettings(copy.paths().settingsFile).settings.windowWidth == 1600);
}

TEST_CASE("GameData: ファイルの変更は、読み込まずに分かる（プレイ中は読み込まず、変更があることだけを知らせる）") {
    TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    CHECK_FALSE(data.hasChangedFiles());
    const auto pieceRadius = [&] {
        return data.character("like_emma")->skill(SkillSlot::D)->numOr("pieceRadius", 0.0f);
    };
    const float before = pieceRadius();
    REQUIRE(before != doctest::Approx(0.25f));

    // キャラのファイルを書き換える
    const std::filesystem::path file = copy.paths().charactersDir / "like_emma.jsonc";
    REQUIRE(TempDataCopy::rewrite(file, std::regex(R"re(("pieceRadius"\s*:\s*)[0-9.]+)re"), "$1 0.25") == 1);
    markModified(file);

    const int generation = data.generation();
    CHECK(data.hasChangedFiles());
    CHECK(data.hasChangedFiles());  // 確かめただけでは、読み込んだことにならない
    CHECK(data.generation() == generation);
    CHECK(pieceRadius() == doctest::Approx(before));

    REQUIRE(data.reloadIfChanged());
    CHECK_FALSE(data.hasChangedFiles());
    CHECK(pieceRadius() == doctest::Approx(0.25f));
}

TEST_CASE("GameData: 解像度を保存しても、まだ読んでいない別のファイルの変更を、読んだことにしない") {
    TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    const auto pieceRadius = [&] {
        return data.character("like_emma")->skill(SkillSlot::D)->numOr("pieceRadius", 0.0f);
    };
    REQUIRE(pieceRadius() != doctest::Approx(0.25f));

    // キャラのファイルを保存した後（まだ読んでいない）、メニューで解像度を選ぶ
    const std::filesystem::path file = copy.paths().charactersDir / "like_emma.jsonc";
    REQUIRE(TempDataCopy::rewrite(file, std::regex(R"re(("pieceRadius"\s*:\s*)[0-9.]+)re"), "$1 0.25") == 1);
    markModified(file);
    data.saveWindowSize(1600, 900);

    CHECK(data.hasChangedFiles());  // キャラのファイルの変更は残っている
    REQUIRE(data.reloadIfChanged());
    CHECK(pieceRadius() == doctest::Approx(0.25f));
    CHECK(data.settings().windowWidth == 1600);  // 保存した解像度も、読み直した後に残っている
    CHECK_FALSE(data.hasChangedFiles());
}

TEST_CASE("GameData: 解像度を保存する前に settings.jsonc が書き換えられていたら、保存した後に読み直す") {
    TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    const float before = data.settings().cameraViewHeight;
    const float edited = before == 2500.0f ? 3000.0f : 2500.0f;

    // settings.jsonc の別の項目を保存した後（まだ読んでいない）、メニューで解像度を選ぶ
    const std::filesystem::path file = copy.paths().settingsFile;
    REQUIRE(TempDataCopy::rewrite(file, std::regex(R"re(("cameraViewHeight"\s*:\s*)[0-9.]+)re"),
                                  std::format("$1 {}", edited)) == 1);
    markModified(file);
    data.saveWindowSize(1600, 900);

    // 解像度だけを書き戻しても、書き換えた項目はファイルに残っているので、読み直して反映する
    CHECK(data.hasChangedFiles());
    REQUIRE(data.reloadIfChanged());
    CHECK(data.settings().cameraViewHeight == doctest::Approx(edited));
    CHECK(data.settings().windowWidth == 1600);
    CHECK(data.settings().windowHeight == 900);
}
