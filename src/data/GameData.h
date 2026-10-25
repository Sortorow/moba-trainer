#pragma once
// 設定ファイル一式（settings.jsonc・ai.jsonc・sandbox.jsonc とキャラクター定義）の読み込みと、自動での再読み込み

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/Paths.h"
#include "data/AiConfig.h"
#include "data/CharacterDef.h"
#include "data/SandboxConfig.h"
#include "data/Settings.h"

namespace moba {

class GameData {
public:
    explicit GameData(AppPaths paths);

    // すべてのファイルを読み込み直す。読み込みに失敗したファイルは、前回の内容をそのまま使う
    void loadAll();

    // 前回の読み込みからファイルが更新されていれば loadAll() する。戻り値は再読み込みしたかどうか
    bool reloadIfChanged();

    const AppPaths& paths() const { return paths_; }
    const Settings& settings() const { return settings_; }
    Settings& mutableSettings() { return settings_; }  // デバッグパネルからの一時的な変更用
    const AiConfig& ai() const { return ai_; }
    const SandboxConfig& sandbox() const { return sandbox_; }

    // 練習場のダミーのプリセットを選び、sandbox.jsonc の "preset" に保存する。戻り値は成功したかどうか
    bool selectSandboxPreset(std::string_view name);
    // ダミーの設定（sandbox.jsonc の中身）が変わるたびに増える番号（ゲームの中のダミーを置き直すかの判断に使う）
    int sandboxRevision() const { return sandboxRevision_; }

    const CharacterDef* character(std::string_view id) const;
    std::vector<const CharacterDef*> characterList() const;  // id 順
    // スキルドッジで敵として出られるキャラ（"usedByEnemy" のスキルを 1 つ以上持つ）
    std::vector<const CharacterDef*> enemyCandidates() const;

    // 直近の読み込みで起きたエラー（画面に表示する）
    const std::vector<std::string>& errors() const { return errors_; }
    void addError(std::string message);

    // 読み込みのたびに増える番号
    int generation() const { return generation_; }

private:
    std::map<std::filesystem::path, std::filesystem::file_time_type> scanFileTimes() const;

    AppPaths paths_;
    Settings settings_;
    AiConfig ai_;
    SandboxConfig sandbox_;
    int sandboxRevision_ = 0;
    std::map<std::string, CharacterDef, std::less<>> characters_;
    std::map<std::filesystem::path, std::filesystem::file_time_type> fileTimes_;
    std::vector<std::string> errors_;
    int generation_ = 0;
};

}  // namespace moba
