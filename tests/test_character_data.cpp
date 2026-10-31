#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <regex>
#include <string>
#include <vector>

#include "TempDataCopy.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/skills/Skill.h"

using namespace moba;

namespace {

/// 一覧の中に、text を含む文があるか
bool containsText(const std::vector<std::string>& lines, std::string_view text) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& line) { return line.find(text) != std::string::npos; });
}

/// 本物の設定ファイルのスキルの定義（params を書き換えて、ルールのチェックを試すため）
SkillDef realSkill(std::string_view characterId, SkillSlot slot) {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const CharacterDef* def = data.character(characterId);
    REQUIRE(def != nullptr);
    REQUIRE(def->skill(slot) != nullptr);
    return *def->skill(slot);
}

}  // namespace

TEST_CASE("data フォルダの設定ファイルがすべて読み込める（params のルールのチェックも通る）") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.setSkillValidator(skills::checkSkillParams);  // ゲームと同じく、params をルールで確かめる
    data.loadAll();
    for (const std::string& error : data.errors()) MESSAGE(error);
    CHECK(data.errors().empty());
    REQUIRE(data.character("like_ezreal") != nullptr);
    REQUIRE(data.character("like_emma") != nullptr);
    REQUIRE(data.character("like_brand") != nullptr);
}

TEST_CASE("キャラの一覧は menuOrder の順（エマ → エズリアル → ブランド）") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const std::vector<const CharacterDef*> list = data.characterList();
    REQUIRE(list.size() >= 3);
    CHECK(list[0]->id == "like_emma");
    CHECK(list[1]->id == "like_ezreal");
    CHECK(list[2]->id == "like_brand");
}

TEST_CASE("すべてのスキルの handler が実在し、params がルールを満たす（警告も無い）") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const CharacterDef* def : data.characterList()) {
        for (const auto& skill : def->skills) {
            if (!skill) continue;
            INFO(std::format("{} {} handler={}", def->id, toString(skill->slot), skill->handler));
            CHECK(skills::handlerExists(skill->handler));
            const SkillCheck check = skills::checkSkillParams(*skill);
            for (const std::string& error : check.errors) MESSAGE(error);
            for (const std::string& warning : check.warnings) MESSAGE(warning);
            CHECK(check.errors.empty());
            CHECK(check.warnings.empty());
        }
    }
}

TEST_CASE("params のルール: 0 を許す項目だけ 0 を通し、範囲外・型違い・書き忘れはエラーにする") {
    SkillDef r = realSkill("like_emma", SkillSlot::R);
    REQUIRE(skills::checkSkillParams(r).errors.empty());

    auto errorsWith = [](SkillDef def, const std::string& key, const nlohmann::json& value) {
        def.params[key] = value;
        return skills::checkSkillParams(def).errors;
    };
    // 0 を許す項目（待ち時間 0 = すぐ）
    CHECK(errorsWith(r, "hatFlightTime", 0).empty());
    // 0 を許さない項目（鳩が飛ぶ時間の計算で割る。B3）
    CHECK(containsText(errorsWith(r, "doveSpeed", 0), "params.doveSpeed は 0 より大きい数"));
    CHECK(containsText(errorsWith(r, "doveSpeed", -100), "params.doveSpeed は 0 より大きい数"));
    CHECK(containsText(errorsWith(r, "dovePathWidth", "wide"), "params.dovePathWidth は 数で書いてください"));
    CHECK(containsText(errorsWith(r, "targetKinds", {"dove", "cat"}), "params.targetKinds は"));
    CHECK(containsText(errorsWith(r, "color", {300, 0, 0}), "params.color は"));
    // 必ず書く項目が無い
    SkillDef noRadius = r;
    noRadius.params.erase("hatRadius");
    CHECK(containsText(skills::checkSkillParams(noRadius).errors, "params.hatRadius がありません"));
    // 効果: 長さ 0・強さが 1 より大きい・印に名前が無い
    CHECK(containsText(errorsWith(r, "hatOnHit", nlohmann::json::parse(R"([{"type": "knockup", "duration": 0}])")),
                       "duration は 0 より大きい数"));
    CHECK(containsText(
        errorsWith(r, "doveOnHit", nlohmann::json::parse(R"([{"type": "slow", "duration": 1, "amount": 1.5}])")),
        "amount は 0 以上 1 以下"));
    CHECK(containsText(errorsWith(r, "doveOnHit", nlohmann::json::parse(R"([{"type": "mark", "duration": 1}])")),
                       "tag（印の名前）"));
    // 存在しない handler
    SkillDef unknown = r;
    unknown.handler = "no_such_handler";
    CHECK(containsText(skills::checkSkillParams(unknown).errors, "handler \"no_such_handler\" は存在しません"));

    // 整数の項目・負の値も許す項目
    SkillDef brandR = realSkill("like_brand", SkillSlot::R);
    CHECK(errorsWith(brandR, "bounces", 0).empty());  // 0 = 跳ね返らない
    CHECK(containsText(errorsWith(brandR, "bounces", 1.5), "0 以上の整数"));
    CHECK(containsText(errorsWith(brandR, "maxHitsPerTarget", 0), "1 以上の整数"));
    SkillDef emmaQ = realSkill("like_emma", SkillSlot::Q);
    CHECK(errorsWith(emmaQ, "doveCardDrawOffsetForward", -70).empty());  // 見た目のずれは負でもよい
    CHECK(containsText(errorsWith(emmaQ, "visual", "sparkle"), "params.visual は \"orb\""));
}

TEST_CASE("params のルール: ルールに無い項目名は、エラーではなく警告にする") {
    SkillDef r = realSkill("like_emma", SkillSlot::R);
    r.params["doveSped"] = 3000;  // doveSpeed の書き間違い
    const SkillCheck check = skills::checkSkillParams(r);
    CHECK(check.errors.empty());
    CHECK(containsText(check.warnings, "params.doveSped は handler \"emma_r\" では使いません"));
}

TEST_CASE("params のチェックに通らないキャラのファイルは使わない（初めてなら選べず、読み直しなら前回の内容を使う）") {
    const test::TempDataCopy copy;
    const std::filesystem::path emmaFile = copy.paths().charactersDir / "like_emma.jsonc";
    GameData data(copy.paths());
    data.setSkillValidator(skills::checkSkillParams);

    // 読み直し: 前に読めた内容（doveSpeed 3000）を使い続け、理由をエラーに出す
    data.loadAll();
    REQUIRE(data.character("like_emma") != nullptr);
    REQUIRE(test::TempDataCopy::rewrite(emmaFile, std::regex(R"("doveSpeed": 3000)"), R"("doveSpeed": 0)") == 1);
    data.loadAll();
    const CharacterDef* kept = data.character("like_emma");
    REQUIRE(kept != nullptr);
    CHECK(kept->skill(SkillSlot::R)->num("doveSpeed", 0.0f) == doctest::Approx(3000.0f));
    CHECK(containsText(data.errors(), "params.doveSpeed は 0 より大きい数"));
    CHECK(containsText(data.errors(), "like_emma.jsonc: スキルの params に誤りがあるため、このファイルを使いません"));

    // 初めての読み込み: そのキャラは選べない（ほかのキャラは使える）
    GameData fresh(copy.paths());
    fresh.setSkillValidator(skills::checkSkillParams);
    fresh.loadAll();
    CHECK(fresh.character("like_emma") == nullptr);
    CHECK(fresh.character("like_ezreal") != nullptr);
}

TEST_CASE("params のチェック: 警告だけのキャラは使え、警告は設定ファイルのエラーの一覧に出る") {
    const test::TempDataCopy copy;
    const std::filesystem::path emmaFile = copy.paths().charactersDir / "like_emma.jsonc";
    REQUIRE(test::TempDataCopy::rewrite(emmaFile, std::regex(R"(("doveLifetime": 6,))"),
                                        R"($1 "doveLifetme": 6,)") == 1);  // doveLifetime の書き間違いを足す
    GameData data(copy.paths());
    data.setSkillValidator(skills::checkSkillParams);
    data.loadAll();
    CHECK(data.character("like_emma") != nullptr);
    CHECK(containsText(data.errors(), "警告: like_emma の Q（カード）: params.doveLifetme は"));
}

TEST_CASE("設定値が読み込まれている") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const SkillDef* q = data.character("like_ezreal")->skill(SkillSlot::Q);
    REQUIRE(q != nullptr);
    CHECK(q->range == doctest::Approx(1200.0f));
    // 列挙型の比較は括弧で囲む（doctest がゲーム側の toString を表示に使おうとするのを避けるため）
    CHECK((q->targeting == Targeting::Direction));
    CHECK(q->num("speed", 0.0f) == doctest::Approx(2000.0f));
    CHECK(data.character("like_ezreal")->skill(SkillSlot::F) == nullptr);  // F は無し

    const SkillDef* e = data.character("like_emma")->skill(SkillSlot::E);
    REQUIRE(e != nullptr);
    const std::vector<EffectSpec> effects = e->effects("onHit");
    REQUIRE(effects.size() == 1);
    CHECK((effects[0].type == EffectType::Polymorph));
}
