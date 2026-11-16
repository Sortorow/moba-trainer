// ゲームロジックのテスト（画面を出さずに World を動かして確かめる）

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "TempDataCopy.h"
#include "core/Paths.h"
#include "data/GameData.h"
#include "game/Sandbox.h"
#include "game/SkillCasting.h"
#include "game/World.h"
#include "game/skills/Skill.h"
#include "raymath.h"

using namespace moba;

namespace {

/// スキルのテスト用の準備。障害物のない World に自キャラを置き、ダミーを足して動かす
class Fixture {
public:
    /// data フォルダの設定を読み込み、障害物のない World に、指定したキャラの自キャラを (0, 0) に置く
    /// @param paths 読む data フォルダ（省略時はソースの data。設定を書き換えて試す時は、一時フォルダに写したもの）
    explicit Fixture(std::string_view characterId, const AppPaths& paths = makeAppPaths(MOBA_SOURCE_DIR))
        : data_(paths) {
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
    /// 読み込んだ設定ファイル
    GameData& data() { return data_; }
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

TEST_CASE("硬直: 硬直のあるスキル（エズリアル Q）は詠唱の間止まり、硬直のないスキル（エマ W）は歩き続けられる") {
    {
        Fixture f("like_ezreal");
        REQUIRE(f.skill(SkillSlot::Q).lockMovement);
        casting::orderMove(f.world(), f.player(), {0, 1000});
        f.run(0.2f);
        f.cast(SkillSlot::Q, {600, 0});
        const Vector2 before = f.player().pos;
        f.run(f.skill(SkillSlot::Q).castTime * 0.8f);  // 詠唱の途中
        CHECK(f.player().cast.active);
        CHECK(Vector2Distance(f.player().pos, before) < 1.0f);
    }
    {
        Fixture f("like_emma");
        REQUIRE_FALSE(f.skill(SkillSlot::W).lockMovement);
        REQUIRE(f.skill(SkillSlot::W).castTime > 0.0f);
        casting::orderMove(f.world(), f.player(), {0, 1000});
        f.run(0.2f);
        f.cast(SkillSlot::W, {600, 0});
        const Vector2 before = f.player().pos;
        const float during = f.skill(SkillSlot::W).castTime * 0.8f;  // 溜めの途中
        f.run(during);
        CHECK(f.player().cast.active);
        CHECK(Vector2Distance(f.player().pos, before) == doctest::Approx(f.player().moveSpeed() * during).epsilon(0.1));
        f.run(1.0f);
        CHECK(f.findProp(PropKind::Hat) != nullptr);  // 溜めの後に、狙った地点に帽子が出る
    }
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

TEST_CASE("エマ Q: 貫通するカードにしても、クールダウンの短縮は 1 回の発動で 1 回だけ（W と同じ）") {
    // 今の設定ではカードは貫通しないので、一時フォルダに写した設定で Q のカードを貫通にする
    const test::TempDataCopy copy;
    REQUIRE(test::TempDataCopy::rewrite(copy.paths().charactersDir / "like_emma.jsonc",
                                        std::regex(R"re(("visual"\s*:\s*"card",))re"), "$1 \"pierce\": true,") == 1);
    Fixture f("like_emma", copy.paths());
    REQUIRE(f.skill(SkillSlot::Q).flagOr("pierce", false));
    const float refund = f.skill(SkillSlot::Q).numOr("cooldownRefundOnHit", 0.0f);
    REQUIRE(refund > 0.0f);

    int hits = 0;
    f.world().setHitObserver([&](int, SkillSlot, const Unit&) { ++hits; });
    f.addDummy({400, 0});
    f.addDummy({700, 0});
    f.cast(SkillSlot::Q, {900, 0});
    const float elapsed = f.timeToReach(SkillSlot::Q, 700.0f) + 0.1f;  // 2 体目に当たった後まで待つ
    f.run(elapsed);

    CHECK(hits == 2);  // 貫通して 2 体に当たった
    // クールダウン - 経過時間 - 短縮分 1 回（2 回短縮されていれば、さらに cooldown × refund だけ短い）
    const float cooldown = f.skill(SkillSlot::Q).cooldown;
    CHECK(f.cooldown(SkillSlot::Q) == doctest::Approx(cooldown - elapsed - cooldown * refund).epsilon(0.02));
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

TEST_CASE("エマ R: R の前の移動指示は捨て、R の後は右クリックするまで移動先にとどまる") {
    Fixture f("like_emma");
    f.cast(SkillSlot::W, {500, 0});
    f.run(1.0f);
    REQUIRE(f.findProp(PropKind::Hat) != nullptr);

    casting::orderMove(f.world(), f.player(), {0, 1000});
    f.run(0.2f);
    f.cast(SkillSlot::R, {500, 0});
    CHECK_FALSE(f.player().moveTarget.has_value());
    f.run(1.0f);
    CHECK(Vector2Distance(f.player().pos, {500, 0}) < 1.0f);  // R の前の移動先へは向かわない

    casting::orderMove(f.world(), f.player(), {500, 500});  // R の後の右クリックには従う
    f.run(0.5f);
    CHECK(f.player().pos.y > 50.0f);
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

namespace {

constexpr float kTick = 1.0f / 120.0f;  // ゲームと同じ 1 刻み

/// 鉄菱（エマの D）のテストに使う値（数値を調整してもテストが壊れないよう、設定ファイルから読む）
struct CaltropsValues {
    float radius = 0.0f;       ///< 鉄菱をまく円の半径
    float duration = 0.0f;     ///< 着地してから消えるまで
    float pieceRadius = 0.0f;  ///< 鉄菱 1 つの当たり判定の半径
    float slowAmount = 0.0f;   ///< 踏んだ時のスロウの強さ
    float slowTime = 0.0f;     ///< 踏んだ時のスロウの長さ
};

/// エマの D の設定ファイルの値
CaltropsValues caltropsValues(Fixture& f) {
    const SkillDef& d = f.skill(SkillSlot::D);
    CaltropsValues values;
    values.radius = d.numOr("radius", 0.0f);
    values.duration = d.numOr("duration", 0.0f);
    values.pieceRadius = d.numOr("pieceRadius", 0.0f);
    const std::vector<EffectSpec> onStep = d.effects("onStep");
    REQUIRE(onStep.size() == 1);
    REQUIRE((onStep[0].type == EffectType::Slow));
    values.slowAmount = onStep[0].amount;
    values.slowTime = onStep[0].duration;
    return values;
}

/// 設定ファイルの rings から、円の中心から見た鉄菱の位置を計算する（ゲームの処理とは別に、仕様どおりの位置を出す）
std::vector<Vector2> expectedCaltropOffsets(Fixture& f) {
    const SkillDef& d = f.skill(SkillSlot::D);
    const float radius = d.numOr("radius", 0.0f);
    std::vector<Vector2> offsets;
    for (const nlohmann::json& ring : d.params.at("rings")) {
        const int count = ring.at("count").get<int>();
        const float distance = radius * ring.at("radiusRatio").get<float>();
        for (int i = 0; i < count; ++i) {
            const float degrees =
                ring.value("angle", 0.0f) + 360.0f * static_cast<float>(i) / static_cast<float>(count);
            offsets.push_back(Vector2Rotate({distance, 0.0f}, degrees * DEG2RAD));
        }
    }
    return offsets;
}

/// 最後に投げた鉄菱の範囲（消えていないもの）。無ければ nullptr
const AreaEffect* latestCaltrops(Fixture& f) {
    const AreaEffect* latest = nullptr;
    for (const AreaEffect& area : f.world().areas()) {
        if (area.alive && area.visual == AreaVisual::Caltrops && (!latest || area.id > latest->id)) latest = &area;
    }
    return latest;
}

/// 最後に投げた鉄菱の、残っている数
std::size_t remainingCaltrops(Fixture& f) {
    const AreaEffect* area = latestCaltrops(f);
    REQUIRE(area != nullptr);
    REQUIRE(area->stepTriggers.has_value());
    return area->stepTriggers->positions.size();
}

/// 鉄菱を center に投げ、詠唱が終わって鉄菱が飛んでいる間（まだ着地していない）まで進める
/// @return 鉄菱の位置（投げた時に決まる）
std::vector<Vector2> throwCaltrops(Fixture& f, Vector2 center) {
    f.cast(SkillSlot::D, center);
    f.run(f.skill(SkillSlot::D).castTime + 0.05f);
    const AreaEffect* area = latestCaltrops(f);
    REQUIRE(area != nullptr);
    REQUIRE_FALSE(area->activated);
    REQUIRE(area->stepTriggers.has_value());
    return area->stepTriggers->positions;
}

/// 鉄菱が着地するまで 1 刻みずつ進める（着地した刻みの直後で止まる）
void runUntilLanded(Fixture& f) {
    for (int i = 0; i < 240; ++i) {
        const AreaEffect* area = latestCaltrops(f);
        REQUIRE(area != nullptr);
        if (area->activated) return;
        f.world().update(kTick);
    }
    FAIL("鉄菱が着地しない");
}

/// pos にいる半径 unitRadius のキャラが触れている鉄菱の数
int touchingCount(const std::vector<Vector2>& pieces, Vector2 pos, float unitRadius, float pieceRadius) {
    return static_cast<int>(std::count_if(pieces.begin(), pieces.end(), [&](Vector2 piece) {
        return Vector2Distance(piece, pos) <= unitRadius + pieceRadius;
    }));
}

/// 外側の鉄菱 piece にだけ、円の外から触れる位置（円の中心から見て piece の向こう側。少し重ねる）
Vector2 touchingFromOutside(Vector2 center, Vector2 piece, float unitRadius, float pieceRadius) {
    return piece + Vector2Normalize(piece - center) * (unitRadius + pieceRadius - 5.0f);
}

/// スロウが効いているか（鉄菱のスロウの強さで）
bool slowedBy(Fixture& f, EntityId id, const CaltropsValues& values) {
    return std::abs(f.unit(id).status.moveSpeedMultiplier() - (1.0f - values.slowAmount)) < 0.001f;
}

}  // namespace

TEST_CASE("エマ D: 着地すると rings のとおりに鉄菱が並び、投げた向きでは並びが回らない") {
    Fixture f("like_emma");
    const std::vector<Vector2> expected = expectedCaltropOffsets(f);
    CHECK(expected.size() == 12);                                       // 外側の輪に 8 個、内側の輪に 4 個（ER の動画）
    for (const Vector2 center : {Vector2{450, 0}, Vector2{0, -450}}) {  // 右へ投げる・上へ投げる
        f.resetCooldowns();
        const std::vector<Vector2> pieces = throwCaltrops(f, center);
        REQUIRE(pieces.size() == expected.size());
        for (std::size_t i = 0; i < pieces.size(); ++i) {
            CAPTURE(i);
            CAPTURE(center.y);
            CHECK(Vector2Distance(pieces[i] - center, expected[i]) < 0.01f);
        }
    }
}

TEST_CASE("エマ D: 鉄菱を 1 つ踏むとスロウが付いてその鉄菱は消え、その場にとどまるとスロウは onStep の長さで切れる") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{450, 0};
    const EntityId id = f.addDummy({450, 900});  // 円の外
    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    runUntilLanded(f);
    CHECK(remainingCaltrops(f) == pieces.size());

    const Vector2 spot = touchingFromOutside(center, pieces[0], f.unit(id).radius, values.pieceRadius);
    REQUIRE(touchingCount(pieces, spot, f.unit(id).radius, values.pieceRadius) == 1);
    f.unit(id).pos = spot;
    f.world().update(kTick);
    CHECK(slowedBy(f, id, values));
    CHECK(remainingCaltrops(f) == pieces.size() - 1);
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 1);

    f.run(values.slowTime - 0.1f);
    CHECK(slowedBy(f, id, values));
    f.run(0.2f);
    CHECK(f.unit(id).status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 踏んだ鉄菱は消えたので、踏み直さない
    CHECK(remainingCaltrops(f) == pieces.size() - 1);
}

TEST_CASE("エマ D: 続けて踏むと、最後に踏んだ時から onStep の長さだけスロウが続く（0.4 秒あけて 2 つ踏む）") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{450, 0};
    const EntityId id = f.addDummy({450, 900});
    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    runUntilLanded(f);
    const float radius = f.unit(id).radius;

    // 1 つ目を踏み、円から離れる向きに歩く（スロウが移動に効いている）
    f.unit(id).pos = touchingFromOutside(center, pieces[0], radius, values.pieceRadius);
    f.world().update(kTick);
    REQUIRE(slowedBy(f, id, values));
    const Vector2 before = f.unit(id).pos;
    casting::orderMove(f.world(), f.unit(id), before + Vector2Normalize(pieces[0] - center) * 1000.0f);
    f.run(0.4f);
    CHECK(Vector2Distance(before, f.unit(id).pos) ==
          doctest::Approx(f.unit(id).baseMoveSpeed * (1.0f - values.slowAmount) * 0.4f).epsilon(0.02));

    // 0.4 秒後に、隣の鉄菱を踏む（その場で止まる）
    const Vector2 second = touchingFromOutside(center, pieces[1], radius, values.pieceRadius);
    REQUIRE(touchingCount(latestCaltrops(f)->stepTriggers->positions, second, radius, values.pieceRadius) == 1);
    f.unit(id).pos = second;
    casting::orderMove(f.world(), f.unit(id), second);
    f.world().update(kTick);
    CHECK(remainingCaltrops(f) == pieces.size() - 2);

    // 1 つ目から数えると onStep の長さより 0.3 秒長くたっているが、2 つ目を踏んだ時から数え直している
    f.run(values.slowTime - 0.1f);
    CHECK(slowedBy(f, id, values));
    f.run(0.2f);
    CHECK(f.unit(id).status.moveSpeedMultiplier() == doctest::Approx(1.0f));
}

TEST_CASE("エマ D: 同時に 2 つの鉄菱に触れると 2 つとも消え、とどまっても次の刻みからは踏まない") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{450, 0};
    const EntityId id = f.addDummy({450, 900});
    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    runUntilLanded(f);

    const Vector2 between = (pieces[0] + pieces[1]) * 0.5f;  // 外側の輪の、隣り合う 2 つの間
    REQUIRE(touchingCount(pieces, between, f.unit(id).radius, values.pieceRadius) == 2);
    f.unit(id).pos = between;
    f.world().update(kTick);
    CHECK(remainingCaltrops(f) == pieces.size() - 2);  // 1 つだけ消すと、残りに触れたままになる
    CHECK(slowedBy(f, id, values));
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 1);

    f.run(values.slowTime - 0.1f);
    CHECK(remainingCaltrops(f) == pieces.size() - 2);
    CHECK(slowedBy(f, id, values));
    f.run(0.2f);
    CHECK(f.unit(id).status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 踏み直していない
}

TEST_CASE("エマ D: 着地した時に鉄菱の上に立っていれば、着地した刻みで踏む") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{450, 0};
    const EntityId id = f.addDummy(center);  // 円の真ん中（内側の輪の鉄菱に触れる）
    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    const int touching = touchingCount(pieces, center, f.unit(id).radius, values.pieceRadius);
    REQUIRE(touching > 0);
    CHECK(f.unit(id).status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // まだ着地していない

    runUntilLanded(f);
    CHECK(remainingCaltrops(f) == pieces.size() - static_cast<std::size_t>(touching));
    CHECK(slowedBy(f, id, values));
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 1);
}

TEST_CASE("エマ D: ワープで鉄菱の上に来ても踏む") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const EntityId id = f.addDummy({450, 900});
    const std::vector<Vector2> pieces = throwCaltrops(f, {450, 0});
    runUntilLanded(f);
    REQUIRE(touchingCount(pieces, pieces[0], f.unit(id).radius, values.pieceRadius) == 1);

    skills::teleport(f.world(), f.unit(id), pieces[0], WHITE);
    f.world().update(kTick);
    CHECK(remainingCaltrops(f) == pieces.size() - 1);
    CHECK(slowedBy(f, id, values));
}

TEST_CASE("エマ D: 円の中でも、鉄菱の無い所にいれば踏まない（スロウも命中も無い）") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{450, 0};

    // 円の中で、どの鉄菱にも触れない所を探す（ダミーを小さくして、鉄菱の間に入れる）
    constexpr float kSmallRadius = 30.0f;
    const std::vector<Vector2> offsets = expectedCaltropOffsets(f);
    std::optional<Vector2> gap;
    for (float y = -values.radius; y <= values.radius && !gap; y += 5.0f) {
        for (float x = -values.radius; x <= values.radius && !gap; x += 5.0f) {
            const Vector2 p{x, y};
            if (Vector2Length(p) <= values.radius - kSmallRadius &&
                touchingCount(offsets, p, kSmallRadius, values.pieceRadius) == 0)
                gap = p;
        }
    }
    REQUIRE(gap.has_value());
    const EntityId id = f.addDummy(center + *gap);
    f.unit(id).radius = kSmallRadius;

    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    runUntilLanded(f);
    f.run(1.0f);
    CHECK(remainingCaltrops(f) == pieces.size());
    CHECK(f.unit(id).status.moveSpeedMultiplier() == doctest::Approx(1.0f));
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 0);
}

TEST_CASE("エマ D: 命中は 1 回の発動につき 1 回だけ数える（3 つ踏んでも 1。2 回投げて、どちらでも踏めば 2）") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const EntityId id = f.addDummy({0, 900});
    const float radius = f.unit(id).radius;

    // 外側の鉄菱を、1 つおきに count 個踏む（1 つずつ、円の外から触れる）
    const auto stepOn = [&](Vector2 center, const std::vector<Vector2>& pieces, int count) {
        for (int i = 0; i < count; ++i) {
            f.unit(id).pos =
                touchingFromOutside(center, pieces[static_cast<std::size_t>(i * 2)], radius, values.pieceRadius);
            f.world().update(kTick);
        }
    };

    const Vector2 first{450, 0};
    std::vector<Vector2> pieces = throwCaltrops(f, first);
    runUntilLanded(f);
    stepOn(first, pieces, 3);
    CHECK(remainingCaltrops(f) == pieces.size() - 3);
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 1);

    f.resetCooldowns();
    f.unit(id).pos = {0, 900};
    const Vector2 second{-450, 0};
    pieces = throwCaltrops(f, second);
    runUntilLanded(f);
    stepOn(second, pieces, 2);
    CHECK(remainingCaltrops(f) == pieces.size() - 2);
    CHECK(f.world().stats().slot(SkillSlot::D).casts == 2);
    CHECK(f.world().stats().slot(SkillSlot::D).hits == 2);
}

TEST_CASE("エマ D: エマ自身が鉄菱の上を歩いても、鉄菱は消えず、スロウも付かない") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const Vector2 center{300, 0};
    const std::vector<Vector2> pieces = throwCaltrops(f, center);
    runUntilLanded(f);
    // 真横に歩いて、円の真ん中を通り抜ける（敵なら踏む鉄菱がある道）
    REQUIRE(std::any_of(pieces.begin(), pieces.end(),
                        [&](Vector2 piece) { return std::abs(piece.y) <= f.player().radius + values.pieceRadius; }));
    casting::orderMove(f.world(), f.player(), {700, 0});
    f.run(2.5f);
    REQUIRE(f.player().pos.x == doctest::Approx(700.0f));
    CHECK(remainingCaltrops(f) == pieces.size());
    CHECK(f.player().status.moveSpeedMultiplier() == doctest::Approx(1.0f));
}

TEST_CASE("エマ D: 鉄菱は着地から duration で消え、全部踏まれても円はそれまで残る") {
    Fixture f("like_emma");
    const CaltropsValues values = caltropsValues(f);
    const EntityId id = f.addDummy({450, 900});
    const std::vector<Vector2> pieces = throwCaltrops(f, {450, 0});
    runUntilLanded(f);
    for (const Vector2 piece : pieces) {  // 1 つずつ上にワープして、全部踏む
        f.unit(id).pos = piece;
        f.world().update(kTick);
    }
    CHECK(remainingCaltrops(f) == 0);

    f.unit(id).pos = {450, 900};
    f.run(values.duration - 0.2f);  // 着地から duration - 0.1 秒
    CHECK(latestCaltrops(f) != nullptr);
    f.run(0.3f);  // 着地から duration + 0.2 秒
    CHECK(latestCaltrops(f) == nullptr);
}

TEST_CASE("踏むと発動する物: 触れている物をすべて消し、消した数を返す") {
    StepTriggers triggers;
    triggers.radius = 10.0f;
    triggers.positions = {{0, 0}, {30, 0}, {100, 0}};
    // 左の 2 つにちょうど接している（円どうしは、ちょうど接している時も触れているとする。geo::circlesOverlap）
    CHECK(triggers.consumeTouching({15, 0}, 5.0f) == 2);
    REQUIRE(triggers.positions.size() == 1);
    CHECK(triggers.positions[0].x == doctest::Approx(100.0f));
    CHECK(triggers.consumeTouching({15, 0}, 5.0f) == 0);  // 消えた物には、もう触れない
    CHECK(triggers.positions.size() == 1);
}

TEST_CASE("踏むと発動する物: 鉄菱以外の範囲にも付けられ、World が触れた物だけ消して onStep を呼ぶ") {
    Fixture f("like_emma");
    const EntityId id = f.addDummy({0, 600});  // 範囲から離れた所
    struct Step {
        EntityId unit;
        bool firstStep;
    };
    std::vector<Step> steps;

    AreaEffect area;  // スキルを使わずに置く、見た目が既定（Explosion）の残る範囲
    area.owner = f.player().id;
    area.team = Team::Player;
    area.label = "test";
    area.center = {500, 0};
    area.radius = 200.0f;
    area.duration = 5.0f;
    StepTriggers triggers;
    triggers.radius = 10.0f;
    triggers.positions = {{400, 0}, {600, 0}};
    triggers.onStep = [&steps](World&, Unit& unit, bool firstStep) { steps.push_back({unit.id, firstStep}); };
    area.stepTriggers = std::move(triggers);
    const EntityId areaId = f.world().spawnArea(std::move(area));
    const auto spawned = [&]() -> const AreaEffect& {
        const std::vector<AreaEffect>& areas = f.world().areas();
        const auto it = std::find_if(areas.begin(), areas.end(), [&](const AreaEffect& a) { return a.id == areaId; });
        REQUIRE(it != areas.end());
        return *it;
    };

    f.world().update(kTick);  // 発動する（誰も触れていない）
    REQUIRE(spawned().activated);
    f.unit(id).pos = {500, 0};  // 円の中でも、物に触れていなければ踏まない
    f.world().update(kTick);
    CHECK(steps.empty());
    CHECK(spawned().hitUnits.empty());

    f.unit(id).pos = {400, 70};  // 左の物にだけ触れる（半径 65 + 10 の内側）
    f.world().update(kTick);
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].unit == id);
    CHECK(steps[0].firstStep);
    CHECK(spawned().stepTriggers->positions.size() == 1);
    CHECK(spawned().hasHit(id));  // 初めて踏んだので、範囲の命中として記録される
    f.world().update(kTick);      // とどまっても、消えた物はもう踏まない
    CHECK(steps.size() == 1);

    f.unit(id).pos = {600, 0};
    f.world().update(kTick);
    REQUIRE(steps.size() == 2);
    CHECK_FALSE(steps[1].firstStep);
    CHECK(spawned().stepTriggers->positions.empty());
    CHECK(spawned().hitUnits.size() == 1);  // 命中は 1 回だけ
}

TEST_CASE("発動の依頼の結果: 詠唱を始めた・予約した・使えなかった・すぐ発動した") {
    Fixture f("like_ezreal");
    REQUIRE(f.skill(SkillSlot::Q).castTime > 0.0f);
    REQUIRE(f.skill(SkillSlot::D).castTime == 0.0f);
    auto request = [&](SkillSlot slot, Vector2 cursor) {
        return casting::requestCast(f.world(), f.player(), CastRequest{slot, cursor});
    };

    CHECK(request(SkillSlot::Q, {600, 0}) == casting::CastOutcome::CastStarted);
    CHECK(request(SkillSlot::W, {600, 0}) == casting::CastOutcome::Buffered);  // Q の詠唱中
    CHECK(request(SkillSlot::Q, {600, 0}) == casting::CastOutcome::Rejected);  // Q はクールダウン中
    f.run(f.skill(SkillSlot::Q).castTime + f.skill(SkillSlot::W).castTime + 0.1f);
    CHECK(request(SkillSlot::D, {300, 0}) == casting::CastOutcome::Fired);     // 詠唱時間 0 のフラッシュ
    CHECK(request(SkillSlot::F, {300, 0}) == casting::CastOutcome::Rejected);  // F にはスキルが無い
}

TEST_CASE("発動の依頼の結果: 対象指定のスキルは、対象がいなければ使えず、射程外なら近づき始める") {
    Fixture f("like_emma");
    const float range = f.skill(SkillSlot::E).range;
    const EntityId target = f.addDummy({range + 300.0f, 0});  // 射程の外で、視界（900）の中
    auto request = [&](Vector2 cursor) {
        return casting::requestCast(f.world(), f.player(), CastRequest{SkillSlot::E, cursor});
    };

    CHECK(request({-(range + 300.0f), 0}) == casting::CastOutcome::Rejected);  // カーソルの位置に敵がいない
    CHECK(request(f.unit(target).pos) == casting::CastOutcome::Approaching);
}

TEST_CASE("設定ファイルのエラー: スキルを使った時に見つかった誤りも一覧に 1 回だけ出て、読み直すとまた出る") {
    // エズリアルの W から、必須の params.speed を消し、効果の種類を知らない名前にする
    const test::TempDataCopy copy;
    const std::filesystem::path file = copy.paths().charactersDir / "like_ezreal.jsonc";
    REQUIRE(test::TempDataCopy::rewrite(file, std::regex(R"re("speed": 1700,\s*)re"), "") == 1);
    REQUIRE(test::TempDataCopy::rewrite(file, std::regex(R"re("type": "mark", "tag": "ezreal_w")re"),
                                        R"("type": "unknown_mark", "tag": "ezreal_w")") == 1);
    Fixture f("like_ezreal", copy.paths());
    REQUIRE(f.data().errors().empty());  // 読み込んだ時点では分からない

    // 一覧の中で、text を含む説明の数
    auto count = [&](std::string_view text) {
        const std::vector<std::string> errors = f.data().errors();
        return std::count_if(errors.begin(), errors.end(),
                             [&](const std::string& error) { return error.find(text) != std::string::npos; });
    };
    auto castW = [&] {
        f.resetCooldowns();
        f.cast(SkillSlot::W, {600, 0});
        f.run(f.skill(SkillSlot::W).castTime + 0.1f);
    };

    castW();
    castW();  // 2 回使っても、同じ誤りは 1 回だけ
    CHECK(count("params.speed") == 1);
    CHECK(count("unknown_mark") == 1);

    f.data().loadAll();  // 直さずに読み直すと、一度消えて
    CHECK(count("params.speed") == 0);
    castW();  // また使うと、もう一度出る
    CHECK(count("params.speed") == 1);
    CHECK(count("unknown_mark") == 1);
}
