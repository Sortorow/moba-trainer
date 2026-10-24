#pragma once
// コマンドライン引数
//
//   --character <id>     メニューを飛ばして、そのキャラで練習場を始める（例: like_emma）
//   --script <file>      入力スクリプトで操作する（自動での動作確認用。--character が無ければ最初のキャラ）
//   --window <W>x<H>     ウィンドウの大きさ（例: 1280x720）
//   --no-debug-panel     デバッグパネルを最初から閉じておく

#include <filesystem>
#include <optional>
#include <string>

namespace moba {

struct CommandLineOptions {
    std::optional<std::string> characterId;
    std::optional<std::filesystem::path> scriptFile;
    std::optional<int> windowWidth;
    std::optional<int> windowHeight;
    bool hideDebugPanel = false;
};

// 解釈できない引数があれば nullopt を返し、error に理由を入れる
std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv, std::string& error);

}  // namespace moba
