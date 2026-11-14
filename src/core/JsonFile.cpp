#include "core/JsonFile.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <string_view>

#include "core/Paths.h"

namespace moba {
namespace {

/// pos から、空白とコメント（// の行コメント・/* */ のブロックコメント）を読み飛ばした位置
std::size_t skipSpaceAndComments(const std::string& text, std::size_t pos) {
    while (pos < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        } else if (text.compare(pos, 2, "//") == 0) {
            pos = text.find('\n', pos);
            if (pos == std::string::npos) return text.size();
        } else if (text.compare(pos, 2, "/*") == 0) {
            pos = text.find("*/", pos + 2);
            if (pos == std::string::npos) return text.size();
            pos += 2;
        } else {
            break;
        }
    }
    return pos;
}

/// pos の " から始まる JSON の文字列の終わり（閉じる " の次の位置。\" などのエスケープも読む）
std::size_t stringEnd(const std::string& text, std::size_t pos) {
    for (++pos; pos < text.size(); ++pos) {
        if (text[pos] == '\\') ++pos;
        else if (text[pos] == '"') return pos + 1;
    }
    return text.size();
}

/// pos から始まる値（文字列・数・true・false・null）の範囲。オブジェクト・配列・値が無い時は nullopt
std::optional<TextRange> scalarValueAt(const std::string& text, std::size_t pos) {
    if (pos >= text.size() || text[pos] == '{' || text[pos] == '[') return std::nullopt;
    if (text[pos] == '"') return TextRange{pos, stringEnd(text, pos)};
    // 数・true・false・null は、区切り（, } ]）・空白・コメントの手前まで
    std::size_t end = pos;
    while (end < text.size() && std::string_view(",}]/").find(text[end]) == std::string_view::npos &&
           !std::isspace(static_cast<unsigned char>(text[end])))
        ++end;
    if (end == pos) return std::nullopt;
    return TextRange{pos, end};
}

/// keyPath を「window.width」のように . でつないだ文字列（エラーの説明用）
std::string joinKeyPath(const std::vector<std::string>& keyPath) {
    std::string joined;
    for (const std::string& key : keyPath) joined += (joined.empty() ? "" : ".") + key;
    return joined;
}

}  // namespace

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

std::optional<TextRange> findJsoncValue(const std::string& text, const std::vector<std::string>& keyPath) {
    if (keyPath.empty()) return std::nullopt;
    int depth = 0;            // { と [ の深さ（一番外側のオブジェクトの中が 1）
    std::size_t entered = 0;  // keyPath の先頭から何個のキーの、値のオブジェクトの中に入ったか
    std::size_t pos = 0;
    while ((pos = skipSpaceAndComments(text, pos)) < text.size()) {
        const char c = text[pos];
        if (c != '"') {
            if (c == '{' || c == '[') ++depth;
            if (c == '}' || c == ']') {
                --depth;
                // 入ったオブジェクトから出た: その中に次のキーは無かった
                if (depth < static_cast<int>(entered) + 1) return std::nullopt;
            }
            ++pos;
            continue;
        }
        const std::size_t end = stringEnd(text, pos);
        const std::string_view token = std::string_view(text).substr(pos + 1, end - pos - 2);  // 両端の " を除く
        if (depth == static_cast<int>(entered) + 1 && token == keyPath[entered]) {
            // キーの後に : が続けば、その後が値
            std::size_t next = skipSpaceAndComments(text, end);
            if (next < text.size() && text[next] == ':') {
                next = skipSpaceAndComments(text, next + 1);
                if (entered + 1 == keyPath.size()) return scalarValueAt(text, next);
                if (next < text.size() && text[next] == '{') {
                    // 途中のキー: 値のオブジェクトの中へ入って、次のキーを探す
                    ++entered;
                    ++depth;
                    pos = next + 1;
                    continue;
                }
            }
        }
        pos = end;
    }
    return std::nullopt;
}

std::string rewriteJsoncValues(const std::filesystem::path& file, const std::vector<JsoncEdit>& edits) {
    std::string text;
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) return std::format("{} を開けません", toUtf8(file));
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // 先にすべての値の位置を探し、1 つでも無ければ何も書き換えない
    std::vector<std::pair<TextRange, const std::string*>> replacements;
    for (const JsoncEdit& edit : edits) {
        const std::optional<TextRange> range = findJsoncValue(text, edit.keyPath);
        if (!range) {
            return std::format("{} に \"{}\" の項目がないため、保存できません", toUtf8(file.filename()),
                               joinKeyPath(edit.keyPath));
        }
        replacements.emplace_back(*range, &edit.value);
    }
    // 後ろの値から置き換える（前を置き換えると、後ろの位置がずれるため）
    std::sort(replacements.begin(), replacements.end(),
              [](const auto& a, const auto& b) { return a.first.begin > b.first.begin; });
    for (const auto& [range, value] : replacements) text.replace(range.begin, range.end - range.begin, *value);

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::format("{} に書き込めません", toUtf8(file));
    out << text;
    return out ? std::string{} : std::format("{} に書き込めません", toUtf8(file));
}

}  // namespace moba
