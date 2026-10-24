// ゲームロジックのテスト（画面を出さずに World を動かして確かめる）

#include <doctest/doctest.h>

#include <memory>

#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/World.h"
#include "raymath.h"

using namespace moba;

namespace {

class Fixture {
public:
    explicit Fixture(std::string_view characterId) : data_(makeAppPaths(MOBA_SOURCE_DIR)) {
        data_.loadAll();
        world_ = std::make_unique<World>(data_, kSandboxBounds);
        const CharacterDef* def = data_.character(characterId);
        REQUIRE(def != nullptr);
        Unit player;
        player.team = Team::Player;
        player.name = def->displayName;
        player.characterId = def->id;
        player.radius = def->hitboxRadius;
        player.baseMoveSpeed = def->moveSpeed;
        playerId_ = world_->addUnit(player);
    }

    EntityId addDummy(Vector2 pos) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 65.0f;
        dummy.baseMoveSpeed = 300.0f;
        return world_->addUnit(dummy);
    }

    World& world() { return *world_; }
    Unit& player() { return *world_->findUnit(playerId_); }
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    void cast(SkillSlot slot, Vector2 cursor) { casting::requestCast(*world_, player(), CastRequest{slot, cursor}); }
    void resetCooldowns() { player().cooldowns = {}; }

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

TEST_CASE("右クリックの移動: 移動速度どおりに進む") {
    Fixture f("like_ezreal");
    casting::orderMove(f.world(), f.player(), {1000, 0});
    f.run(1.0f);
    CHECK(f.player().pos.x == doctest::Approx(325.0f).epsilon(0.02));
}

TEST_CASE("エズリアル Q: 正面の敵に当たり、外せば当たらない") {
    Fixture f("like_ezreal");
    f.addDummy({600, 0});
    f.cast(SkillSlot::Q, {600, 0});
    f.run(1.0f);
    CHECK(f.world().stats().slot(SkillSlot::Q).casts == 1);
    CHECK(f.world().stats().slot(SkillSlot::Q).hits == 1);

    f.resetCooldowns();
    f.cast(SkillSlot::Q, {0, 600});
    f.run(1.0f);
    CHECK(f.world().stats().slot(SkillSlot::Q).casts == 2);
    CHECK(f.world().stats().slot(SkillSlot::Q).hits == 1);
}

TEST_CASE("エズリアル Q: 詠唱中は移動せず、詠唱後に元の目的地へ向かう") {
    Fixture f("like_ezreal");
    casting::orderMove(f.world(), f.player(), {1000, 0});
    f.cast(SkillSlot::Q, {0, 600});
    f.run(0.2f);  // 詠唱 0.25 秒の途中
    CHECK(f.player().pos.x == doctest::Approx(0.0f));
    f.run(0.5f);
    CHECK(f.player().pos.x > 50.0f);
}

TEST_CASE("エズリアル W: 貫通しない弾は最初の敵にだけ当たり、Q で印が起爆する") {
    Fixture f("like_ezreal");
    const EntityId front = f.addDummy({400, 0});
    const EntityId back = f.addDummy({800, 0});
    f.cast(SkillSlot::W, {800, 0});
    f.run(1.0f);
    CHECK(f.unit(front).status.hasMark("ezreal_w"));
    CHECK_FALSE(f.unit(back).status.hasMark("ezreal_w"));

    f.cast(SkillSlot::Q, {400, 0});
    f.run(0.6f);
    CHECK_FALSE(f.unit(front).status.hasMark("ezreal_w"));  // 起爆して消えた
}

TEST_CASE("エズリアル R: 貫通する弾は直線上の全員に当たる") {
    Fixture f("like_ezreal");
    f.addDummy({400, 0});
    f.addDummy({900, 30});
    f.cast(SkillSlot::R, {1000, 0});
    f.run(1.0f + 0.6f);  // 詠唱 1 秒 + 飛んでいる時間
    REQUIRE(f.world().projectiles().size() == 1);
    CHECK(f.world().projectiles().front().hitUnits.size() == 2);
}

TEST_CASE("詠唱中に押したスキルは予約され、詠唱後に発動する") {
    Fixture f("like_ezreal");
    f.cast(SkillSlot::Q, {600, 0});
    f.cast(SkillSlot::W, {600, 0});  // Q の詠唱中
    f.run(0.8f);
    CHECK(f.world().stats().slot(SkillSlot::Q).casts == 1);
    CHECK(f.world().stats().slot(SkillSlot::W).casts == 1);
}

TEST_CASE("フラッシュ: 射程の端までしか移動しない") {
    Fixture f("like_ezreal");
    f.cast(SkillSlot::D, {1000, 0});
    f.run(0.05f);
    CHECK(f.player().pos.x == doctest::Approx(400.0f));
}

TEST_CASE("エマ E: 射程外の敵には近づいてから撃ち、当たると兎になる") {
    Fixture f("like_emma");
    const EntityId target = f.addDummy({900, 0});
    f.cast(SkillSlot::E, {900, 0});
    CHECK(f.player().pendingCast.has_value());
    f.run(2.0f);
    CHECK(f.unit(target).status.has(EffectType::Polymorph));
    CHECK_FALSE(f.unit(target).status.canCast());
}

TEST_CASE("エマ R（兎）: 兎の位置へ移動し、周りの敵も兎にする") {
    Fixture f("like_emma");
    const EntityId rabbit = f.addDummy({400, 0});
    const EntityId neighbor = f.addDummy({520, 120});
    f.cast(SkillSlot::E, {400, 0});
    f.run(0.6f);
    REQUIRE(f.unit(rabbit).status.has(EffectType::Polymorph));
    CHECK_FALSE(f.unit(neighbor).status.has(EffectType::Polymorph));

    f.cast(SkillSlot::R, {400, 0});
    f.run(0.2f);
    CHECK(f.unit(neighbor).status.has(EffectType::Polymorph));
    CHECK(Vector2Distance(f.player().pos, f.unit(rabbit).pos) < 150.0f);
}

TEST_CASE("エマ Q: 鳩を残し、次の Q で鳩のカードも飛ぶ。2 枚当たるとスロウ") {
    Fixture f("like_emma");
    const EntityId target = f.addDummy({500, 0});
    f.cast(SkillSlot::Q, {500, 0});
    f.run(0.5f);
    REQUIRE(f.world().props().size() == 1);  // (0, 0) に鳩
    CHECK_FALSE(f.unit(target).status.has(EffectType::Slow));

    f.player().pos = {0, 250};  // 鳩から少し離れた位置から投げる
    f.resetCooldowns();
    f.cast(SkillSlot::Q, {500, 0});
    f.run(0.7f);
    CHECK(f.unit(target).status.has(EffectType::Slow));
    REQUIRE(f.world().props().size() == 1);  // 古い鳩はカードになり、新しい鳩が 1 羽
    CHECK(f.world().props().front().pos.y == doctest::Approx(250.0f));
}

TEST_CASE("エマ W → R（帽子）: 帽子の位置へ移動し、元の位置で打ち上げる") {
    Fixture f("like_emma");
    const EntityId nearStart = f.addDummy({80, 80});
    f.cast(SkillSlot::W, {600, 0});
    f.run(0.8f);
    REQUIRE(f.world().props().size() == 1);
    const Vector2 hatPos = f.world().props().front().pos;

    f.cast(SkillSlot::R, hatPos);
    f.run(0.4f);
    CHECK(Vector2Distance(f.player().pos, hatPos) < 1.0f);
    CHECK(f.unit(nearStart).status.has(EffectType::Knockup));
}

TEST_CASE("エマ D: 範囲に入った敵にスロウがかかり続ける") {
    Fixture f("like_emma");
    const EntityId target = f.addDummy({450, 0});
    f.cast(SkillSlot::D, {450, 0});
    f.run(1.0f);
    CHECK(f.unit(target).status.has(EffectType::Slow));
    CHECK(f.unit(target).moveSpeed() == doctest::Approx(150.0f));
}
