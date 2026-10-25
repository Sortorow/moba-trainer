#pragma once
// コマンドライン引数（主に自動での動作確認用）
//
//   --character <id>     メニューを飛ばして、そのキャラで始める（例: like_emma）
//   --mode <name>        sandbox（練習場、既定）/ dodge（スキルドッジ）
//   --stage <id>         ステージ（data/stages の id。例: open / alley / forest。無ければ一覧の最初のステージ）
//   --enemies <n>        スキルドッジの敵の数（1〜5）
//   --ai <n>             スキルドッジの敵の賢さ（1〜5）
//   --time <seconds>     スキルドッジの制限時間（秒。メニューでは 1〜5 分）
//   --seed <n>           乱数の種（同じ展開を再現したい時）
//   --no-cooldown        クールダウン「なし」で始める
//   --script <file>      入力スクリプトで操作する（--character が無ければ最初のキャラ）
//   --window <W>x<H>     ウィンドウの大きさ（例: 1280x720。Windows の拡大率を掛けない実ピクセル）
//   --no-debug-panel     デバッグパネルを最初から閉じておく

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace moba {

struct CommandLineOptions {
    std::optional<std::string> characterId;
    std::optional<std::string> mode;
    std::optional<std::string> stageId;
    std::optional<int> enemyCount;
    std::optional<int> aiLevel;
    std::optional<float> timeSeconds;
    std::optional<std::uint32_t> seed;
    bool noCooldown = false;
    std::optional<std::filesystem::path> scriptFile;
    std::optional<int> windowWidth;
    std::optional<int> windowHeight;
    bool hideDebugPanel = false;
};

/// コマンドライン引数を読む（使える引数は、このファイルの先頭の一覧）。
/// 解釈できない引数があれば nullopt を返し、error に理由を入れる
std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv, std::string& error);

}  // namespace moba
