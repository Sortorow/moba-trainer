// スキルショットのテスト: 的の置き方・的の回避・賢さとヒット率・ヒット率の集計

#include <doctest/doctest.h>

#include <cmath>
#include <format>
#include <iterator>
#include <map>
#include <memory>

#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/World.h"
#include "game/ai/SkillShotTarget.h"
#include "raymath.h"

using namespace moba;

namespace {

/// 的の回避のテスト用の準備。障害物のない World の (0, 0) に自キャラ、(distance, 0) に的を置き、的の AI で動かす
class ShotFixture {
public:
    /// level の賢さの的を置く。的は動き回らず（その場で止まり）、弾・範囲を避ける時だけ動く
    ShotFixture(std::string_view playerCharacter, SkillShotLevel level, float distance)
        : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        const CharacterDef* def = data_.character(playerCharacter);
        REQUIRE(def != nullptr);
        playerId_ = world_->addUnit(makePlayerUnit(*def));

        SkillShotConfig config = data_.ai().skillShot;
        level.stopChance = 1.0f;  // 動き回らない（避けるかどうかだけを確かめる）
        level.moveIntervalMin = level.moveIntervalMax = 100.0f;
        config.levels = {level};
        target_.setup(config, 1, 7);

        Unit target;
        target.team = Team::Enemy;
        target.name = "的";
        target.pos = {distance, 0.0f};
        target.radius = config.targetRadius;
        target.baseMoveSpeed = config.targetMoveSpeed;
        targetId_ = world_->addUnit(target);
        target_.setTarget(targetId_);
    }

    /// 自キャラに、カーソルを cursor に置いてスキルのキーを押させる
    void cast(SkillSlot slot, Vector2 cursor) { casting::requestCast(*world_, player(), CastRequest{slot, cursor}); }
    /// 的の AI と World を seconds 秒進める（ゲームと同じ 1/120 秒刻み）
    void run(float seconds) {
        const int steps = static_cast<int>(seconds * 120.0f);
        for (int i = 0; i < steps; ++i) {
            target_.update(*world_, 1.0f / 120.0f);
            world_->update(1.0f / 120.0f);
        }
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 自キャラ
    Unit& player() { return *world_->findUnit(playerId_); }
    /// 的
    Unit& target() { return *world_->findUnit(targetId_); }
    /// 的の AI
    const ai::SkillShotTarget& ai() const { return target_; }

private:
    GameData data_;
    std::unique_ptr<World> world_;
    ai::SkillShotTarget target_;
    EntityId playerId_ = kInvalidId;
    EntityId targetId_ = kInvalidId;
};

/// 反応が速く、必ず避けようとする的の設定
SkillShotLevel alwaysDodge(bool readCast) {
    SkillShotLevel level;
    level.reactionTime = 0.1f;
    level.dodgeChance = 1.0f;
    level.readCast = readCast;
    return level;
}

/// スキルショットを seconds 秒遊んだ時のヒット率。自キャラ（エマ）は (0, 0) から動かず、
/// Q が使えるたびに、的の今の位置へ撃つ（クールダウン「なし」なので、詠唱が終わればすぐ次を撃つ）
HitCount playShot(const GameData& data, int targetLevel, std::uint32_t seed, float seconds) {
    MatchConfig config;
    config.mode = GameMode::SkillShot;
    config.characterId = "like_emma";
    config.stageId = "open";
    config.cooldownMode = CooldownMode::None;
    config.targetLevel = targetLevel;
    config.timeLimitSeconds = seconds;
    config.seed = seed;
    Match match(data, config);
    REQUIRE(match.valid());

    REQUIRE(match.world().player() != nullptr);
    REQUIRE(match.world().findUnit(match.target().unitId()) != nullptr);

    const InputFrame noInput;
    float nextCast = 0.5f;
    while (!match.finished()) {
        Unit* player = match.world().player();
        const Unit* target = match.world().findUnit(match.target().unitId());
        if (match.elapsed() >= nextCast && !player->cast.active) {
            casting::requestCast(match.world(), *player, CastRequest{SkillSlot::Q, target->pos});
            nextCast = match.elapsed() + 1.2f;
        }
        match.update(1.0f / 120.0f, noInput);
    }
    return match.result().playerHits;
}

}  // namespace

TEST_CASE("スキルショット: 的が 1 体、自キャラから保つ距離の位置に、ai.jsonc の大きさ・移動速度で置かれる") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    MatchConfig config;
    config.mode = GameMode::SkillShot;
    config.characterId = "like_ezreal";
    config.stageId = "open";
    config.seed = 5;
    Match match(data, config);
    REQUIRE(match.valid());

    int enemies = 0;
    for (const Unit& unit : match.world().units()) enemies += unit.team == Team::Enemy ? 1 : 0;
    CHECK(enemies == 1);
    const Unit* target = match.world().findUnit(match.target().unitId());
    REQUIRE(target != nullptr);
    const SkillShotConfig& shot = data.ai().skillShot;
    CHECK(target->radius == doctest::Approx(shot.targetRadius));
    CHECK(target->baseMoveSpeed == doctest::Approx(shot.targetMoveSpeed));
    const float distance = Vector2Distance(target->pos, match.world().player()->pos);
    CHECK(distance >= shot.distanceMin - 1.0f);
    CHECK(distance <= shot.distanceMax + 1.0f);
    CHECK(std::fabs(target->pos.y - match.world().player()->pos.y) <= shot.maxVerticalOffset + 1.0f);
}

TEST_CASE("スキルショット: 何もしなくても的は動き回り、制限時間でヒット率 0 / 0 のまま終わる") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    MatchConfig config;
    config.mode = GameMode::SkillShot;
    config.characterId = "like_ezreal";
    config.targetLevel = 1;
    config.timeLimitSeconds = 10.0f;
    config.seed = 11;
    Match match(data, config);
    REQUIRE(match.valid());
    const Vector2 start = match.world().findUnit(match.target().unitId())->pos;

    const InputFrame noInput;
    for (int i = 0; i < 10 * 120 + 10 && !match.finished(); ++i) match.update(1.0f / 120.0f, noInput);
    CHECK(match.finished());
    CHECK(Vector2Distance(match.world().findUnit(match.target().unitId())->pos, start) > 100.0f);
    // 動き回っても、上下には離れすぎない（避ける時の移動の分だけ、はみ出すことはある）
    const SkillShotConfig& shot = data.ai().skillShot;
    CHECK(std::fabs(match.world().findUnit(match.target().unitId())->pos.y - match.world().player()->pos.y) <=
          shot.maxVerticalOffset + 1.0f);
    const MatchResult result = match.result();
    CHECK(result.playerHits.casts == 0);
    CHECK(result.aiLevelName == data.ai().skillShot.level(1).name);
}

TEST_CASE("的の回避: 避ける的は、遅い弾（エマ Q）を、弾が出てから気づいて避ける") {
    ShotFixture f("like_emma", alwaysDodge(false), 800.0f);
    f.cast(SkillSlot::Q, {800, 0});
    f.run(2.0f);
    CHECK(f.world().stats().total().casts == 1);
    CHECK(f.world().stats().total().hits == 0);
    CHECK(f.ai().dodgeCount() >= 1);
}

TEST_CASE("的の回避: 避けない的（dodgeChance 0）には当たる") {
    SkillShotLevel level = alwaysDodge(true);
    level.dodgeChance = 0.0f;
    ShotFixture f("like_emma", level, 800.0f);
    f.cast(SkillSlot::Q, {800, 0});
    f.run(2.0f);
    CHECK(f.world().stats().total().hits == 1);
    CHECK(f.ai().dodgeCount() == 0);
}

TEST_CASE("的の回避: 速い弾（エズリアル Q）は、弾を見てからでは間に合わず、詠唱を見て避ける的なら避けられる") {
    {
        ShotFixture f("like_ezreal", alwaysDodge(false), 800.0f);
        f.cast(SkillSlot::Q, {800, 0});
        f.run(1.5f);
        CHECK(f.world().stats().total().hits == 1);
    }
    {
        ShotFixture f("like_ezreal", alwaysDodge(true), 800.0f);
        f.cast(SkillSlot::Q, {800, 0});
        f.run(1.5f);
        CHECK(f.world().stats().total().hits == 0);
    }
}

TEST_CASE("的の回避: 地点に遅れて発動する範囲（エマ W）の端寄りにいれば、範囲の外へ出る") {
    // 範囲の真ん中にいると、発動までに外へ出られない（半径 + 的の大きさ分を、反応の遅れの後の時間で走れない）
    ShotFixture f("like_emma", alwaysDodge(false), 600.0f);
    const Vector2 center{600, 120};
    f.cast(SkillSlot::W, center);
    f.run(1.5f);
    CHECK(f.world().stats().total().hits == 0);
    const float radius = f.world().data().character("like_emma")->skill(SkillSlot::W)->num("radius", 0.0f);
    CHECK(Vector2Distance(f.target().pos, center) > radius + f.target().radius);
}

TEST_CASE("ヒット率: 制限時間の終了時に飛んでいる途中だった発動は、リザルトの集計に含めない") {
    ShotFixture f("like_ezreal", alwaysDodge(false), 800.0f);
    f.cast(SkillSlot::Q, {0, 800});  // 的のいない方向へ撃つ（外れ）
    f.run(1.5f);
    CHECK(f.world().stats().settledTotal().casts == 1);

    f.player().cooldowns = {};
    f.cast(SkillSlot::Q, {0, -800});
    f.run(0.4f);                                         // まだ飛んでいる途中
    CHECK(f.world().stats().total().casts == 2);         // 画面の表示は撃った時点で数える
    CHECK(f.world().stats().settledTotal().casts == 1);  // リザルトには、結果が出た発動だけ
    CHECK(f.world().stats().settledSlot(SkillSlot::Q).casts == 1);
    f.run(1.0f);
    CHECK(f.world().stats().settledTotal().casts == 2);
    CHECK(f.world().stats().settledTotal().hits == 0);
}

TEST_CASE("賢い的ほど、スキルが当たりにくい") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    constexpr std::uint32_t kSeeds[] = {3, 17, 29, 101};
    std::map<int, float> rates;
    for (const int level : {1, 3, 5}) {
        HitCount sum;
        for (const std::uint32_t seed : kSeeds) {
            const HitCount hits = playShot(data, level, seed, 30.0f);
            sum.casts += hits.casts;
            sum.hits += hits.hits;
        }
        REQUIRE(sum.casts > 0);
        rates[level] = sum.rate();
    }
    MESSAGE(std::format("エマ Q を的の位置へ撃った時のヒット率: 賢さ1 {:.0f}% / 3 {:.0f}% / 5 {:.0f}%", rates[1] * 100,
                        rates[3] * 100, rates[5] * 100));
    CHECK(rates[1] > rates[3]);
    CHECK(rates[3] > rates[5]);
}
