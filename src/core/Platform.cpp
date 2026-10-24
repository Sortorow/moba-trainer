#include "core/Platform.h"

#include "core/Log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <imm.h>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace moba::platform {
namespace {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int size = static_cast<int>(utf8.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), size, result.data(), length);
    return result;
}

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* record = info ? info->ExceptionRecord : nullptr;
    const unsigned long code = record ? record->ExceptionCode : 0;
    const auto address = reinterpret_cast<std::uintptr_t>(record ? record->ExceptionAddress : nullptr);
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    log::write(log::Level::Fatal, "crash",
               std::format("未処理の例外でクラッシュしました code=0x{:08X} address=0x{:X} exe先頭=0x{:X}", code, address,
                           base));
    log::shutdown();
    return EXCEPTION_CONTINUE_SEARCH;
}

void onTerminate() {
    std::string what = "不明";
    if (std::exception_ptr current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& e) {
            what = e.what();
        } catch (...) {
        }
    }
    log::write(log::Level::Fatal, "crash", std::format("std::terminate が呼ばれました（{}）", what));
    log::shutdown();
    std::abort();
}

}  // namespace

std::filesystem::path executableDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

void installCrashHandler() {
    SetUnhandledExceptionFilter(onUnhandledException);
    std::set_terminate(onTerminate);
}

void disableIme(void* nativeWindowHandle) {
    if (!nativeWindowHandle) return;
    ImmAssociateContext(static_cast<HWND>(nativeWindowHandle), nullptr);
    log::info("platform", "ウィンドウの IME を無効にしました（日本語入力がオンでもキー操作が届くように）");
}

void showErrorDialog(std::string_view titleUtf8, std::string_view messageUtf8) {
    MessageBoxW(nullptr, widen(messageUtf8).c_str(), widen(titleUtf8).c_str(), MB_OK | MB_ICONERROR);
}

}  // namespace moba::platform
