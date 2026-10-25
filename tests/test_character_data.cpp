#include <doctest/doctest.h>

#include <format>

#include "core/Paths.h"
#include "data/GameData.h"
#include "game/skills/Skill.h"

using namespace moba;

TEST_CASE("data フォルダの設定ファイルがすべて読み込める") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const std::string& error : data.errors()) MESSAGE(error);
    CHECK(data.errors().empty());
    REQUIRE(data.character("like_ezreal") != nullptr);
    REQUIRE(data.character("like_emma") != nullptr);
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

TEST_CASE("すべてのスキルの handler が実在する") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const CharacterDef* def : data.characterList()) {
        for (const auto& skill : def->skills) {
            if (!skill) continue;
            INFO(std::format("{} {} handler={}", def->id, toString(skill->slot), skill->handler));
            CHECK(skills::handlerExists(skill->handler));
        }
    }
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
