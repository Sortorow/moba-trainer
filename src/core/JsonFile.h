#pragma once
// コメント付き JSON（.jsonc）の読み込みと、コメントを残したままの値の書き換え

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace moba {

/// JSON ファイルの読み込み結果（読んだ内容か、読めなかった理由）
struct JsonLoadResult {
    nlohmann::json value;
    std::string error;  // 空なら成功
    /// 読み込めたか（error が空か）
    bool ok() const { return error.empty(); }
};

/// コメント（// と /* */）を書ける JSON ファイル（.jsonc）を読み込む。先頭の BOM は無視する
/// @return 読み込んだ内容。開けない・書き方に誤りがある時は error に理由（ファイル名と、行・列）が入る
JsonLoadResult loadJsonFile(const std::filesystem::path& file);

/// 文字列の中の範囲 [begin, end)
struct TextRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

/// コメント付き JSON の text の中で、keyPath の値の位置を探す。keyPath は外側からのキーの並び
/// （{"window", "width"} なら、一番外側のオブジェクトの "window" の中の "width"）。
/// 文字列・行コメント・ブロックコメントを区別しながら読むので、それらの中にある同じ文字は無視する。
/// 探せる値は、文字列・数・true・false・null（オブジェクトと配列の値は探さない）
/// @return 値の範囲（文字列なら両端の " を含む）。見つからなければ nullopt
std::optional<TextRange> findJsoncValue(const std::string& text, const std::vector<std::string>& keyPath);

/// ファイルの書き換え 1 つ（keyPath の値を value にする）
struct JsoncEdit {
    std::vector<std::string> keyPath;
    std::string value;  // JSON の書き方の値（文字列なら " で囲み、エスケープしたもの。nlohmann::json(...).dump() など）
};

/// コメント付き JSON のファイルの、指定した値だけを書き換えて保存する（コメント・空白・ほかの値はそのまま残る）。
/// 1 つでも keyPath が見つからなければ、ファイルを変えずに失敗する
/// @return 失敗した理由（成功なら空）
std::string rewriteJsoncValues(const std::filesystem::path& file, const std::vector<JsoncEdit>& edits);

}  // namespace moba
