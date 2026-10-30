#include "data/SandboxConfig.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <set>

#include "core/JsonFile.h"
#include "core/Paths.h"
#include "data/CharacterDef.h"

namespace moba {
namespace {

using nlohmann::json;

/// 文字列の中の範囲 [begin, end)
struct TextRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

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

/// コメント付き JSON の text の中で、一番外側のオブジェクトの key の値（文字列）の位置を探す。
/// 文字列・行コメント・ブロックコメントを区別しながら読むので、それらの中にある同じ文字は無視する
/// @return 値の文字列の範囲（両端の " を含む）。見つからなければ nullopt
std::optional<TextRange> findTopLevelStringValue(const std::string& text, std::string_view key) {
    int depth = 0;  // { と [ の深さ（一番外側のオブジェクトの中が 1）
    std::size_t pos = 0;
    while ((pos = skipSpaceAndComments(text, pos)) < text.size()) {
        const char c = text[pos];
        if (c != '"') {
            if (c == '{' || c == '[') ++depth;
            if (c == '}' || c == ']') --depth;
            ++pos;
            continue;
        }
        const std::size_t end = stringEnd(text, pos);
        const std::string_view token = std::string_view(text).substr(pos + 1, end - pos - 2);  // 両端の " を除く
        if (depth == 1 && token == key) {
            // key の後に : と文字列が続けば、それが値
            std::size_t next = skipSpaceAndComments(text, end);
            if (next < text.size() && text[next] == ':') {
                next = skipSpaceAndComments(text, next + 1);
                if (next < text.size() && text[next] == '"') return TextRange{next, stringEnd(text, next)};
            }
        }
        pos = end;
    }
    return std::nullopt;
}

/// j[key] の [x, y] を座標として読む。書かれていない・2 つの数でなければ nullopt
std::optional<Vector2> pointFromJson(const json& j, std::string_view key) {
    const auto it = j.find(std::string(key));
    if (it == j.end() || !it->is_array() || it->size() != 2) return std::nullopt;
    return Vector2{(*it)[0].get<float>(), (*it)[1].get<float>()};
}

/// ダミー 1 体の設定を読む。プリセットに書いた radius / moveSpeed / color を、ダミーごとの値の既定値にする
/// @param defaults プリセットの値（ダミーに書かれていない項目に使う）
/// @param index プリセットの中で何体目か（0 始まり。名前を省略した時の「ダミー1」などに使う）
/// @param warnings 読めたがおかしかった項目の説明を足す
DummyDef parseDummy(const json& j, const DummyDef& defaults, std::size_t index, const std::string& presetName,
                    std::vector<std::string>& warnings) {
    DummyDef dummy = defaults;
    dummy.name = j.value("name", std::format("ダミー{}", index + 1));
    if (const std::optional<Vector2> position = pointFromJson(j, "position")) {
        dummy.position = *position;
    } else {
        warnings.push_back(std::format("プリセット \"{}\" の {} に \"position\" がありません。(0, 0) に置きます",
                                       presetName, dummy.name));
    }
    dummy.patrolTo = pointFromJson(j, "patrolTo");
    dummy.radius = std::clamp(j.value("radius", dummy.radius), 10.0f, 400.0f);
    dummy.moveSpeed = std::clamp(j.value("moveSpeed", dummy.moveSpeed), 0.0f, 2000.0f);
    if (const auto it = j.find("color"); it != j.end()) dummy.color = colorFromJson(*it, dummy.color);
    return dummy;
}

/// 2 つの座標がまったく同じか
bool samePoint(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

/// 2 体のダミーの設定（名前・位置・往復先・大きさ・速さ・色）がすべて同じか
bool sameDummy(const DummyDef& a, const DummyDef& b) {
    const bool samePatrol =
        a.patrolTo.has_value() == b.patrolTo.has_value() && (!a.patrolTo || samePoint(*a.patrolTo, *b.patrolTo));
    return a.name == b.name && samePoint(a.position, b.position) && samePatrol && a.radius == b.radius &&
           a.moveSpeed == b.moveSpeed && a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b &&
           a.color.a == b.color.a;
}

/// 2 つのプリセット（名前・説明・すべてのダミー）が同じか
bool samePreset(const SandboxPreset& a, const SandboxPreset& b) {
    return a.name == b.name && a.description == b.description &&
           std::equal(a.dummies.begin(), a.dummies.end(), b.dummies.begin(), b.dummies.end(), sameDummy);
}

}  // namespace

bool sameSandboxConfig(const SandboxConfig& a, const SandboxConfig& b) {
    return a.selectedPreset == b.selectedPreset &&
           std::equal(a.presets.begin(), a.presets.end(), b.presets.begin(), b.presets.end(), samePreset);
}

const SandboxPreset* SandboxConfig::preset(std::string_view name) const {
    const auto it =
        std::find_if(presets.begin(), presets.end(), [&](const SandboxPreset& p) { return p.name == name; });
    return it == presets.end() ? nullptr : &*it;
}

const SandboxPreset* SandboxConfig::current() const {
    if (const SandboxPreset* selected = preset(selectedPreset)) return selected;
    return presets.empty() ? nullptr : &presets.front();
}

SandboxConfigLoadResult loadSandboxConfig(const std::filesystem::path& file) {
    SandboxConfigLoadResult result;
    const JsonLoadResult loaded = loadJsonFile(file);
    if (!loaded.ok()) {
        result.error = loaded.error;
        return result;
    }

    const json& j = loaded.value;
    SandboxConfig& c = result.config;
    try {
        std::set<std::string> names;
        for (const json& item : j.at("presets")) {
            SandboxPreset preset;
            preset.name = item.value("name", std::format("プリセット{}", c.presets.size() + 1));
            preset.description = item.value("description", std::string{});
            if (!names.insert(preset.name).second) {
                result.warnings.push_back(
                    std::format("プリセット \"{}\" が重複しています（後の方は使いません）", preset.name));
                continue;
            }
            DummyDef defaults;
            defaults.radius = std::clamp(item.value("radius", defaults.radius), 10.0f, 400.0f);
            defaults.moveSpeed = std::clamp(item.value("moveSpeed", defaults.moveSpeed), 0.0f, 2000.0f);
            if (const auto it = item.find("color"); it != item.end())
                defaults.color = colorFromJson(*it, defaults.color);
            for (const json& dummy : item.value("dummies", json::array())) {
                preset.dummies.push_back(
                    parseDummy(dummy, defaults, preset.dummies.size(), preset.name, result.warnings));
            }
            c.presets.push_back(std::move(preset));
        }
        if (c.presets.empty()) {
            result.error = std::format("{}: \"presets\" が空です", toUtf8(file.filename()));
            return result;
        }
        c.selectedPreset = j.value("preset", c.presets.front().name);
        if (!c.preset(c.selectedPreset)) {
            result.warnings.push_back(std::format("\"preset\": \"{}\" は \"presets\" にないため、\"{}\" を使います",
                                                  c.selectedPreset, c.presets.front().name));
            c.selectedPreset = c.presets.front().name;
        }
    } catch (const json::exception& e) {
        result.error = std::format("{}: {}", toUtf8(file.filename()), e.what());
    }
    return result;
}

std::string saveSelectedSandboxPreset(const std::filesystem::path& file, std::string_view presetName) {
    std::string text;
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) return std::format("{} を開けません", toUtf8(file));
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // 一番外側の "preset": "..." の値だけを書き換える（コメント・文字列・内側のオブジェクトの中は対象外）
    const std::optional<TextRange> value = findTopLevelStringValue(text, "preset");
    if (!value) return std::format("{} に \"preset\" の項目がないため、保存できません", toUtf8(file.filename()));
    const std::string quoted = json(std::string(presetName)).dump();  // " や \ を含む名前もエスケープする
    text.replace(value->begin, value->end - value->begin, quoted);

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::format("{} に書き込めません", toUtf8(file));
    out << text;
    return out ? std::string{} : std::format("{} に書き込めません", toUtf8(file));
}

}  // namespace moba
