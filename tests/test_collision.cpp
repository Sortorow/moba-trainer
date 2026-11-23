// キャラ同士の押し合い（UnitCollision）と、押し合いを無くす許可、弾が出た位置より後ろに当たらないことのテスト

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TempDataCopy.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "data/StageDef.h"
#include "game/DummyController.h"
#include "game/Match.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/UnitCollision.h"
#include "game/World.h"
#include "input/InputFrame.h"
#include "raymath.h"

using namespace moba;

namespace {

/// 押し合いのテスト用の準備。障害物のない World に自キャラを (0, 0) に置き、ダミーを足して動かす
class CollisionFixture {
public:
    /// data フォルダの設定を読み込み、指定したキャラの自キャラを (0, 0) に置く（押し合いの半径はキャラファイルの値）
    explicit CollisionFixture(std::string_view characterId) : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        const CharacterDef* def = data_.character(characterId);
        REQUIRE(def != nullptr);
        playerId_ = world_->addUnit(makePlayerUnit(*def));
    }

    /// 敵の陣営のダミーを置く（当たり判定の半径 65・押し合いの半径 20 = 練習場のダミーの規定値・移動速度 300）
    EntityId addDummy(Vector2 pos, float collisionRadius = 20.0f) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 65.0f;
        dummy.collisionRadius = collisionRadius;
        dummy.baseMoveSpeed = 300.0f;
        return world_->addUnit(dummy);
    }

    World& world() { return *world_; }
    GameData& data() { return data_; }
    Unit& player() { return *world_->findUnit(playerId_); }
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    /// 自キャラに、カーソルを cursor に置いてスキルのキーを押させる
    void cast(SkillSlot slot, Vector2 cursor) { casting::requestCast(*world_, player(), CastRequest{slot, cursor}); }

    /// World を seconds 秒進める（ゲームと同じ 1/120 秒刻み）。刻みごとに onStep を呼ぶ
    template <typename F>
    void run(float seconds, F onStep) {
        const int steps = static_cast<int>(seconds * 120.0f + 0.5f);
        for (int i = 0; i < steps; ++i) {
            world_->update(1.0f / 120.0f);
            onStep();
        }
    }
    /// World を seconds 秒進める
    void run(float seconds) {
        run(seconds, [] {});
    }

private:
    GameData data_;
    std::unique_ptr<World> world_;
    EntityId playerId_ = kInvalidId;
};

/// 2 体の中心の距離
float distanceBetween(const Unit& a, const Unit& b) { return Vector2Distance(a.pos, b.pos); }

}  // namespace

// ---------------------------------------------------------------------------
// 押し合い
// ---------------------------------------------------------------------------

TEST_CASE("押し合い: 動いた側だけが相手の円に沿って回り込み、止まっている相手は動かない") {
    CollisionFixture f("like_emma");
    const EntityId dummy = f.addDummy({200, 20});
    const Vector2 dummyStart = f.unit(dummy).pos;
    const float minDistance = f.player().collisionRadius + f.unit(dummy).collisionRadius;

    f.player().moveTarget = Vector2{400, 0};
    float closest = 1e9f;
    f.run(2.0f, [&] { closest = std::min(closest, distanceBetween(f.player(), f.unit(dummy))); });

    CHECK(f.unit(dummy).pos.x == doctest::Approx(dummyStart.x));  // 止まっている側は押されない
    CHECK(f.unit(dummy).pos.y == doctest::Approx(dummyStart.y));
    CHECK(closest >= minDistance - 0.5f);                            // 重ならない（1 刻みの誤差は許す）
    CHECK(Vector2Distance(f.player().pos, Vector2{400, 0}) < 1.0f);  // 回り込んで目的地に着く
}

TEST_CASE("押し合い: 真正面からぶつかると、進む向きの右側へ回り込んで抜ける") {
    // 画面の座標は y が下なので、右へ進む時の右側は +y、左へ進む時の右側は -y
    struct Case {
        Vector2 start;
        Vector2 goal;
        float rightSign;
    };
    for (const Case& c : {Case{{0, 0}, {400, 0}, 1.0f}, Case{{400, 0}, {0, 0}, -1.0f}}) {
        CAPTURE(c.goal.x);
        CollisionFixture f("like_emma");
        f.player().pos = f.player().prevPos = c.start;
        const EntityId dummy = f.addDummy({200, 0});  // 通り道の真ん中
        f.player().moveTarget = c.goal;

        float sideWhenPassing = 0.0f;
        bool passed = false;
        f.run(3.0f, [&] {
            const bool beyond = c.rightSign > 0.0f ? f.player().pos.x >= 200.0f : f.player().pos.x <= 200.0f;
            if (beyond && !passed) {
                sideWhenPassing = f.player().pos.y;
                passed = true;
            }
        });
        REQUIRE(passed);  // 相手の前で止まったままにならない
        CHECK(sideWhenPassing * c.rightSign > 20.0f);
        CHECK(Vector2Distance(f.player().pos, c.goal) < 1.0f);
        CHECK(f.unit(dummy).pos.x == doctest::Approx(200.0f));
    }
}

TEST_CASE("押し合い: 向かい合って歩く 2 体は、それぞれ右側へよけてすれ違う（重ならず、処理の順番にもよらない）") {
    CollisionFixture f("like_emma");
    const EntityId other = f.addDummy({400, 0}, 30.0f);
    f.player().moveTarget = Vector2{400, 0};
    f.unit(other).moveTarget = Vector2{0, 0};
    const float minDistance = f.player().collisionRadius + f.unit(other).collisionRadius;

    float closest = 1e9f;
    float playerSide = 0.0f;
    float otherSide = 0.0f;
    f.run(4.0f, [&] {
        const float d = distanceBetween(f.player(), f.unit(other));
        if (d < closest) {
            closest = d;
            playerSide = f.player().pos.y;
            otherSide = f.unit(other).pos.y;
        }
    });
    CHECK(closest >= minDistance - 0.5f);
    CHECK(playerSide > 0.0f);  // 右へ進む自キャラは +y 側へ
    CHECK(otherSide < 0.0f);   // 左へ進む相手は -y 側へ
    CHECK(Vector2Distance(f.player().pos, Vector2{400, 0}) < 1.0f);
    CHECK(Vector2Distance(f.unit(other).pos, Vector2{0, 0}) < 1.0f);
}

TEST_CASE("押し合い: ワープで相手の上に移った側が押し出され、相手は動かない") {
    CollisionFixture f("like_ezreal");
    const EntityId dummy = f.addDummy({300, 0});
    f.cast(SkillSlot::D, {300, 0});  // フラッシュ（射程 400）で、ダミーの真上へ
    f.run(1.0f / 120.0f);
    CHECK(distanceBetween(f.player(), f.unit(dummy)) >= f.player().collisionRadius + 20.0f - 0.01f);
    CHECK(f.unit(dummy).pos.x == doctest::Approx(300.0f));
    CHECK(f.unit(dummy).pos.y == doctest::Approx(0.0f));
}

TEST_CASE("押し合い: 押し出した先が障害物（ステージの外）にかかる時は、障害物を優先する") {
    CollisionFixture f("like_emma");
    const Rectangle bounds = f.world().bounds();
    const float edge = bounds.x;  // ステージの左端
    // ダミーを左端のすぐそばに置き、自キャラを端に向かって歩かせる（間に挟まって動けなくなってもよい）
    const EntityId dummy = f.addDummy({edge + 80.0f, 0});
    f.player().pos = f.player().prevPos = Vector2{edge + 300.0f, 0};
    f.player().moveTarget = Vector2{edge + f.player().radius + 1.0f, 0};  // 端ぎりぎりの立てる位置（ダミーの向こう側）
    bool insideStage = true;
    f.run(2.0f, [&] { insideStage = insideStage && f.player().pos.x - f.player().radius >= edge - 0.01f; });
    CHECK(insideStage);
    CHECK(f.unit(dummy).pos.x == doctest::Approx(edge + 80.0f));
}

TEST_CASE("押し合い: 真正面からぶつかった時、右側が障害物でふさがっていれば、左側へ回り込んで抜ける") {
    // ステージの下の端に沿って右へ歩く（右へ進む時の右側は +y = ステージの外）。同じ端に沿って立つダミーに真正面からぶつかる
    CollisionFixture f("like_emma");
    const float edgeY = f.world().bounds().y + f.world().bounds().height;
    const float y = edgeY - f.player().radius;  // 自キャラが下の端に触れる高さ
    f.player().pos = f.player().prevPos = Vector2{0, y};
    const EntityId dummy = f.addDummy({200, y});
    f.unit(dummy).radius = f.player().radius;  // 障害物との当たりも同じ高さで端に触れる大きさにする
    f.player().moveTarget = Vector2{400, y};

    float sideWhenPassing = 0.0f;
    bool passed = false;
    f.run(3.0f, [&] {
        if (f.player().pos.x >= 200.0f && !passed) {
            sideWhenPassing = f.player().pos.y - y;
            passed = true;
        }
    });
    REQUIRE(passed);                  // 壁とダミーの間で止まったままにならない
    CHECK(sideWhenPassing < -20.0f);  // 左側（-y、ステージの内側）を通った
    CHECK(Vector2Distance(f.player().pos, Vector2{400, y}) < 1.0f);
}

TEST_CASE("練習場: 両側が壁の狭い通路で静止のダミーにふさがれた往復のダミーは、約 1 秒で折り返す") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    // 幅 100 の横長の通路（y -50〜50）。当たり判定の半径 40 のダミーは y が -10〜10 しか通れず、静止のダミーの横を抜けられない
    StageDef stage;
    stage.id = "corridor";
    stage.bounds = Rectangle{-1000.0f, -1000.0f, 2000.0f, 2000.0f};
    for (const Rectangle wall : {Rectangle{-800, -250, 1600, 200}, Rectangle{-800, 50, 1600, 200}}) {
        Shape shape;
        shape.rect = wall;
        stage.obstacles.push_back(Obstacle{ObstacleKind::Wall, shape});
    }
    World world(data, Terrain(stage));
    auto addDummy = [&](Vector2 pos) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 40.0f;
        dummy.collisionRadius = 20.0f;
        dummy.baseMoveSpeed = 300.0f;
        return world.addUnit(dummy);
    };
    const EntityId blocker = addDummy({0, 0});
    const EntityId walker = addDummy({-400, 0});
    DummyController dummies;
    DummyBrain still{blocker, DummyBehavior::Stationary, {0, 0}, {0, 0}};
    DummyBrain patrol{walker, DummyBehavior::Patrol, {-400, 0}, {400, 0}};
    dummies.add(still);
    dummies.add(patrol);

    float turnedAt = -1.0f;
    float farthest = -1000.0f;
    for (int i = 0; i < 120 * 4 && turnedAt < 0.0f; ++i) {
        dummies.update(world);
        world.update(1.0f / 120.0f);
        const Unit& w = *world.findUnit(walker);
        farthest = std::max(farthest, w.pos.x);
        if (w.moveTarget && w.moveTarget->x < 0.0f) turnedAt = world.time();  // (-400, 0) へ向かい直した
    }
    REQUIRE(turnedAt > 0.0f);
    // (400, 0) へは着いていない（静止のダミーの手前で折り返した）。ぶつかってから約 1 秒（歩く時間 1.2 秒 + 1 秒前後）
    CHECK(farthest < -30.0f);
    CHECK(turnedAt == doctest::Approx(2.2f).epsilon(0.15));
    CHECK(world.findUnit(blocker)->pos.x == doctest::Approx(0.0f));  // 静止のダミーは押されない
}

TEST_CASE("練習場: どのステージ・プリセットでも、往復するダミーが詰まって折り返すことなく往復する") {
    // 2026-11-24 の不具合: 路地裏 ×「小さい的」で、建物の縁に沿って回り道をする往復のダミーが、同じ縁に押し付けられた
    // 静止のダミーに真正面からぶつかり、右側（建物）へ回り込もうとして止まっていた
    test::TempDataCopy copy;
    GameData data(copy.paths());
    data.loadAll();
    for (const char* stage : {"open", "alley", "forest"}) {
        for (const SandboxPreset& preset : data.sandbox().presets) {
            CAPTURE(stage);
            CAPTURE(preset.name);
            REQUIRE(data.selectSandboxPreset(preset.name));
            MatchConfig config;
            config.mode = GameMode::Sandbox;
            config.characterId = "like_emma";
            config.stageId = stage;
            Match match(data, config);
            REQUIRE(match.valid());

            // 往復の折り返し（目的地に着く前に、目的地が変わった）の回数。進めなくなった時だけ起きるので、0 回であること
            std::map<EntityId, std::optional<Vector2>> lastTarget;
            int turnBacks = 0;
            int arrivals = 0;
            const InputFrame input;
            for (int i = 0; i < 120 * 30; ++i) {
                match.update(1.0f / 120.0f, input);
                for (const DummyBrain& brain : match.dummies().brains()) {
                    if (brain.behavior != DummyBehavior::Patrol) continue;
                    const Unit& u = *match.world().findUnit(brain.unitId);
                    std::optional<Vector2>& last = lastTarget[u.id];
                    if (last && (!u.moveTarget || Vector2Distance(*u.moveTarget, *last) > 1.0f)) {
                        if (Vector2Distance(u.pos, *last) > 5.0f) {
                            ++turnBacks;
                        } else {
                            ++arrivals;
                        }
                    }
                    last = u.moveTarget;
                }
            }
            CHECK(turnBacks == 0);
            const bool hasPatrol =
                std::any_of(preset.dummies.begin(), preset.dummies.end(), [](const DummyDef& d) { return d.patrolTo; });
            if (hasPatrol) CHECK(arrivals > 0);  // 実際に往復している
        }
    }
}

TEST_CASE("押し合いの許可: 時間で終わる許可は、残り時間が過ぎたら外れて、押し合いに戻る") {
    CollisionFixture f("like_emma");
    const EntityId dummy = f.addDummy({20, 0});
    OverlapPass ghost;
    ghost.reason = "すり抜け（テスト）";
    ghost.allUnits = true;
    ghost.remaining = 0.5f;
    f.player().overlapPasses.push_back(ghost);

    f.player().moveTarget = Vector2{0, 0};
    f.player().pos = Vector2{25, 0};  // 動いたことにして、押し合いの対象にする
    f.run(0.1f);
    CHECK(collision::passes(f.player(), f.unit(dummy)));
    CHECK(distanceBetween(f.player(), f.unit(dummy)) < 30.0f);  // 許可がある間は重なったまま

    f.run(0.5f);
    CHECK(f.player().overlapPasses.empty());
    f.player().moveTarget = Vector2{10, 0};  // 歩いて、押し合いの対象になる
    f.run(0.2f);
    CHECK(distanceBetween(f.player(), f.unit(dummy)) >= 50.0f - 0.5f);
}

// ---------------------------------------------------------------------------
// 特例: エマの R（兎）
// ---------------------------------------------------------------------------

namespace {

/// エマで、(400, 0) のダミーを E で兎にしてから R で兎へ移動するまで進める
/// @return 兎にしたダミーの通し番号
EntityId emmaLandsOnRabbit(CollisionFixture& f) {
    const EntityId rabbit = f.addDummy({400, 0});
    f.cast(SkillSlot::E, {400, 0});
    f.run(0.6f);
    REQUIRE(f.unit(rabbit).status.has(EffectType::Polymorph));
    f.cast(SkillSlot::R, {400, 0});
    f.run(1.0f / 120.0f);
    return rabbit;
}

}  // namespace

TEST_CASE("エマ R（兎）: 兎の中心へ移動し、どちらも動かなければ重なったまま") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = emmaLandsOnRabbit(f);
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) < 1.0f);
    REQUIRE(f.player().overlapPasses.size() == 1);
    f.run(1.0f);
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) < 1.0f);
}

TEST_CASE("エマ R（兎）: 兎の向こう側から飛んでも、兎の中心へ移動する") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = f.addDummy({-300, -250});
    f.cast(SkillSlot::E, {-300, -250});
    f.run(0.6f);
    REQUIRE(f.unit(rabbit).status.has(EffectType::Polymorph));
    f.cast(SkillSlot::R, {-300, -250});
    f.run(1.0f / 120.0f);
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) < 1.0f);
}

TEST_CASE("エマ R（兎）: 兎が歩いて離れる間は押し合わず、離れきったら許可が外れる") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = emmaLandsOnRabbit(f);
    const Vector2 emmaPos = f.player().pos;
    f.unit(rabbit).moveTarget = Vector2{400, 300};
    f.run(1.0f);
    CHECK(Vector2Distance(f.player().pos, emmaPos) < 0.01f);  // エマは押されない
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) > 50.0f);
    CHECK(f.player().overlapPasses.empty());
}

TEST_CASE("エマ R（兎）: 重なったまま歩いている時に新しく詠唱を始めると、エマだけが押し出される（飛ぶ）") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = emmaLandsOnRabbit(f);
    f.player().moveTarget = Vector2{400, 300};  // 兎から下へ歩き出す
    f.run(0.05f);
    REQUIRE(f.player().walking);
    REQUIRE(distanceBetween(f.player(), f.unit(rabbit)) < 30.0f);  // まだ重なっている

    f.cast(SkillSlot::Q, {900, 0});
    f.run(1.0f / 120.0f);
    CHECK(f.player().overlapPasses.empty());
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) >= f.player().collisionRadius + 20.0f - 0.01f);
    CHECK(f.player().pos.y > 0.0f);                          // 兎から離れる向き（歩いていた下の向き）へ
    CHECK(f.unit(rabbit).pos.x == doctest::Approx(400.0f));  // 兎は動かない
}

TEST_CASE("エマ R（兎）: 止まったまま詠唱を始めても押し出されない（R の直後の Q）") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = emmaLandsOnRabbit(f);
    f.cast(SkillSlot::Q, {900, 0});
    f.run(0.5f);
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) < 1.0f);
}

TEST_CASE("エマ EQR: Q の溜めの間に R で兎へ移動しても、カードが出た時に押し出されない") {
    CollisionFixture f("like_emma");
    const EntityId rabbit = f.addDummy({400, 0});
    f.cast(SkillSlot::E, {400, 0});
    f.run(0.6f);
    REQUIRE(f.unit(rabbit).status.has(EffectType::Polymorph));
    f.cast(SkillSlot::Q, {400, 0});
    f.run(0.05f);
    f.cast(SkillSlot::R, {400, 0});
    f.run(0.5f);  // カードが出るまで（溜め 0.2 秒）より長く
    CHECK(distanceBetween(f.player(), f.unit(rabbit)) < 1.0f);
}

// ---------------------------------------------------------------------------
// 弾は出た位置より後ろには当たらない
// ---------------------------------------------------------------------------

TEST_CASE("弾: まっすぐ飛ぶ弾は、出た位置より後ろにいる相手には当たらない") {
    // エズリアルの Q（弾の半径 60）を右へ撃つ。ダミーの当たり判定の半径は 65
    struct Case {
        Vector2 dummyPos;
        bool hit;
        const char* note;
    };
    const Case cases[] = {
        {{-70, 0}, false, "体がすべて後ろ"},
        {{-40, 0}, true, "中心は後ろだが、体が出た位置より前に出ている"},
        {{0, 130}, false, "真横で、弾の幅より外"},
        {{0, 120}, true, "真横で、弾の幅の中"},
        {{300, 0}, true, "前"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.note);
        CollisionFixture f("like_ezreal");
        const EntityId dummy = f.addDummy(c.dummyPos);
        bool hit = false;
        f.world().setHitObserver([&](int, SkillSlot slot, const Unit& target) {
            if (slot == SkillSlot::Q && target.id == dummy) hit = true;
        });
        f.cast(SkillSlot::Q, {1000, 0});
        f.run(1.0f);
        CHECK(hit == c.hit);
    }
}

// ---------------------------------------------------------------------------
// 設定ファイル
// ---------------------------------------------------------------------------

TEST_CASE("設定: 押し合いの半径は、キャラは 30・練習場のダミーと的は規定値 20") {
    GameData data(makeAppPaths(MOBA_SOURCE_DIR));
    data.loadAll();
    for (const char* id : {"like_emma", "like_ezreal", "like_brand"}) {
        CAPTURE(id);
        REQUIRE(data.character(id) != nullptr);
        CHECK(data.character(id)->collisionRadius == doctest::Approx(30.0f));
    }
    for (const SandboxPreset& preset : data.sandbox().presets) {
        for (const DummyDef& dummy : preset.dummies) CHECK(dummy.collisionRadius == doctest::Approx(20.0f));
    }
    CHECK(data.ai().skillShot.collisionRadius == doctest::Approx(20.0f));
}

TEST_CASE("設定: キャラファイルに collisionRadius が無ければ、警告を出して 30 にする") {
    test::TempDataCopy temp;
    const std::filesystem::path file = temp.paths().charactersDir / "like_emma.jsonc";
    std::string text = test::readText(file);
    const std::size_t at = text.find("\"collisionRadius\"");
    REQUIRE(at != std::string::npos);
    text.erase(at, text.find('\n', at) - at);
    {
        std::ofstream out(file, std::ios::binary);
        out << text;
    }
    GameData data(temp.paths());
    data.loadAll();
    REQUIRE(data.character("like_emma") != nullptr);
    CHECK(data.character("like_emma")->collisionRadius == doctest::Approx(30.0f));
    const std::vector<std::string> errors = data.errors();
    CHECK(std::any_of(errors.begin(), errors.end(),
                      [](const std::string& e) { return e.find("collisionRadius") != std::string::npos; }));
}
