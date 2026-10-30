#pragma once
// キャラ以外のゲーム内の物体: 弾・地面の範囲効果・設置物・演出

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/GameTypes.h"
#include "raylib.h"

namespace moba {

class World;
struct Unit;

// ---------------------------------------------------------------------------
// 弾
// ---------------------------------------------------------------------------

/// 弾の見た目の種類（当たり判定はどれも同じ）
enum class ProjectileVisual { Orb, Ring, Crescent, Card, Beam, Bolt, Dove };
/// 設定ファイルの "visual"（"orb" / "ring" / "crescent" / "card" / "beam" / "bolt" / "dove"）を弾の見た目にする。
/// 知らない表記なら Orb（球）
ProjectileVisual projectileVisualFromString(std::string_view text);

/// 飛んでいる弾 1 つ。まっすぐ飛ぶ弾と、対象を追尾する弾がある
struct Projectile {
    EntityId id = kInvalidId;
    EntityId owner = kInvalidId;
    Team team = Team::Player;
    int castId = 0;
    SkillSlot slot = SkillSlot::Q;
    std::string label;  // ログ・表示用の名前

    Vector2 pos{};
    Vector2 dir{1.0f, 0.0f};
    float speed = 1000.0f;
    float radius = 40.0f;  // 当たり判定の半径（幅の半分）
    float maxDistance = 1000.0f;
    float traveled = 0.0f;
    float age = 0.0f;
    bool pierce = false;                 // true: 当たっても消えずに進む（地形は常に貫通する）
    EntityId homingTarget = kInvalidId;  // 追尾する対象（対象指定スキル）

    ProjectileVisual visual = ProjectileVisual::Orb;
    Color color = WHITE;
    // 見た目だけずらして描く量（当たり判定の位置は pos のまま）。forward は進む方向、side は進む方向の右側（画面上）が正
    float drawOffsetForward = 0.0f;
    float drawOffsetSide = 0.0f;

    std::vector<EntityId> hitUnits;  // 既に当たった相手（同じ相手には 1 回だけ当たる）
    std::function<void(World&, Projectile&, Unit&)> onHit;
    std::function<void(World&, Projectile&)> onExpire;  // 射程の端まで飛んだ時
    bool alive = true;

    /// この相手に既に当たったか（貫通する弾が、同じ相手に 2 回当たらないように）
    bool hasHit(EntityId unit) const { return std::find(hitUnits.begin(), hitUnits.end(), unit) != hitUnits.end(); }
};

// ---------------------------------------------------------------------------
// 地面の範囲効果
// delay 秒後に発動する。duration が 0 なら発動の瞬間だけ判定し、0 より大きければその間ずっと残る
// ---------------------------------------------------------------------------

/// 地面の範囲効果の見た目の種類
enum class AreaVisual {
    Explosion,  // 発動まで範囲の輪だけを表示
    Telegraph,  // 予告円の内側が少しずつ埋まり、埋まりきると爆発（ブランドの W のような表示）
    ThrownHat,  // 帽子が飛んでいく
    Caltrops,   // 鉄菱が飛んでいき、着地後は範囲に残る
};
/// 設定ファイルの "visual"（"explosion" / "telegraph" / "hat" / "caltrops"）を範囲の見た目にする。
/// 知らない表記なら Explosion
AreaVisual areaVisualFromString(std::string_view text);

/// 地面の範囲効果 1 つ。delay 秒後に発動し、duration が 0 なら発動の瞬間だけ、0 より大きければその間ずっと判定する
struct AreaEffect {
    EntityId id = kInvalidId;
    EntityId owner = kInvalidId;
    Team team = Team::Player;
    int castId = 0;
    SkillSlot slot = SkillSlot::Q;
    std::string label;

    Vector2 origin{};  // 投げた位置（飛んでいく演出用）
    Vector2 center{};
    float radius = 100.0f;
    float delay = 0.0f;
    float duration = 0.0f;
    float elapsed = 0.0f;
    bool activated = false;

    AreaVisual visual = AreaVisual::Explosion;
    Color color = WHITE;

    std::vector<EntityId> hitUnits;                       // 当たった相手（範囲に残る効果では「一度でも入った相手」）
    std::function<void(World&, AreaEffect&)> onActivate;  // 発動した瞬間に 1 回
    std::function<void(World&, AreaEffect&, Unit&)> onUnitHit;    // 相手ごとに 1 回
    std::function<void(World&, AreaEffect&, Unit&)> whileInside;  // 範囲に残る効果で、中にいる間ずっと
    bool alive = true;

    /// この相手に既に当たったか（範囲に残る効果では、一度でも入ったことがあるか）
    bool hasHit(EntityId unit) const { return std::find(hitUnits.begin(), hitUnits.end(), unit) != hitUnits.end(); }
};

// ---------------------------------------------------------------------------
// 設置物（エマの鳩・帽子）
// ---------------------------------------------------------------------------

/// 設置物の種類
enum class PropKind { Dove, Hat };
/// 設置物の、ログに出す名前（「鳩」/「帽子」）
std::string_view displayName(PropKind kind);

/// 設置物 1 つ（エマの鳩・帽子）。エマの R の対象になる
struct Prop {
    EntityId id = kInvalidId;
    EntityId owner = kInvalidId;
    PropKind kind = PropKind::Dove;
    Vector2 pos{};
    float lifetime = 0.0f;
    float remaining = 0.0f;
    float hiddenTime = 0.0f;  // 0 より大きい間は表示せず、対象にもできない（飛んで移動している最中など）
    bool alive = true;
};

// ---------------------------------------------------------------------------
// 演出（当たり判定は無い）
// ---------------------------------------------------------------------------

/// 演出の種類
enum class VfxKind { Burst, Ring, Teleport, ClickMarker, Text };

/// 演出 1 つ（当たり判定は無い。残り時間が 0 になったら消える）
struct Vfx {
    VfxKind kind = VfxKind::Burst;
    Vector2 pos{};
    Vector2 pos2{};  // Teleport の移動先
    float radius = 0.0f;
    float duration = 0.3f;
    float remaining = 0.3f;
    Color color = WHITE;
    std::string text;
};

/// 範囲が発動した時の、円が光って消える演出
Vfx makeBurstVfx(Vector2 pos, float radius, Color color, float duration = 0.35f);
/// 輪が広がりながら消える演出（命中・ダウンなど）
Vfx makeRingVfx(Vector2 pos, float radius, Color color, float duration = 0.4f);
/// ワープの演出（移動前と移動後の輪と、その間の線）
Vfx makeTeleportVfx(Vector2 from, Vector2 to, Color color);
/// 右クリックした地点に出る、縮んで消える輪
Vfx makeClickMarkerVfx(Vector2 pos);
/// 文字の演出（「起爆!」「ダウン!」など。少しずつ上へ浮かんで消える。描くのは Hud.cpp）
Vfx makeTextVfx(Vector2 pos, std::string text, Color color);

}  // namespace moba
