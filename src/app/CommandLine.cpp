#include "app/CommandLine.h"

#include <charconv>
#include <format>
#include <string_view>

namespace moba {
namespace {

bool parseInt(std::string_view text, int& out) {
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    return ec == std::errc{} && end == text.data() + text.size();
}

}  // namespace

std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv, std::string& error) {
    CommandLineOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--character" && hasValue) {
            options.characterId = argv[++i];
        } else if (arg == "--script" && hasValue) {
            options.scriptFile = std::filesystem::path(argv[++i]);
        } else if (arg == "--window" && hasValue) {
            const std::string_view value = argv[++i];
            const std::size_t x = value.find('x');
            int width = 0;
            int height = 0;
            const bool ok = x != std::string_view::npos && parseInt(value.substr(0, x), width) &&
                            parseInt(value.substr(x + 1), height) && width >= 320 && height >= 240;
            if (!ok) {
                error = std::format("--window の値が読めません: {}", value);
                return std::nullopt;
            }
            options.windowWidth = width;
            options.windowHeight = height;
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
