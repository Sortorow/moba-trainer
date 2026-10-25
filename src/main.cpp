// MOBA Trainer: MOBA のスキルショット・スキルドッジの練習ソフト

#include <format>
#include <string>

#include "app/App.h"
#include "app/CommandLine.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Platform.h"

/// プログラムの入口。data フォルダを探し、ログとクラッシュ時の記録を準備し、コマンドライン引数を読んでから App を動かす
/// @return 終了コード（0 = 正常。data フォルダが無い・引数が読めない時は 1）
int main(int argc, char** argv) {
    using namespace moba;

    const std::filesystem::path exeDir = platform::executableDirectory();
    const std::optional<AppPaths> paths = locateAppPaths(exeDir);
    if (!paths) {
        platform::showErrorDialog(
            "MOBA Trainer", std::format("data/settings.jsonc が見つかりません。\nexe の場所: {}", toUtf8(exeDir)));
        return 1;
    }

    log::init(log::Config{paths->logsDir});
    platform::installCrashHandler();
    log::info("app", "MOBA Trainer {} を起動します（データ: {}）", MOBA_VERSION, toUtf8(paths->dataDir));

    std::string error;
    const std::optional<CommandLineOptions> options = parseCommandLine(argc, argv, error);
    if (!options) {
        log::error("app", "{}", error);
        platform::showErrorDialog("MOBA Trainer", error);
        log::shutdown();
        return 1;
    }

    App app(*paths, *options);
    const int exitCode = app.run();
    log::info("app", "終了します（終了コード {}）", exitCode);
    log::shutdown();
    return exitCode;
}
