#include "core/JsonFile.h"

#include <format>
#include <fstream>
#include <iterator>

#include "core/Paths.h"

namespace moba {

JsonLoadResult loadJsonFile(const std::filesystem::path& file) {
    JsonLoadResult result;
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        result.error = std::format("ファイルを開けません: {}", toUtf8(file));
        return result;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);  // UTF-8 の BOM を取り除く

    try {
        result.value = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/true, /*ignore_comments=*/true);
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::format("{} の書き方に誤りがあります: {}", toUtf8(file.filename()), e.what());
    }
    return result;
}

}  // namespace moba
