#pragma once
// 設定ファイル一式（settings.jsonc・ai.jsonc・sandbox.jsonc・キャラクター・ステージ）の読み込みと、自動での再読み込み

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
#include "data/StageDef.h"

namespace moba {

/// 設定ファイル一式（settings・ai・sandbox・キャラ・ステージ）。読み込みと、変更を検知した時の再読み込みを行う
class GameData {
public:
    /// ファイルの場所だけを覚える（読み込みは loadAll() で行う）
    explicit GameData(AppPaths paths);

    /// キャラのファイルを読むたびに、各スキルの params を確かめる関数を決める（App が skills::checkSkillParams を渡す）。
    /// エラーがあったキャラのファイルは、読めなかったファイルと同じく使わない。決めなければ確かめない（自動テストの多く）
    void setSkillValidator(SkillValidator validator) { skillValidator_ = std::move(validator); }

    /// すべてのファイルを読み込み直す。読み込みに失敗したファイル（params の確認で通らなかったキャラも含む）は、
    /// 前回の内容をそのまま使う
    void loadAll();

    /// 前回の読み込みからファイルが更新されていれば loadAll() する（1 秒ごとに App から呼ばれる）
    /// @return 再読み込みしたかどうか
    bool reloadIfChanged();

    /// data フォルダの各ファイルの場所
    const AppPaths& paths() const { return paths_; }
    /// settings.jsonc の内容
    const Settings& settings() const { return settings_; }
    /// settings.jsonc の内容を書き換えるための参照（デバッグパネルからの一時的な変更用。ファイルには保存しない）
    Settings& mutableSettings() { return settings_; }
    /// ai.jsonc の内容（スキルドッジの敵 AI）
    const AiConfig& ai() const { return ai_; }
    /// sandbox.jsonc の内容（練習場のダミーのプリセット）
    const SandboxConfig& sandbox() const { return sandbox_; }

    /// 練習場のダミーのプリセットを選び、sandbox.jsonc の "preset" に保存する
    /// @return 選べたかどうか（その名前のプリセットが無ければ false。保存に失敗しても、選択は反映して true）
    bool selectSandboxPreset(std::string_view name);
    /// ダミーの設定（sandbox.jsonc の中身や選択）が変わるたびに増える番号（ゲームの中のダミーを置き直すかの判断に使う）
    int sandboxRevision() const { return sandboxRevision_; }

    /// ID でキャラを探す。見つからなければ nullptr
    const CharacterDef* character(std::string_view id) const;
    /// キャラの一覧。各キャラのファイルの menuOrder の順（同じなら id の順）。最初のキャラがメニューの初期値になる
    std::vector<const CharacterDef*> characterList() const;
    /// スキルドッジで敵として出られるキャラ（"usedByEnemy" のスキルを 1 つ以上持つ）。並びは characterList() と同じ
    std::vector<const CharacterDef*> enemyCandidates() const;

    /// ID でステージを探す。見つからなければ nullptr
    const StageDef* stage(std::string_view id) const;
    /// ステージの一覧。menuOrder の順（同じなら id の順）。最初のステージがメニューの初期値になる
    std::vector<const StageDef*> stageList() const;

    /// 設定ファイルのエラー（メニューとデバッグパネルに赤字で表示する）。直近の読み込みで起きたエラーと、
    /// その後にスキルを使った時に見つかった誤り（params の書き忘れなど。DataWarnings.h）を、この順に並べる
    std::vector<std::string> errors() const;

    /// 読み込みのたびに増える番号
    int generation() const { return generation_; }

private:
    /// 監視するすべての設定ファイルと、その更新日時
    std::map<std::filesystem::path, std::filesystem::file_time_type> scanFileTimes() const;
    /// フォルダの中の .jsonc ファイル（名前順）。フォルダが無ければ空
    static std::vector<std::filesystem::path> jsoncFilesIn(const std::filesystem::path& dir);
    /// 読み込んだキャラの各スキルの params を確かめる。警告はエラーの一覧に足す
    /// @return 通らなかった理由（空なら通った）
    std::vector<std::string> checkCharacter(const CharacterDef& def);

    AppPaths paths_;
    Settings settings_;
    AiConfig ai_;
    SandboxConfig sandbox_;
    int sandboxRevision_ = 0;
    std::map<std::string, CharacterDef, std::less<>> characters_;
    std::map<std::string, StageDef, std::less<>> stages_;
    std::map<std::filesystem::path, std::filesystem::file_time_type> fileTimes_;
    std::vector<std::string> errors_;
    int generation_ = 0;
    SkillValidator skillValidator_;  // スキルの params を確かめる関数（空なら確かめない）
};

}  // namespace moba
