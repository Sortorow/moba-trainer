// 探索のテスト: エリア分け・探索済みの記録・確かめ撃ちの先・見失った自キャラを探す敵 AI

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

#include "core/Paths.h"
#include "data/AiConfig.h"
#include "data/GameData.h"
#include "data/Settings.h"
#include "data/StageDef.h"
#include "game/Sandbox.h"
#include "game/Terrain.h"
#include "game/World.h"
#include "game/ai/EnemyAI.h"
#include "game/ai/SearchMap.h"
#include "raymath.h"

using namespace moba;

namespace {

constexpr float kDt = 1.0f / 120.0f;
using Mode = ai::EnemyDirector::Mode;

/// 建物（長方形）を作る（左上の x, y と幅・高さ）
Obstacle building(float x, float y, float w, float h) {
    Obstacle o;
    o.kind = ObstacleKind::Building;
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

/// size × size（中心が (0, 0)）のテスト用ステージを作る
StageDef makeStage(float size, std::vector<Obstacle> obstacles, std::vector<Shape> bushes = {}) {
    StageDef stage;
    stage.id = "search_test";
    stage.bounds = Rectangle{-size * 0.5f, -size * 0.5f, size, size};
    stage.obstacles = std::move(obstacles);
    stage.bushes = std::move(bushes);
    return stage;
}

/// 探索のテスト用の準備。data フォルダの設定を読み込み、stage の地形で World を作る
class SearchFixture {
public:
    explicit SearchFixture(const StageDef& stage) : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, Terrain(stage));
    }

    /// 自キャラ（like_ezreal）を pos に置く
    EntityId addPlayer(Vector2 pos) {
        Unit player = makePlayerUnit(*data_.character("like_ezreal"));
        player.pos = pos;
        return world_->addUnit(player);
    }

    /// 敵（like_ezreal）を pos に置く
    EntityId addEnemy(Vector2 pos) {
        const CharacterDef* def = data_.character("like_ezreal");
        Unit unit;
        unit.team = Team::Enemy;
        unit.name = "enemy";
        unit.characterId = def->id;
        unit.radius = def->hitboxRadius;
        unit.baseMoveSpeed = def->moveSpeed;
        unit.pos = pos;
        return world_->addUnit(unit);
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 読み込んだ設定ファイル
    const GameData& data() const { return data_; }
    /// 通し番号のキャラ（いることが分かっている時に使う）
    Unit& unit(EntityId id) { return *world_->findUnit(id); }
    /// 敵 AI と World を 1 刻み進める
    void step(ai::EnemyDirector& director) {
        director.update(*world_, kDt);
        world_->update(kDt);
    }

private:
    GameData data_;
    std::unique_ptr<World> world_;
};

/// 見失った自キャラを探す動きの結果
struct SearchResult {
    bool searched = false;     // 探索に移った
    bool found = false;        // 見つけ直した
    float foundAfter = 0.0f;   // 消えてから見つけ直すまでの時間（秒）
    bool enemyInBush = false;  // 見つけた時、敵がブッシュの中にいた
};

/// 自キャラを (0, 0)、敵を (-700, 0) に置いて 0.5 秒進めてから（自キャラは見えている）、自キャラを hideAt へ消し、
/// 見つけ直すまで（最長 limit 秒）進める
SearchResult hideAndSeek(SearchFixture& f, int aiLevel, Vector2 hideAt, float limit) {
    const EntityId playerId = f.addPlayer({0.0f, 0.0f});
    const EntityId enemy = f.addEnemy({-700.0f, 0.0f});
    ai::EnemyDirector director;
    director.setup(f.data().ai(), aiLevel, 7);
    director.addEnemy(enemy);
    for (int i = 0; i < 60; ++i) f.step(director);
    REQUIRE(f.world().canSee(Team::Enemy, f.unit(playerId)));

    f.unit(playerId).pos = hideAt;
    SearchResult result;
    for (int i = 1; i <= static_cast<int>(limit * 120.0f) && !result.found; ++i) {
        f.step(director);
        if (director.mode() == Mode::Search) result.searched = true;
        if (f.world().canSee(Team::Enemy, f.unit(playerId))) {
            result.found = true;
            result.foundAfter = static_cast<float>(i) * kDt;
            result.enemyInBush = f.world().terrain().bushAt(f.unit(enemy).pos) >= 0;
        }
    }
    // 見つけたら、探索をやめて元の動きに戻る
    if (result.found) {
        f.step(director);
        CHECK(director.mode() == Mode::Fight);
    }
    return result;
}

}  // namespace

TEST_CASE("探索のエリア: ブッシュは 1 つで 1 エリア、地面は areaSize 四方に分ける。立てない所には点を置かない") {
    // 4000 × 4000。真ん中に 1000 四方の建物、右に半径 200 のブッシュ、左下に点の間隔より小さい半径 40 のブッシュ
    const Terrain terrain(makeStage(4000.0f, {building(-500.0f, -500.0f, 1000.0f, 1000.0f)},
                                    {circle(1200.0f, 0.0f, 200.0f), circle(-1250.0f, 1250.0f, 40.0f)}));
    const SearchConfig config;  // エリア 500 四方、点の間隔 125
    ai::SearchMap map;
    map.build(terrain, config, 65.0f);
    REQUIRE(map.built());

    for (const ai::SearchMap::Point& point : map.points()) {
        CHECK_FALSE(terrain.blocked(point.pos, 65.0f));              // 立てない所には置かない
        CHECK(map.bushOf(point.area) == terrain.bushAt(point.pos));  // ブッシュの中の点は、そのブッシュのエリア
    }
    int bushAreas = 0;
    int groundAreas = 0;
    for (int area = 0; area < map.areaCount(); ++area) ++(map.bushOf(area) >= 0 ? bushAreas : groundAreas);
    CHECK(bushAreas == 2);     // 点の間隔より小さいブッシュにも、点がある
    CHECK(groundAreas == 60);  // 8 × 8 マスのうち、建物で埋まった 4 マスを除く
    CHECK(map.exploredCount() == 0);
}

TEST_CASE("探索の記録: 敵の陣営から見えた点だけを見た扱いにし、全部見たエリアは探索済み。reset で消える") {
    // 壁 (300〜400, -1000〜1000) の向こうは、(0, 0) の敵から見えない
    SearchFixture f(makeStage(4000.0f, {building(300.0f, -1000.0f, 100.0f, 2000.0f)}));
    f.addEnemy({0.0f, 0.0f});
    ai::SearchMap map;
    map.build(f.world().terrain(), f.data().ai().search, 65.0f);
    map.markSeen(f.world());

    for (const ai::SearchMap::Point& point : map.points())
        CHECK(point.seen == f.world().canSeePoint(Team::Enemy, point.pos));
    CHECK(map.explored(map.areaNear({-250.0f, -250.0f})));  // 敵のいるマスは、全部見えている
    const int behindWall = map.areaNear({750.0f, 250.0f});  // 壁の向こうのマス
    CHECK_FALSE(map.explored(behindWall));
    CHECK(map.exploredCount() > 0);
    CHECK_FALSE(map.allExplored());

    // 次に探すエリアは、見失った位置 (700, 0) の近く。他の敵が向かっているエリアは避ける
    const int first = map.chooseArea({0.0f, 0.0f}, {700.0f, 0.0f}, {});
    REQUIRE(first >= 0);
    CHECK_FALSE(map.explored(first));
    CHECK(Vector2Distance(map.nearestUnseenPoint(first, {0.0f, 0.0f}), {700.0f, 0.0f}) < 400.0f);
    const int second = map.chooseArea({0.0f, 0.0f}, {700.0f, 0.0f}, {first});
    CHECK(second >= 0);
    CHECK(second != first);

    // あきらめたエリアは探索済み
    map.giveUp(behindWall);
    CHECK(map.explored(behindWall));

    map.reset();
    CHECK(map.exploredCount() == 0);
    CHECK(std::none_of(map.points().begin(), map.points().end(),
                       [](const ai::SearchMap::Point& point) { return point.seen; }));
}

TEST_CASE(
    "確かめ撃ちの先: 見えない所（ブッシュの中・ブッシュの陰）を選び、1 エリアに 1 "
    "回だけ。視界の半径より遠い所は選ばない") {
    // (0, 0) の敵から 600 先の左右に、ブッシュ 2 つ。下の 1600 先のブッシュは、視界の半径の外
    SearchFixture f(makeStage(
        4000.0f, {}, {circle(600.0f, 0.0f, 150.0f), circle(-600.0f, 0.0f, 150.0f), circle(0.0f, 1600.0f, 150.0f)}));
    f.addEnemy({0.0f, 0.0f});
    const Terrain& terrain = f.world().terrain();
    ai::SearchMap map;
    map.build(terrain, f.data().ai().search, 65.0f);
    map.markSeen(f.world());

    std::mt19937 rng(1);
    std::vector<int> areas;
    std::vector<int> bushes;
    for (int i = 0; i < 50; ++i) {
        const auto target = map.chooseProbe(f.world(), {0.0f, 0.0f}, 1200.0f, {500.0f, 0.0f}, rng);
        if (!target) break;
        CHECK_FALSE(f.world().canSeePoint(Team::Enemy, target->point));
        CHECK(Vector2Distance(target->point, {0.0f, 0.0f}) <= f.data().settings().visionRadius);
        CHECK(std::find(areas.begin(), areas.end(), target->area) == areas.end());  // 同じエリアには 2 回撃たない
        areas.push_back(target->area);
        if (map.bushOf(target->area) >= 0) bushes.push_back(map.bushOf(target->area));
        map.markProbed(target->area);
    }
    REQUIRE_FALSE(areas.empty());
    // 最初は、見失った位置 (500, 0) に最も近い右のブッシュ。左のブッシュにも撃ち、遠いブッシュには撃たない
    CHECK(map.bushOf(areas.front()) == terrain.bushAt({600.0f, 0.0f}));
    CHECK(std::count(bushes.begin(), bushes.end(), terrain.bushAt({-600.0f, 0.0f})) == 1);
    CHECK(std::count(bushes.begin(), bushes.end(), terrain.bushAt({0.0f, 1600.0f})) == 0);
}

TEST_CASE("敵 AI: 追いかけた先にいなければ探索に移り、離れたブッシュに隠れた自キャラを、ブッシュに入って見つける") {
    // 3000 × 3000。自キャラは、見えている (0, 0) から右上のブッシュの中へ消える（追いかける先とは別の所）
    SearchFixture f(makeStage(3000.0f, {}, {circle(1100.0f, -1100.0f, 200.0f)}));
    const SearchResult result = hideAndSeek(f, 3, {1100.0f, -1100.0f}, 90.0f);
    CHECK(result.searched);
    CHECK(result.found);
    CHECK(result.enemyInBush);
    MESSAGE("ブッシュに隠れた自キャラを見つけるまで ", result.foundAfter, " 秒");
}

TEST_CASE("敵 AI: ブッシュの中の自キャラを見つけたら、最も近い敵だけがブッシュに留まり、見失わない") {
    // 半径 250 のブッシュに自キャラが隠れる。敵 2 体のうち、見つけた後も 1 体はブッシュの中に留まる
    SearchFixture f(makeStage(3000.0f, {}, {circle(800.0f, -600.0f, 250.0f)}));
    const EntityId playerId = f.addPlayer({0.0f, 0.0f});
    const EntityId first = f.addEnemy({-700.0f, 0.0f});
    const EntityId second = f.addEnemy({-700.0f, 300.0f});
    ai::EnemyDirector director;
    director.setup(f.data().ai(), 3, 7);
    director.addEnemy(first);
    director.addEnemy(second);
    for (int i = 0; i < 60; ++i) f.step(director);
    REQUIRE(f.world().canSee(Team::Enemy, f.unit(playerId)));

    f.unit(playerId).pos = {800.0f, -600.0f};
    bool found = false;
    for (int i = 0; i < 30 * 120 && !found; ++i) {
        f.step(director);
        found = f.world().canSee(Team::Enemy, f.unit(playerId));
    }
    REQUIRE(found);

    // 見つけてから 5 秒: 見え続け、見つける・見失うを繰り返さない
    int visible = 0;
    int changes = 0;
    bool wasVisible = true;
    for (int i = 0; i < 5 * 120; ++i) {
        f.step(director);
        const bool now = f.world().canSee(Team::Enemy, f.unit(playerId));
        if (now) ++visible;
        if (now != wasVisible) ++changes;
        wasVisible = now;
    }
    CHECK(visible == 5 * 120);
    CHECK(changes == 0);
    const Terrain& terrain = f.world().terrain();
    const int inBush = static_cast<int>(terrain.bushAt(f.unit(first).pos) >= 0) +
                       static_cast<int>(terrain.bushAt(f.unit(second).pos) >= 0);
    CHECK(inBush == 1);  // ブッシュに留まるのは 1 体だけ（もう 1 体はいつもの間合い）
    CHECK(director.mode() == Mode::Fight);
}

TEST_CASE("敵 AI: 建物の陰に隠れた自キャラを、回り込んで見つける") {
    // 建物 (200〜700, -800〜800) の右側へ、自キャラが消える
    SearchFixture f(makeStage(3000.0f, {building(200.0f, -800.0f, 500.0f, 1600.0f)}));
    const SearchResult result = hideAndSeek(f, 1, {900.0f, 0.0f}, 90.0f);
    CHECK(result.searched);
    CHECK(result.found);
    MESSAGE("建物の陰の自キャラを見つけるまで ", result.foundAfter, " 秒");
}

TEST_CASE("敵 AI: 全エリアを探しても見つからなければ、たどり着けないエリアはあきらめて、最初から探し直す") {
    // 2000 × 2000。右下に、建物で四方を囲まれた部屋（中に入れず、外からも見えない）。自キャラは部屋の中へ消える
    SearchFixture f(
        makeStage(2000.0f, {building(340.0f, 340.0f, 520.0f, 60.0f), building(340.0f, 800.0f, 520.0f, 60.0f),
                            building(340.0f, 400.0f, 60.0f, 400.0f), building(800.0f, 400.0f, 60.0f, 400.0f)}));
    const EntityId playerId = f.addPlayer({0.0f, 0.0f});
    const EntityId enemy = f.addEnemy({-700.0f, 0.0f});
    ai::EnemyDirector director;
    director.setup(f.data().ai(), 3, 7);
    director.addEnemy(enemy);
    for (int i = 0; i < 60; ++i) f.step(director);
    REQUIRE(f.world().canSee(Team::Enemy, f.unit(playerId)));

    f.unit(playerId).pos = {600.0f, 600.0f};
    bool seen = false;
    int i = 0;
    for (; i < 120 * 120 && director.searchRound() < 2; ++i) {
        f.step(director);
        if (f.world().canSee(Team::Enemy, f.unit(playerId))) seen = true;
    }
    CHECK_FALSE(seen);
    CHECK(director.searchRound() >= 2);
    CHECK(director.mode() == Mode::Search);
    // 探し直した直後は、今見えている所だけが探索済み
    CHECK(director.searchMap().exploredCount() < director.searchMap().areaCount());
    MESSAGE("全エリアを 1 周探すまで ", static_cast<float>(i) * kDt, " 秒");
}

TEST_CASE(
    "敵 AI: 賢さ 4 は見えないブッシュへ確かめ撃ちをし、当たったら向かう。賢さ 3 "
    "は撃たない。確かめ撃ちはドッジ率に数えない") {
    // 敵 (-700, 0) から離れた (0, -800) に、半径 100 の小さいブッシュ。自キャラはブッシュの中心へ消える
    const auto probeRun = [](int aiLevel, int& hiddenHits, bool& shotWhileHidden, int& dodgeTotal) {
        SearchFixture f(makeStage(3000.0f, {}, {circle(0.0f, -800.0f, 100.0f)}));
        const EntityId playerId = f.addPlayer({0.0f, 0.0f});
        const EntityId enemy = f.addEnemy({-700.0f, 0.0f});
        ai::EnemyDirector director;
        director.setup(f.data().ai(), aiLevel, 7);
        director.addEnemy(enemy);
        hiddenHits = 0;
        f.world().setHitObserver([&](int, SkillSlot, const Unit& target) {
            if (target.team == Team::Player && !f.world().canSee(Team::Enemy, target)) ++hiddenHits;
        });
        for (int i = 0; i < 60; ++i) f.step(director);
        REQUIRE(f.world().canSee(Team::Enemy, f.unit(playerId)));

        f.unit(playerId).pos = {0.0f, -800.0f};
        shotWhileHidden = false;
        bool chasedHit = false;
        bool found = false;
        for (int i = 0; i < 30 * 120 && !found; ++i) {
            f.step(director);
            found = f.world().canSee(Team::Enemy, f.unit(playerId));
            if (!found && !f.world().projectiles().empty()) shotWhileHidden = true;
            if (hiddenHits > 0 && director.mode() == Mode::Chase) chasedHit = true;
        }
        CHECK(found);
        dodgeTotal = f.world().dodge().total;  // 見つけるまでは、自キャラを狙って撃っていない
        return chasedHit;
    };

    int hiddenHits = 0;
    bool shotWhileHidden = false;
    int dodgeTotal = -1;
    const bool chasedHit = probeRun(4, hiddenHits, shotWhileHidden, dodgeTotal);
    CHECK(shotWhileHidden);
    CHECK(hiddenHits >= 1);  // 確かめ撃ちが、ブッシュの中の自キャラに当たった
    CHECK(chasedHit);        // 当たった位置へ追いかけた
    CHECK(dodgeTotal == 0);  // 確かめ撃ちはドッジ率に数えない

    probeRun(3, hiddenHits, shotWhileHidden, dodgeTotal);
    CHECK_FALSE(shotWhileHidden);
    CHECK(hiddenHits == 0);
}
