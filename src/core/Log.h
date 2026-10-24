#pragma once
// 開発者用ログ
//
// ・logs フォルダに「moba_trainer_YYYYMMDD_HHMMSS.log」の名前で保存する（UTF-8）
// ・1 ファイルが 20MB を超えたら、新しいファイルに切り替える
// ・ファイルの作成日時で数えて最新 10 日分を残す。11 日目のファイルを作る時に、最も古い日のファイルを削除する
// ・書式: 2026-10-24 17:30:01.123 [INFO ] [skill] メッセージ
//
// 使い方: moba::log::info("skill", "Q を発動 castId={}", castId);

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace moba::log {

enum class Level { Trace, Debug, Info, Warn, Error, Fatal };

struct Config {
    std::filesystem::path directory;                    // ログを保存するフォルダ
    Level minLevel = Level::Debug;                      // これより低いレベルは書き込まない
    std::uint64_t maxFileBytes = 20ull * 1024 * 1024;   // 1 ファイルの上限
    int maxDays = 10;                                   // 残す日数
    std::string filePrefix = "moba_trainer";            // ファイル名の先頭
};

void init(const Config& config);
void shutdown();

void setMinLevel(Level level);
Level minLevel();
inline bool isEnabled(Level level) { return level >= minLevel(); }

std::string_view levelName(Level level);
bool parseLevel(std::string_view text, Level& out);

// 今書き込んでいるログファイルのパス（未初期化なら空）
std::filesystem::path currentFilePath();

// 1 行書き込む。category は "skill" や "input" などの分類名
void write(Level level, std::string_view category, std::string_view message);

template <class... Args>
void print(Level level, std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (!isEnabled(level)) return;
    write(level, category, std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
void trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Trace, category, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void debug(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Debug, category, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void info(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Info, category, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Warn, category, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void error(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Error, category, fmt, std::forward<Args>(args)...);
}

}  // namespace moba::log
