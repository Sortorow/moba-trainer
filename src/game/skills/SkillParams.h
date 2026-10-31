#pragma once
// スキルの params のルール（handler ごとに、書ける項目と値の範囲）と、設定ファイルを読み込む時のチェック
//
// ルールは handler の処理のすぐ近く（skills/*.cpp）に書き、SkillRegistry.cpp の対応表で handler と結び付ける。
// 数の項目は「0 より大きい」が基本で、0 に意味がある項目（待ち時間・スロウの強さ・クールダウンの短縮・跳ね返る回数など）
// だけ 0 を許す。見た目のずれは負の値も許す（claude_directive.txt の「設定・調整」、2026/10/31）。
//
// チェックは GameData::loadAll がキャラのファイルを読むたびに行う（App が checkSkillParams を渡す）。
//   ・ルールを満たさない値・必須の項目が無い → エラー。そのキャラのファイルを使わない（前回読めた内容を使い続ける）
//   ・ルールに無い項目名（書き間違いなど）     → 警告。そのまま使う（その項目は使われず、既定値になる）

#include <string_view>
#include <utility>
#include <vector>

#include "data/CharacterDef.h"

namespace moba::skills {

/// params の項目の値の種類
enum class ParamKind {
    Positive,            // 0 より大きい数（速さ・幅・半径・持続時間など。0 だと動きが成り立たないもの）
    NonNegative,         // 0 以上の数（0 に意味があるもの。待ち時間 0 = すぐ、など）
    Ratio,               // 0 以上 1 以下の数（割合。スロウの強さ・クールダウンの短縮など）
    AnyNumber,           // どんな数でもよい（見た目のずれ。負の値も使う）
    PositiveInteger,     // 1 以上の整数
    NonNegativeInteger,  // 0 以上の整数
    Flag,                // true / false
    Text,                // 文字列（印の名前など）
    Choice,              // 決まった文字列のどれか（見た目の名前など）
    ChoiceList,          // 決まった文字列の配列（targetKinds）
    Color,               // [R, G, B] か [R, G, B, A]（各 0〜255 の整数）
    Effects,             // 効果の配列（onHit など。各効果の書き方も確かめる）
};

/// params の項目 1 つのルール
struct ParamRule {
    std::string_view key;
    ParamKind kind = ParamKind::Positive;
    bool required = false;                  // 書いていなければエラー
    std::vector<std::string_view> choices;  // Choice / ChoiceList の時に、書ける文字列
};

/// handler 1 つの、params のルールの一覧
using ParamRules = std::vector<ParamRule>;

/// 必須の項目のルール
inline ParamRule requiredParam(std::string_view key, ParamKind kind) { return ParamRule{key, kind, true, {}}; }
/// 書かなくてもよい項目のルール
inline ParamRule optionalParam(std::string_view key, ParamKind kind) { return ParamRule{key, kind, false, {}}; }
/// 決まった文字列のどれかを書く項目のルール（書かなくてもよい）
/// @param kind Choice（1 つ）か ChoiceList（配列）
inline ParamRule choiceParam(std::string_view key, ParamKind kind, std::vector<std::string_view> choices) {
    return ParamRule{key, kind, false, std::move(choices)};
}

/// ルールの一覧をつなげる（共通の部品のルールに、handler 固有のルールを足す時など）
ParamRules joinRules(ParamRules base, const ParamRules& more);

/// スキルの params を、その handler のルールで確かめる（GameData::setSkillValidator に渡す関数）
/// @return エラー（ルールを満たさない値・必須の項目が無い・handler が存在しない）と、警告（ルールに無い項目名）
SkillCheck checkSkillParams(const SkillDef& def);

}  // namespace moba::skills
