#pragma once
// ログファイルの保持ルール。
// どのファイルを消すかの判断だけを行い、ファイル操作はしない（テストしやすくするため）。

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace moba::log {

/// 削除の判断に使う、ログファイル 1 つの情報（パスと作成日）
struct LogFileEntry {
    std::filesystem::path path;
    int date = 0;  // 作成日（ローカル時刻）を YYYYMMDD の整数で表したもの
};

/// 「prefix_YYYYMMDD_HHMMSS.log」（同じ秒に複数作った場合は末尾に _2 などが付く）の形式か
bool isLogFileName(const std::filesystem::path& file, std::string_view prefix);

/// ファイル名に含まれる日付（YYYYMMDD）を取り出す
/// @return 日付。ログファイルの名前の形式でなければ nullopt
std::optional<int> dateFromLogFileName(const std::filesystem::path& file, std::string_view prefix);

/// 日付が newFileDate のログファイルを新しく作る前に、削除すべきファイルを選ぶ。
/// 既存ファイルの日付と newFileDate を合わせた日数が maxDays を超える間、最も古い日のファイルをまとめて削除対象にする。
/// （例: 10 日分が溜まっている状態で 11 日目のファイルを作る → 最も古い 1 日分を削除）
std::vector<std::filesystem::path> selectLogFilesToDelete(const std::vector<LogFileEntry>& files, int newFileDate,
                                                          int maxDays);

}  // namespace moba::log
