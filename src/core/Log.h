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
    std::filesystem::path directory;                   // ログを保存するフォルダ
    Level minLevel = Level::Debug;                     // これより低いレベルは書き込まない
    std::uint64_t maxFileBytes = 20ull * 1024 * 1024;  // 1 ファイルの上限
    int maxDays = 10;                                  // 残す日数
    std::string filePrefix = "moba_trainer";           // ファイル名の先頭
};

/// ログを始める。保存するフォルダを作り、古いログを削除してから、新しいログファイルを開く
void init(const Config& config);
/// ログを終える（ファイルを閉じる）。終了時に呼ぶ
void shutdown();

/// 書き込む最低レベルを変える（settings.jsonc の logLevel）。これより低いレベルのログは捨てる
void setMinLevel(Level level);
/// 今の、書き込む最低レベル
Level minLevel();
/// このレベルのログが書き込まれるか（重い文字列を作る前の確認用）
inline bool isEnabled(Level level) { return level >= minLevel(); }

/// レベルの表示名（"INFO" など。ログの各行に入る）
std::string_view levelName(Level level);
/// 設定ファイルの文字（"trace" / "debug" / "info" / "warn" / "error"）をレベルにする
/// @param out 読めた時のレベル
/// @return 読めたかどうか（知らない文字なら false で、out は変えない）
bool parseLevel(std::string_view text, Level& out);

/// 今書き込んでいるログファイルのパス（未初期化なら空）
std::filesystem::path currentFilePath();

/// 1 行書き込む。ファイルが上限を超えそうなら、新しいファイルに切り替える
/// @param category "skill" や "input" などの分類名（ログの [ ] の中に入る）
void write(Level level, std::string_view category, std::string_view message);

/// std::format の書式で 1 行書き込む。レベルが低くて書き込まない時は、文字列を作らない
template <class... Args>
void print(Level level, std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (!isEnabled(level)) return;
    write(level, category, std::format(fmt, std::forward<Args>(args)...));
}

/// TRACE（最も詳しい）のログを書く。毎フレームの細かい様子など
template <class... Args>
void trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Trace, category, fmt, std::forward<Args>(args)...);
}

/// DEBUG のログを書く。不具合を調べる時に役立つ途中経過など
template <class... Args>
void debug(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Debug, category, fmt, std::forward<Args>(args)...);
}

/// INFO のログを書く。スキルの発動・命中など、主な出来事
template <class... Args>
void info(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Info, category, fmt, std::forward<Args>(args)...);
}

/// WARN のログを書く。動きは続けられるが、設定の誤りなど気を付けるべきこと
template <class... Args>
void warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Warn, category, fmt, std::forward<Args>(args)...);
}

/// ERROR のログを書く。設定ファイルが読めないなど、うまく動かなかったこと
template <class... Args>
void error(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    print<Args...>(Level::Error, category, fmt, std::forward<Args>(args)...);
}

}  // namespace moba::log
