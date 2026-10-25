#pragma once
// ゲームが使うフォルダの場所

#include <filesystem>
#include <optional>
#include <string>

namespace moba {

struct AppPaths {
    std::filesystem::path root;           // data フォルダがある場所（プロジェクトのルート、または exe のフォルダ）
    std::filesystem::path dataDir;        // root/data
    std::filesystem::path charactersDir;  // root/data/characters
    std::filesystem::path stagesDir;      // root/data/stages
    std::filesystem::path settingsFile;   // root/data/settings.jsonc
    std::filesystem::path aiFile;         // root/data/ai.jsonc
    std::filesystem::path sandboxFile;    // root/data/sandbox.jsonc
    std::filesystem::path logsDir;        // root/logs
};

AppPaths makeAppPaths(const std::filesystem::path& root);

// startDir から親フォルダへ順に data/settings.jsonc を探す。
// ビルドフォルダ（build/release）の exe からでも、ソースの data フォルダを直接使えるようにするため。
std::optional<AppPaths> locateAppPaths(const std::filesystem::path& startDir, int maxLevelsUp = 5);

// パスを UTF-8 の文字列にする（ログ・画面表示用）
std::string toUtf8(const std::filesystem::path& path);

}  // namespace moba
