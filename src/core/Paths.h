#pragma once
// ゲームが使うフォルダの場所

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace moba {

/// ゲームが使うファイルとフォルダの場所（data フォルダの各設定ファイルと logs・results フォルダ）
struct AppPaths {
    std::filesystem::path root;           // data フォルダがある場所（プロジェクトのルート、または exe のフォルダ）
    std::filesystem::path dataDir;        // root/data
    std::filesystem::path charactersDir;  // root/data/characters
    std::filesystem::path stagesDir;      // root/data/stages
    std::filesystem::path settingsFile;   // root/data/settings.jsonc
    std::filesystem::path aiFile;         // root/data/ai.jsonc
    std::filesystem::path sandboxFile;    // root/data/sandbox.jsonc
    std::filesystem::path logsDir;        // root/logs
    std::filesystem::path resultsDir;     // root/results（保存したリザルト）
};

/// root を基準に、data フォルダの各ファイルと logs・results フォルダの場所を決める（ファイルがあるかは確かめない）
AppPaths makeAppPaths(const std::filesystem::path& root);

/// startDir から親フォルダへ順に data/settings.jsonc を探す。
/// ビルドフォルダ（build/release）の exe からでも、ソースの data フォルダを直接使えるようにするため。
/// @param maxLevelsUp 親フォルダへさかのぼる回数の上限
/// @return 見つかった場所。見つからなければ nullopt
std::optional<AppPaths> locateAppPaths(const std::filesystem::path& startDir, int maxLevelsUp = 5);

/// パスを UTF-8 の文字列にする（ログ・画面表示用。日本語のフォルダ名も文字化けしない）
std::string toUtf8(const std::filesystem::path& path);

/// UTF-8 の文字列をパスにする（toUtf8 の逆）。
/// std::filesystem::path(std::string) は、文字列を Windows のコードページ（日本語なら Shift_JIS）として読むので、
/// UTF-8 の文字列（コマンドライン引数など）からパスを作る時はこれを使う
std::filesystem::path pathFromUtf8(std::string_view text);

}  // namespace moba
