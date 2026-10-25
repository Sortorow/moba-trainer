#pragma once
// リザルト（プレイの結果）の保存と読み込み
//
// 保存先: results/<モード>/<YYYY-MM-DD>.json（モード = dodge（スキルドッジ）/ shot（スキルショット））。
// 1 ファイルに、その日のそのモードの結果を、古い順に並べる。
//   ・1 日に保存するのは、モードごとに 20 件まで。21 件目からは、その日の最も古い結果を消して上書きする
//   ・残すのは 180 日分（暦の日数。今日を含めて 180 日より前の日のファイルは、起動時と保存時に消す）
//   ・制限時間まで遊んだプレイだけを保存する（ポーズからメニューに戻った・リスタートしたプレイは保存しない）
// 直近 10 件はリザルト画面と記録画面で表示し、保存したすべての結果は統計（手順 6）で使う。

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "game/Match.h"
#include "game/MatchConfig.h"

namespace moba {

inline constexpr std::size_t kMaxResultsPerDay = 20;   // 1 日・1 モードあたりの保存数の上限
inline constexpr int kResultKeepDays = 180;            // 結果を残す日数（約 6 か月）
inline constexpr std::size_t kRecentResultCount = 10;  // リザルト画面・記録画面に出す直近の件数

/// 結果を保存した日時（ローカル時刻）
struct LocalTimestamp {
    int date = 0;      // YYYYMMDD の整数（2026 年 10 月 25 日なら 20261025）
    std::string text;  // 「2026-10-25 22:40:12」
};

/// 今のローカル時刻
LocalTimestamp localNow();

/// 保存した結果 1 件（保存した日時と、リザルト画面に出した内容）
struct SavedResult {
    std::string playedAt;  // 保存した日時（「2026-10-25 22:40:12」）
    int date = 0;          // 保存した日（YYYYMMDD）
    MatchResult result;
};

/// 記録の並べ替えに使うスコアの数（スキルドッジ: 避けた数 / 総数、スキルショット: 当たった数 / 総数）
struct ResultScore {
    int count = 0;
    int total = 0;
    /// 割合（0〜1）。総数が 0 なら 0
    float rate() const { return total > 0 ? static_cast<float>(count) / static_cast<float>(total) : 0.0f; }
};

/// 結果のスコア（スキルドッジはドッジ率、スキルショットはヒット率）
ResultScore scoreOf(const MatchResult& result);

/// 記録の並び順で a が b より上か（割合が高い順。同じなら総数が多い順、さらに同じなら新しい順）
bool rankedHigher(const SavedResult& a, const SavedResult& b);

/// 結果を保存するモードの、フォルダ名（"dodge" / "shot"）。保存しないモード（練習場）なら nullopt
std::optional<std::string> resultFolderName(GameMode mode);

/// 1 日分の結果に result を加える。上限 limit を超えたら、古いものから消す
void appendWithDailyLimit(std::vector<SavedResult>& day, SavedResult result, std::size_t limit);

/// 残す期間を過ぎた日を選ぶ。today を含めて keepDays 日より前の日が対象
/// @param dates 日付（YYYYMMDD）の一覧
std::vector<int> selectExpiredDates(const std::vector<int>& dates, int today, int keepDays);

/// SavedResult を JSON にする（保存用）
nlohmann::json toJson(const SavedResult& saved);
/// JSON から SavedResult を読む。読めなければ nullopt（error に理由）
std::optional<SavedResult> savedResultFromJson(const nlohmann::json& j, std::string& error);

/// リザルトの保存と読み込み（results フォルダ）。読み込んだ結果はモードごとにメモリにも持つ
class ResultStore {
public:
    /// 保存先のフォルダを覚える（読み込みは load() で行う）
    explicit ResultStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

    /// 保存されている結果をすべて読み込み、残す期間を過ぎた日のファイルを消す
    /// @param today 今日（YYYYMMDD）
    void load(int today);
    /// 結果を保存する（その日のファイルに加え、上限を超えたら古いものから消す）。保存しないモードなら何もしない
    /// @return 保存した結果。保存しなかった・書き込めなかった時は nullopt
    std::optional<SavedResult> save(const MatchResult& result, const LocalTimestamp& at);

    /// そのモードの直近の結果を、新しい順に count 件まで
    std::vector<SavedResult> recent(GameMode mode, std::size_t count) const;
    /// そのモードの保存されているすべての結果（古い順）
    const std::vector<SavedResult>& all(GameMode mode) const;
    /// 保存先のフォルダ
    const std::filesystem::path& dir() const { return dir_; }
    /// 直近の読み込み・保存で起きたエラー
    const std::vector<std::string>& errors() const { return errors_; }

private:
    /// そのモードのフォルダの中の、日付ごとのファイル
    std::filesystem::path dayFile(const std::string& folder, int date) const;
    /// そのモードの、残す期間を過ぎた日のファイルを消し、メモリからも外す
    void removeExpired(const std::string& folder, int today);

    std::filesystem::path dir_;
    std::map<std::string, std::vector<SavedResult>> results_;  // フォルダ名（モード）ごとの、すべての結果（古い順）
    std::vector<std::string> errors_;
};

}  // namespace moba
