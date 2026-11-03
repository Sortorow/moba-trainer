// 視界のテスト: 視線を遮る物・視界の半径・ブッシュの奥・撃った時に見える時間・敵 AI・視界の形

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <random>

#include "core/Geometry.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "data/StageDef.h"
#include "game/Match.h"
#include "game/MatchConfig.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/Terrain.h"
#include "game/Vision.h"
#include "game/World.h"
#include "game/ai/EnemyAI.h"
#include "raymath.h"

using namespace moba;

namespace {

/// 長方形の障害物を作る（左上の x, y と幅・高さ）
Obstacle rectObstacle(ObstacleKind kind, float x, float y, float w, float h) {
    Obstacle o;
    o.kind = kind;
    o.shape.kind = Shape::Kind::Rect;
    o.shape.rect = Rectangle{x, y, w, h};
    return o;
}

/// 円の形を作る（中心と半径）
Shape circle(float x, float y, float r) {
    Shape s;
    s.kind = Shape::Kind::Circle;
    s.center = Vector2{x, y};
    s.radius = r;
    return s;
}

/// 4000 × 4000 のテスト用ステージを作る
StageDef makeStage(std::vector<Obstacle> obstacles, std::vector<Shape> bushes = {}) {
    StageDef stage;
    stage.id = "vision_test";
    stage.bounds = Rectangle{-2000.0f, -2000.0f, 4000.0f, 4000.0f};
    stage.obstacles = std::move(obstacles);
    stage.bushes = std::move(bushes);
    return stage;
}

/// 視界のテスト用の準備。data フォルダの設定を読み込み、stage の地形で World を作る
class VisionFixture {
public:
    explicit VisionFixture(const StageDef& stage) : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, Terrain(stage));
    }

    /// キャラを置く。characterId を指定すると、そのキャラの設定（スキル・大きさ・移動速度）を使う
    /// @return キャラの通し番号
    EntityId addUnit(Team team, Vector2 pos, std::string_view characterId = {}) {
        Unit unit;
        unit.team = team;
        unit.name = team == Team::Player ? "player" : "enemy";
        unit.pos = pos;
        unit.radius = 50.0f;
        unit.baseMoveSpeed = 400.0f;
        if (const CharacterDef* def = characterId.empty() ? nullptr : data_.character(characterId)) {
            unit.characterId = def->id;
            unit.radius = def->hitboxRadius;
            unit.baseMoveSpeed = def->moveSpeed;
        }
        return world_->addUnit(unit);
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 読み込んだ設定ファイル
    const GameData& data() const { return data_; }
    /// 通し番号のキャラ（いることが分かっている時に使う）
    Unit& unit(EntityId id) { return *world_->findUnit(id); }
    /// seconds 秒だけ進める
    void run(float seconds) {
        const int steps = static_cast<int>(seconds * 120.0f);
        for (int i = 0; i < steps; ++i) world_->update(1.0f / 120.0f);
    }

private:
    GameData data_;
    std::unique_ptr<World> world_;
};

/// 視界の多角形の、point の向きでの境目までの距離（両隣の光線の先を結んだ線分との交点）
float boundaryDistance(Vector2 origin, const std::vector<vision::VisionRay>& rays, Vector2 point) {
    const Vector2 d = point - origin;
    const float angle = std::atan2(d.y, d.x);
    for (std::size_t i = 0; i < rays.size(); ++i) {
        const vision::VisionRay& a = rays[i];
        const vision::VisionRay& b = rays[(i + 1) % rays.size()];
        const float angleA = std::atan2(a.dir.y, a.dir.x);
        float angleB = std::atan2(b.dir.y, b.dir.x);
        float target = angle;
        if (angleB < angleA) angleB += 2.0f * PI;  // 最後の光線と最初の光線の間（-π をまたぐ）
        if (target < angleA) target += 2.0f * PI;
        if (target < angleA || target > angleB) continue;
        // 線分 pa-pb と、origin から point の向きの直線の交点までの距離
        const Vector2 pa = a.dir * a.distance;
        const Vector2 pb = b.dir * b.distance;
        const Vector2 dir = Vector2Normalize(d);
        const Vector2 edge = pb - pa;
        const float denom = dir.x * edge.y - dir.y * edge.x;
        if (std::fabs(denom) < 1e-6f) return std::min(a.distance, b.distance);
        return (pa.x * edge.y - pa.y * edge.x) / denom;
    }
    return 0.0f;
}

}  // namespace

TEST_CASE("視界: 建物・壁・木は視線を遮り、箱は遮らない") {
    VisionFixture f(makeStage({
        rectObstacle(ObstacleKind::Wall, 200.0f, -100.0f, 50.0f, 200.0f),       // x 200〜250 の壁
        rectObstacle(ObstacleKind::Crate, -250.0f, -100.0f, 50.0f, 200.0f),     // x -250〜-200 の箱
        rectObstacle(ObstacleKind::Building, -100.0f, 200.0f, 200.0f, 100.0f),  // y 200〜300 の建物
    }));
    Obstacle tree;
    tree.kind = ObstacleKind::Tree;
    tree.shape = circle(0.0f, -300.0f, 60.0f);
    const Terrain terrain(makeStage({tree}));

    const Terrain& t = f.world().terrain();
    CHECK_FALSE(vision::lineOfSight(t, {0.0f, 0.0f}, {500.0f, 0.0f}, -1));         // 壁の向こう
    CHECK(vision::lineOfSight(t, {0.0f, 0.0f}, {-500.0f, 0.0f}, -1));              // 箱の向こう
    CHECK_FALSE(vision::lineOfSight(t, {0.0f, 0.0f}, {0.0f, 500.0f}, -1));         // 建物の向こう
    CHECK(vision::lineOfSight(t, {0.0f, 0.0f}, {150.0f, 0.0f}, -1));               // 壁の手前
    CHECK_FALSE(vision::lineOfSight(terrain, {0.0f, 0.0f}, {0.0f, -500.0f}, -1));  // 木の向こう
    CHECK(vision::lineOfSight(terrain, {0.0f, 0.0f}, {200.0f, -500.0f}, -1));      // 木の横

    // キャラから見ても同じ
    f.addUnit(Team::Player, {0.0f, 0.0f});
    const EntityId behindWall = f.addUnit(Team::Enemy, {500.0f, 0.0f});
    const EntityId behindCrate = f.addUnit(Team::Enemy, {-500.0f, 0.0f});
    CHECK_FALSE(f.world().canSee(Team::Player, f.unit(behindWall)));
    CHECK(f.world().canSee(Team::Player, f.unit(behindCrate)));
    // 壁の向こうの敵からは見えないが、箱の向こうの敵からは見える。陣営の誰か 1 人から見えれば見える
    CHECK(f.world().canSee(Team::Enemy, *f.world().player()));
    f.unit(behindCrate).alive = false;
    CHECK_FALSE(f.world().canSee(Team::Enemy, *f.world().player()));
}

TEST_CASE("視界: 視界の半径（visionRadius）より遠いキャラは見えない。同じ陣営は遠くても見える") {
    VisionFixture f(makeStage({}));
    const float radius = f.world().settings().visionRadius;
    REQUIRE(radius == doctest::Approx(1000.0f));
    f.addUnit(Team::Player, {0.0f, 0.0f});
    const EntityId near = f.addUnit(Team::Enemy, {radius - 1.0f, 0.0f});
    const EntityId far = f.addUnit(Team::Enemy, {0.0f, radius + 1.0f});
    CHECK(f.world().canSee(Team::Player, f.unit(near)));
    CHECK_FALSE(f.world().canSee(Team::Player, f.unit(far)));
    CHECK(f.world().canSee(Team::Enemy, f.unit(far)));  // 同じ陣営
    CHECK_FALSE(f.world().canSeePoint(Team::Player, {radius + 1.0f, 0.0f}));
    CHECK(f.world().canSeePoint(Team::Enemy, {radius + 1.0f, 0.0f}));  // 近い敵の視界に入っている

    // 見えない敵は、カーソルで対象に選べない
    CHECK(f.world().enemyAt({0.0f, radius + 1.0f}, Team::Player, 60.0f) == nullptr);
}

TEST_CASE("視界: ブッシュの奥は見えない。自分が入っているブッシュは視線を遮らない") {
    // (300, 0) の小さなブッシュと、それに重なる (450, 0) のブッシュ（2 つで 1 つのブッシュ）と、離れた (0, 500) のブッシュ
    VisionFixture f(
        makeStage({}, {circle(300.0f, 0.0f, 100.0f), circle(450.0f, 0.0f, 100.0f), circle(0.0f, 500.0f, 80.0f)}));
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f});
    const EntityId behind = f.addUnit(Team::Enemy, {700.0f, 0.0f});  // ブッシュの奥
    const EntityId beside = f.addUnit(Team::Enemy, {700.0f, 300.0f});

    CHECK_FALSE(f.world().canSee(Team::Player, f.unit(behind)));
    CHECK(f.world().canSee(Team::Player, f.unit(beside)));

    // ブッシュに入ると、重なっているブッシュも含めて遮られず、奥まで見える
    f.unit(player).pos = {300.0f, 0.0f};
    CHECK(f.world().canSee(Team::Player, f.unit(behind)));
    // 自分のいる場所から見て、別のブッシュの奥は見えない
    f.unit(behind).pos = {0.0f, 700.0f};
    f.unit(player).pos = {0.0f, 0.0f};
    CHECK_FALSE(f.world().canSee(Team::Player, f.unit(behind)));
}

TEST_CASE("視界: 見えていない所から対象指定のスキルで撃つと、attackRevealTime 秒だけ見える。見えている時は変わらない") {
    // 自キャラはブッシュ (0, 0) の中から、ブッシュの外の敵を撃つ
    VisionFixture f(makeStage({}, {circle(0.0f, 0.0f, 200.0f)}));
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f}, "like_emma");
    const EntityId enemy = f.addUnit(Team::Enemy, {400.0f, 0.0f});
    REQUIRE_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));
    REQUIRE(f.world().canSee(Team::Player, f.unit(enemy)));

    // 対象指定のスキル（エマの E）で撃つと、相手の陣営から見えるようになる
    casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::E, {400.0f, 0.0f}});
    f.run(0.3f);
    CHECK(f.unit(player).revealTime > 0.0f);
    CHECK(f.world().canSee(Team::Enemy, f.unit(player)));
    f.run(f.world().settings().attackRevealTime);
    CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(player)));

    // ブッシュの外（相手から見えている所。エマの E の射程 475 の内側）から撃っても、見える時間は付かない
    f.unit(player).pos = {100.0f, 230.0f};
    f.unit(player).cooldowns = {};
    REQUIRE(f.world().canSee(Team::Enemy, f.unit(player)));
    REQUIRE(casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::E, {400.0f, 0.0f}}) !=
            casting::CastOutcome::Rejected);
    f.run(0.3f);
    CHECK(f.unit(player).revealTime == 0.0f);
}

TEST_CASE("視界: 射程外の対象へ近づいている途中で対象が見えなくなると、発動をやめる") {
    VisionFixture f(makeStage({}, {circle(800.0f, 400.0f, 150.0f)}));
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f}, "like_brand");
    const EntityId enemy = f.addUnit(Team::Enemy, {800.0f, 0.0f});
    // ブランドの E（射程 675）は届かないので、近づき始める
    REQUIRE(casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::E, {800.0f, 0.0f}}) ==
            casting::CastOutcome::Approaching);
    f.run(0.05f);
    REQUIRE(f.unit(player).pendingCast.has_value());

    f.unit(enemy).pos = {800.0f, 400.0f};  // ブッシュに隠れる
    f.run(0.05f);
    CHECK_FALSE(f.unit(player).pendingCast.has_value());
    CHECK_FALSE(f.unit(player).moveTarget.has_value());
}

TEST_CASE("敵 AI: 自キャラが見えない間は撃たない") {
    // 自キャラはブッシュの中。ブッシュの外の敵からは見えない
    const auto enemyCasts = [](bool withBush, float seconds) {
        VisionFixture f(
            makeStage({}, withBush ? std::vector<Shape>{circle(0.0f, 0.0f, 250.0f)} : std::vector<Shape>{}));
        const CharacterDef* def = f.data().character("like_ezreal");
        REQUIRE(def != nullptr);
        Unit player = makePlayerUnit(*def);
        player.pos = {0.0f, 0.0f};
        f.world().addUnit(player);
        const EntityId enemy = f.addUnit(Team::Enemy, {800.0f, 0.0f}, "like_ezreal");

        ai::EnemyDirector director;
        director.setup(f.data().ai(), 5, 7);
        director.addEnemy(enemy);
        bool cast = false;
        const int steps = static_cast<int>(seconds * 120.0f);
        for (int i = 0; i < steps; ++i) {
            director.update(f.world(), 1.0f / 120.0f);
            f.world().update(1.0f / 120.0f);
            // 詠唱を始めたか、クールダウンが入っていれば撃った
            const Unit& u = f.unit(enemy);
            cast = cast || u.cast.active ||
                   std::any_of(u.cooldowns.begin(), u.cooldowns.end(), [](float c) { return c > 0.0f; });
        }
        return cast;
    };
    CHECK(enemyCasts(false, 6.0f));  // 見えていれば撃つ
    CHECK_FALSE(enemyCasts(true, 6.0f));
}

TEST_CASE("敵 AI: 自キャラと保つ距離は、視界の半径より内側") {
    VisionFixture f(makeStage({}));
    const CharacterDef* def = f.data().character("like_ezreal");
    REQUIRE(def != nullptr);
    Unit player = makePlayerUnit(*def);
    player.pos = {0.0f, 0.0f};
    f.world().addUnit(player);
    // エズリアルの Q・W（射程 1200）だけを使う敵は、そのままだと視界の外（1000）から撃とうとする
    const EntityId enemy = f.addUnit(Team::Enemy, {1500.0f, 0.0f}, "like_ezreal");
    ai::EnemyDirector director;
    director.setup(f.data().ai(), 1, 7);  // 賢さ 1 は横移動しない
    director.addEnemy(enemy);
    director.update(f.world(), 1.0f / 120.0f);
    const std::optional<Vector2> target = f.unit(enemy).moveTarget;
    REQUIRE(target.has_value());
    CHECK(Vector2Length(*target) <= f.world().settings().visionRadius - f.data().ai().visionMargin + 1.0f);
}

TEST_CASE("スキルショット: 的と自キャラの距離の上限は、視界の内側（visionRadius − visionMargin）") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    const SkillShotConfig& shot = data.ai().skillShot;
    const float radius = data.settings().visionRadius;
    CHECK(shot.maxDistanceWithin(radius) == doctest::Approx(radius - shot.visionMargin));
    CHECK(shot.maxDistanceWithin(5000.0f) == doctest::Approx(shot.maxDistance));          // 視界が広ければ maxDistance
    CHECK(shot.maxDistanceWithin(100.0f) == doctest::Approx(shot.minDistance + 100.0f));  // 狭すぎても下限は保つ
}

TEST_CASE("スキルショット: 自キャラが離れても、的は近づき直して視界の外に出たままにならない") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    MatchConfig config;
    config.mode = GameMode::SkillShot;
    config.characterId = "like_ezreal";
    config.stageId = "open";
    config.targetLevel = 3;
    config.timeLimitSeconds = 30.0f;
    config.seed = 5;
    Match match(data, config);
    REQUIRE(match.valid());
    const float maxDistance = data.ai().skillShot.maxDistanceWithin(data.settings().visionRadius);
    const auto target = [&]() -> const Unit& { return *match.world().findUnit(match.target().unitId()); };

    // 自キャラが的から離れる向きに 4 秒歩き続ける（ステージの端に着いたら、その場で止まる）
    const InputFrame noInput;
    int visible = 0;
    int steps = 0;
    for (int i = 0; i < 4 * 120; ++i) {
        Unit& player = *match.world().player();
        if (i % 36 == 0) {
            const Vector2 away = geo::normalizedOr(player.pos - target().pos, Vector2{1.0f, 0.0f});
            casting::orderMove(match.world(), player, player.pos + away * 400.0f);
        }
        match.update(1.0f / 120.0f, noInput);
        ++steps;
        if (match.world().canSee(Team::Player, target())) ++visible;
    }
    CHECK(static_cast<float>(visible) / static_cast<float>(steps) > 0.9f);

    // 止まると、的は上限の内側に戻ってくる
    match.world().player()->moveTarget.reset();
    for (int i = 0; i < 3 * 120; ++i) match.update(1.0f / 120.0f, noInput);
    CHECK(Vector2Distance(target().pos, match.world().player()->pos) <= maxDistance + 5.0f);
    CHECK(match.world().canSee(Team::Player, target()));
}

TEST_CASE("全視界: 壁の向こう・ブッシュの中・視界の半径より遠い所も、敵味方とも見える") {
    VisionFixture f(
        makeStage({rectObstacle(ObstacleKind::Wall, 200.0f, -100.0f, 50.0f, 200.0f)}, {circle(0.0f, 600.0f, 150.0f)}));
    WorldRules rules;
    rules.fullVision = true;
    f.world().setRules(rules);
    const EntityId player = f.addUnit(Team::Player, {0.0f, 0.0f}, "like_emma");
    const EntityId behindWall = f.addUnit(Team::Enemy, {500.0f, 0.0f});
    const EntityId inBush = f.addUnit(Team::Enemy, {0.0f, 600.0f});
    const EntityId far = f.addUnit(Team::Enemy, {-1900.0f, 0.0f});
    CHECK(f.world().canSee(Team::Player, f.unit(behindWall)));
    CHECK(f.world().canSee(Team::Player, f.unit(inBush)));
    CHECK(f.world().canSee(Team::Player, f.unit(far)));
    CHECK(f.world().canSee(Team::Enemy, f.unit(player)));
    CHECK(f.world().canSeePoint(Team::Player, {1900.0f, 1900.0f}));

    // 見えているので、ブッシュの中から撃っても「見える時間」は付かない
    f.unit(player).pos = {0.0f, 600.0f};
    casting::requestCast(f.world(), f.unit(player), CastRequest{SkillSlot::E, f.unit(inBush).pos});
    f.run(0.3f);
    CHECK(f.unit(player).revealTime == 0.0f);
}

TEST_CASE("全視界: 敵 AI は、ブッシュに隠れた自キャラにも撃つ") {
    VisionFixture f(makeStage({}, {circle(0.0f, 0.0f, 250.0f)}));
    WorldRules rules;
    rules.fullVision = true;
    f.world().setRules(rules);
    const CharacterDef* def = f.data().character("like_ezreal");
    REQUIRE(def != nullptr);
    Unit player = makePlayerUnit(*def);
    f.world().addUnit(player);
    const EntityId enemy = f.addUnit(Team::Enemy, {800.0f, 0.0f}, "like_ezreal");
    ai::EnemyDirector director;
    director.setup(f.data().ai(), 5, 7);
    director.addEnemy(enemy);
    bool cast = false;
    for (int i = 0; i < 6 * 120; ++i) {
        director.update(f.world(), 1.0f / 120.0f);
        f.world().update(1.0f / 120.0f);
        const Unit& u = f.unit(enemy);
        cast = cast || u.cast.active ||
               std::any_of(u.cooldowns.begin(), u.cooldowns.end(), [](float c) { return c > 0.0f; });
    }
    CHECK(cast);
}

TEST_CASE("全視界: 視界の初期値はモードごと（settings.jsonc の fullVision。練習場だけ全視界）") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    CHECK(defaultFullVision(data.settings(), GameMode::Sandbox));
    CHECK_FALSE(defaultFullVision(data.settings(), GameMode::SkillDodge));
    CHECK_FALSE(defaultFullVision(data.settings(), GameMode::SkillShot));

    // Match は、設定の視界を World のルールにする
    MatchConfig config;
    config.mode = GameMode::Sandbox;
    config.characterId = "like_emma";
    config.fullVision = true;
    const Match sandbox(data, config);
    REQUIRE(sandbox.valid());
    CHECK(sandbox.world().rules().fullVision);
    config.fullVision = false;
    const Match normal(data, config);
    CHECK_FALSE(normal.world().rules().fullVision);
}

TEST_CASE("敵 AI: 見失ったら、消えた方向の先へ追いかける。ブッシュに入った自キャラは、ブッシュに入って見つけ直す") {
    // (900, 0) を中心に半径 250 のブッシュ。自キャラはブッシュの中心まで歩いて止まる
    VisionFixture f(makeStage({}, {circle(900.0f, 0.0f, 250.0f)}));
    const CharacterDef* def = f.data().character("like_ezreal");
    REQUIRE(def != nullptr);
    Unit player = makePlayerUnit(*def);
    const EntityId playerId = f.world().addUnit(player);
    const EntityId enemy = f.addUnit(Team::Enemy, {-700.0f, 0.0f}, "like_ezreal");
    ai::EnemyDirector director;
    director.setup(f.data().ai(), 3, 7);
    director.addEnemy(enemy);
    casting::orderMove(f.world(), f.unit(playerId), {900.0f, 0.0f});

    bool lost = false;
    bool foundAgain = false;
    for (int i = 0; i < 8 * 120 && !foundAgain; ++i) {
        director.update(f.world(), 1.0f / 120.0f);
        f.world().update(1.0f / 120.0f);
        const bool visible = f.world().canSee(Team::Enemy, f.unit(playerId));
        if (!visible) lost = true;
        if (lost && visible) foundAgain = true;
    }
    CHECK(lost);
    CHECK(foundAgain);
    CHECK(f.world().terrain().bushAt(f.unit(enemy).pos) >= 0);  // ブッシュに入って見つけた
}

TEST_CASE(
    "敵 AI: 賢さ 1 は最後に見えた位置へ、賢さ 5 は消えた方向の先（chasePredict）まで追いかけ、着いても見つからなければ"
    "探索に移る") {
    // 自キャラが +X へ歩いている途中で、敵から見えない遠くへ消えた時の、敵の行き先を比べる
    const auto chaseFrom = [](int level) {
        VisionFixture f(makeStage({}));
        const CharacterDef* def = f.data().character("like_ezreal");
        REQUIRE(def != nullptr);
        Unit player = makePlayerUnit(*def);
        const EntityId playerId = f.world().addUnit(player);
        const EntityId enemy = f.addUnit(Team::Enemy, {-700.0f, 0.0f}, "like_ezreal");
        ai::EnemyDirector director;
        director.setup(f.data().ai(), level, 7);
        director.addEnemy(enemy);
        casting::orderMove(f.world(), f.unit(playerId), {1500.0f, 0.0f});
        for (int i = 0; i < 120; ++i) {
            director.update(f.world(), 1.0f / 120.0f);
            f.world().update(1.0f / 120.0f);
        }
        REQUIRE(f.world().canSee(Team::Enemy, f.unit(playerId)));
        const Vector2 lastSeen = f.unit(playerId).pos;
        const float speed = f.unit(playerId).moveSpeed();
        f.unit(playerId).pos = {0.0f, 1900.0f};  // 遠くへ消える（敵の視界の外）
        f.unit(playerId).moveTarget.reset();
        // 追跡から探索に移った時の、敵の位置（= 追いかけた先）
        std::optional<Vector2> chaseEnd;
        for (int i = 0; i < 6 * 120 && !chaseEnd; ++i) {
            const ai::EnemyDirector::Mode before = director.mode();
            director.update(f.world(), 1.0f / 120.0f);
            f.world().update(1.0f / 120.0f);
            if (before == ai::EnemyDirector::Mode::Chase && director.mode() == ai::EnemyDirector::Mode::Search)
                chaseEnd = f.unit(enemy).pos;
        }
        CHECK_FALSE(f.world().canSee(Team::Enemy, f.unit(playerId)));
        REQUIRE(chaseEnd.has_value());  // 着いても見つからなければ、探索に移る
        const float predict = f.data().ai().level(level).chasePredict;
        const Vector2 expected = lastSeen + Vector2{speed * predict, 0.0f};
        return Vector2Distance(*chaseEnd, expected);
    };
    CHECK(chaseFrom(1) < 60.0f);  // 最後に見えた位置
    CHECK(chaseFrom(5) < 60.0f);  // 最後に見えた位置 + 速度 × chasePredict（2 秒 = 約 650 先）
}

TEST_CASE("視界の形: 壁に向けた光線は壁の手前で止まり、遮る物の無い向きは視界の半径まで届く") {
    const Terrain terrain(makeStage({rectObstacle(ObstacleKind::Wall, 300.0f, -50.0f, 100.0f, 100.0f)}));
    const std::vector<vision::VisionRay> rays = vision::visionPolygon(terrain, {0.0f, 0.0f}, 900.0f, -1);
    REQUIRE(rays.size() >= 360);
    bool foundWall = false;
    for (const vision::VisionRay& ray : rays) {
        CHECK(ray.distance <= 900.0f);
        if (std::fabs(ray.dir.y) < 1e-6f && ray.dir.x > 0.0f) {
            CHECK(ray.distance == doctest::Approx(300.0f));
            foundWall = true;
        }
        if (ray.dir.x < 0.0f) CHECK(ray.distance == doctest::Approx(900.0f));
    }
    CHECK(foundWall);
    // 壁の角（300, -50）の向きにも光線がある（多角形の頂点が角に合う）
    bool foundCorner = false;
    for (const vision::VisionRay& ray : rays) {
        const Vector2 p = ray.dir * ray.distance;
        if (Vector2Distance(p, {300.0f, -50.0f}) < 0.5f) foundCorner = true;
    }
    CHECK(foundCorner);
}

TEST_CASE("視界の形: 路地裏・森で、多角形の内側かどうかと、視線の判定が一致する") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    for (const char* stageId : {"alley", "forest"}) {
        INFO(stageId);
        const StageDef* stage = data.stage(stageId);
        REQUIRE(stage != nullptr);
        const Terrain terrain(*stage);
        for (const Vector2 origin : {Vector2{0.0f, 0.0f}, Vector2{-800.0f, 300.0f}, Vector2{900.0f, -400.0f}}) {
            if (terrain.blocked(origin, 1.0f)) continue;
            const int ignoreBush = terrain.bushAt(origin);
            const std::vector<vision::VisionRay> rays = vision::visionPolygon(terrain, origin, 900.0f, ignoreBush);
            int checked = 0;
            for (int i = 0; i < 2000; ++i) {
                const Vector2 point = origin + Vector2{unit(rng), unit(rng)} * 880.0f;
                const float distance = Vector2Distance(origin, point);
                if (distance > 880.0f || distance < 1.0f) continue;
                const float boundary = boundaryDistance(origin, rays, point);
                if (std::fabs(boundary - distance) < 3.0f) continue;  // 境目のすぐ近くは、光線の間の近似で揺れる
                INFO("origin " << origin.x << "," << origin.y << " point " << point.x << "," << point.y);
                CHECK((distance < boundary) == vision::lineOfSight(terrain, origin, point, ignoreBush));
                ++checked;
            }
            CHECK(checked > 1000);
        }
    }
}
