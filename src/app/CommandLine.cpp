#include "app/CommandLine.h"

#include <charconv>
#include <format>
#include <string_view>

namespace moba {
namespace {

/// 文字列全体を数として読む（途中に数字以外があれば失敗）
/// @return 読めたかどうか
template <class T>
bool parseNumber(std::string_view text, T& out) {
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    return ec == std::errc{} && end == text.data() + text.size();
}

}  // namespace

std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv, std::string& error) {
    CommandLineOptions options;
    auto fail = [&](std::string_view arg, std::string_view value) {
        error = std::format("{} の値が読めません: {}", arg, value);
        return std::nullopt;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        const std::string_view value = hasValue ? std::string_view(argv[i + 1]) : std::string_view{};
        if (arg == "--character" && hasValue) {
            options.characterId = std::string(value);
            ++i;
        } else if (arg == "--mode" && hasValue) {
            if (value != "sandbox" && value != "dodge" && value != "shot") return fail(arg, value);
            options.mode = std::string(value);
            ++i;
        } else if (arg == "--stage" && hasValue) {
            options.stageId = std::string(value);
            ++i;
        } else if (arg == "--enemies" && hasValue) {
            int n = 0;
            if (!parseNumber(value, n) || n < 1 || n > 5) return fail(arg, value);
            options.enemyCount = n;
            ++i;
        } else if (arg == "--ai" && hasValue) {
            int n = 0;
            if (!parseNumber(value, n) || n < 1 || n > 5) return fail(arg, value);
            options.aiLevel = n;
            ++i;
        } else if (arg == "--time" && hasValue) {
            float seconds = 0.0f;
            if (!parseNumber(value, seconds) || seconds <= 0.0f) return fail(arg, value);
            options.timeSeconds = seconds;
            ++i;
        } else if (arg == "--seed" && hasValue) {
            std::uint32_t seed = 0;
            if (!parseNumber(value, seed) || seed == 0) return fail(arg, value);
            options.seed = seed;
            ++i;
        } else if (arg == "--no-cooldown") {
            options.noCooldown = true;
        } else if (arg == "--script" && hasValue) {
            options.scriptFile = std::filesystem::path(value);
            ++i;
        } else if (arg == "--window" && hasValue) {
            const std::size_t x = value.find('x');
            int width = 0;
            int height = 0;
            if (x == std::string_view::npos || !parseNumber(value.substr(0, x), width) ||
                !parseNumber(value.substr(x + 1), height) || width < 320 || height < 240) {
                return fail(arg, value);
            }
            options.windowWidth = width;
            options.windowHeight = height;
            ++i;
        } else if (arg == "--no-debug-panel") {
            options.hideDebugPanel = true;
        } else {
            error = std::format("不明な引数です: {}", arg);
            return std::nullopt;
        }
    }
    return options;
}

}  // namespace moba
