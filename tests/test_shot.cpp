// スキルショットのテスト: 的の置き方・反応の下限・動き方のパターン・的の回避・賢さとヒット率・ヒット率の集計

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>

#include "core/Paths.h"
#include "data/AiConfig.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/World.h"
#include "game/ai/SkillShotTarget.h"
#include "raymath.h"

using namespace moba;

namespace {

/// 的のテスト用の準備。障害物のない World の (0, 0) に自キャラ、(distance, 0) に的を置き、的の AI で動かす
class ShotFixture {
public:
    /// level の賢さの的（半径 65・移動速度 340）を置く
    ShotFixture(std::string_view playerCharacter, const SkillShotLevel& level, float distance)
        : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        const CharacterDef* def = data_.character(playerCharacter);
        REQUIRE(def != nullptr);
        playerId_ = world_->addUnit(makePlayerUnit(*def));

        SkillShotConfig config = data_.ai().skillShot;
        config.levels = {level};
        target_.setup(config, 1, 7);

        Unit target;
        target.team = Team::Enemy;
        target.name = "的";
        target.pos = {distance, 0.0f};
        target.radius = 65.0f;
        target.baseMoveSpeed = 340.0f;
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

/// 立ち止まり続ける（避ける時だけ動く）的の設定
SkillShotLevel standStill() {
    SkillShotLevel level;
    level.weightStrafe = 0.0f;
    level.weightStop = 1.0f;
    level.stopTimeMin = level.stopTimeMax = 100.0f;
    return level;
}

/// 反応が速く（人の反応の限界の 0.15 秒）、必ず避けようとする、立ち止まり続ける的の設定
SkillShotLevel alwaysDodge(bool readCast) {
    SkillShotLevel level = standStill();
    level.reactionTime = kMinTargetReactionTime;
    level.dodgeChance = 1.0f;
    level.readCast = readCast;
    return level;
}

/// 避けずに、往復だけをする的の設定（回数 swings・幅 width で、リズムを崩す確率 rhythmBreak）
SkillShotLevel strafeOnly(float swings, float width, float rhythmBreak) {
    SkillShotLevel level;
    level.weightStrafe = 1.0f;
    level.strafeSwingsMin = level.strafeSwingsMax = swings;
    level.strafeWidthMin = level.strafeWidthMax = width;
    level.rhythmBreak = rhythmBreak;
    return level;
}

/// 的が移動先に着くたびの位置を、seconds 秒の間記録する（往復の 1 回ごとの動きを調べる）
std::vector<Vector2> arrivals(ShotFixture& f, float seconds) {
    std::vector<Vector2> points{f.target().pos};
    bool wasMoving = false;
    const int steps = static_cast<int>(seconds * 120.0f);
    for (int i = 0; i < steps; ++i) {
        f.run(1.0f / 120.0f);
        const bool moving = f.target().moveTarget.has_value();
        if (wasMoving && !moving) points.push_back(f.target().pos);
        wasMoving = moving;
    }
    return points;
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

TEST_CASE("スキルショット: 的が 1 体、メニューで選んだ大きさ・移動速度で、自キャラから離れた位置に置かれる") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const SkillShotConfig& shot = data.ai().skillShot;
    REQUIRE(shot.sizes.size() >= 2);
    REQUIRE(shot.moveSpeeds.size() >= 2);
    MatchConfig config;
    config.mode = GameMode::SkillShot;
    config.characterId = "like_ezreal";
    config.stageId = "open";
    config.targetRadius = shot.sizes.front().radius;
    config.targetMoveSpeed = shot.moveSpeeds.back();
    config.seed = 5;
    Match match(data, config);
    REQUIRE(match.valid());

    int enemies = 0;
    for (const Unit& unit : match.world().units()) enemies += unit.team == Team::Enemy ? 1 : 0;
    CHECK(enemies == 1);
    const Unit* target = match.world().findUnit(match.target().unitId());
    REQUIRE(target != nullptr);
    CHECK(target->radius == doctest::Approx(shot.sizes.front().radius));
    CHECK(target->baseMoveSpeed == doctest::Approx(shot.moveSpeeds.back()));
    const float distance = Vector2Distance(target->pos, match.world().player()->pos);
    CHECK(distance >= shot.minDistance - 1.0f);
    CHECK(distance <= shot.maxDistance + 1.0f);
    CHECK(match.result().targetSizeName == shot.sizes.front().name);
}

TEST_CASE("的の反応の遅れは、ai.jsonc にそれより小さい値を書いても 0.15 秒より短くならない") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const SkillShotLevel& level : data.ai().skillShot.levels) CHECK(level.reactionTime >= kMinTargetReactionTime);

    // ai.jsonc の写しの、すべての reactionTime を 0.05 にして読み込む
    std::ifstream in(data.paths().aiFile);
    std::stringstream text;
    text << in.rdbuf();
    std::string json = text.str();
    for (std::size_t at = json.find("\"reactionTime\":"); at != std::string::npos;
         at = json.find("\"reactionTime\":", at + 1)) {
        const std::size_t value = at + std::string("\"reactionTime\":").size();
        const std::size_t end = json.find(',', value);
        json.replace(value, end - value, " 0.05");
    }
    const std::filesystem::path copy = std::filesystem::temp_directory_path() / "moba_trainer_test_ai.jsonc";
    std::ofstream(copy) << json;
    const AiConfigLoadResult loaded = loadAiConfig(copy);
    std::filesystem::remove(copy);
    REQUIRE(loaded.error.empty());
    for (const SkillShotLevel& level : loaded.config.skillShot.levels) {
        CHECK(level.reactionTime == doctest::Approx(kMinTargetReactionTime));
    }
    CHECK(loaded.config.level(1).reactionTime == doctest::Approx(0.05f));  // スキルドッジの敵の設定は別
}

TEST_CASE("的の動き（往復）: リズムを崩さない的は、自キャラから見て左右に、同じ幅で切り返す") {
    ShotFixture f("like_emma", strafeOnly(4, 200, 0.0f), 700.0f);
    const std::vector<Vector2> points = arrivals(f, 2.5f);
    REQUIRE(points.size() >= 4);
    for (std::size_t i = 1; i < 4; ++i) {
        // 自キャラ (0, 0) から見て横に 200 ずつ、向きを変えながら動く（動いた向きが、自キャラへの向きとほぼ直角）
        CHECK(Vector2Distance(points[i], points[i - 1]) == doctest::Approx(200.0f).epsilon(0.1));
        const Vector2 moved = Vector2Normalize(points[i] - points[i - 1]);
        const Vector2 toPlayer = Vector2Normalize(Vector2{} - points[i - 1]);
        CHECK(std::fabs(Vector2DotProduct(moved, toPlayer)) < 0.2f);
    }
    CHECK((points[1].y - points[0].y) * (points[2].y - points[1].y) < 0.0f);  // 切り返している
}

TEST_CASE("的の動き（往復）: リズムを崩す的は、2 回切り返した後、いつもと違う幅で動く") {
    ShotFixture f("like_emma", strafeOnly(6, 200, 1.0f), 700.0f);
    const std::vector<Vector2> points = arrivals(f, 3.0f);
    REQUIRE(points.size() >= 4);
    CHECK(Vector2Distance(points[1], points[0]) == doctest::Approx(200.0f).epsilon(0.1));
    CHECK(Vector2Distance(points[2], points[1]) == doctest::Approx(200.0f).epsilon(0.1));
    // 3 回目からは、大きく（2.5 倍）動くか、少し（4 分の 1）だけ動く
    CHECK(std::fabs(Vector2Distance(points[3], points[2]) - 200.0f) > 100.0f);
}

TEST_CASE("的の動き（接近）: 自キャラへ近づき、射程に入ると、反応の遅れの後に横へ切り返す") {
    SkillShotLevel level;
    level.weightStrafe = 0.0f;
    level.weightApproach = 1.0f;
    level.reactionTime = kMinTargetReactionTime;
    ShotFixture f("like_emma", level, 1080.0f);  // エマのヒット率に数えるスキルの最も長い射程は 950
    float closest = 1080.0f;
    float sideways = 0.0f;
    for (int i = 0; i < 3 * 120; ++i) {
        f.run(1.0f / 120.0f);
        closest = std::min(closest, Vector2Length(f.target().pos));
        sideways = std::max(sideways, std::fabs(f.target().pos.y));
    }
    CHECK(closest < 950.0f);   // 射程の中まで近づいた
    CHECK(sideways > 200.0f);  // 横へ切り返した
    CHECK(closest > 300.0f);   // 近づきすぎない
}

TEST_CASE("スキルショット: 何もしなくても的は動き回り（離れすぎず）、制限時間でヒット率 0 / 0 のまま終わる") {
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

    // 動き回っても、自キャラから maxDistance より離れない
    const float maxDistance = data.ai().skillShot.maxDistance;
    float farthest = 0.0f;
    const InputFrame noInput;
    for (int i = 0; i < 10 * 120 + 10 && !match.finished(); ++i) {
        match.update(1.0f / 120.0f, noInput);
        farthest = std::max(farthest, Vector2Distance(match.world().findUnit(match.target().unitId())->pos,
                                                      match.world().player()->pos));
    }
    CHECK(match.finished());
    CHECK(farthest <= maxDistance + 5.0f);
    CHECK(Vector2Distance(match.world().findUnit(match.target().unitId())->pos, start) > 100.0f);
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
