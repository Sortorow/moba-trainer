#pragma once
// 1 回のプレイの中身（キャラ・弾・範囲効果・設置物・演出）をまとめて持ち、時間を進める

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/GameTypes.h"
#include "game/CastStats.h"
#include "game/Entities.h"
#include "game/Unit.h"
#include "raylib.h"

namespace moba {

class GameData;
struct CharacterDef;
struct Settings;

// モードごとのルール
struct WorldRules {
    bool movementSkillsOnly = false;  // 自キャラは移動系のスキルだけ使える（今はどのモードでも使っていない）
};

class World {
public:
    World(const GameData& data, Rectangle bounds);

    // 固定の時間刻み（秒）で呼ぶ
    void update(float dt);

    // --- 追加 ---
    EntityId addUnit(Unit unit);
    // 弾・範囲・設置物は、更新処理の区切りでまとめて追加される（更新中に配列が変わらないように）
    EntityId spawnProjectile(Projectile projectile);
    EntityId spawnArea(AreaEffect area);
    EntityId spawnProp(Prop prop);
    void addVfx(Vfx vfx);
    int newCastId() { return ++lastCastId_; }

    // --- 検索 ---
    Unit* findUnit(EntityId id);
    const Unit* findUnit(EntityId id) const;
    Prop* findProp(EntityId id);
    Unit* player();
    const Unit* player() const;
    const CharacterDef* characterOf(const Unit& unit) const;
    // point の位置にいる敵（カーソルで対象を選ぶ時用。tolerance だけ判定を広げる）
    Unit* enemyAt(Vector2 point, Team myTeam, float tolerance);
    // from から maxDistance 以内で最も近い敵。preferMark の印が付いた敵がいれば、そちらを優先する
    Unit* nearestEnemy(Vector2 from, Team myTeam, float maxDistance, std::string_view preferMark = {});
    std::vector<Unit*> enemiesInCircle(Vector2 center, float radius, Team myTeam);

    // スキルが当たった時の共通処理（ヒット率・ドッジ率の集計、被弾演出、ログ）
    void registerHit(int castId, SkillSlot slot, std::string_view label, Unit& target);

    // 命中を外から観察する（テスト・集計用）
    using HitObserver = std::function<void(int castId, SkillSlot slot, const Unit& target)>;
    void setHitObserver(HitObserver observer) { hitObserver_ = std::move(observer); }

    // 発動の記録（ドッジ率の集計用）。SkillCasting がスキルの処理の前後に呼ぶ
    void beginCast(int castId, Team team, SkillSlot slot);
    void endCastIfIdle(int castId);  // 弾も範囲も出さなかった発動は、ここで記録を閉じる

    Vector2 clampToBounds(Vector2 point, float radius) const;
    // 設定ファイルの再読み込み後に、キャラの大きさ・移動速度・色を反映する
    void refreshUnitsFromData();

    // クールダウンの倍率（自キャラと敵で別々に持つ。0 ならクールダウンなし）
    void setCooldownScale(Team team, float scale);
    float cooldownScale(Team team) const { return team == Team::Player ? playerCooldownScale_ : enemyCooldownScale_; }
    float cooldownScaleFor(const Unit& unit) const { return cooldownScale(unit.team); }

    void setRules(const WorldRules& rules) { rules_ = rules; }
    const WorldRules& rules() const { return rules_; }

    // --- 参照 ---
    const GameData& data() const { return *data_; }
    const Settings& settings() const;
    Rectangle bounds() const { return bounds_; }
    float time() const { return time_; }
    CastStats& stats() { return stats_; }
    const CastStats& stats() const { return stats_; }
    const DodgeCount& dodge() const { return dodge_; }
    std::vector<Unit>& units() { return units_; }
    const std::vector<Unit>& units() const { return units_; }
    std::vector<Prop>& props() { return props_; }
    const std::vector<Prop>& props() const { return props_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<AreaEffect>& areas() const { return areas_; }
    const std::vector<Vfx>& vfx() const { return vfx_; }

private:
    // 発動 1 回分の記録。出した弾・範囲が残っている間は結果が確定しない
    struct CastRecord {
        Team team = Team::Player;
        SkillSlot slot = SkillSlot::Q;
        int liveEntities = 0;
        bool hitPlayer = false;
    };

    void updateUnit(Unit& unit, float dt);
    void updateProjectiles(float dt);
    void updateHomingProjectile(Projectile& projectile, float dt);
    void hitWithProjectile(Projectile& projectile, Unit& target);
    void expireProjectile(Projectile& projectile);
    void updateAreas(float dt);
    void hitWithArea(AreaEffect& area, Unit& target);
    void updateProps(float dt);
    void updateVfx(float dt);
    void flushSpawns();
    void removeDead();
    void releaseCastEntity(int castId);
    void finishCastRecord(int castId);
    void damagePlayer(Unit& player);

    const GameData* data_;
    Rectangle bounds_;
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
