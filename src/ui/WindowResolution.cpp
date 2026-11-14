#include "ui/WindowResolution.h"

#include <cstdint>

namespace moba::ui {

const std::vector<WindowSize>& resolutionList() {
    // 16:9 と 16:10 の代表的なものに、ユーザーの指定した 2160x1440（3:2）と 2400x1350（16:9）を加える
    static const std::vector<WindowSize> kList = {
        {1280, 720},  {1280, 800},  {1440, 900},  {1600, 900},  {1680, 1050}, {1920, 1080},
        {1920, 1200}, {2160, 1440}, {2400, 1350}, {2560, 1440}, {2560, 1600}, {3840, 2160},
    };
    return kList;
}

bool fitsInArea(WindowSize size, WindowSize frame, WindowSize area) {
    if (area.width <= 0 || area.height <= 0) return true;
    return size.width + frame.width <= area.width && size.height + frame.height <= area.height;
}

WindowSize startupWindowSize(WindowSize wanted, WindowSize frame, WindowSize area) {
    if (fitsInArea(wanted, frame, area)) return wanted;
    const std::vector<WindowSize>& list = resolutionList();
    // 縦横比が同じか（割り算の誤差が出ないよう、掛け算で比べる）
    const auto sameAspect = [&](WindowSize size) {
        return static_cast<std::int64_t>(size.width) * wanted.height ==
               static_cast<std::int64_t>(wanted.width) * size.height;
    };
    // 一覧はピクセル数の少ない順なので、後ろから探すと最も大きいものが見つかる
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        if (sameAspect(*it) && fitsInArea(*it, frame, area)) return *it;
    }
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        if (fitsInArea(*it, frame, area)) return *it;
    }
    return list.front();
}

}  // namespace moba::ui
