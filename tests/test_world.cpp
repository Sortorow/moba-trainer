// ゲームロジックのテスト（画面を出さずに World を動かして確かめる）

#include <doctest/doctest.h>

#include <memory>
#include <vector>

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
    /// data フォルダの設定を読み込み、障害物のない World に、指定したキャラの自キャラを (0, 0) に置く
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

    /// 敵の陣営のダミー（半径 65・移動速度 300）を置く
    /// @return ダミーの通し番号
    EntityId addDummy(Vector2 pos) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = "dummy";
        dummy.pos = pos;
        dummy.radius = 65.0f;
        dummy.baseMoveSpeed = 300.0f;
        return world_->addUnit(dummy);
    }

    /// テストで動かす World
    World& world() { return *world_; }
    /// 自キャラ
    Unit& player() { return *world_->findUnit(playerId_); }
    /// 通し番号のキャラ（いることが分かっている時に使う）
    Unit& unit(EntityId id) { return *world_->findUnit(id); }

    /// 自キャラに、カーソルを cursor に置いてスキルのキーを押させる
    void cast(SkillSlot slot, Vector2 cursor) { casting::requestCast(*world_, player(), CastRequest{slot, cursor}); }
    /// 自キャラのクールダウンをすべて 0 にする
    void resetCooldowns() { player().cooldowns = {}; }
    /// 自キャラのそのキーのクールダウンの残り（秒）
    float cooldown(SkillSlot slot) { return player().cooldowns[static_cast<std::size_t>(index(slot))]; }

    /// 設定ファイルのスキル定義（数値を調整してもテストが壊れないよう、期待値はここから計算する）
    const SkillDef& skill(SkillSlot slot) { return *data_.character(player().characterId)->skill(slot); }
    /// スキルを使ってから、弾が distance だけ進むまでの時間
    float timeToReach(SkillSlot slot, float distance) {
        const SkillDef& def = skill(slot);
        return def.castTime + distance / def.numOr("speed", 1.0f);
    }

    /// その種類の設置物（消えていないもの）を 1 つ探す。無ければ nullptr
    const Prop* findProp(PropKind kind) {
        for (const Prop& prop : world_->props()) {
            if (prop.alive && prop.kind == kind) return &prop;
        }
        return nullptr;
    }

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

TEST_CASE("右クリックの移動: 移動速度どおりに進む") {
    Fixture f("like_ezreal");
    casting::orderMove(f.world(), f.player(), {1000, 0});
    f.run(1.0f);
    CHECK(f.player().pos.x == doctest::Approx(f.player().baseMoveSpeed).epsilon(0.02));
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
    f.run(f.timeToReach(SkillSlot::Q, 500.0f) + 0.1f);
    REQUIRE(f.world().props().size() == 1);  // (0, 0) に鳩
    CHECK_FALSE(f.unit(target).status.has(EffectType::Slow));

    f.player().pos = {0, 250};  // 鳩から少し離れた位置から投げる
    f.resetCooldowns();
    f.cast(SkillSlot::Q, {500, 0});
    f.run(f.timeToReach(SkillSlot::Q, 600.0f) + 0.1f);
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

TEST_CASE("エマ Q: 鳩がどれだけ離れていても、鳩のカードが飛ぶ") {
    Fixture f("like_emma");
    f.cast(SkillSlot::Q, {500, 0});
    f.run(f.timeToReach(SkillSlot::Q, f.skill(SkillSlot::Q).range) + 0.1f);  // 1 回目のカードが射程の端で消えるまで待つ
    REQUIRE(f.findProp(PropKind::Dove) != nullptr);                          // (0, 0) に鳩

    f.player().pos = {2000, 0};  // 鳩から 2000 離れる
    f.resetCooldowns();
    f.cast(SkillSlot::Q, {2500, 0});
    f.run(0.3f);
    REQUIRE(f.world().projectiles().size() == 2);  // エマのカードと鳩のカード
    // doveCardSpeed を書かなければ、鳩のカードはエマ本体と同じ弾速
    if (!f.skill(SkillSlot::Q).params.contains("doveCardSpeed")) {
        CHECK(f.world().projectiles()[0].speed == doctest::Approx(f.world().projectiles()[1].speed));
    }
    REQUIRE(f.world().props().size() == 1);  // 古い鳩はカードになり、新しい鳩が 1 羽
    CHECK(f.world().props().front().pos.x == doctest::Approx(2000.0f));
}

TEST_CASE("エマ Q: エマのカードが当たると、Q のクールダウンが本来の 30% 分短くなる") {
    Fixture f("like_emma");
    f.addDummy({500, 0});
    f.cast(SkillSlot::Q, {500, 0});
    const float elapsed = f.timeToReach(SkillSlot::Q, 500.0f) + 0.1f;  // カードが当たった後まで待つ
    f.run(elapsed);
    // クールダウン - 経過時間 - 短縮分（本来のクールダウンの 30%）
    const float cooldown = f.skill(SkillSlot::Q).cooldown;
    CHECK(f.cooldown(SkillSlot::Q) == doctest::Approx(cooldown - elapsed - cooldown * 0.3f).epsilon(0.02));
}

TEST_CASE("エマ W: 当たるとクールダウンが 30% 分短くなる（複数に当たっても 1 回だけ）") {
    Fixture f("like_emma");
    f.addDummy({450, 0});
    f.addDummy({500, 60});
    f.cast(SkillSlot::W, {450, 0});
    f.run(0.8f);
    REQUIRE(f.findProp(PropKind::Hat) != nullptr);
    // クールダウン 10 秒 - 経過 0.8 秒 - 短縮 3 秒（10 秒の 30%）
    CHECK(f.cooldown(SkillSlot::W) == doctest::Approx(6.2f).epsilon(0.02));
}

TEST_CASE("エマ R（鳩）: 入れ替わった鳩は消えずに残り、残り時間が最初に戻る") {
    Fixture f("like_emma");
    f.cast(SkillSlot::Q, {500, 0});
    f.run(0.3f);
    f.player().pos = {600, 0};
    f.run(2.0f);
    REQUIRE(f.findProp(PropKind::Dove) != nullptr);
    CHECK(f.findProp(PropKind::Dove)->remaining < 5.0f);

    f.cast(SkillSlot::R, {0, 0});
    f.run(0.5f);
    CHECK(Vector2Distance(f.player().pos, {0, 0}) < 1.0f);
    const Prop* dove = f.findProp(PropKind::Dove);
    REQUIRE(dove != nullptr);
    CHECK(dove->pos.x == doctest::Approx(600.0f));
    CHECK(dove->remaining > 5.0f);
    CHECK(dove->hiddenTime == doctest::Approx(0.0f));
}

TEST_CASE("エマ R（帽子）: 入れ替わった帽子は消えずにエマの元の位置に残る") {
    Fixture f("like_emma");
    f.cast(SkillSlot::W, {500, 0});
    f.run(2.0f);
    REQUIRE(f.findProp(PropKind::Hat) != nullptr);
    CHECK(f.findProp(PropKind::Hat)->remaining < 5.0f);

    f.cast(SkillSlot::R, {500, 0});
    f.run(0.5f);
    CHECK(Vector2Distance(f.player().pos, {500, 0}) < 1.0f);
    const Prop* hat = f.findProp(PropKind::Hat);
    REQUIRE(hat != nullptr);
    CHECK(Vector2Distance(hat->pos, {0, 0}) < 1.0f);
    CHECK(hat->remaining > 5.0f);
}

TEST_CASE("エマ Q の溜めの間に R で帽子へ移動すると、移動先からカードが出て、鳩もそこに残る") {
    Fixture f("like_emma");
    f.cast(SkillSlot::W, {500, 0});
    f.run(1.0f);
    REQUIRE(f.findProp(PropKind::Hat) != nullptr);
    f.resetCooldowns();

    f.cast(SkillSlot::Q, {500, 600});
    f.cast(SkillSlot::R, {500, 0});  // Q の溜めの間に、R で帽子へ
    CHECK(f.player().cast.active);   // Q の溜めは続いている
    CHECK((f.player().cast.request.slot == SkillSlot::Q));
    CHECK(Vector2Distance(f.player().pos, {500, 0}) < 1.0f);  // R ですぐに移動した

    f.run(0.3f);
    const Prop* dove = f.findProp(PropKind::Dove);
    REQUIRE(dove != nullptr);
    CHECK(Vector2Distance(dove->pos, {500, 0}) < 1.0f);  // 鳩は移動先に残る
    const Prop* hat = f.findProp(PropKind::Hat);
    REQUIRE(hat != nullptr);
    CHECK(Vector2Distance(hat->pos, {0, 0}) < 1.0f);  // 帽子はエマの元の位置へ
    REQUIRE(f.world().projectiles().size() == 1);
    const Projectile& card = f.world().projectiles().front();
    CHECK(card.pos.x == doctest::Approx(500.0f).epsilon(0.01));  // 移動先から狙った地点 (500, 600) へ飛んでいる
    CHECK(card.pos.y > 50.0f);
}

TEST_CASE("エマ Q の溜めの間に R で鳩と入れ替わると、鳩のカードは入れ替わる前の鳩の位置から出る") {
    Fixture f("like_emma");
    f.cast(SkillSlot::Q, {500, 0});
    f.run(f.timeToReach(SkillSlot::Q, f.skill(SkillSlot::Q).range) + 0.1f);  // 1 回目のカードが消えるまで待つ
    REQUIRE(f.findProp(PropKind::Dove) != nullptr);                          // (0, 0) に鳩
    f.player().pos = {0, 600};
    f.resetCooldowns();

    f.cast(SkillSlot::Q, {500, 0});
    f.cast(SkillSlot::R, {0, 0});  // Q の溜めの間に、R で鳩と入れ替わる
    CHECK(f.player().cast.active);
    CHECK(Vector2Distance(f.player().pos, {0, 0}) < 1.0f);

    f.run(0.3f);
    // R の鳩の通り道（貫通する弾）を除いた、Q のカード 2 枚
    std::vector<const Projectile*> cards;
    for (const Projectile& p : f.world().projectiles()) {
        if (p.slot == SkillSlot::Q) cards.push_back(&p);
    }
    REQUIRE(cards.size() == 2);
    for (const Projectile* card : cards) {
        // 2 枚とも (0, 0) から (500, 0) へ飛んでいる（エマから 2 枚飛んでいるように見える）
        CHECK(card->pos.x > 50.0f);
        CHECK(card->pos.y == doctest::Approx(0.0f).epsilon(0.01));
    }
    // 重なって 1 枚に見えないよう、鳩のカードだけ見た目をずらす（当たり判定の位置はずらさない）
    const SkillDef& q = f.skill(SkillSlot::Q);
    CHECK(cards[0]->drawOffsetForward == doctest::Approx(0.0f));
    CHECK(cards[1]->drawOffsetForward == doctest::Approx(q.numOr("doveCardDrawOffsetForward", 0.0f)));
    CHECK(cards[1]->drawOffsetSide == doctest::Approx(q.numOr("doveCardDrawOffsetSide", 0.0f)));
}

TEST_CASE("クールダウン「なし」: 使ってもクールダウンが 0 のまま") {
    Fixture f("like_ezreal");
    f.world().setCooldownScale(Team::Player, 0.0f);
    f.cast(SkillSlot::Q, {600, 0});
    f.run(0.3f);
    CHECK(f.cooldown(SkillSlot::Q) == doctest::Approx(0.0f));
}

TEST_CASE("エズリアル E: 追尾弾でも W の印が起爆する") {
    Fixture f("like_ezreal");
    const EntityId target = f.addDummy({700, 0});
    f.cast(SkillSlot::W, {700, 0});
    f.run(0.8f);
    REQUIRE(f.unit(target).status.hasMark("ezreal_w"));
    f.cast(SkillSlot::E, {300, 0});
    f.run(0.6f);
    CHECK_FALSE(f.unit(target).status.hasMark("ezreal_w"));
}

TEST_CASE("エマ D: 範囲に入った敵にスロウがかかり続ける") {
    Fixture f("like_emma");
    const EntityId target = f.addDummy({450, 0});
    f.cast(SkillSlot::D, {450, 0});
    f.run(1.0f);
    CHECK(f.unit(target).status.has(EffectType::Slow));
    CHECK(f.unit(target).moveSpeed() == doctest::Approx(150.0f));
}
