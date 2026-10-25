#pragma once
// 1 回のプレイの中身（キャラ・弾・範囲効果・設置物・演出）をまとめて持ち、時間を進める

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/GameTypes.h"
#include "game/CastStats.h"
#include "game/Entities.h"
#include "game/Terrain.h"
#include "game/Unit.h"
#include "raylib.h"

namespace moba {

class GameData;
struct CharacterDef;
struct Settings;

/// モードごとのルール
struct WorldRules {
    bool movementSkillsOnly = false;  // 自キャラは移動系のスキルだけ使える（今はどのモードでも使っていない）
};

/// 1 回のプレイの中身（キャラ・弾・範囲効果・設置物・演出・地形）をまとめて持ち、時間を進める
class World {
public:
    /// 障害物のないステージで作る（自動テスト用）
    World(const GameData& data, Rectangle bounds);
    /// ステージの地形で作る
    World(const GameData& data, Terrain terrain);

    /// ゲームを dt 秒進める（キャラの移動と詠唱 → 弾 → 範囲 → 設置物 → 演出）。固定の時間刻み（秒）で呼ぶ
    void update(float dt);

    // --- 追加 ---

    /// キャラを加える（通し番号を振る）
    /// @return 振った通し番号
    EntityId addUnit(Unit unit);
    /// キャラを取り除く（練習場のダミーを置き直す時用）
    void removeUnit(EntityId id);
    // 弾・範囲・設置物は、更新処理の区切りでまとめて追加される（更新中に配列が変わらないように）

    /// 弾を加える（更新の区切りで実際に加わる）。発動の記録に「まだ飛んでいる物」として数える
    EntityId spawnProjectile(Projectile projectile);
    /// 地面の範囲効果を加える（更新の区切りで実際に加わる）。発動の記録に「まだ残っている物」として数える
    EntityId spawnArea(AreaEffect area);
    /// 設置物（鳩・帽子）を加える（更新の区切りで実際に加わる）
    EntityId spawnProp(Prop prop);
    /// 演出を加える（当たり判定は無い）
    void addVfx(Vfx vfx);
    /// 新しい発動の番号（1 回の発動で出た弾・範囲をまとめて集計するための番号）
    int newCastId() { return ++lastCastId_; }

    // --- 検索 ---

    /// 通し番号でキャラを探す。見つからなければ nullptr
    Unit* findUnit(EntityId id);
    /// 通し番号でキャラを探す。見つからなければ nullptr
    const Unit* findUnit(EntityId id) const;
    /// 通し番号で設置物を探す。見つからなければ nullptr
    Prop* findProp(EntityId id);
    /// 自キャラ。いなければ nullptr
    Unit* player();
    /// 自キャラ。いなければ nullptr
    const Unit* player() const;
    /// キャラの設定（自キャラ・敵キャラ）。ダミーには設定が無いので nullptr
    const CharacterDef* characterOf(const Unit& unit) const;
    /// point の位置にいる敵（カーソルで対象を選ぶ時用。tolerance だけ判定を広げる）。myTeam から見えない敵は選ばない
    Unit* enemyAt(Vector2 point, Team myTeam, float tolerance);
    /// from から maxDistance 以内で最も近い敵。preferMark の印が付いた敵がいれば、そちらを優先する。見えない敵は選ばない
    Unit* nearestEnemy(Vector2 from, Team myTeam, float maxDistance, std::string_view preferMark = {});
    /// 円の範囲に当たり判定がかかっている敵（myTeam ではないキャラ）。範囲の命中判定用なので、見えない敵も含む
    std::vector<Unit*> enemiesInCircle(Vector2 center, float radius, Team myTeam);

    // --- ブッシュと見え方 ---

    /// observer の陣営から target が見えるか。ブッシュの中のキャラは、同じブッシュに observer の陣営のキャラがいるか、
    /// 対象指定のスキルを使って見えている間（revealTime）だけ見える
    bool canSee(Team observer, const Unit& target) const;
    /// ブッシュの中で対象指定のスキルを使った時に呼ぶ（bushRevealTime 秒だけ外から見えるようにする）。ブッシュの外なら何もしない
    void revealFromBush(Unit& unit);

    /// スキルが当たった時の共通処理（ヒット率・ドッジ率の集計、被弾の演出、自キャラの HP、ログ）
    /// @param label ログに出す弾・範囲の名前
    void registerHit(int castId, SkillSlot slot, std::string_view label, Unit& target);

    // 命中を外から観察する（テスト・集計用）
    using HitObserver = std::function<void(int castId, SkillSlot slot, const Unit& target)>;
    /// 命中のたびに呼ばれる関数を登録する（自動テストで命中を数える時用）
    void setHitObserver(HitObserver observer) { hitObserver_ = std::move(observer); }

    // 発動の記録（ドッジ率の集計用）。SkillCasting がスキルの処理の前後に呼ぶ

    /// 発動の記録を始める（この後に出す弾・範囲がすべて消えた時に、当たったかどうかが確定する）
    void beginCast(int castId, Team team, SkillSlot slot);
    /// 弾も範囲も出さなかった発動（ワープなど）は、ここで記録を閉じる
    void endCastIfIdle(int castId);

    /// point をステージの範囲の内側（半径 radius の円がはみ出さない位置）に収める。障害物は考えない
    Vector2 clampToBounds(Vector2 point, float radius) const;
    /// point の近くで、半径 radius のキャラが立てる位置（障害物とステージの外から押し出す）
    Vector2 placeable(Vector2 point, float radius) const { return terrain_.pushOut(point, radius); }
    /// 設定ファイルの再読み込み後に、キャラの大きさ・移動速度・色・名前を反映する（ダミーは対象外）
    void refreshUnitsFromData();

    // クールダウンの倍率（自キャラと敵で別々に持つ。0 ならクールダウンなし）

    /// 陣営ごとのクールダウンの倍率を決める（0 ならクールダウンなし。マイナスは 0 にする）
    void setCooldownScale(Team team, float scale);
    /// 陣営ごとのクールダウンの倍率
    float cooldownScale(Team team) const { return team == Team::Player ? playerCooldownScale_ : enemyCooldownScale_; }
    /// そのキャラの陣営のクールダウンの倍率
    float cooldownScaleFor(const Unit& unit) const { return cooldownScale(unit.team); }

    /// モードごとのルールを決める
    void setRules(const WorldRules& rules) { rules_ = rules; }
    /// モードごとのルール
    const WorldRules& rules() const { return rules_; }

    // --- 参照 ---

    /// 設定ファイル一式
    const GameData& data() const { return *data_; }
    /// settings.jsonc の内容
    const Settings& settings() const;
    /// ステージの範囲
    Rectangle bounds() const { return terrain_.bounds(); }
    /// ステージの地形（障害物・ブッシュ・経路探索）
    const Terrain& terrain() const { return terrain_; }
    /// 始めてからの経過時間（秒）
    float time() const { return time_; }
    /// 自キャラのスキルヒット率の集計
    CastStats& stats() { return stats_; }
    /// 自キャラのスキルヒット率の集計
    const CastStats& stats() const { return stats_; }
    /// スキルドッジ率の集計（敵の発動ごとの、当たった・避けた）
    const DodgeCount& dodge() const { return dodge_; }
    /// すべてのキャラ（自キャラ・敵・ダミー）
    std::vector<Unit>& units() { return units_; }
    /// すべてのキャラ（自キャラ・敵・ダミー）
    const std::vector<Unit>& units() const { return units_; }
    /// すべての設置物（鳩・帽子）
    std::vector<Prop>& props() { return props_; }
    /// すべての設置物（鳩・帽子）
    const std::vector<Prop>& props() const { return props_; }
    /// 飛んでいるすべての弾
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    /// 地面のすべての範囲効果
    const std::vector<AreaEffect>& areas() const { return areas_; }
    /// すべての演出
    const std::vector<Vfx>& vfx() const { return vfx_; }

private:
    /// 発動 1 回分の記録。出した弾・範囲が残っている間は結果が確定しない
    struct CastRecord {
        Team team = Team::Player;
        SkillSlot slot = SkillSlot::Q;
        int liveEntities = 0;
        bool hitPlayer = false;
    };

    /// キャラ 1 体を進める（状態異常・クールダウン・詠唱・移動。最後に障害物から押し出す）
    void updateUnit(Unit& unit, float dt);
    /// 目的地（moveTarget）へ、壁を回り込む道のり（path）に沿って進む。必要なら道のりを作り直す
    void moveAlongPath(Unit& unit, float dt);
    /// すべての弾を進め、通り道で触れた敵に当てる（貫通しない弾は、最も手前の敵だけ）
    void updateProjectiles(float dt);
    /// 追尾する弾を、対象へ向けて進める。対象がいなくなったら消える
    void updateHomingProjectile(Projectile& projectile, float dt);
    /// 弾が target に当たった時の処理（命中の記録と、その弾の onHit）
    void hitWithProjectile(Projectile& projectile, Unit& target);
    /// 弾が射程の端まで飛んだ時の処理（onExpire を呼んで消す）
    void expireProjectile(Projectile& projectile);
    /// すべての範囲効果を進め、発動したら範囲の敵に当てる
    void updateAreas(float dt);
    /// 範囲が target に当たった時の処理（命中の記録と、その範囲の onUnitHit）
    void hitWithArea(AreaEffect& area, Unit& target);
    /// 設置物の残り時間を減らし、時間切れなら消す
    void updateProps(float dt);
    /// 演出の残り時間を減らす
    void updateVfx(float dt);
    /// 更新の途中で出た弾・範囲・設置物を、本体の配列へ移す
    void flushSpawns();
    /// 消えた弾・範囲・設置物・演出を取り除く（弾・範囲は発動の記録からも外す）
    void removeDead();
    /// 発動で出た弾・範囲が 1 つ消えたことを記録する。すべて消えたら発動の結果を確定する
    void releaseCastEntity(int castId);
    /// 発動の結果（当たった・避けた）を確定し、敵の発動ならドッジ率に数える
    void finishCastRecord(int castId);
    /// 自キャラの HP を 1 減らす。0 になったらダウン数を増やして全回復する
    void damagePlayer(Unit& player);

    const GameData* data_;
    Terrain terrain_;
    float time_ = 0.0f;
    float playerCooldownScale_ = 1.0f;
    float enemyCooldownScale_ = 1.0f;
    WorldRules rules_;
    EntityId lastEntityId_ = kInvalidId;
    int lastCastId_ = 0;
    CastStats stats_;
    DodgeCount dodge_;
    std::unordered_map<int, CastRecord> casts_;
    HitObserver hitObserver_;

    std::vector<Unit> units_;
    std::vector<Projectile> projectiles_;
    std::vector<Projectile> pendingProjectiles_;
    std::vector<AreaEffect> areas_;
    std::vector<AreaEffect> pendingAreas_;
    std::vector<Prop> props_;
    std::vector<Prop> pendingProps_;
    std::vector<Vfx> vfx_;
};

}  // namespace moba
