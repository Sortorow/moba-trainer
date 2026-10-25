#pragma once
// ステージの設定（data/stages/*.jsonc）: 広さ・障害物（建物・壁・木など）・ブッシュ

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

namespace moba {

// 障害物やブッシュの形。長方形か円
struct Shape {
    enum class Kind { Rect, Circle };
    Kind kind = Kind::Rect;
    Rectangle rect{};  // Rect: 左上の x, y と、幅・高さ
    Vector2 center{};  // Circle: 中心
    float radius = 0.0f;
};

// 障害物の種類（見た目だけが違う。どれも通れない）
enum class ObstacleKind { Building, Wall, Tree, Crate };

std::string_view toString(ObstacleKind kind);
bool obstacleKindFromString(std::string_view text, ObstacleKind& out);

struct Obstacle {
    ObstacleKind kind = ObstacleKind::Wall;
    Shape shape;
};

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

struct StageLoadResult {
    std::optional<StageDef> def;
    std::string error;  // def が空の時の理由
};

StageLoadResult loadStageFile(const std::filesystem::path& file);

}  // namespace moba
