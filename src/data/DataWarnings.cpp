#include "data/DataWarnings.h"

#include <set>
#include <utility>

namespace moba {
namespace {

/// 記録した誤り（プログラムに 1 つだけ。ゲームは 1 つのスレッドで動くので、ロックはしない）
struct WarningLog {
    std::set<std::string, std::less<>> keys;  // 記録した誤りの key（同じ誤りを 2 回記録しないため）
    std::vector<std::string> messages;        // 記録した順の説明
};

/// 記録した誤り（初めて使う時に作る）
WarningLog& warningLog() {
    static WarningLog log;
    return log;
}

}  // namespace

bool recordDataWarning(std::string_view key, std::string message) {
    WarningLog& log = warningLog();
    if (!log.keys.emplace(key).second) return false;
    log.messages.push_back(std::move(message));
    return true;
}

const std::vector<std::string>& dataWarnings() { return warningLog().messages; }

void clearDataWarnings() {
    WarningLog& log = warningLog();
    log.keys.clear();
    log.messages.clear();
}

}  // namespace moba
