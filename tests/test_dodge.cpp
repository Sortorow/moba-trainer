// スキルドッジのテスト: 敵の発動とドッジ率・HP・移動系スキルの制限・敵 AI の狙い・ブランドの R

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <string>

#include "TempDataCopy.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "data/StageDef.h"
#include "game/Match.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/Terrain.h"
#include "game/World.h"
#include "game/ai/Aim.h"
#include "game/ai/EnemyAI.h"
#include "raymath.h"

using namespace moba;

namespace {

/// スキルドッジのテスト用の準備。障害物のない World に自キャラを置き、敵やダミーを足して動かす
class DodgeFixture {
public:
    /// data フォルダの設定を読み込み、障害物のない World に自キャラを置く
    /// @param withPlayerHp 自キャラに HP（5）を持たせるか（スキルドッジと同じ状態にするか）
    explicit DodgeFixture(std::string_view playerCharacter, bool withPlayerHp = true)
        : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        const CharacterDef* def = data_.character(playerCharacter);
        REQUIRE(def != nullptr);
        Unit player = makePlayerUnit(*def);
        if (withPlayerHp) player.maxHp = player.hp = 5;
        playerId_ = world_->addUnit(player);
    }

    /// キャラの設定のとおりの敵キャラを置く（敵 AI は付けない）
    /// @return 敵の通し番号
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

    /// 敵の陣営のダミー（半径 65）を置く
    /// @return ダミーの通し番号
    EntityId addDummy(Vector2 pos) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 65.0f;
        return world_->addUnit(dummy);
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 読み込んだ設定ファイル
    const GameData& data() const { return data_; }
    /// 自キャラ
    Unit& player() { return *world_->findUnit(playerId_); }
    /// 通し番号のキャラ（いることが分かっている時に使う）
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    /// キャラ caster に、カーソルを cursor に置いてスキルのキーを押させる
    void cast(EntityId caster, SkillSlot slot, Vector2 cursor) {
        casting::requestCast(*world_, unit(caster), CastRequest{slot, cursor});
    }
    /// 自キャラに、カーソルを cursor に置いてスキルのキーを押させる
    void playerCast(SkillSlot slot, Vector2 cursor) { cast(playerId_, slot, cursor); }

    /// World を seconds 秒進める（ゲームと同じ 1/120 秒刻み）
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

TEST_CASE("「移動系だけ使える」ルールを有効にすると、移動系のスキルだけ使える") {
    DodgeFixture f("like_ezreal");
    f.world().setRules(WorldRules{.movementSkillsOnly = true});
    f.playerCast(SkillSlot::Q, {600, 0});
    CHECK_FALSE(f.player().cast.active);
    CHECK(f.player().cooldowns[static_cast<std::size_t>(index(SkillSlot::Q))] == doctest::Approx(0.0f));

    f.playerCast(SkillSlot::D, {1000, 0});  // フラッシュは使える
    CHECK(f.player().pos.x == doctest::Approx(400.0f));
}

TEST_CASE("スキルドッジでも、自キャラは攻撃スキルを使える") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    MatchConfig config;
    config.mode = GameMode::SkillDodge;
    config.characterId = "like_emma";
    config.enemyCount = 1;
    config.seed = 3;
    Match match(data, config);
    REQUIRE(match.valid());
    CHECK_FALSE(match.world().rules().movementSkillsOnly);
    Unit* player = match.world().player();
    casting::requestCast(match.world(), *player, CastRequest{SkillSlot::Q, {600, 0}});
    CHECK(player->cast.active);
}

TEST_CASE("エマは E で兎にした敵キャラへ R で移動できる（回避に使える）。敵にダメージは無い") {
    DodgeFixture f("like_emma");
    const EntityId brand = f.addEnemy("like_brand", {400, 0});
    f.playerCast(SkillSlot::E, {400, 0});
    f.run(0.6f);
    REQUIRE(f.unit(brand).status.has(EffectType::Polymorph));
    CHECK_FALSE(f.unit(brand).status.canCast());  // 兎になっている間はスキルを撃てない

    f.playerCast(SkillSlot::R, {400, 0});
    f.run(0.2f);
    CHECK(Vector2Distance(f.player().pos, f.unit(brand).pos) < 150.0f);
    CHECK(f.unit(brand).alive);
    CHECK(f.unit(brand).maxHp == 0);  // 敵には HP が無い
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
    auto dodgeRateAgainstRandomWalker = [&](int aiLevel, std::uint32_t seed) {
        MatchConfig config;
        config.mode = GameMode::SkillDodge;
        config.characterId = "like_ezreal";
        config.enemyCount = 3;
        config.aiLevel = aiLevel;
        config.timeLimitSeconds = 60.0f;
        config.seed = seed;
        Match match(data, config);
        std::mt19937 rng(seed + 100);
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

    // 1 回だけだと、キャラの数値を少し調整しただけで順位が入れ替わることがあるので、乱数の種を変えた平均で比べる
    constexpr std::uint32_t kSeeds[] = {7, 8, 9, 10};
    float rates[6] = {};
    for (int level = 1; level <= 5; ++level) {
        for (const std::uint32_t seed : kSeeds) rates[level] += dodgeRateAgainstRandomWalker(level, seed);
        rates[level] /= static_cast<float>(std::size(kSeeds));
    }
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

namespace {

/// data フォルダを一時フォルダへ写し、すべてのスキルの詠唱時間と、敵のクールダウン倍率を 0 にする
/// （詠唱もクールダウンも無いスキルを敵が撃つ状態を作るため。本物の data フォルダは書き換えない）
class NoCastTimeData {
public:
    /// data フォルダを一時フォルダへ丸ごと写し、キャラの "castTime" と ai.jsonc の "enemyCooldownScale" を 0 に書き換える
    NoCastTimeData() {
        const AppPaths copied = copy_.paths();
        for (const auto& entry : std::filesystem::directory_iterator(copied.charactersDir)) {
            test::TempDataCopy::rewrite(entry.path(), std::regex(R"re(("castTime"\s*:\s*)[0-9.]+)re"), "$1 0");
        }
        test::TempDataCopy::rewrite(copied.aiFile, std::regex(R"re(("enemyCooldownScale"\s*:\s*)[0-9.]+)re"), "$1 0");
    }
    /// 写した data フォルダの各ファイルの場所
    AppPaths paths() const { return copy_.paths(); }

private:
    test::TempDataCopy copy_;
};

}  // namespace

TEST_CASE("詠唱時間もクールダウンも 0 のスキルでも、敵は撃つ間隔（castInterval）を守る") {
    const NoCastTimeData temp;
    GameData data(temp.paths());
    data.loadAll();
    REQUIRE(data.errors().empty());
    REQUIRE(data.ai().enemyCooldownScale == 0.0f);
    for (const CharacterDef* def : data.enemyCandidates()) {
        for (const auto& skill : def->skills) {
            if (skill && skill->usedByEnemy) REQUIRE(skill->castTime == 0.0f);
        }
    }

    // 賢さ 1 は punishCC（自キャラが動けない時は間隔を待たずに撃つ）が無いので、間隔だけで撃つ回数が決まる
    MatchConfig config;
    config.mode = GameMode::SkillDodge;
    config.characterId = "like_ezreal";
    config.enemyCount = 1;
    config.aiLevel = 1;
    config.timeLimitSeconds = 60.0f;
    config.seed = 42;
    Match match(data, config);
    REQUIRE(match.valid());

    constexpr float kSeconds = 8.0f;
    const InputFrame noInput;
    for (int i = 0; i < static_cast<int>(kSeconds * 120.0f); ++i) match.update(1.0f / 120.0f, noInput);

    // 撃てるのは、最初の 1 回（firstCastDelay の後）と、その後は castInterval の最小ごとに 1 回まで
    const AiLevel& level = data.ai().level(config.aiLevel);
    const int maxCasts = 1 + static_cast<int>(std::ceil((kSeconds - data.ai().firstCastDelay) / level.castIntervalMin));
    const int casts = match.world().dodge().total;  // 結果が確定した敵の発動の数
    MESSAGE(std::format("{:.0f} 秒間に敵が撃った回数: {}（上限 {}）", kSeconds, casts, maxCasts));
    CHECK(casts >= 1);
    CHECK(casts <= maxCasts);
}

TEST_CASE("punishCC の敵は、動けない自キャラにも punishMinInterval の間隔を空けて撃つ（クールダウン 0 でも）") {
    const NoCastTimeData temp;
    GameData data(temp.paths());
    data.loadAll();
    REQUIRE(data.errors().empty());
    REQUIRE(data.ai().punishMinInterval > 0.0f);

    MatchConfig config;
    config.mode = GameMode::SkillDodge;
    config.characterId = "like_ezreal";
    config.stageId = "open";  // ブッシュに隠れて見えなくならないように、障害物のないステージ
    config.enemyCount = 1;
    config.aiLevel = 3;
    config.timeLimitSeconds = 60.0f;
    config.seed = 42;
    REQUIRE(data.ai().level(config.aiLevel).punishCC);
    Match match(data, config);
    REQUIRE(match.valid());
    // 自キャラは最初から最後まで動けない（敵は間隔を待たずに撃とうとする）
    match.world().player()->status.apply(EffectSpec{EffectType::Stun, 100.0f, 0.0f, ""}, kInvalidId);

    constexpr float kSeconds = 8.0f;
    const InputFrame noInput;
    for (int i = 0; i < static_cast<int>(kSeconds * 120.0f); ++i) match.update(1.0f / 120.0f, noInput);

    // 撃てるのは、punishMinInterval ごとに 1 回まで
    const int maxCasts = 1 + static_cast<int>(std::ceil(kSeconds / data.ai().punishMinInterval));
    const int casts = match.world().dodge().total;
    MESSAGE(std::format("{:.0f} 秒間に、動けない自キャラへ敵が撃った回数: {}（上限 {}）", kSeconds, casts, maxCasts));
    CHECK(casts >= 2);
    CHECK(casts <= maxCasts);
}

TEST_CASE("敵 AI: 自キャラが最初からブッシュに隠れていて一度も見えていなければ、(0, 0) ではなく今の位置を基準に動く") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const CharacterDef* def = data.character("like_ezreal");
    REQUIRE(def != nullptr);

    // (1500, 0) にブッシュがあるステージ。自キャラはその中から始まる
    StageDef stage;
    stage.id = "bush_test";
    stage.bounds = kSandboxBounds;
    Shape bush;
    bush.kind = Shape::Kind::Circle;
    bush.center = Vector2{1500.0f, 0.0f};
    bush.radius = 250.0f;
    stage.bushes.push_back(bush);
    World world(data, Terrain(stage));
    Unit player = makePlayerUnit(*def);
    player.pos = bush.center;
    world.addUnit(player);

    // 敵は自キャラの向こう側（原点から見て、自キャラより遠い所）
    Unit enemy;
    enemy.team = Team::Enemy;
    enemy.name = "敵";
    enemy.characterId = def->id;
    enemy.radius = def->hitboxRadius;
    enemy.baseMoveSpeed = def->moveSpeed;
    enemy.pos = Vector2{2200.0f, 0.0f};
    const EntityId enemyId = world.addUnit(enemy);
    REQUIRE_FALSE(world.canSee(Team::Enemy, *world.player()));

    ai::EnemyDirector director;
    director.setup(data.ai(), 1, 42);
    director.addEnemy(enemyId);
    director.update(world, 1.0f / 120.0f);

    // 自キャラから見て敵のいる側（+X）に、保つ距離だけ離れた所へ向かう。
    // (0, 0) を自キャラの位置と思っていると、原点から敵の方へ保つ距離だけの所（自キャラより手前）になる
    const std::optional<Vector2> target = world.findUnit(enemyId)->moveTarget;
    REQUIRE(target.has_value());
    CHECK(target->x > bush.center.x);
}
