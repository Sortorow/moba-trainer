#pragma once
// コメント付き JSON（.jsonc）の読み込み

#include <filesystem>
#include <string>

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

}  // namespace moba
