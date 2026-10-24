#pragma once
// 入力の取得元（実際の操作 / 入力スクリプト）

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "input/InputFrame.h"
#include "raylib.h"

namespace moba {

struct InputPollContext {
    const Camera2D& camera;
    float simTime = 0.0f;        // ゲーム内の経過時間（入力スクリプトの時刻と比べる）
    bool uiWantsMouse = false;   // マウスが UI（デバッグパネルなど）の上にある
    bool uiWantsText = false;    // UI で文字入力中
};

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual InputFrame poll(const InputPollContext& ctx) = 0;
    virtual bool isScripted() const = 0;
};

// 実際のマウス・キーボード
class LiveInput final : public InputSource {
public:
    InputFrame poll(const InputPollContext& ctx) override;
    bool isScripted() const override { return false; }
};

// 入力スクリプト（自動での動作確認用）。書式は ScriptInput.cpp の先頭を参照
class ScriptInput final : public InputSource {
public:
    static std::unique_ptr<ScriptInput> load(const std::filesystem::path& file, std::string& error);

    InputFrame poll(const InputPollContext& ctx) override;
    bool isScripted() const override { return true; }

private:
    enum class Kind { Move, Cast, Cursor, Screenshot, ResetCooldowns, Exit };
    struct Command {
        float time = 0.0f;
        Kind kind = Kind::Move;
        SkillSlot slot = SkillSlot::Q;
        Vector2 point{};
        std::string text;
    };

    std::vector<Command> commands_;
    std::size_t next_ = 0;
    Vector2 cursor_{};
};

}  // namespace moba
