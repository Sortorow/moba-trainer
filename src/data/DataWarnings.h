#pragma once
// 設定ファイルの値を「使う時」に見つかった誤り（スキルの params の書き忘れ・効果の書き方の誤り）を集める。
//
// params は handler がスキルを使う時に読むので、設定ファイルを読み込んだ時点では誤りが分からない。
// 見つかった誤りをここに集め、GameData::errors() に含めて、メニューとデバッグパネルに赤字で出す。
// 設定ファイルを読み直すと（GameData::loadAll）記録を消すので、直していなければ、次に使った時にもう一度記録される。

#include <string>
#include <string_view>
#include <vector>

namespace moba {

/// 設定ファイルの値を使う時に見つかった誤りを記録する。同じ key の誤りは、記録を消すまで 1 回だけ記録する
/// @param key 同じ誤りかを見分ける文字列（「like_emma.Q.speed」など）
/// @param message 画面に出す説明
/// @return 初めての誤りなら true（呼んだ側は、その時だけ警告ログを出す）
bool recordDataWarning(std::string_view key, std::string message);

/// 記録した誤りの説明の一覧（記録した順）
const std::vector<std::string>& dataWarnings();

/// 記録をすべて消す（設定ファイルを読み直す時に、GameData::loadAll が呼ぶ）
void clearDataWarnings();

}  // namespace moba
