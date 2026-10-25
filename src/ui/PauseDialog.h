#pragma once
// ポーズ中の確認ダイアログ（スキルドッジ・スキルショットで Esc を押した時）

namespace moba {

/// ポーズ中の確認ダイアログで押されたボタン
enum class PauseAction {
    None,     // まだ押されていない
    Resume,   // いいえ（プレイに戻る）
    Menu,     // はい（ゲームを中断して、メニューに戻る）
    Restart,  // リスタート（同じ設定で最初からやり直す）
};

/// 画面中央に「ゲームを中断して、メニューに戻りますか？」の確認ダイアログを描く（はい・いいえ・リスタート）
/// @return 押されたボタン。押されていなければ None
PauseAction drawPauseDialog();

}  // namespace moba
