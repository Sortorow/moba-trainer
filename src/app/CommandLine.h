#pragma once
// コマンドライン引数（主に自動での動作確認用）
//
//   --character <id>     メニューを飛ばして、そのキャラで始める（例: like_emma）
//   --mode <name>        sandbox（練習場、既定）/ dodge（スキルドッジ）/ shot（スキルショット）
//   --stage <id>         ステージ（data/stages の id。例: open / alley / forest。無ければ一覧の最初のステージ）
//   --enemies <n>        スキルドッジの敵の数（1〜5）
//   --ai <n>             スキルドッジの敵の賢さ・スキルショットの的の賢さ（1〜5）
//   --time <seconds>     スキルドッジ・スキルショットの制限時間（秒。メニューでは 1〜5 分）
//   --seed <n>           乱数の種（同じ展開を再現したい時）
//   --no-cooldown        クールダウン「なし」で始める
//   --script <file>      入力スクリプトで操作する（--character が無ければ最初のキャラ）
//   --window <W>x<H>     ウィンドウの大きさ（例: 1280x720。Windows の拡大率を掛けない実ピクセル）
//   --no-debug-panel     デバッグパネルを最初から閉じておく
//   --results-dir <dir>  リザルトの保存先（既定は results フォルダ）。入力スクリプトの時は、これを指定した時だけ保存する
//   --screen <name>      メニューの画面を開いて始める（動作確認用。プレイは始めない）: records（記録）/
//                        stats-dodge（統計のスキルドッジ）/ stats-shot（統計のスキルショット）

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moba {

/// コマンドライン引数で指定された内容（指定されなかった項目は空。引数の一覧はこのファイルの先頭）
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
    std::optional<std::filesystem::path> resultsDir;
    std::optional<std::string> screen;
};

/// コマンドライン引数を読む（使える引数は、このファイルの先頭の一覧）。
/// 解釈できない引数があれば nullopt を返し、error に理由（UTF-8）を入れる
/// @param args exe の名前を除いた引数。UTF-8 の文字列（platform::commandLineArgs() の戻り値）
std::optional<CommandLineOptions> parseCommandLine(const std::vector<std::string>& args, std::string& error);

}  // namespace moba
