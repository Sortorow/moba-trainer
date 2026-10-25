#pragma once
// 入力の取得元（実際の操作 / 入力スクリプト）

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "input/InputFrame.h"
#include "raylib.h"

namespace moba {

/// 入力を取り出す時に渡す情報（カメラ・時刻・UI がマウスやキーを使っているか）
struct InputPollContext {
    const Camera2D& camera;
    float simTime = 0.0f;       // ゲーム内の経過時間（入力スクリプトの時刻と比べる）
    bool uiWantsMouse = false;  // マウスが UI（デバッグパネルなど）の上にある
    bool uiWantsText = false;   // UI で文字入力中
};

/// 入力の取得元（実際の操作 / 入力スクリプト）の共通の形
class InputSource {
public:
    virtual ~InputSource() = default;
    /// 1 フレーム分の入力を取り出す（毎フレーム 1 回呼ぶ）
    virtual InputFrame poll(const InputPollContext& ctx) = 0;
    /// 入力スクリプトか（スクリプトなら、時間を一定の刻みで進め、実際のマウスを使わない）
    virtual bool isScripted() const = 0;
};

/// 実際のマウス・キーボード
class LiveInput final : public InputSource {
public:
    /// 今のマウスとキーボードの状態を読む。UI の上にカーソルがある時の右クリックと、文字入力中のキーは無視する
    InputFrame poll(const InputPollContext& ctx) override;
    /// 実際の操作なので false
    bool isScripted() const override { return false; }
};

/// 入力スクリプト（自動での動作確認用）。書式は ScriptInput.cpp の先頭を参照
class ScriptInput final : public InputSource {
public:
    /// 入力スクリプトのファイルを読み込む（命令は時刻の順に並べ替える）
    /// @param error 読めなかった時の理由（何行目が読めないか）
    /// @return 読み込んだスクリプト。読めなければ nullptr
    static std::unique_ptr<ScriptInput> load(const std::filesystem::path& file, std::string& error);

    /// ctx.simTime までに時刻が来た命令を実行して、1 フレーム分の入力にする
    InputFrame poll(const InputPollContext& ctx) override;
    /// 入力スクリプトなので true
    bool isScripted() const override { return true; }

private:
    /// 入力スクリプトの命令の種類
    enum class Kind { Move, Cast, Cursor, Screenshot, ResetCooldowns, ToggleCamera, Escape, Exit };
    /// 入力スクリプトの命令 1 つ（時刻・種類・キー・座標・文字）
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
