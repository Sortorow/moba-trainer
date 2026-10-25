#include "data/GameData.h"

#include <algorithm>
#include <format>

#include "core/Log.h"

namespace moba {

GameData::GameData(AppPaths paths) : paths_(std::move(paths)) {}

void GameData::loadAll() {
    errors_.clear();

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
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(paths_.charactersDir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".jsonc") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    std::map<std::string, CharacterDef, std::less<>> loaded;
    for (const std::filesystem::path& file : files) {
        CharacterLoadResult result = loadCharacterFile(file);
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
    fileTimes_ = scanFileTimes();
    ++generation_;

    log::info("data", "設定を読み込みました（{} 回目）: キャラ {} 体、エラー {} 件", generation_, characters_.size(),
              errors_.size());
}

bool GameData::reloadIfChanged() {
    if (scanFileTimes() == fileTimes_) return false;
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
    for (const auto& [id, def] : characters_) list.push_back(&def);
    return list;
}

std::vector<const CharacterDef*> GameData::enemyCandidates() const {
    std::vector<const CharacterDef*> list;
    for (const auto& [id, def] : characters_) {
        const bool usable = std::any_of(def.skills.begin(), def.skills.end(), [](const std::optional<SkillDef>& skill) {
            return skill && skill->usedByEnemy;
        });
        if (usable) list.push_back(&def);
    }
    return list;
}

bool GameData::selectSandboxPreset(std::string_view name) {
    if (!sandbox_.preset(name)) return false;
    sandbox_.selectedPreset = std::string(name);
    ++sandboxRevision_;
    const std::string error = saveSelectedSandboxPreset(paths_.sandboxFile, name);
    if (!error.empty()) {
        log::error("data", "練習場のダミーの選択を保存できませんでした: {}", error);
        errors_.push_back(error);
        return true;  // 保存はできなかったが、選択は反映している
    }
    // 自分で書き込んだ変更で、再読み込みが起きないようにする
    fileTimes_ = scanFileTimes();
    log::info("data", "練習場のダミーを「{}」にして、sandbox.jsonc に保存しました", name);
    return true;
}

void GameData::addError(std::string message) { errors_.push_back(std::move(message)); }

std::map<std::filesystem::path, std::filesystem::file_time_type> GameData::scanFileTimes() const {
    std::map<std::filesystem::path, std::filesystem::file_time_type> times;
    std::error_code ec;
    times[paths_.settingsFile] = std::filesystem::last_write_time(paths_.settingsFile, ec);
    times[paths_.aiFile] = std::filesystem::last_write_time(paths_.aiFile, ec);
    times[paths_.sandboxFile] = std::filesystem::last_write_time(paths_.sandboxFile, ec);
    for (const auto& entry : std::filesystem::directory_iterator(paths_.charactersDir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".jsonc") {
            times[entry.path()] = std::filesystem::last_write_time(entry.path(), ec);
        }
    }
    return times;
}

}  // namespace moba
