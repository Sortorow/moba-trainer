#include "data/GameData.h"

#include <algorithm>
#include <format>

#include "core/Log.h"
#include "data/DataWarnings.h"

namespace moba {

GameData::GameData(AppPaths paths) : paths_(std::move(paths)) {}

void GameData::loadAll() {
    // 更新日時は読む前に取る（読んでいる途中に保存された変更を、読んだことにしないように。次の確認で読み直す）
    std::map<std::filesystem::path, std::filesystem::file_time_type> times = scanFileTimes();
    errors_.clear();
    clearDataWarnings();  // スキルを使った時の誤りも、読み直したら一度消す（直っていなければ、次に使った時にまた出る）

    // --- 設定 ---
    SettingsLoadResult settings = loadSettings(paths_.settingsFile);
    if (settings.error.empty()) {
        settings_ = settings.settings;
        for (const std::string& warning : settings.warnings) {
            log::warn("data", "settings.jsonc: {}", warning);
            errors_.push_back("settings.jsonc: " + warning);
        }
    } else {
        log::error("data", "{}", settings.error);
        errors_.push_back(settings.error);
    }

    // --- 敵 AI ---
    AiConfigLoadResult ai = loadAiConfig(paths_.aiFile);
    if (ai.error.empty()) {
        ai_ = std::move(ai.config);
    } else {
        log::error("data", "{}", ai.error);
        errors_.push_back(ai.error);
    }

    // --- 練習場のダミー ---
    SandboxConfigLoadResult sandbox = loadSandboxConfig(paths_.sandboxFile);
    if (sandbox.error.empty()) {
        // 中身が変わった時だけ番号を増やす（キャラのファイルを保存しただけで、ダミーが置き直されないように）
        if (sandboxRevision_ == 0 || !sameSandboxConfig(sandbox_, sandbox.config)) ++sandboxRevision_;
        sandbox_ = std::move(sandbox.config);
        for (const std::string& warning : sandbox.warnings) {
            log::warn("data", "sandbox.jsonc: {}", warning);
            errors_.push_back("sandbox.jsonc: " + warning);
        }
    } else {
        log::error("data", "{}", sandbox.error);
        errors_.push_back(sandbox.error);
    }

    // --- キャラクター ---
    std::map<std::string, CharacterDef, std::less<>> loaded;
    for (const std::filesystem::path& file : jsoncFilesIn(paths_.charactersDir)) {
        CharacterLoadResult result = loadCharacterFile(file);
        // 書き忘れなどで規定値を使った項目（キャラは使える）
        for (const std::string& warning : result.warnings) {
            log::warn("data", "{}", warning);
            errors_.push_back("警告: " + warning);
        }
        // スキルの params がルールを満たさなければ、読めなかったファイルと同じく使わない
        if (result.def) {
            const std::vector<std::string> problems = checkCharacter(*result.def);
            for (const std::string& problem : problems) {
                log::error("data", "{}", problem);
                errors_.push_back(problem);
            }
            if (!problems.empty()) {
                result.error = std::format(
                    "{}: スキルの params に誤りがあるため、このファイルを使いません"
                    "（前回読めた内容があれば、それを使います）",
                    toUtf8(file.filename()));
                result.def.reset();
            }
        }
        if (!result.def) {
            log::error("data", "{}", result.error);
            errors_.push_back(result.error);
            // 前回読み込めていたなら、その内容を使い続ける
            for (const auto& [id, previous] : characters_) {
                if (previous.sourceFile == file) loaded.emplace(id, previous);
            }
            continue;
        }
        const std::string id = result.def->id;
        if (loaded.contains(id)) {
            const std::string message =
                std::format("{}: id \"{}\" が他のファイルと重複しています", toUtf8(file.filename()), id);
            log::error("data", "{}", message);
            errors_.push_back(message);
            continue;
        }
        loaded.emplace(id, std::move(*result.def));
    }
    characters_ = std::move(loaded);

    // --- ステージ ---
    std::map<std::string, StageDef, std::less<>> loadedStages;
    for (const std::filesystem::path& file : jsoncFilesIn(paths_.stagesDir)) {
        StageLoadResult result = loadStageFile(file);
        if (!result.def) {
            log::error("data", "{}", result.error);
            errors_.push_back(result.error);
            for (const auto& [id, previous] : stages_) {
                if (previous.sourceFile == file) loadedStages.emplace(id, previous);
            }
            continue;
        }
        const std::string id = result.def->id;
        if (loadedStages.contains(id)) {
            const std::string message =
                std::format("{}: id \"{}\" が他のファイルと重複しています", toUtf8(file.filename()), id);
            log::error("data", "{}", message);
            errors_.push_back(message);
            continue;
        }
        loadedStages.emplace(id, std::move(*result.def));
    }
    stages_ = std::move(loadedStages);

    fileTimes_ = std::move(times);
    ++generation_;

    log::info("data", "設定を読み込みました（{} 回目）: キャラ {} 体、ステージ {} 個、エラー {} 件", generation_,
              characters_.size(), stages_.size(), errors_.size());
}

std::vector<std::filesystem::path> GameData::jsoncFilesIn(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".jsonc") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

const StageDef* GameData::stage(std::string_view id) const {
    const auto it = stages_.find(id);
    return it == stages_.end() ? nullptr : &it->second;
}

std::vector<const StageDef*> GameData::stageList() const {
    std::vector<const StageDef*> list;
    for (const auto& [id, def] : stages_) list.push_back(&def);  // stages_ は id 順
    std::stable_sort(list.begin(), list.end(),
                     [](const StageDef* a, const StageDef* b) { return a->menuOrder < b->menuOrder; });
    return list;
}

bool GameData::hasChangedFiles() const { return scanFileTimes() != fileTimes_; }

bool GameData::reloadIfChanged() {
    if (!hasChangedFiles()) return false;
    log::info("data", "設定ファイルの変更を検知したため、再読み込みします");
    loadAll();
    return true;
}

const CharacterDef* GameData::character(std::string_view id) const {
    const auto it = characters_.find(id);
    return it == characters_.end() ? nullptr : &it->second;
}

std::vector<const CharacterDef*> GameData::characterList() const {
    std::vector<const CharacterDef*> list;
    for (const auto& [id, def] : characters_) list.push_back(&def);  // characters_ は id 順
    std::stable_sort(list.begin(), list.end(),
                     [](const CharacterDef* a, const CharacterDef* b) { return a->menuOrder < b->menuOrder; });
    return list;
}

std::vector<const CharacterDef*> GameData::enemyCandidates() const {
    std::vector<const CharacterDef*> list;
    for (const CharacterDef* def : characterList()) {
        const bool usable =
            std::any_of(def->skills.begin(), def->skills.end(),
                        [](const std::optional<SkillDef>& skill) { return skill && skill->usedByEnemy; });
        if (usable) list.push_back(def);
    }
    return list;
}

bool GameData::selectSandboxPreset(std::string_view name) {
    if (!sandbox_.preset(name)) return false;
    sandbox_.selectedPreset = std::string(name);
    ++sandboxRevision_;
    const bool upToDate = isUpToDate(paths_.sandboxFile);
    const std::string error = saveSelectedSandboxPreset(paths_.sandboxFile, name);
    if (!error.empty()) {
        log::error("data", "練習場のダミーの選択を保存できませんでした: {}", error);
        errors_.push_back(error);
        return true;  // 保存はできなかったが、選択は反映している
    }
    // 自分で書き込んだ変更で、再読み込みが起きないようにする。覚え直すのは書き込んだファイルの日時だけ（ほかのファイルの
    // まだ読んでいない変更を、読んだことにしないように）。書き込む前からこのファイルに読んでいない変更があった時は、
    // 覚え直さずに、次の確認で読み直す（書き戻すのは "preset" の値だけなので、その変更もファイルに残っている）
    if (upToDate) rememberFileTime(paths_.sandboxFile);
    log::info("data", "練習場のダミーを「{}」にして、sandbox.jsonc に保存しました", name);
    return true;
}

void GameData::saveWindowSize(int width, int height) {
    settings_.windowWidth = width;
    settings_.windowHeight = height;
    const bool upToDate = isUpToDate(paths_.settingsFile);
    const std::string error = moba::saveWindowSize(paths_.settingsFile, width, height);
    if (!error.empty()) {
        log::error("data", "解像度を保存できませんでした: {}", error);
        errors_.push_back(error);
        return;
    }
    // 自分で書き込んだ変更で、再読み込みが起きないようにする（覚え直す範囲は selectSandboxPreset と同じ）
    if (upToDate) rememberFileTime(paths_.settingsFile);
    log::info("data", "解像度 {}x{} を settings.jsonc に保存しました", width, height);
}

std::vector<std::string> GameData::checkCharacter(const CharacterDef& def) {
    std::vector<std::string> problems;
    if (!skillValidator_) return problems;
    for (const auto& skill : def.skills) {
        if (!skill) continue;
        const SkillCheck check = skillValidator_(*skill);
        for (const std::string& warning : check.warnings) {
            log::warn("data", "{}", warning);
            errors_.push_back("警告: " + warning);
        }
        problems.insert(problems.end(), check.errors.begin(), check.errors.end());
    }
    return problems;
}

std::vector<std::string> GameData::errors() const {
    std::vector<std::string> all = errors_;
    const std::vector<std::string>& warnings = dataWarnings();
    all.insert(all.end(), warnings.begin(), warnings.end());
    return all;
}

std::map<std::filesystem::path, std::filesystem::file_time_type> GameData::scanFileTimes() const {
    std::map<std::filesystem::path, std::filesystem::file_time_type> times;
    times[paths_.settingsFile] = fileTime(paths_.settingsFile);
    times[paths_.aiFile] = fileTime(paths_.aiFile);
    times[paths_.sandboxFile] = fileTime(paths_.sandboxFile);
    for (const std::filesystem::path& file : jsoncFilesIn(paths_.charactersDir)) times[file] = fileTime(file);
    for (const std::filesystem::path& file : jsoncFilesIn(paths_.stagesDir)) times[file] = fileTime(file);
    return times;
}

std::filesystem::file_time_type GameData::fileTime(const std::filesystem::path& file) {
    std::error_code ec;
    return std::filesystem::last_write_time(file, ec);  // 読めなければ ec が立ち、file_time_type::min() が返る
}

bool GameData::isUpToDate(const std::filesystem::path& file) const {
    const auto it = fileTimes_.find(file);
    return it != fileTimes_.end() && it->second == fileTime(file);
}

void GameData::rememberFileTime(const std::filesystem::path& file) { fileTimes_[file] = fileTime(file); }

}  // namespace moba
