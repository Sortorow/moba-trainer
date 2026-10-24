#pragma once
// キャラクターとスキルの定義（data/characters/*.jsonc）

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/GameTypes.h"
#include "data/EffectSpec.h"
#include "raylib.h"

namespace moba {

// スキルの対象の取り方
enum class Targeting {
    None,       // 対象なし（自己強化など）
    Direction,  // 方向指定（カーソルの方向）
    Point,      // 地点指定（カーソルの位置。射程より遠い場合は射程の端）
    Unit,       // 対象指定（カーソルの位置にいる敵。射程外なら近づいてから発動）
    Prop,       // 自分の設置物などを指定（エマの R。射程外なら近づいてから発動）
};

std::string_view toString(Targeting targeting);
bool targetingFromString(std::string_view text, Targeting& out);

struct SkillDef {
    std::string characterId;
    SkillSlot slot = SkillSlot::Q;
    std::string name;                     // 表示名
    std::string handler;                  // 処理の種類（skills/SkillRegistry.cpp に登録された名前）
    Targeting targeting = Targeting::Direction;
    float castTime = 0.0f;                // 詠唱時間（秒）。この間は移動できない
    float cooldown = 0.0f;                // クールダウン（秒）
    float range = 0.0f;                   // 射程
    bool countsForHitRate = false;        // スキルヒット率の集計に含めるか
    bool movementSkill = false;           // 移動系のスキルか（「移動系だけ使える」ルール WorldRules を有効にした時に使う）
    bool usedByEnemy = false;             // スキルドッジでこのキャラが敵として出た時に使うか
    float aiPriority = 1.0f;              // 敵 AI がスキルを選ぶ時の重み（大きいほど選ばれやすい）
    nlohmann::json params = nlohmann::json::object();  // 処理ごとの個別パラメータ

    // params から値を読む。
    // num は必須項目用で、書かれていない場合は警告ログを 1 回だけ出して fallback を返す。
    // numOr / flagOr / text / color / effects は任意項目用で、書かれていなくても警告しない。
    float num(std::string_view key, float fallback) const;
    float numOr(std::string_view key, float fallback) const;
    bool flagOr(std::string_view key, bool fallback) const;
    std::string text(std::string_view key, std::string_view fallback) const;
    std::vector<std::string> texts(std::string_view key) const;
    Color color(std::string_view key, Color fallback) const;
    std::vector<EffectSpec> effects(std::string_view key) const;
};

struct CharacterDef {
    std::string id;           // "like_ezreal"
    std::string displayName;  // "like ezreal"
    std::string origin;       // "LoL" / "ER"
    float moveSpeed = 325.0f;
    float hitboxRadius = 65.0f;
    Color color{200, 200, 200, 255};
    std::array<std::optional<SkillDef>, kSkillSlotCount> skills;
    std::filesystem::path sourceFile;

    const SkillDef* skill(SkillSlot slot) const;
};

struct CharacterLoadResult {
    std::optional<CharacterDef> def;
    std::string error;  // 空なら成功
};

CharacterLoadResult loadCharacterFile(const std::filesystem::path& file);

// [R, G, B] または [R, G, B, A] の配列を色にする
Color colorFromJson(const nlohmann::json& value, Color fallback);

}  // namespace moba
