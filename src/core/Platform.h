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

void showErrorDialog(std::string_view titleUtf8, std::string_view messageUtf8);

}  // namespace moba::platform
