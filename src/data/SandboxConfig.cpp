#include "data/SandboxConfig.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>

#include "core/JsonFile.h"
#include "core/Paths.h"
#include "data/CharacterDef.h"

namespace moba {
namespace {

using nlohmann::json;

std::optional<Vector2> pointFromJson(const json& j, std::string_view key) {
    const auto it = j.find(std::string(key));
    if (it == j.end() || !it->is_array() || it->size() != 2) return std::nullopt;
    return Vector2{(*it)[0].get<float>(), (*it)[1].get<float>()};
}

// プリセットに書いた radius / moveSpeed / color を、ダミーごとの値の既定値にする
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

bool samePoint(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

bool sameDummy(const DummyDef& a, const DummyDef& b) {
    const bool samePatrol =
        a.patrolTo.has_value() == b.patrolTo.has_value() && (!a.patrolTo || samePoint(*a.patrolTo, *b.patrolTo));
    return a.name == b.name && samePoint(a.position, b.position) && samePatrol && a.radius == b.radius &&
           a.moveSpeed == b.moveSpeed && a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b &&
           a.color.a == b.color.a;
}

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

    // コメントの行（// で始まる行）を除いて、最初の "preset": "..." の値を書き換える
    static const std::regex kPresetValue(R"re(("preset"\s*:\s*)"(?:[^"\\]|\\.)*")re");
    const std::string quoted = json(std::string(presetName)).dump();  // " や \ を含む名前もエスケープする
    std::size_t lineStart = 0;
    while (lineStart < text.size()) {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = text.size();
        const std::string line = text.substr(lineStart, lineEnd - lineStart);
        const std::size_t firstChar = line.find_first_not_of(" \t");
        const bool comment = firstChar != std::string::npos && line.compare(firstChar, 2, "//") == 0;
        std::smatch match;
        if (!comment && std::regex_search(line, match, kPresetValue)) {
            const std::size_t begin = lineStart + static_cast<std::size_t>(match.position(0));
            text.replace(begin, static_cast<std::size_t>(match.length(0)), match[1].str() + quoted);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out) return std::format("{} に書き込めません", toUtf8(file));
            out << text;
            return out ? std::string{} : std::format("{} に書き込めません", toUtf8(file));
        }
        lineStart = lineEnd + 1;
    }
    return std::format("{} に \"preset\" の行がないため、保存できません", toUtf8(file.filename()));
}

}  // namespace moba
