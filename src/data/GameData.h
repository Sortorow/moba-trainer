#pragma once
// 設定ファイル一式（settings.jsonc とキャラクター定義）の読み込みと、自動での再読み込み

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/Paths.h"
#include "data/CharacterDef.h"
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

    const CharacterDef* character(std::string_view id) const;
    std::vector<const CharacterDef*> characterList() const;  // id 順

    // 直近の読み込みで起きたエラー（画面に表示する）
    const std::vector<std::string>& errors() const { return errors_; }
    void addError(std::string message);

    // 読み込みのたびに増える番号
    int generation() const { return generation_; }

private:
    std::map<std::filesystem::path, std::filesystem::file_time_type> scanFileTimes() const;

    AppPaths paths_;
    Settings settings_;
    std::map<std::string, CharacterDef, std::less<>> characters_;
    std::map<std::filesystem::path, std::filesystem::file_time_type> fileTimes_;
    std::vector<std::string> errors_;
    int generation_ = 0;
};

}  // namespace moba
