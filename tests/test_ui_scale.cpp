// 文字の倍率（ウィンドウの高さに合わせる）と、解像度の一覧・画面に収まるかの判定のテスト

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

#include "ui/UiScale.h"
#include "ui/WindowResolution.h"

using namespace moba;

TEST_CASE("文字の倍率: ウィンドウの高さ ÷ 900 × uiScale") {
    CHECK(ui::scaleForWindowHeight(900, 1.0f) == doctest::Approx(1.0f));
    CHECK(ui::scaleForWindowHeight(1080, 1.0f) == doctest::Approx(1.2f));
    CHECK(ui::scaleForWindowHeight(720, 1.0f) == doctest::Approx(0.8f));
    CHECK(ui::scaleForWindowHeight(1600, 1.0f) == doctest::Approx(1600.0f / 900.0f));  // 2560x1600 の全画面
    CHECK(ui::scaleForWindowHeight(1080, 0.5f) == doctest::Approx(0.6f));              // uiScale はさらに掛ける
    // 高さが分からない時（0 以下）は uiScale のまま
    CHECK(ui::scaleForWindowHeight(0, 1.25f) == doctest::Approx(1.25f));
}

TEST_CASE("解像度の一覧: ピクセル数の少ない順で、既定の 1920x1080 と、指定された 2160x1440・2400x1350 を含む") {
    const std::vector<ui::WindowSize>& list = ui::resolutionList();
    REQUIRE_FALSE(list.empty());
    for (std::size_t i = 1; i < list.size(); ++i) {
        CHECK(list[i - 1].width * list[i - 1].height < list[i].width * list[i].height);
    }
    CHECK(std::find(list.begin(), list.end(), ui::WindowSize{1920, 1080}) != list.end());
    CHECK(std::find(list.begin(), list.end(), ui::WindowSize{2160, 1440}) != list.end());
    CHECK(std::find(list.begin(), list.end(), ui::WindowSize{2400, 1350}) != list.end());
}

TEST_CASE("画面に収まるか: 中の大きさにタイトルバーと枠を足して、作業領域と比べる") {
    const ui::WindowSize frame{6, 45};      // 枠の左右の合計 6、タイトルバーと枠の上下の合計 45
    const ui::WindowSize area{2560, 1528};  // 2560x1600 のモニターから、タスクバーを除いた範囲
    CHECK(ui::fitsInArea({1920, 1080}, frame, area));
    CHECK(ui::fitsInArea({1920, 1200}, frame, area));
    CHECK(ui::fitsInArea({2160, 1440}, frame, area));
    CHECK(ui::fitsInArea({2400, 1350}, frame, area));
    CHECK_FALSE(ui::fitsInArea({2560, 1440}, frame, area));  // 幅に枠の分が入らない
    CHECK_FALSE(ui::fitsInArea({2560, 1600}, frame, area));
    // ちょうど収まる大きさは収まる。1 ピクセルでもはみ出すと収まらない
    CHECK(ui::fitsInArea({2554, 1483}, frame, area));
    CHECK_FALSE(ui::fitsInArea({2554, 1484}, frame, area));
    // 作業領域が分からない時は、収まるとみなす
    CHECK(ui::fitsInArea({3840, 2160}, frame, {0, 0}));
}

TEST_CASE("起動時の大きさ: 収まればそのまま。収まらなければ、同じ縦横比で収まる最も大きいもの") {
    const ui::WindowSize frame{2, 31};
    const ui::WindowSize fullHd{1920, 1040};  // 1920x1080 のモニターから、タスクバーを除いた範囲

    // 収まる大きさは、一覧に無い大きさでもそのまま
    CHECK(ui::startupWindowSize({1600, 900}, frame, fullHd) == ui::WindowSize{1600, 900});
    CHECK(ui::startupWindowSize({1234, 567}, frame, fullHd) == ui::WindowSize{1234, 567});
    // 16:9 の 1920x1080 は高さが入らない → 16:9 で収まる最も大きい 1600x900
    CHECK(ui::startupWindowSize({1920, 1080}, frame, fullHd) == ui::WindowSize{1600, 900});
    // 16:10 の 2560x1600 → 16:10 で収まる最も大きい 1440x900（ピクセル数がより多い 1600x900 より、縦横比を優先）
    CHECK(ui::startupWindowSize({2560, 1600}, frame, fullHd) == ui::WindowSize{1440, 900});
    // 同じ縦横比が一覧に無ければ、収まる中で最も大きいもの
    CHECK(ui::startupWindowSize({3000, 3000}, frame, fullHd) == ui::WindowSize{1600, 900});
    // 3:2 は一覧に 2160x1440 しか無いので、それが入らなければ、収まる中で最も大きいもの
    CHECK(ui::startupWindowSize({2160, 1440}, frame, fullHd) == ui::WindowSize{1600, 900});
    // 2560x1600 のモニター（タスクバーを除いて 2560x1528）では、16:9 の 2560x1440・3840x2160 → 16:9 で収まる最も大きい
    // 2400x1350
    const ui::WindowSize wqxga{2560, 1528};
    CHECK(ui::startupWindowSize({2560, 1440}, {6, 45}, wqxga) == ui::WindowSize{2400, 1350});
    CHECK(ui::startupWindowSize({3840, 2160}, {6, 45}, wqxga) == ui::WindowSize{2400, 1350});
    // どれも収まらなければ、一覧の最も小さいもの
    CHECK(ui::startupWindowSize({1920, 1080}, frame, {1024, 700}) == ui::resolutionList().front());
}
