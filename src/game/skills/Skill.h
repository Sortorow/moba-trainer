#pragma once
// スキルの処理（handler）
//
// 設定ファイルの "handler" に書いた名前で、どの処理を使うかが決まる（一覧は SkillRegistry.cpp）。
// 数値はすべて設定ファイルの params から読むので、調整はファイルの編集だけで済む。

#include <string_view>
#include <vector>

#include "data/CharacterDef.h"
#include "game/Entities.h"
#include "game/Unit.h"
#include "raylib.h"

namespace moba {
class World;
}

namespace moba::skills {

// スキルの処理に渡す情報
struct SkillContext {
    World& world;
    Unit& caster;
    const SkillDef& def;
    const CastRequest& request;
    int castId;
};

using SkillHandler = void (*)(SkillContext&);

SkillHandler findHandler(std::string_view name);
bool handlerExists(std::string_view name);
std::vector<std::string_view> handlerNames();

// --- 共通の部品（SkillHelpers.cpp） ---
Vector2 aimDirection(const SkillContext& ctx);  // 発動者からカーソルへの向き
Vector2 aimPoint(const SkillContext& ctx);      // カーソル位置（射程より遠ければ射程の端）
// 直線の弾を作る（params の speed / width / pierce / visual / color を使う）
Projectile makeProjectile(const SkillContext& ctx, Vector2 origin, Vector2 dir, float range);
AreaEffect makeArea(const SkillContext& ctx, Vector2 center);
// 地点が遅れて爆発する範囲（予告円つき）。params の delay / radius / onHit / cooldownRefundOnHit を使う
AreaEffect makeGroundBlast(const SkillContext& ctx, Vector2 center);
void applyEffects(World& world, Unit& target, const std::vector<EffectSpec>& effects, EntityId source);
// 印が付いていれば消費して起爆の演出を出す。起爆したら true
bool detonateMark(World& world, Unit& target, std::string_view markTag, Color color);
void teleport(World& world, Unit& unit, Vector2 destination, Color color);
// スキル slot のクールダウンを、本来のクールダウン（倍率込み）の ratio 分だけ短くする（0.3 = 30%）
void refundCooldown(World& world, Unit& unit, SkillSlot slot, float ratio);

// --- 汎用のスキル（CommonSkills.cpp） ---
void handleProjectile(SkillContext& ctx);        // "projectile": 直線に飛ぶ弾
void handleHomingProjectile(SkillContext& ctx);  // "homing_projectile": 対象を追尾する弾（必ず当たる）
void handleBlink(SkillContext& ctx);             // "blink": 指定方向へ瞬間移動（フラッシュ）
void handleGroundZone(SkillContext& ctx);        // "ground_zone": 地点に投げて、一定時間残る範囲
void handleGroundBlast(SkillContext& ctx);       // "ground_blast": 地点が遅れて爆発する（予告円つき）
void handleTargetedBlast(SkillContext& ctx);     // "targeted_blast": 対象の敵とその周りに、すぐに範囲効果

// --- キャラ固有のスキル ---
void handleEzrealE(SkillContext& ctx);  // "ezreal_e": ワープ後、近くの敵に追尾弾
void handleEmmaQ(SkillContext& ctx);    // "emma_q": カードを投げて鳩を残す。鳩もカードになる
void handleEmmaW(SkillContext& ctx);    // "emma_w": 地点が遅れて爆発し、帽子が残る
void handleEmmaR(SkillContext& ctx);    // "emma_r": 鳩・帽子・兎と入れ替わり、それぞれの追加効果
void handleBrandR(SkillContext& ctx);   // "brand_r": 敵と自分の間を跳ね返る火炎弾

}  // namespace moba::skills
