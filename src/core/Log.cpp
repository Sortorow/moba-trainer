#include "core/Log.h"

#include "core/LogRetention.h"
#include "core/Paths.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <share.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <optional>
#include <vector>

namespace moba::log {
namespace {

struct State {
    std::mutex mutex;
    Config config;
    std::FILE* file = nullptr;
    std::filesystem::path filePath;
    std::uint64_t fileBytes = 0;
    std::atomic<Level> minLevel{Level::Debug};
    bool initialized = false;
};

State& state() {
    static State s;
    return s;
}

struct Timestamp {
    std::tm local{};
    int millis = 0;
};

Timestamp now() {
    const auto tp = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    Timestamp ts;
    localtime_s(&ts.local, &t);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    ts.millis = static_cast<int>(ms % 1000);
    return ts;
}

int dateNumber(const std::tm& tm) { return (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday; }

std::string formatLine(const Timestamp& ts, Level level, std::string_view category, std::string_view message) {
    const std::tm& t = ts.local;
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} [{:<5}] [{}] {}\n", t.tm_year + 1900, t.tm_mon + 1,
                       t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, ts.millis, levelName(level), category, message);
}

// ファイルの作成日時（ローカル時刻の YYYYMMDD）。取得できなければ nullopt
std::optional<int> fileCreationDate(const std::filesystem::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA attr{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr)) return std::nullopt;
    SYSTEMTIME utc{};
    SYSTEMTIME local{};
    if (!FileTimeToSystemTime(&attr.ftCreationTime, &utc)) return std::nullopt;
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return std::nullopt;
    return local.wYear * 10000 + local.wMonth * 100 + local.wDay;
}

// ファイルへ書き込む（ロックを取った状態で呼ぶ）
void writeRaw(State& s, const std::string& line) {
    if (!s.file) return;
    std::fwrite(line.data(), 1, line.size(), s.file);
    std::fflush(s.file);  // クラッシュしても直前までの内容が残るよう、毎行フラッシュする
    s.fileBytes += line.size();
}

// 保持日数を超えた古いログを削除し、結果の説明を返す（ロックを取った状態で呼ぶ）
std::vector<std::string> applyRetention(State& s, int newFileDate) {
    std::vector<LogFileEntry> entries;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(s.config.directory, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::filesystem::path& path = entry.path();
        if (!isLogFileName(path, s.config.filePrefix)) continue;
        // 作成日時で管理する。取得できなかった場合だけファイル名のタイムスタンプを使う
        std::optional<int> date = fileCreationDate(path);
        if (!date) date = dateFromLogFileName(path, s.config.filePrefix);
        if (!date) continue;
        entries.push_back({path, *date});
    }

    std::vector<std::string> notes;
    for (const std::filesystem::path& path : selectLogFilesToDelete(entries, newFileDate, s.config.maxDays)) {
        std::error_code removeError;
        if (std::filesystem::remove(path, removeError)) {
            notes.push_back(std::format("古いログを削除しました: {}", toUtf8(path.filename())));
        } else {
            notes.push_back(std::format("古いログの削除に失敗しました: {} (code={})", toUtf8(path.filename()),
                                        removeError.value()));
        }
    }
    return notes;
}

// 新しいログファイルを開く（ロックを取った状態で呼ぶ）
void openNewFile(State& s, std::string_view reason) {
    if (s.file) {
        std::fclose(s.file);
        s.file = nullptr;
    }

    const Timestamp ts = now();
    std::error_code ec;
    std::filesystem::create_directories(s.config.directory, ec);
    const std::vector<std::string> notes = applyRetention(s, dateNumber(ts.local));

    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &ts.local);
    std::filesystem::path path = s.config.directory / std::format("{}_{}.log", s.config.filePrefix, stamp);
    for (int seq = 2; std::filesystem::exists(path, ec); ++seq) {
        path = s.config.directory / std::format("{}_{}_{}.log", s.config.filePrefix, stamp, seq);
    }

    // ゲームの起動中でもエディターなどでログを読めるように、読み取りは共有する（書き込みだけ禁止する）
    s.file = _wfsopen(path.c_str(), L"wb", _SH_DENYWR);
    if (!s.file) {
        OutputDebugStringW(L"[moba_trainer] ログファイルを開けませんでした\n");
        return;
    }
    s.filePath = path;
    s.fileBytes = 0;

    writeRaw(s, formatLine(ts, Level::Info, "log", std::format("ログファイルを開始しました（理由: {}）", reason)));
    for (const std::string& note : notes) writeRaw(s, formatLine(ts, Level::Info, "log", note));
}

}  // namespace

void init(const Config& config) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    s.config = config;
    s.minLevel = config.minLevel;
    s.initialized = true;
    openNewFile(s, "起動");
}

void shutdown() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    if (s.file) {
        writeRaw(s, formatLine(now(), Level::Info, "log", "ログを終了します"));
        std::fclose(s.file);
        s.file = nullptr;
    }
    s.initialized = false;
}

void setMinLevel(Level level) { state().minLevel = level; }

Level minLevel() { return state().minLevel.load(); }

std::string_view levelName(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?";
}

bool parseLevel(std::string_view text, Level& out) {
    if (text == "trace") out = Level::Trace;
    else if (text == "debug") out = Level::Debug;
    else if (text == "info") out = Level::Info;
    else if (text == "warn") out = Level::Warn;
    else if (text == "error") out = Level::Error;
    else return false;
    return true;
}

std::filesystem::path currentFilePath() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    return s.filePath;
}

void write(Level level, std::string_view category, std::string_view message) {
    State& s = state();
    if (level < s.minLevel.load()) return;
    const std::string line = formatLine(now(), level, category, message);

    std::lock_guard lock(s.mutex);
    if (!s.initialized) {
        OutputDebugStringA(line.c_str());
        return;
    }
    if (s.file && s.fileBytes + line.size() > s.config.maxFileBytes) {
        openNewFile(s, std::format("サイズ上限 {}MB に到達", s.config.maxFileBytes / (1024 * 1024)));
    }
    writeRaw(s, line);
#ifndef NDEBUG
    std::fputs(line.c_str(), stderr);
#endif
}

}  // namespace moba::log
