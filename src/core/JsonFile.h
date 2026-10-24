#pragma once
// コメント付き JSON（.jsonc）の読み込み

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace moba {

struct JsonLoadResult {
    nlohmann::json value;
    std::string error;  // 空なら成功
    bool ok() const { return error.empty(); }
};

// コメント（// と /* */）を書ける JSON ファイルを読み込む
JsonLoadResult loadJsonFile(const std::filesystem::path& file);

}  // namespace moba
