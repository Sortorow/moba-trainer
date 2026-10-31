// Dear ImGui に文字列を渡す時の部品（ui/ImGuiText.h）のテスト

#include <doctest/doctest.h>

#include <cstdio>
#include <string>

#include "ui/ImGuiText.h"

using namespace moba;

TEST_CASE("書式に入れる文字列の % は %% になり、書式として使っても元の文字のまま出る") {
    CHECK(ui::escapeFormat("ふつう") == "ふつう");
    CHECK(ui::escapeFormat("100%避ける") == "100%%避ける");
    CHECK(ui::escapeFormat("%s%d") == "%%s%%d");

    // メニューのスライダーと同じ形（"%d（名前）"）で使っても、名前の % が書式として読まれない
    const std::string format = "%d（" + ui::escapeFormat("100%避ける") + "）";
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format.c_str(), 3);
    CHECK(std::string(buffer) == "3（100%避ける）");
}
