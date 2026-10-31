// コマンドライン引数の読み込み（app/CommandLine）と、UTF-8 の文字列とパスの変換（core/Paths）のテスト

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "app/CommandLine.h"
#include "core/Paths.h"

using namespace moba;

TEST_CASE("コマンドライン引数: 数・名前・フラグを読み、読めない値や知らない引数はエラーにする") {
    std::string error;
    const std::optional<CommandLineOptions> options =
        parseCommandLine({"--character", "like_emma", "--mode", "dodge", "--enemies", "3", "--seed", "42", "--window",
                          "1280x720", "--no-cooldown"},
                         error);
    REQUIRE(options.has_value());
    CHECK(*options->characterId == "like_emma");
    CHECK(*options->mode == "dodge");
    CHECK(*options->enemyCount == 3);
    CHECK(*options->seed == 42u);
    CHECK(*options->windowWidth == 1280);
    CHECK(*options->windowHeight == 720);
    CHECK(options->noCooldown);

    CHECK_FALSE(parseCommandLine({"--enemies", "9"}, error).has_value());
    CHECK(error == "--enemies の値が読めません: 9");
    CHECK_FALSE(parseCommandLine({"--unknown"}, error).has_value());
    CHECK(error == "不明な引数です: --unknown");
    CHECK(parseCommandLine({}, error).has_value());  // 引数なし = メニューから始める
}

TEST_CASE("コマンドライン引数: 日本語や Shift_JIS で書けない文字を含むパスも、そのままのパスになる（B5）") {
    std::string error;
    const std::optional<CommandLineOptions> options =
        parseCommandLine({"--script", "C:/練習/한글𠮷/input.txt", "--results-dir", "D:/結果/𠮷野家"}, error);
    REQUIRE(options.has_value());
    // UTF-16 で比べる（Windows に渡すパスは UTF-16。UTF-8 を Shift_JIS として読むと、ここが合わなくなる）
    CHECK(options->scriptFile->wstring() == L"C:/練習/한글𠮷/input.txt");
    CHECK(options->resultsDir->wstring() == L"D:/結果/𠮷野家");
}

TEST_CASE("コマンドライン引数: エラーの文に入る値は UTF-8 のまま（文字化けしない）") {
    std::string error;
    CHECK_FALSE(parseCommandLine({"--mode", "練習"}, error).has_value());
    CHECK(error == "--mode の値が読めません: 練習");
}

TEST_CASE("pathFromUtf8 と toUtf8 は互いに逆で、Shift_JIS で書けない名前のファイルも開ける") {
    const std::string utf8 = "한글𠮷/日本語.txt";
    CHECK(toUtf8(pathFromUtf8(utf8)) == utf8);

    const std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                      std::format("moba_tests_{}", std::random_device{}()) / pathFromUtf8("한글𠮷");
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = pathFromUtf8(toUtf8(dir) + "/日本語.txt");
    std::ofstream(file) << "ok";
    CHECK(std::filesystem::exists(dir / L"日本語.txt"));
    std::error_code ec;
    std::filesystem::remove_all(dir.parent_path(), ec);
}
