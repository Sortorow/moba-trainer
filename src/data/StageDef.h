#pragma once
// ステージの設定（data/stages/*.jsonc）: 広さ・障害物（建物・壁・木など）・ブッシュ

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

namespace moba {

/// 障害物やブッシュの形。長方形か円
struct Shape {
    /// 形の種類（長方形・円）
    enum class Kind { Rect, Circle };
    Kind kind = Kind::Rect;
    Rectangle rect{};  // Rect: 左上の x, y と、幅・高さ
    Vector2 center{};  // Circle: 中心
    float radius = 0.0f;
};

/// 障害物の種類（見た目だけが違う。どれも通れない）
enum class ObstacleKind { Building, Wall, Tree, Crate };

/// 障害物の種類の、設定ファイルでの表記（"building" など）
std::string_view toString(ObstacleKind kind);
/// 設定ファイルの表記（"building" / "wall" / "tree" / "crate"）を障害物の種類にする
/// @return 読めたかどうか（知らない表記なら false で、out は変えない）
bool obstacleKindFromString(std::string_view text, ObstacleKind& out);

/// 通れない場所 1 つ（種類と形）
struct Obstacle {
    ObstacleKind kind = ObstacleKind::Wall;
    Shape shape;
};

/// ステージ 1 つの定義（data/stages/*.jsonc の 1 ファイル）。広さ・障害物・ブッシュ
struct StageDef {
    std::string id;           // "open" / "alley" / "forest"
    std::string displayName;  // "路地裏" など
    std::string description;
    int menuOrder = 1000;             // メニューに並べる順（小さいほど先）
    Rectangle bounds{};               // ステージの広さ（中心が (0, 0)）
    std::vector<Obstacle> obstacles;  // 通れない場所
    std::vector<Shape> bushes;        // ブッシュ（重なっているものは 1 つのブッシュとして扱う）
    std::filesystem::path sourceFile;
};

/// ステージのファイルの読み込み結果（ステージか、読めなかった理由）
struct StageLoadResult {
    std::optional<StageDef> def;
    std::string error;  // def が空の時の理由
};

/// ステージのファイル（data/stages/*.jsonc）を 1 つ読み込む
/// @return 読み込んだステージ。読めない時は def が空で、error に理由（何番目の障害物か など）が入る
StageLoadResult loadStageFile(const std::filesystem::path& file);

}  // namespace moba
