#pragma once
// Windows 固有の処理（windows.h は raylib と名前がぶつかるため、このファイルの .cpp だけで使う）

#include <filesystem>
#include <string_view>

namespace moba::platform {

std::filesystem::path executableDirectory();

// クラッシュ時（未処理の例外・std::terminate）に、原因をログへ記録する
void installCrashHandler();

// 日本語入力（IME）がオンのままでも Q/W/E/R などのキーがゲームに届くよう、ウィンドウの IME を無効にする
void disableIme(void* nativeWindowHandle);

// ウィンドウがあるモニターの拡大率（Windows の設定「拡大/縮小」。150% なら 1.5）。
// raylib の GetWindowScaleDPI() は FLAG_WINDOW_HIGHDPI を付けないと常に 1.0 を返すため、Windows から直接取得する
float windowDpiScale(void* nativeWindowHandle);

// ウィンドウがあるモニターの作業領域（タスクバーを除いた範囲。実ピクセル）
struct ScreenRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
ScreenRect monitorWorkArea(void* nativeWindowHandle);

void showErrorDialog(std::string_view titleUtf8, std::string_view messageUtf8);

}  // namespace moba::platform
