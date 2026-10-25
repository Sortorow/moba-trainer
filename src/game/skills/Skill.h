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

/// 設定ファイルの "handler" の名前から、スキルの処理を探す。無ければ nullptr
SkillHandler findHandler(std::string_view name);
/// その名前の handler があるか（設定ファイルの確認用）
bool handlerExists(std::string_view name);
/// すべての handler の名前
std::vector<std::string_view> handlerNames();

// --- 共通の部品（SkillHelpers.cpp） ---
/// 発動者からカーソルへの向き（カーソルが発動者と重なっていれば、今向いている方向）
Vector2 aimDirection(const SkillContext& ctx);
/// カーソルの位置（射程より遠ければ射程の端）
Vector2 aimPoint(const SkillContext& ctx);
/// 直線の弾を作る（params の speed / width / pierce / visual / color を使う。World にはまだ加えない）
/// @param origin 弾が出る位置
/// @param range 飛ぶ距離（射程の端で消える）
Projectile makeProjectile(const SkillContext& ctx, Vector2 origin, Vector2 dir, float range);
/// 地面の範囲効果の共通部分を作る（発動者・陣営・発動の番号・名前・中心・params の color。省略時はキャラの色）。
/// 半径や発動までの時間は、呼んだ側で決める。World にはまだ加えない
AreaEffect makeArea(const SkillContext& ctx, Vector2 center);
/// 地点が遅れて爆発する範囲（予告円つき）を作る。params の delay / radius / onHit / cooldownRefundOnHit を使う
AreaEffect makeGroundBlast(const SkillContext& ctx, Vector2 center);
/// 効果（スロウ・拘束など）を target に付け、効果の名前の文字演出を出す（印は文字を出さない）
/// @param source 効果を付けたキャラ
void applyEffects(World& world, Unit& target, const std::vector<EffectSpec>& effects, EntityId source);
/// 印が付いていれば消費して起爆の演出を出す
/// @return 起爆したら true（印が無ければ false）
bool detonateMark(World& world, Unit& target, std::string_view markTag, Color color);
/// unit を destination へ瞬間移動させ、ワープの演出を出す。建物などの中なら、近くの立てる位置にする
void teleport(World& world, Unit& unit, Vector2 destination, Color color);
/// スキル slot のクールダウンを、本来のクールダウン（倍率込み）の ratio 分だけ短くする（0.3 = 30%）
void refundCooldown(World& world, Unit& unit, SkillSlot slot, float ratio);

// --- 汎用のスキル（CommonSkills.cpp） ---
/// "projectile": 直線に飛ぶ弾
void handleProjectile(SkillContext& ctx);
/// "homing_projectile": 対象を追尾する弾（必ず当たる）
void handleHomingProjectile(SkillContext& ctx);
/// "blink": 指定方向へ瞬間移動（フラッシュ）
void handleBlink(SkillContext& ctx);
/// "ground_zone": 地点に投げて、一定時間残る範囲（鉄菱など）
void handleGroundZone(SkillContext& ctx);
/// "ground_blast": 地点が遅れて爆発する（予告円つき）
void handleGroundBlast(SkillContext& ctx);
/// "targeted_blast": 対象の敵とその周りに、すぐに範囲効果
void handleTargetedBlast(SkillContext& ctx);

// --- キャラ固有のスキル ---
/// "ezreal_e": ワープ後、近くの敵に追尾弾
void handleEzrealE(SkillContext& ctx);
/// "emma_q": カードを投げて鳩を残す。前に置いた鳩もカードになって飛ぶ
void handleEmmaQ(SkillContext& ctx);
/// "emma_w": 地点が遅れて爆発し、帽子が残る
void handleEmmaW(SkillContext& ctx);
/// "emma_r": 鳩・帽子・兎の位置へ移動し、それぞれの追加効果
void handleEmmaR(SkillContext& ctx);
/// "brand_r": 敵と自分の間を跳ね返る火炎弾
void handleBrandR(SkillContext& ctx);

}  // namespace moba::skills
