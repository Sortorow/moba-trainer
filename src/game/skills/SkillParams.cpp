#include "game/skills/SkillParams.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>

#include "data/EffectSpec.h"
#include "game/skills/Skill.h"

namespace moba::skills {
namespace {

using nlohmann::json;

/// エラー・警告の文の頭（「like_emma の R（マジックショー）」）
std::string where(const SkillDef& def) {
    return std::format("{} の {}（{}）", def.characterId, toString(def.slot), def.name);
}

/// JSON の値を、エラーの文に入れるために短く表示する
std::string shown(const json& value) {
    std::string text = value.dump();
    if (text.size() > 40) text = text.substr(0, 37) + "...";
    return text;
}

/// 数の項目の値が、ルールの範囲に入っているか
/// @return 入っていなければ、どう直せばよいかの説明。入っていれば nullopt
std::optional<std::string> checkNumber(const json& value, ParamKind kind) {
    if (!value.is_number()) return "数で書いてください";
    const double x = value.get<double>();
    const bool integer = std::floor(x) == x;
    switch (kind) {
        case ParamKind::Positive:
            if (!(x > 0.0)) return "0 より大きい数にしてください";
            break;
        case ParamKind::NonNegative:
            if (!(x >= 0.0)) return "0 以上の数にしてください";
            break;
        case ParamKind::Ratio:
            if (!(x >= 0.0 && x <= 1.0)) return "0 以上 1 以下の数にしてください";
            break;
        case ParamKind::PositiveInteger:
            if (!(x >= 1.0 && integer)) return "1 以上の整数にしてください";
            break;
        case ParamKind::NonNegativeInteger:
            if (!(x >= 0.0 && integer)) return "0 以上の整数にしてください";
            break;
        default: break;  // AnyNumber はどんな数でもよい
    }
    return std::nullopt;
}

/// 色の値（[R, G, B] か [R, G, B, A]。各 0〜255 の整数）か
/// @return 違えば、どう直せばよいかの説明
std::optional<std::string> checkColor(const json& value) {
    const bool shapeOk = value.is_array() && (value.size() == 3 || value.size() == 4);
    const bool channelsOk =
        shapeOk && std::all_of(value.begin(), value.end(), [](const json& channel) {
            return channel.is_number_integer() && channel.get<int>() >= 0 && channel.get<int>() <= 255;
        });
    if (!channelsOk) return "[R, G, B] か [R, G, B, A]（各 0〜255 の整数）で書いてください";
    return std::nullopt;
}

/// 選択肢を「"a" / "b" / "c"」の形にする（エラーの文に入れる）
std::string choiceText(const std::vector<std::string_view>& choices) {
    std::string text;
    for (const std::string_view choice : choices) text += std::format("{}\"{}\"", text.empty() ? "" : " / ", choice);
    return text;
}

/// 効果の配列（onHit など）を確かめ、エラー・警告を check に足す
/// @param prefix 文の頭（「like_emma の R（マジックショー）: params.hatOnHit」）
void checkEffects(const json& value, const std::string& prefix, SkillCheck& check) {
    if (!value.is_array()) {
        check.errors.push_back(
            std::format("{} は効果の配列 [ {{ \"type\": ... }} ] で書いてください（今: {}）", prefix, shown(value)));
        return;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        const json& item = value[i];
        const std::string at = std::format("{} の {} 番目の効果", prefix, i + 1);
        if (!item.is_object()) {
            check.errors.push_back(
                std::format("{} は {{ \"type\": ... }} の形で書いてください（今: {}）", at, shown(item)));
            continue;
        }
        EffectType type = EffectType::Slow;
        const auto typeIt = item.find("type");
        if (typeIt == item.end() || !typeIt->is_string() || !effectTypeFromString(typeIt->get<std::string>(), type)) {
            check.errors.push_back(
                std::format("{}: type は \"slow\" / \"snare\" / \"stun\" / \"knockup\" / \"polymorph\" / \"mark\" "
                            "のどれかにしてください（今: {}）",
                            at, typeIt == item.end() ? std::string("なし") : shown(*typeIt)));
            continue;
        }
        // 長さ 0 の効果は、かかった次の瞬間に消えるので、0 より大きいこと
        const auto durationIt = item.find("duration");
        if (durationIt == item.end()) {
            check.errors.push_back(std::format("{}: duration（長さ・秒）がありません", at));
        } else if (const std::optional<std::string> reason = checkNumber(*durationIt, ParamKind::Positive)) {
            check.errors.push_back(std::format("{}: duration は {}（今: {}）", at, *reason, shown(*durationIt)));
        }
        // 強さは、スロウ・変身では移動速度の低下率。拘束・スタン・打ち上げ・印では使わない（0 でよい）
        if (const auto amountIt = item.find("amount"); amountIt != item.end()) {
            if (const std::optional<std::string> reason = checkNumber(*amountIt, ParamKind::Ratio))
                check.errors.push_back(std::format("{}: amount は {}（今: {}）", at, *reason, shown(*amountIt)));
        }
        const auto tagIt = item.find("tag");
        if (type == EffectType::Mark &&
            (tagIt == item.end() || !tagIt->is_string() || tagIt->get<std::string>().empty()))
            check.errors.push_back(std::format("{}: 印（mark）には tag（印の名前）を書いてください", at));
        else if (tagIt != item.end() && !tagIt->is_string())
            check.errors.push_back(std::format("{}: tag は文字列で書いてください（今: {}）", at, shown(*tagIt)));
        for (const auto& [key, unused] : item.items()) {
            if (key != "type" && key != "duration" && key != "amount" && key != "tag")
                check.warnings.push_back(std::format(
                    "{}: {} は効果では使いません（書き間違いかもしれません。この項目は無視します）", at, key));
        }
    }
}

/// 項目 1 つの値をルールで確かめ、エラー・警告を check に足す
void checkValue(const json& value, const ParamRule& rule, const std::string& prefix, SkillCheck& check) {
    std::optional<std::string> reason;
    switch (rule.kind) {
        case ParamKind::Positive:
        case ParamKind::NonNegative:
        case ParamKind::Ratio:
        case ParamKind::AnyNumber:
        case ParamKind::PositiveInteger:
        case ParamKind::NonNegativeInteger: reason = checkNumber(value, rule.kind); break;
        case ParamKind::Flag:
            if (!value.is_boolean()) reason = "true か false で書いてください";
            break;
        case ParamKind::Text:
            if (!value.is_string()) reason = "文字列で書いてください";
            break;
        case ParamKind::Choice: {
            const bool ok =
                value.is_string() && std::ranges::find(rule.choices, value.get<std::string>()) != rule.choices.end();
            if (!ok) reason = std::format("{} のどれかにしてください", choiceText(rule.choices));
            break;
        }
        case ParamKind::ChoiceList: {
            const bool ok = value.is_array() && std::all_of(value.begin(), value.end(), [&](const json& item) {
                                return item.is_string() &&
                                       std::ranges::find(rule.choices, item.get<std::string>()) != rule.choices.end();
                            });
            if (!ok) reason = std::format("{} を並べた配列にしてください", choiceText(rule.choices));
            break;
        }
        case ParamKind::Color: reason = checkColor(value); break;
        case ParamKind::Effects: checkEffects(value, prefix, check); return;
    }
    if (reason) check.errors.push_back(std::format("{} は {}（今: {}）", prefix, *reason, shown(value)));
}

}  // namespace

ParamRules joinRules(ParamRules base, const ParamRules& more) {
    base.insert(base.end(), more.begin(), more.end());
    return base;
}

SkillCheck checkSkillParams(const SkillDef& def) {
    SkillCheck check;
    const std::optional<ParamRules> rules = findParamRules(def.handler);
    if (!rules) {
        check.errors.push_back(std::format("{}: handler \"{}\" は存在しません", where(def), def.handler));
        return check;
    }

    // ルールに無い項目名は、書き間違いかもしれないので警告する（使われず、既定値になるだけなので、エラーにはしない）
    for (const auto& [key, unused] : def.params.items()) {
        const bool known =
            std::any_of(rules->begin(), rules->end(), [&](const ParamRule& rule) { return rule.key == key; });
        if (!known)
            check.warnings.push_back(std::format(
                "{}: params.{} は handler \"{}\" では使いません（書き間違いかもしれません。この項目は無視します）",
                where(def), key, def.handler));
    }

    for (const ParamRule& rule : *rules) {
        const std::string prefix = std::format("{}: params.{}", where(def), rule.key);
        const auto it = def.params.find(std::string(rule.key));
        if (it == def.params.end()) {
            if (rule.required) check.errors.push_back(std::format("{} がありません（必ず書く項目です）", prefix));
            continue;
        }
        checkValue(*it, rule, prefix, check);
    }
    return check;
}

}  // namespace moba::skills
