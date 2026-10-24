// スキルドッジのテスト: 敵の発動とドッジ率・HP・移動系スキルの制限・敵 AI の狙い・ブランドの R

#include <doctest/doctest.h>

#include <cmath>
#include <format>
#include <map>
#include <memory>
#include <random>

#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/World.h"
#include "game/ai/Aim.h"
#include "raymath.h"

using namespace moba;

namespace {

class DodgeFixture {
public:
    explicit DodgeFixture(std::string_view playerCharacter, bool dodgeRules = true)
        : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        if (dodgeRules) world_->setRules(WorldRules{.movementSkillsOnly = true});
        const CharacterDef* def = data_.character(playerCharacter);
        REQUIRE(def != nullptr);
        Unit player = makePlayerUnit(*def);
        if (dodgeRules) player.maxHp = player.hp = 5;
        playerId_ = world_->addUnit(player);
    }

    EntityId addEnemy(std::string_view characterId, Vector2 pos) {
        const CharacterDef* def = data_.character(characterId);
        REQUIRE(def != nullptr);
        Unit enemy;
        enemy.team = Team::Enemy;
        enemy.name = def->displayName;
        enemy.characterId = def->id;
        enemy.pos = pos;
        enemy.radius = def->hitboxRadius;
        enemy.baseMoveSpeed = def->moveSpeed;
        return world_->addUnit(enemy);
    }

    EntityId addDummy(Vector2 pos) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 65.0f;
        return world_->addUnit(dummy);
    }

    World& world() { return *world_; }
    const GameData& data() const { return data_; }
    Unit& player() { return *world_->findUnit(playerId_); }
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    void cast(EntityId caster, SkillSlot slot, Vector2 cursor) {
        casting::requestCast(*world_, unit(caster), CastRequest{slot, cursor});
    }
    void playerCast(SkillSlot slot, Vector2 cursor) { cast(playerId_, slot, cursor); }

    void run(float seconds) {
        const int steps = static_cast<int>(seconds * 120.0f);
        for (int i = 0; i < steps; ++i) world_->update(1.0f / 120.0f);
    }

private:
    GameData data_;
    std::unique_ptr<World> world_;
    EntityId playerId_ = kInvalidId;
};

}  // namespace

TEST_CASE("敵のスキルが当たると HP が 1 減り、「被弾」として数える") {
    DodgeFixture f("like_ezreal");
    const EntityId brand = f.addEnemy("like_brand", {700, 0});
    f.cast(brand, SkillSlot::Q, {0, 0});
    f.run(1.0f);
    CHECK(f.player().hp == 4);
    CHECK(f.world().dodge().total == 1);
    CHECK(f.world().dodge().hit == 1);
    CHECK(f.world().dodge().dodged == 0);
}

TEST_CASE("当たらなかった発動は「回避」として数え、飛んでいる途中はまだ数えない") {
    DodgeFixture f("like_ezreal");
    const EntityId brand = f.addEnemy("like_brand", {700, 0});
    f.cast(brand, SkillSlot::Q, {700, 600});  // 自キャラとは別の方向へ
    f.run(0.4f);
    CHECK(f.world().dodge().total == 0);  // まだ飛んでいる
    f.run(1.0f);
    CHECK(f.world().dodge().total == 1);
    CHECK(f.world().dodge().dodged == 1);
    CHECK(f.player().hp == 5);
}

TEST_CASE("5 回当たると、ダウン数が 1 増えて全回復する") {
    DodgeFixture f("like_ezreal");
    const EntityId brand = f.addEnemy("like_brand", {700, 0});
    for (int i = 0; i < 5; ++i) {
        f.unit(brand).cooldowns = {};
        f.cast(brand, SkillSlot::Q, {0, 0});
        f.run(1.0f);
    }
    CHECK(f.player().deaths == 1);
    CHECK(f.player().hp == 5);
    CHECK(f.world().dodge().hit == 5);
}

TEST_CASE("遅れて爆発するスキルは、予告の間に範囲から出れば当たらない") {
    DodgeFixture f("like_ezreal");
    const EntityId brand = f.addEnemy("like_brand", {700, 0});
    // 円の中心からは、半径 260 + 当たり判定 65 = 325 離れれば当たらない。
    // 爆発までの約 0.875 秒で抜けられるよう、このテストでは移動速度を上げる（325 のままでは 284 しか動けない）
    f.player().baseMoveSpeed = 500.0f;
    f.cast(brand, SkillSlot::W, {0, 0});
    casting::orderMove(f.world(), f.player(), {0, 600});
    f.run(1.5f);
    CHECK(f.player().hp == 5);
    CHECK(f.world().dodge().dodged == 1);

    f.unit(brand).cooldowns = {};
    f.cast(brand, SkillSlot::W, f.player().pos);  // 今度は動かない
    f.run(1.5f);
    CHECK(f.player().hp == 4);
    CHECK(f.world().dodge().hit == 1);
}

TEST_CASE("スキルドッジでは、自キャラは移動系のスキルだけ使える") {
    DodgeFixture f("like_ezreal");
    f.playerCast(SkillSlot::Q, {600, 0});
    CHECK_FALSE(f.player().cast.active);
    CHECK(f.player().cooldowns[static_cast<std::size_t>(index(SkillSlot::Q))] == doctest::Approx(0.0f));

    f.playerCast(SkillSlot::D, {1000, 0});  // フラッシュは使える
    CHECK(f.player().pos.x == doctest::Approx(400.0f));
}

TEST_CASE("敵 AI の狙い: 移動を続ける前提なら偏差撃ちになり、横に動く相手に当たる") {
    DodgeFixture f("like_ezreal");
    const SkillDef* q = f.data().character("like_ezreal")->skill(SkillSlot::Q);
    REQUIRE(q != nullptr);
    const std::optional<ai::AimModel> model = ai::aimModelFor(*q);
    REQUIRE(model.has_value());
    CHECK((model->shape == ai::AimShape::Line));

    ai::AimTarget target;
    target.pos = {800, 0};
    target.velocity = {0, 325};
    target.moveSpeed = 325;
    target.radius = 65;
    const float t = model->windup + (800.0f - 65.0f) / model->speed;
    const Vector2 future = target.pos + target.velocity * t;

    const auto lead = ai::chooseAim({0, 0}, *model, target, ai::AimWeights{0, 1, 0, 0}, 0.25f);
    REQUIRE(lead.has_value());
    CHECK(ai::wouldHit({0, 0}, *model, lead->aimPoint, future, target.radius));

    const auto naive = ai::chooseAim({0, 0}, *model, target, ai::AimWeights{1, 0, 0, 0}, 0.25f);
    REQUIRE(naive.has_value());
    CHECK_FALSE(ai::wouldHit({0, 0}, *model, naive->aimPoint, future, target.radius));

    // 実際に撃って確かめる: 自キャラは (800, 0) から下へ移動し続ける
    const EntityId ezreal = f.addEnemy("like_ezreal", {0, 0});
    f.player().pos = {800, 0};
    casting::orderMove(f.world(), f.player(), {800, 1500});
    f.cast(ezreal, SkillSlot::Q, lead->aimPoint);
    f.run(1.0f);
    CHECK(f.player().hp == 4);
}

TEST_CASE("敵 AI の狙い: 射程外なら撃たない") {
    DodgeFixture f("like_ezreal");
    const std::optional<ai::AimModel> model = ai::aimModelFor(*f.data().character("like_brand")->skill(SkillSlot::W));
    REQUIRE(model.has_value());
    CHECK((model->shape == ai::AimShape::Circle));
    ai::AimTarget target;
    target.pos = {2000, 0};
    target.radius = 65;
    CHECK_FALSE(ai::chooseAim({0, 0}, *model, target, ai::AimWeights{}, 0.25f).has_value());
}

TEST_CASE("ブランド R: 敵が 1 体なら敵と自分の間を跳ね返り、同じ敵には 3 回まで当たる") {
    DodgeFixture f("like_brand", false);
    const EntityId dummy = f.addDummy({400, 0});
    int hits = 0;
    f.world().setHitObserver([&](int, SkillSlot, const Unit& target) {
        if (target.id == dummy) ++hits;
    });
    f.playerCast(SkillSlot::R, {400, 0});
    f.run(8.0f);
    CHECK(hits == 3);
    CHECK(f.world().projectiles().empty());
}

TEST_CASE("ブランド R: 敵が 2 体なら両方に跳ね返り、どちらも 3 回を超えない") {
    DodgeFixture f("like_brand", false);
    const EntityId a = f.addDummy({400, 0});
    const EntityId b = f.addDummy({400, 300});
    std::map<EntityId, int> hits;
    f.world().setHitObserver([&](int, SkillSlot, const Unit& target) { ++hits[target.id]; });
    f.playerCast(SkillSlot::R, {400, 0});
    f.run(8.0f);
    CHECK(hits[a] >= 1);
    CHECK(hits[b] >= 1);
    CHECK(hits[a] <= 3);
    CHECK(hits[b] <= 3);
    CHECK(hits[a] + hits[b] >= 3);
}

TEST_CASE("賢さが高いほど、ランダムに動き回る相手に当てやすい") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();

    // 0.5 秒ごとにランダムな方向へ向きを変えて動き回る相手に、敵 3 体で 60 秒間撃たせた時のドッジ率
    auto dodgeRateAgainstRandomWalker = [&](int aiLevel) {
        MatchConfig config;
        config.mode = GameMode::SkillDodge;
        config.characterId = "like_ezreal";
        config.enemyCount = 3;
        config.aiLevel = aiLevel;
        config.timeLimitSeconds = 60.0f;
        config.seed = 7;
        Match match(data, config);
        std::mt19937 rng(123);
        std::uniform_real_distribution<float> angle(-PI, PI);
        const InputFrame noInput;
        for (int i = 0; !match.finished(); ++i) {
            if (i % 60 == 0) {
                Unit* player = match.world().player();
                const float a = angle(rng);
                casting::orderMove(match.world(), *player, player->pos + Vector2{std::cos(a), std::sin(a)} * 400.0f);
            }
            match.update(1.0f / 120.0f, noInput);
        }
        return match.result().dodge.rate();
    };

    float rates[6] = {};
    for (int level = 1; level <= 5; ++level) rates[level] = dodgeRateAgainstRandomWalker(level);
    MESSAGE(std::format("ランダムに動く相手のドッジ率: 賢さ1 {:.0f}% / 2 {:.0f}% / 3 {:.0f}% / 4 {:.0f}% / 5 {:.0f}%",
                        rates[1] * 100, rates[2] * 100, rates[3] * 100, rates[4] * 100, rates[5] * 100));
    CHECK(rates[1] > rates[3]);
    CHECK(rates[3] > rates[5]);
}

TEST_CASE("スキルドッジを制限時間まで遊ぶと、敵がスキルを撃ち、ドッジ率が集計される") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    MatchConfig config;
    config.mode = GameMode::SkillDodge;
    config.characterId = "like_ezreal";
    config.enemyCount = 3;
    config.aiLevel = 3;
    config.timeLimitSeconds = 20.0f;
    config.seed = 42;
    Match match(data, config);
    REQUIRE(match.valid());
    CHECK(match.enemies().enemyCount() == 3);

    const InputFrame noInput;
    for (int i = 0; i < 20 * 120 + 10 && !match.finished(); ++i) match.update(1.0f / 120.0f, noInput);
    CHECK(match.finished());
    const MatchResult result = match.result();
    CHECK(result.dodge.total > 0);
    CHECK(result.hitsTaken > 0);  // 動かない相手には当たる
    CHECK(result.dodge.dodged + result.dodge.hit == result.dodge.total);
}
