#include "core/LogRetention.h"

#include <set>
#include <string>

#include "core/Paths.h"

namespace moba::log {
namespace {

std::optional<int> parseEightDigits(std::string_view text) {
    if (text.size() < 8) return std::nullopt;
    int value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        const char c = text[i];
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + (c - '0');
    }
    return value;
}

}  // namespace

bool isLogFileName(const std::filesystem::path& file, std::string_view prefix) {
    return dateFromLogFileName(file, prefix).has_value();
}

std::optional<int> dateFromLogFileName(const std::filesystem::path& file, std::string_view prefix) {
    const std::string name = toUtf8(file.filename());
    const std::string_view view = name;
    if (!view.ends_with(".log")) return std::nullopt;
    if (view.size() <= prefix.size() + 1 || !view.starts_with(prefix) || view[prefix.size()] != '_') {
        return std::nullopt;
    }
    return parseEightDigits(view.substr(prefix.size() + 1));
}

std::vector<std::filesystem::path> selectLogFilesToDelete(const std::vector<LogFileEntry>& files, int newFileDate,
                                                          int maxDays) {
    std::set<int> dates;
    for (const LogFileEntry& file : files) dates.insert(file.date);
    dates.insert(newFileDate);

    std::set<int> datesToDelete;
    while (static_cast<int>(dates.size()) > maxDays) {
        const int oldest = *dates.begin();
        if (oldest == newFileDate) break;  // これから作るファイルの日付は消さない
        datesToDelete.insert(oldest);
        dates.erase(dates.begin());
    }

    std::vector<std::filesystem::path> result;
    for (const LogFileEntry& file : files) {
        if (datesToDelete.contains(file.date)) result.push_back(file.path);
    }
    return result;
}

}  // namespace moba::log
