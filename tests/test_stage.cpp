// ステージ（地形・経路探索・ブッシュ）のテスト

#include <doctest/doctest.h>

#include <memory>

#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Match.h"
#include "game/SkillCasting.h"
#include "game/Terrain.h"
#include "game/World.h"
#include "raymath.h"

using namespace moba;

namespace {

/// 長方形の形を作る（左上の x, y と幅・高さ）
Shape rect(float x, float y, float w, float h) {
    Shape s;
    s.kind = Shape::Kind::Rect;
    s.rect = Rectangle{x, y, w, h};
    return s;
}

/// 円の形を作る（中心と半径）
Shape circle(float x, float y, float r) {
    Shape s;
    s.kind = Shape::Kind::Circle;
    s.center = Vector2{x, y};
    s.radius = r;
    return s;
}

/// 2000 × 2000 のテスト用ステージを作る
/// @param walls 壁（通れない場所）
/// @param bushes ブッシュ
StageDef makeStage(std::vector<Shape> walls, std::vector<Shape> bushes = {}) {
    StageDef stage;
    stage.id = "test";
    stage.bounds = Rectangle{-1000.0f, -1000.0f, 2000.0f, 2000.0f};
    for (const Shape& wall : walls) stage.obstacles.push_back(Obstacle{ObstacleKind::Wall, wall});
    stage.bushes = std::move(bushes);
    return stage;
}

/// 地形のあるステージで、キャラを置いて動かす
class StageFixture {
public:
    /// data フォルダの設定を読み込み、stage の地形で World を作る
    explicit StageFixture(const StageDef& stage) : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, Terrain(stage));
    }

    /// 半径 50・移動速度 400 のキャラを置く
    /// @param characterId キャラの設定の ID（スキルを使わせる時だけ指定する）
    /// @return キャラの通し番号
    EntityId addUnit(Team team, Vector2 pos, std::string_view characterId = {}) {
        Unit unit;
        unit.team = team;
        unit.name = team == Team::Player ? "player" : "enemy";
        unit.characterId = std::string(characterId);
        unit.pos = pos;
        unit.radius = 50.0f;
        unit.baseMoveSpeed = 400.0f;
        return world_->addUnit(unit);
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 通し番号のキャラ（いることが分かっている時に使う）
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    /// 動かしながら、キャラが障害物にめり込まなかったかを確かめる
    bool runWithoutOverlap(float seconds) {
        bool ok = true;
        const int steps = static_cast<int>(seconds * 120.0f);
        for (int i = 0; i < steps; ++i) {
            world_->update(1.0f / 120.0f);
            for (const Unit& u : world_->units()) ok = ok && !world_->terrain().blocked(u.pos, u.radius);
        }
        return ok;
    }

private:
    GameData data_;
    std::unique_ptr<World> world_;
};

}  // namespace

TEST_CASE("ステージ: data/stages の 3 つが読み込め、(0, 0) から始められる") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const std::vector<const StageDef*> stages = data.stageList();
    REQUIRE(stages.size() >= 3);
    CHECK(stages[0]->id == "open");
    CHECK(stages[1]->id == "alley");
    CHECK(stages[2]->id == "forest");
    for (const StageDef* stage : stages) {
        INFO(stage->id);
        const Terrain terrain(*stage);
        CHECK_FALSE(terrain.blocked(Vector2{0.0f, 0.0f}, 65.0f));
    }
    CHECK(data.stage("alley")->obstacles.size() > 10);
    CHECK(data.stage("forest")->bushes.size() > 5);
}

TEST_CASE("経路探索: 壁の向こうへは、壁を回り込んで進む") {
    StageFixture f(makeStage({rect(-50.0f, -500.0f, 100.0f, 1000.0f)}));  // 縦長の壁（x -50〜50、y -500〜500）
    const EntityId id = f.addUnit(Team::Player, {-300.0f, 0.0f});
    casting::orderMove(f.world(), f.unit(id), {300.0f, 0.0f});
    CHECK(f.runWithoutOverlap(4.0f));
    CHECK(Vector2Distance(f.unit(id).pos, {300.0f, 0.0f}) < 1.0f);
    CHECK_FALSE(f.unit(id).moveTarget.has_value());
}

TEST_CASE("経路探索: 建物の中をクリックすると、建物の手前の立てる場所まで進む") {
    StageFixture f(makeStage({rect(200.0f, -200.0f, 400.0f, 400.0f)}));  // x 200〜600 の建物
    const EntityId id = f.addUnit(Team::Player, {-300.0f, 0.0f});
    casting::orderMove(f.world(), f.unit(id), {400.0f, 0.0f});  // 建物の中
    CHECK(f.runWithoutOverlap(3.0f));
    CHECK(f.unit(id).pos.x == doctest::Approx(150.0f).epsilon(0.01));  // 建物の左の壁（200）に、半径 50 で触れる位置
}

TEST_CASE("経路探索: 壁に囲まれた場所へは行けない（その場で止まる）") {
    // 中央を囲む 4 枚の壁
    StageFixture f(makeStage(
        {rect(-300, -300, 600, 40), rect(-300, 260, 600, 40), rect(-300, -300, 40, 600), rect(260, -300, 40, 600)}));
    const EntityId id = f.addUnit(Team::Player, {-600.0f, 0.0f});
    CHECK(f.world().terrain().findPath({-600.0f, 0.0f}, {0.0f, 0.0f}, 50.0f).empty());
    casting::orderMove(f.world(), f.unit(id), {0.0f, 0.0f});
    CHECK(f.runWithoutOverlap(1.0f));
    CHECK(f.unit(id).pos.x == doctest::Approx(-600.0f));
}

TEST_CASE("ブッシュ: 外からは見えず、同じブッシュの中からは見える。重なったブッシュは 1 つとして扱う") {
    // 重なり合う 2 つの円（1 つのブッシュ）と、離れた円（別のブッシュ）
    StageFixture f(makeStage({}, {circle(0, 0, 200), circle(300, 0, 200), circle(-700, 0, 150)}));
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f});
    const EntityId enemy = f.addUnit(Team::Enemy, {0.0f, 600.0f});

    CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));  // ブッシュの外の敵からは見えない
    CHECK(f.world().canSee(Team::Player, f.unit(enemy)));        // ブッシュの外の敵は見える

    f.unit(enemy).pos = {350.0f, 0.0f};  // 重なっている隣の円 = 同じブッシュ
    CHECK(f.world().canSee(Team::Enemy, f.unit(player)));

    f.unit(enemy).pos = {-700.0f, 0.0f};  // 別のブッシュ
    CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));
    CHECK_FALSE(f.world().canSee(Team::Player, f.unit(enemy)));

    // 見えない敵は、カーソルで対象に選べない
    CHECK(f.world().enemyAt({-700.0f, 0.0f}, Team::Player, 60.0f) == nullptr);
}

TEST_CASE("ブッシュ: 中で対象指定のスキルを使うと、bushRevealTime 秒だけ外から見える") {
    StageFixture f(makeStage({}, {circle(0, 0, 200)}));
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f}, "like_emma");
    f.addUnit(Team::Enemy, {400.0f, 0.0f});
    REQUIRE_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));

    // 方向指定のスキル（エマの Q）では見えない
    casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::Q, {400.0f, 0.0f}});
    f.runWithoutOverlap(0.5f);
    CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));

    // 対象指定のスキル（エマの E）で見える
    casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::E, {400.0f, 0.0f}});
    f.runWithoutOverlap(0.5f);
    CHECK(f.world().canSee(Team::Enemy, f.unit(player)));

    const float reveal = f.world().settings().bushRevealTime;
    f.runWithoutOverlap(reveal);
    CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));
}

TEST_CASE("路地裏・森でスキルドッジを遊んでも、キャラが障害物に入り込まない") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const char* stageId : {"alley", "forest"}) {
        INFO(stageId);
        MatchConfig config;
        config.mode = GameMode::SkillDodge;
        config.characterId = "like_ezreal";
        config.stageId = stageId;
        config.enemyCount = 5;
        config.aiLevel = 5;
        config.timeLimitSeconds = 30.0f;
        config.seed = 11;
        Match match(data, config);
        REQUIRE(match.valid());
        bool ok = true;
        const InputFrame noInput;
        while (!match.finished()) {
            match.update(1.0f / 120.0f, noInput);
            for (const Unit& u : match.world().units()) ok = ok && !match.world().terrain().blocked(u.pos, u.radius);
        }
        CHECK(ok);
        CHECK(match.result().dodge.total > 0);  // 敵がスキルを撃っている
        CHECK(match.result().stageName.empty() == false);
    }
}
