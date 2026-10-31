#pragma once
// Windows 固有の処理（windows.h は raylib と名前がぶつかるため、このファイルの .cpp だけで使う）

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace moba::platform {

/// 実行中の exe があるフォルダ
std::filesystem::path executableDirectory();

/// コマンドライン引数（exe の名前を除く）を UTF-8 の文字列で返す。取得できなければ空（ERROR のログを出す）。
/// main の argv は日本語の Windows では Shift_JIS に変換された文字列で、Shift_JIS で書けない文字（「𠮷」や韓国語など）は
/// 「?」に置き換わってしまうため、Windows から UTF-16 のまま受け取って UTF-8 にする
std::vector<std::string> commandLineArgs();

/// クラッシュ時（未処理の例外・std::terminate）に、原因をログへ記録する仕組みを入れる。起動時に 1 回呼ぶ
void installCrashHandler();

/// 日本語入力（IME）がオンのままでも Q/W/E/R などのキーがゲームに届くよう、ウィンドウの IME を無効にする
/// @param nativeWindowHandle ウィンドウのハンドル（raylib の GetWindowHandle()）
void disableIme(void* nativeWindowHandle);

/// ウィンドウがあるモニターの拡大率（Windows の設定「拡大/縮小」。150% なら 1.5）。
/// raylib の GetWindowScaleDPI() は FLAG_WINDOW_HIGHDPI を付けないと常に 1.0 を返すため、Windows から直接取得する
float windowDpiScale(void* nativeWindowHandle);

/// ウィンドウがあるモニターの作業領域（タスクバーを除いた範囲。実ピクセル）
struct ScreenRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
/// ウィンドウがあるモニターの作業領域（タスクバーを除いた範囲。実ピクセル）。取得できなければ大きさ 0
ScreenRect monitorWorkArea(void* nativeWindowHandle);

/// カーソルが動ける範囲を、ウィンドウの中（タイトルバーと枠を除いた部分）に制限する。
/// ウィンドウの移動・大きさの変更に追従するため、制限したい間は毎フレーム呼ぶ
void confineCursorToWindow(void* nativeWindowHandle);
/// カーソルの制限を解除する
void releaseCursor();

/// エラーのダイアログを出す（ウィンドウを開く前の致命的なエラー用。閉じるまで待つ）
void showErrorDialog(std::string_view titleUtf8, std::string_view messageUtf8);

}  // namespace moba::platform
