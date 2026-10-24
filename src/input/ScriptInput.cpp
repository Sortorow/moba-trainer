// 入力スクリプト（自動での動作確認用）
//
// 1 行に 1 つの命令を書く。# 以降はコメント。時刻はゲーム開始からの秒数、座標はワールド座標。
//   0.5  move 600 0          右クリックで (600, 0) へ移動
//   1.0  cast Q 1200 0       カーソルを (1200, 0) に置いて Q を押す
//   1.0  cursor 300 300      カーソルだけ動かす
//   2.0  screenshot shot.png 画面を保存する
//   2.5  resetcd             自キャラのクールダウンを解除する
//   3.0  exit                終了する

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>

#include "core/Log.h"
#include "core/Paths.h"
#include "input/InputSource.h"

namespace moba {

std::unique_ptr<ScriptInput> ScriptInput::load(const std::filesystem::path& file, std::string& error) {
    std::ifstream in(file);
    if (!in) {
        error = std::format("入力スクリプトを開けません: {}", toUtf8(file));
        return nullptr;
    }

    auto script = std::unique_ptr<ScriptInput>(new ScriptInput());
    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (const auto comment = line.find('#'); comment != std::string::npos) line.erase(comment);
        std::istringstream words(line);
        Command command;
        std::string kind;
        if (!(words >> command.time >> kind)) continue;  // 空行

        bool ok = true;
        if (kind == "move" || kind == "cursor") {
            command.kind = kind == "move" ? Kind::Move : Kind::Cursor;
            ok = static_cast<bool>(words >> command.point.x >> command.point.y);
        } else if (kind == "cast") {
            command.kind = Kind::Cast;
            std::string slot;
            ok = static_cast<bool>(words >> slot >> command.point.x >> command.point.y);
            const auto it = std::find_if(kAllSkillSlots.begin(), kAllSkillSlots.end(),
                                         [&](SkillSlot s) { return toString(s) == slot; });
            ok = ok && it != kAllSkillSlots.end();
            if (ok) command.slot = *it;
        } else if (kind == "screenshot") {
            command.kind = Kind::Screenshot;
            ok = static_cast<bool>(words >> command.text);
        } else if (kind == "resetcd") {
            command.kind = Kind::ResetCooldowns;
        } else if (kind == "exit") {
            command.kind = Kind::Exit;
        } else {
            ok = false;
        }
        if (!ok) {
            error = std::format("{} の {} 行目を読めません: {}", toUtf8(file.filename()), lineNumber, line);
            return nullptr;
        }
        script->commands_.push_back(command);
    }
    std::stable_sort(script->commands_.begin(), script->commands_.end(),
                     [](const Command& a, const Command& b) { return a.time < b.time; });
    log::info("input", "入力スクリプトを読み込みました: {}（{} 命令）", toUtf8(file), script->commands_.size());
    return script;
}

InputFrame ScriptInput::poll(const InputPollContext& ctx) {
    InputFrame frame;
    while (next_ < commands_.size() && commands_[next_].time <= ctx.simTime) {
        const Command& command = commands_[next_++];
        switch (command.kind) {
            case Kind::Move:
                cursor_ = command.point;
                frame.rightPressed = true;
                frame.rightDown = true;
                break;
            case Kind::Cast:
                cursor_ = command.point;
                frame.skillPressed[static_cast<std::size_t>(index(command.slot))] = true;
                break;
            case Kind::Cursor:
                cursor_ = command.point;
                break;
            case Kind::Screenshot:
                frame.screenshotRequest = command.text;
                break;
            case Kind::ResetCooldowns:
                frame.resetCooldownsRequested = true;
                break;
            case Kind::Exit:
                frame.exitRequested = true;
                break;
        }
    }
    frame.cursorWorld = cursor_;
    return frame;
}

}  // namespace moba
