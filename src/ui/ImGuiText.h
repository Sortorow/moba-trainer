#pragma once
// Dear ImGui に文字列を渡す時の部品

#include <string>
#include <string_view>

namespace moba::ui {

/// 文字列を、Dear ImGui の書式（printf と同じ形。SliderInt の format など）の中にそのまま出せるようにする。
/// % を %% にする。設定ファイルの名前（賢さの名前など）を書式に入れる時に使う
/// （そのまま入れると、名前の中の % が書式の指定として読まれ、関係の無い値を読みに行ってしまう）
inline std::string escapeFormat(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        escaped += c;
        if (c == '%') escaped += '%';
    }
    return escaped;
}

}  // namespace moba::ui
