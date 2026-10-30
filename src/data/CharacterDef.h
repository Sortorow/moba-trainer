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

/// スキルの対象の取り方
enum class Targeting {
    None,       // 対象なし（自己強化など）
    Direction,  // 方向指定（カーソルの方向）
    Point,      // 地点指定（カーソルの位置。射程より遠い場合は射程の端）
    Unit,       // 対象指定（カーソルの位置にいる敵。射程外なら近づいてから発動）
    Prop,       // 自分の設置物などを指定（エマの R。射程外なら近づいてから発動）
};

/// 対象の取り方の、設定ファイルでの表記（"direction" など）
std::string_view toString(Targeting targeting);
/// 設定ファイルの表記（"direction" / "point" / "unit" / "prop" / "none"）を対象の取り方にする
/// @return 読めたかどうか（知らない表記なら false で、out は変えない）
bool targetingFromString(std::string_view text, Targeting& out);

/// スキル 1 つの定義（設定ファイルの "Q": { ... } の中身）。共通の項目と、handler ごとの params
struct SkillDef {
    std::string characterId;
    SkillSlot slot = SkillSlot::Q;
    std::string name;     // 表示名
    std::string handler;  // 処理の種類（skills/SkillRegistry.cpp に登録された名前）
    Targeting targeting = Targeting::Direction;
    float castTime = 0.0f;          // 詠唱時間（秒）。キーを押してからスキルが出るまで
    bool lockMovement = true;       // 硬直: true なら詠唱時間の間は移動できない。false なら詠唱中も移動できる
    float cooldown = 0.0f;          // クールダウン（秒）
    float range = 0.0f;             // 射程
    bool countsForHitRate = false;  // スキルヒット率の集計に含めるか
    bool movementSkill = false;     // 移動系のスキルか（「移動系だけ使える」ルール WorldRules を有効にした時に使う）
    bool usedByEnemy = false;       // スキルドッジでこのキャラが敵として出た時に使うか
    float aiPriority = 1.0f;        // 敵 AI がスキルを選ぶ時の重み（大きいほど選ばれやすい）
    // このスキルの詠唱中でも、詠唱を中断せずにすぐ使えるスキル（詠唱時間 0 のものだけ。エマの Q の溜めの間の R など）
    std::vector<SkillSlot> usableDuringCast;
    nlohmann::json params = nlohmann::json::object();  // 処理ごとの個別パラメータ

    // params から値を読む関数。
    // num は必須項目用で、書かれていない場合は警告を 1 回だけ出して fallback を返す
    // （警告はログと、設定ファイルのエラーの一覧（DataWarnings.h）に出る。設定ファイルを読み直すと、また 1 回出る）。
    // numOr / flagOr / text / color / effects は任意項目用で、書かれていなくても警告しない。

    /// params の数値（必須項目用）。書かれていなければ警告を 1 回だけ出して（ログと設定ファイルのエラーの一覧）fallback を返す
    float num(std::string_view key, float fallback) const;
    /// params の数値（任意項目用）。書かれていなければ警告せずに fallback を返す
    float numOr(std::string_view key, float fallback) const;
    /// params の true / false（任意項目用）。書かれていなければ fallback
    bool flagOr(std::string_view key, bool fallback) const;
    /// params の文字列（任意項目用）。書かれていなければ fallback
    std::string text(std::string_view key, std::string_view fallback) const;
    /// params の文字列の配列（例: ["dove", "hat"]）。書かれていなければ空
    std::vector<std::string> texts(std::string_view key) const;
    /// params の色（[R, G, B] または [R, G, B, A]）。書かれていなければ fallback
    Color color(std::string_view key, Color fallback) const;
    /// params の効果の配列（"onHit" など）。書かれていなければ空。おかしな要素は警告ログを出して飛ばす
    std::vector<EffectSpec> effects(std::string_view key) const;
};

/// キャラ 1 体の定義（data/characters/*.jsonc の 1 ファイル）。ステータスと Q〜F のスキル
struct CharacterDef {
    std::string id;           // "like_ezreal"
    std::string displayName;  // "like ezreal"
    std::string origin;       // "LoL" / "ER"
    int menuOrder = 1000;     // メニューなどに並べる順（小さいほど先。同じなら id の順）
    float moveSpeed = 325.0f;
    float hitboxRadius = 65.0f;
    Color color{200, 200, 200, 255};
    std::array<std::optional<SkillDef>, kSkillSlotCount> skills;
    std::filesystem::path sourceFile;

    /// そのキーのスキル。設定ファイルで null にしたキー（スキルが無い）なら nullptr
    const SkillDef* skill(SkillSlot slot) const;
};

/// キャラのファイルの読み込み結果（キャラか、読めなかった理由）
struct CharacterLoadResult {
    std::optional<CharacterDef> def;
    std::string error;  // 空なら成功
};

/// キャラのファイル（data/characters/*.jsonc）を 1 つ読み込む
/// @return 読み込んだキャラ。読めない時は def が空で、error に理由（ファイル名・スキルのキーなど）が入る
CharacterLoadResult loadCharacterFile(const std::filesystem::path& file);

/// [R, G, B] または [R, G, B, A] の配列（各 0〜255）を色にする。配列でなければ fallback
Color colorFromJson(const nlohmann::json& value, Color fallback);

}  // namespace moba
