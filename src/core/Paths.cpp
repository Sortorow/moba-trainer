#include "core/Paths.h"

namespace moba {

AppPaths makeAppPaths(const std::filesystem::path& root) {
    AppPaths paths;
    paths.root = root;
    paths.dataDir = root / "data";
    paths.charactersDir = paths.dataDir / "characters";
    paths.settingsFile = paths.dataDir / "settings.jsonc";
    paths.aiFile = paths.dataDir / "ai.jsonc";
    paths.logsDir = root / "logs";
    return paths;
}

std::optional<AppPaths> locateAppPaths(const std::filesystem::path& startDir, int maxLevelsUp) {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(startDir, ec);
    for (int level = 0; level <= maxLevelsUp && !dir.empty(); ++level) {
        if (std::filesystem::exists(dir / "data" / "settings.jsonc", ec)) return makeAppPaths(dir);
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return std::nullopt;
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

}  // namespace moba
