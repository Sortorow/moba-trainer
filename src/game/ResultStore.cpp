#include "game/ResultStore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <fstream>
#include <set>

#include "core/JsonFile.h"
#include "core/Log.h"
#include "core/Paths.h"

namespace moba {
namespace {

using nlohmann::json;

constexpr int kFormatVersion = 1;  // 保存する JSON の形式の版（形式を変えた時に読み分けるため）

/// YYYYMMDD の整数を、暦の日（日数の計算用）にする
std::chrono::sys_days toDays(int date) {
    return std::chrono::sys_days{std::chrono::year_month_day{
        std::chrono::year{date / 10000}, std::chrono::month{static_cast<unsigned>(date / 100 % 100)},
        std::chrono::day{static_cast<unsigned>(date % 100)}}};
}

/// 日付ごとのファイル名（「2026-10-25.json」）
std::string dayFileName(int date) {
    return std::format("{:04}-{:02}-{:02}.json", date / 10000, date / 100 % 100, date % 100);
}

/// 「2026-10-25.json」の形のファイル名から日付（YYYYMMDD）を読む。形が違えば nullopt
std::optional<int> dateFromDayFileName(const std::filesystem::path& file) {
    const std::string name = toUtf8(file.filename());
    if (name.size() != 15 || !name.ends_with(".json") || name[4] != '-' || name[7] != '-') return std::nullopt;
    int value = 0;
    for (const std::size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
        if (name[i] < '0' || name[i] > '9') return std::nullopt;
        value = value * 10 + (name[i] - '0');
    }
    return value;
}

/// HitCount を JSON にする（{"casts": 発動数, "hits": 命中数}）
json toJson(const HitCount& count) { return json{{"casts", count.casts}, {"hits", count.hits}}; }

/// JSON から HitCount を読む（無い項目は 0）
HitCount hitCountFromJson(const json& j) {
    HitCount count;
    count.casts = j.value("casts", 0);
    count.hits = j.value("hits", 0);
    return count;
}

/// モードの、保存する時の名前（"sandbox" / "dodge" / "shot"）
std::string modeName(GameMode mode) {
    switch (mode) {
        case GameMode::Sandbox: return "sandbox";
        case GameMode::SkillDodge: return "dodge";
        case GameMode::SkillShot: return "shot";
    }
    return "?";
}

/// 保存する時の名前からモードを読む。知らない名前なら nullopt
std::optional<GameMode> modeFromName(std::string_view name) {
    for (const GameMode mode : {GameMode::Sandbox, GameMode::SkillDodge, GameMode::SkillShot}) {
        if (modeName(mode) == name) return mode;
    }
    return std::nullopt;
}

/// day の結果を、JSON の配列としてファイルに書く（一時ファイルに書いてから置き換える）
/// @return 書けなかった理由（空なら成功）
std::string writeDayFile(const std::filesystem::path& file, const std::vector<SavedResult>& day) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    json array = json::array();
    for (const SavedResult& saved : day) array.push_back(toJson(saved));

    std::filesystem::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return std::format("{} に書き込めません", toUtf8(temp));
        out << array.dump(2) << '\n';
        if (!out) return std::format("{} に書き込めません", toUtf8(temp));
    }
    std::filesystem::rename(temp, file, ec);
    if (ec) return std::format("{} を保存できません: {}", toUtf8(file), ec.message());
    return {};
}

}  // namespace

LocalTimestamp localNow() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_s(&local, &t);
    LocalTimestamp now;
    now.date = (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
    now.text = std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                           local.tm_hour, local.tm_min, local.tm_sec);
    return now;
}

ResultScore scoreOf(const MatchResult& result) {
    if (result.config.mode == GameMode::SkillDodge) return ResultScore{result.dodge.dodged, result.dodge.total};
    return ResultScore{result.playerHits.hits, result.playerHits.casts};
}

bool rankedHigher(const SavedResult& a, const SavedResult& b) {
    const ResultScore sa = scoreOf(a.result);
    const ResultScore sb = scoreOf(b.result);
    if (sa.rate() != sb.rate()) return sa.rate() > sb.rate();
    if (sa.total != sb.total) return sa.total > sb.total;
    return a.playedAt > b.playedAt;
}

std::optional<std::string> resultFolderName(GameMode mode) {
    if (mode == GameMode::Sandbox) return std::nullopt;
    return modeName(mode);
}

void appendWithDailyLimit(std::vector<SavedResult>& day, SavedResult result, std::size_t limit) {
    day.push_back(std::move(result));
    if (day.size() > limit) day.erase(day.begin(), day.begin() + static_cast<std::ptrdiff_t>(day.size() - limit));
}

int daysBetween(int from, int to) { return static_cast<int>((toDays(to) - toDays(from)).count()); }

std::vector<int> selectExpiredDates(const std::vector<int>& dates, int today, int keepDays) {
    std::vector<int> expired;
    for (const int date : dates) {
        if (daysBetween(date, today) >= keepDays) expired.push_back(date);
    }
    return expired;
}

json toJson(const SavedResult& saved) {
    const MatchResult& r = saved.result;
    const MatchConfig& c = r.config;
    json bySlot = json::object();
    for (const SkillSlot slot : kAllSkillSlots) {
        const HitCount& count = r.playerHitsBySlot[static_cast<std::size_t>(index(slot))];
        if (count.casts > 0) bySlot[std::string(toString(slot))] = toJson(count);
    }
    json j{
        {"version", kFormatVersion},
        {"playedAt", saved.playedAt},
        {"mode", modeName(c.mode)},
        {"character", {{"id", c.characterId}, {"name", r.characterName}}},
        {"stage", {{"id", c.stageId}, {"name", r.stageName}}},
        {"timeLimit", c.timeLimitSeconds},
        {"playedSeconds", std::round(r.playedSeconds * 100.0f) / 100.0f},
        {"cooldown",
         {{"mode", c.cooldownMode == CooldownMode::None ? "none" : "standard"}, {"scale", c.cooldownScale}}},
        {"seed", c.seed},
        {"hits", toJson(r.playerHits)},
        {"hitsBySlot", bySlot},
    };
    if (c.mode == GameMode::SkillDodge) {
        j["enemies"] = {{"count", c.enemyCount}, {"level", c.aiLevel}, {"levelName", r.aiLevelName}};
        j["dodge"] = {{"total", r.dodge.total}, {"dodged", r.dodge.dodged}, {"hit", r.dodge.hit}};
        j["hitsTaken"] = r.hitsTaken;
        j["deaths"] = r.deaths;
    } else if (c.mode == GameMode::SkillShot) {
        j["target"] = {{"level", c.targetLevel},
                       {"levelName", r.aiLevelName},
                       {"radius", c.targetRadius},
                       {"sizeName", r.targetSizeName},
                       {"moveSpeed", c.targetMoveSpeed}};
    }
    return j;
}

std::optional<SavedResult> savedResultFromJson(const json& j, std::string& error) {
    try {
        SavedResult saved;
        saved.playedAt = j.at("playedAt").get<std::string>();
        const std::optional<GameMode> mode = modeFromName(j.at("mode").get<std::string>());
        if (!mode) {
            error = std::format("不明なモードです: {}", j.at("mode").dump());
            return std::nullopt;
        }
        // 日付は playedAt の「YYYY-MM-DD」から読む
        if (saved.playedAt.size() < 10) {
            error = std::format("日時が読めません: {}", saved.playedAt);
            return std::nullopt;
        }
        saved.date = std::stoi(saved.playedAt.substr(0, 4)) * 10000 + std::stoi(saved.playedAt.substr(5, 2)) * 100 +
                     std::stoi(saved.playedAt.substr(8, 2));

        MatchResult& r = saved.result;
        MatchConfig& c = r.config;
        c.mode = *mode;
        c.characterId = j.at("character").value("id", "");
        r.characterName = j.at("character").value("name", "");
        c.stageId = j.at("stage").value("id", "");
        r.stageName = j.at("stage").value("name", "");
        c.timeLimitSeconds = j.value("timeLimit", 0.0f);
        r.playedSeconds = j.value("playedSeconds", 0.0f);
        if (const auto it = j.find("cooldown"); it != j.end()) {
            c.cooldownMode = it->value("mode", "standard") == "none" ? CooldownMode::None : CooldownMode::Standard;
            c.cooldownScale = it->value("scale", c.cooldownScale);
        }
        c.seed = j.value("seed", 0u);
        if (const auto it = j.find("hits"); it != j.end()) r.playerHits = hitCountFromJson(*it);
        if (const auto it = j.find("hitsBySlot"); it != j.end()) {
            for (const SkillSlot slot : kAllSkillSlots) {
                if (const auto s = it->find(std::string(toString(slot))); s != it->end())
                    r.playerHitsBySlot[static_cast<std::size_t>(index(slot))] = hitCountFromJson(*s);
            }
        }
        if (const auto it = j.find("enemies"); it != j.end()) {
            c.enemyCount = it->value("count", c.enemyCount);
            c.aiLevel = it->value("level", c.aiLevel);
            r.aiLevelName = it->value("levelName", "");
        }
        if (const auto it = j.find("dodge"); it != j.end()) {
            r.dodge.total = it->value("total", 0);
            r.dodge.dodged = it->value("dodged", 0);
            r.dodge.hit = it->value("hit", 0);
        }
        r.hitsTaken = j.value("hitsTaken", 0);
        r.deaths = j.value("deaths", 0);
        if (const auto it = j.find("target"); it != j.end()) {
            c.targetLevel = it->value("level", c.targetLevel);
            r.aiLevelName = it->value("levelName", "");
            c.targetRadius = it->value("radius", c.targetRadius);
            r.targetSizeName = it->value("sizeName", "");
            c.targetMoveSpeed = it->value("moveSpeed", c.targetMoveSpeed);
        }
        return saved;
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// ResultStore
// ---------------------------------------------------------------------------
std::filesystem::path ResultStore::dayFile(const std::string& folder, int date) const {
    return dir_ / folder / dayFileName(date);
}

void ResultStore::load(int today) {
    results_.clear();
    errors_.clear();
    std::size_t total = 0;
    for (const GameMode mode : {GameMode::SkillDodge, GameMode::SkillShot}) {
        const std::string folder = *resultFolderName(mode);
        std::vector<SavedResult>& list = results_[folder];
        std::error_code ec;
        if (!std::filesystem::is_directory(dir_ / folder, ec)) continue;

        // 日付の古い順に読む（ファイル名が日付なので、名前順 = 日付順）
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(dir_ / folder, ec)) {
            if (entry.is_regular_file() && dateFromDayFileName(entry.path())) files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        for (const std::filesystem::path& file : files) {
            const JsonLoadResult json = loadJsonFile(file);
            std::string error = json.error;
            if (error.empty() && !json.value.is_array()) error = "結果の配列ではありません";
            if (error.empty()) {
                for (const nlohmann::json& item : json.value) {
                    std::string itemError;
                    if (std::optional<SavedResult> saved = savedResultFromJson(item, itemError)) {
                        list.push_back(std::move(*saved));
                    } else {
                        log::warn("results", "{} の結果を 1 件読み飛ばしました: {}", toUtf8(file.filename()),
                                  itemError);
                    }
                }
                continue;
            }
            // 読めないファイルは、次の保存で上書きしないように名前を変えて残す
            std::filesystem::path broken = file;
            broken += ".broken";
            std::filesystem::rename(file, broken, ec);
            const std::string message = std::format("結果のファイル {} が読めないため、{} に名前を変えました: {}",
                                                    toUtf8(file), toUtf8(broken.filename()), error);
            log::error("results", "{}", message);
            errors_.push_back(message);
        }
        removeExpired(folder, today);
        total += list.size();
    }
    log::info("results", "保存されている結果を読み込みました: {} 件（{}）", total, toUtf8(dir_));
}

void ResultStore::removeExpired(const std::string& folder, int today) {
    std::vector<SavedResult>& list = results_[folder];
    std::set<int> dates;
    for (const SavedResult& saved : list) dates.insert(saved.date);
    // ファイルはあるが中身が空の日も消せるように、フォルダの中のファイルの日付も見る
    std::error_code ec;
    if (std::filesystem::is_directory(dir_ / folder, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir_ / folder, ec)) {
            if (const std::optional<int> date = dateFromDayFileName(entry.path())) dates.insert(*date);
        }
    }
    const std::vector<int> expired = selectExpiredDates({dates.begin(), dates.end()}, today, kResultKeepDays);
    for (const int date : expired) {
        std::filesystem::remove(dayFile(folder, date), ec);
        log::info("results", "{} 日より前の結果を消しました: {}/{}", kResultKeepDays, folder, dayFileName(date));
    }
    std::erase_if(list, [&](const SavedResult& saved) {
        return std::find(expired.begin(), expired.end(), saved.date) != expired.end();
    });
}

std::optional<SavedResult> ResultStore::save(const MatchResult& result, const LocalTimestamp& at) {
    const std::optional<std::string> folder = resultFolderName(result.config.mode);
    if (!folder) return std::nullopt;

    SavedResult saved;
    saved.playedAt = at.text;
    saved.date = at.date;
    saved.result = result;

    // その日の結果に加え、上限を超えたら古いものから消す
    std::vector<SavedResult>& list = results_[*folder];
    std::vector<SavedResult> day;
    for (const SavedResult& item : list) {
        if (item.date == at.date) day.push_back(item);
    }
    appendWithDailyLimit(day, saved, kMaxResultsPerDay);
    if (const std::string error = writeDayFile(dayFile(*folder, at.date), day); !error.empty()) {
        log::error("results", "結果を保存できませんでした: {}", error);
        errors_.push_back(error);
        return std::nullopt;
    }

    std::erase_if(list, [&](const SavedResult& item) { return item.date == at.date; });
    list.insert(list.end(), day.begin(), day.end());
    std::stable_sort(list.begin(), list.end(),
                     [](const SavedResult& a, const SavedResult& b) { return a.date < b.date; });
    log::info("results", "結果を保存しました: {}/{}（この日 {} 件目 / 上限 {} 件）", *folder, dayFileName(at.date),
              day.size(), kMaxResultsPerDay);
    removeExpired(*folder, at.date);
    return saved;
}

std::vector<SavedResult> ResultStore::recent(GameMode mode, std::size_t count) const {
    const std::vector<SavedResult>& list = all(mode);
    const std::size_t n = std::min(count, list.size());
    return std::vector<SavedResult>(list.rbegin(), list.rbegin() + static_cast<std::ptrdiff_t>(n));
}

const std::vector<SavedResult>& ResultStore::all(GameMode mode) const {
    static const std::vector<SavedResult> kEmpty;
    const std::optional<std::string> folder = resultFolderName(mode);
    if (!folder) return kEmpty;
    const auto it = results_.find(*folder);
    return it == results_.end() ? kEmpty : it->second;
}

}  // namespace moba
