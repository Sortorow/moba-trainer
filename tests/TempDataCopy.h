#pragma once
// テスト用の部品: data フォルダを一時フォルダへ写して使う（本物の data フォルダを書き換えないように）

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <random>
#include <regex>
#include <string>
#include <system_error>

#include "core/Paths.h"

namespace moba::test {

/// data フォルダを一時フォルダへ写して使う。ファイルに書き込むテストや、設定ファイルの一部を書き換えて試すテストで使う
class TempDataCopy {
public:
    /// data フォルダを、一時フォルダの中の新しいフォルダへ丸ごと写す
    TempDataCopy() {
        root_ = std::filesystem::temp_directory_path() / std::format("moba_tests_{}", std::random_device{}());
        std::filesystem::create_directories(root_);
        std::filesystem::copy(std::filesystem::path(MOBA_SOURCE_DIR) / "data", root_ / "data",
                              std::filesystem::copy_options::recursive);
    }
    /// 写した一時フォルダを消す
    ~TempDataCopy() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    TempDataCopy(const TempDataCopy&) = delete;
    TempDataCopy& operator=(const TempDataCopy&) = delete;

    /// 写した data フォルダの各ファイルの場所
    AppPaths paths() const { return makeAppPaths(root_); }

    /// file の中で pattern に合う所を、すべて replacement に置き換える（文字コードや改行は変えない）
    /// @param replacement std::regex_replace の書式（$1 で 1 つ目のかっこの中身）
    /// @return 置き換えた数（0 なら、パターンが見つからなかった）
    static int rewrite(const std::filesystem::path& file, const std::regex& pattern, const std::string& replacement) {
        std::string text;
        {
            std::ifstream in(file, std::ios::binary);
            text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        const auto count =
            std::distance(std::sregex_iterator(text.begin(), text.end(), pattern), std::sregex_iterator());
        std::ofstream(file, std::ios::binary | std::ios::trunc) << std::regex_replace(text, pattern, replacement);
        return static_cast<int>(count);
    }

private:
    std::filesystem::path root_;
};

}  // namespace moba::test
