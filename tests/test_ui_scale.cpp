#include <doctest/doctest.h>

#include "ui/UiScale.h"

using namespace moba;

TEST_CASE("ウィンドウの大きさ: Windows の拡大率を掛け、画面の 85% に収める") {
    // 拡大率 150%・作業領域 2560x1528: 1600x900 → 2400x1350 は幅が収まらないので、幅 2560 の 85% に縮める
    const ui::WindowSize big = ui::fitWindowSize(1600, 900, 1.5f, 2560, 1528);
    CHECK(big.width == 2176);
    CHECK(big.height == 1224);

    // 拡大率 100%・作業領域 1920x1040: 高さ 900 が 1040 の 85% に収まらないので縮める（docs/dpi-scaling.md の表の例）
    const ui::WindowSize fullHd = ui::fitWindowSize(1600, 900, 1.0f, 1920, 1040);
    CHECK(fullHd.width == 1572);
    CHECK(fullHd.height == 884);

    // 拡大率 100%・十分に広い画面では、そのままの大きさ
    const ui::WindowSize same = ui::fitWindowSize(1280, 720, 1.0f, 2560, 1528);
    CHECK(same.width == 1280);
    CHECK(same.height == 720);

    // 作業領域が取れなかった時は、拡大率を掛けるだけ
    const ui::WindowSize noArea = ui::fitWindowSize(1600, 900, 1.25f, 0, 0);
    CHECK(noArea.width == 2000);
    CHECK(noArea.height == 1125);
}
