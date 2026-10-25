#include "data/StageDef.h"

#include <algorithm>
#include <format>

#include "core/JsonFile.h"
#include "core/Paths.h"

namespace moba {
namespace {

using nlohmann::json;

// { "rect": [左, 上, 幅, 高さ] } か { "circle": [x, y, 半径] } を読む
std::optional<Shape> parseShape(const json& j, std::string& error) {
    if (const auto it = j.find("rect"); it != j.end()) {
        if (!it->is_array() || it->size() != 4) {
            error = "\"rect\" は [左, 上, 幅, 高さ] の 4 つの数で書いてください";
            return std::nullopt;
        }
        Shape shape;
        shape.kind = Shape::Kind::Rect;
        shape.rect = Rectangle{(*it)[0].get<float>(), (*it)[1].get<float>(), std::max(1.0f, (*it)[2].get<float>()),
                               std::max(1.0f, (*it)[3].get<float>())};
        return shape;
    }
    if (const auto it = j.find("circle"); it != j.end()) {
        if (!it->is_array() || it->size() != 3) {
            error = "\"circle\" は [x, y, 半径] の 3 つの数で書いてください";
            return std::nullopt;
        }
        Shape shape;
        shape.kind = Shape::Kind::Circle;
        shape.center = Vector2{(*it)[0].get<float>(), (*it)[1].get<float>()};
        shape.radius = std::max(1.0f, (*it)[2].get<float>());
        return shape;
    }
    error = "\"rect\" か \"circle\" が必要です";
    return std::nullopt;
}

}  // namespace

std::string_view toString(ObstacleKind kind) {
    switch (kind) {
        case ObstacleKind::Building: return "building";
        case ObstacleKind::Wall: return "wall";
        case ObstacleKind::Tree: return "tree";
        case ObstacleKind::Crate: return "crate";
    }
    return "?";
}

bool obstacleKindFromString(std::string_view text, ObstacleKind& out) {
    for (ObstacleKind kind : {ObstacleKind::Building, ObstacleKind::Wall, ObstacleKind::Tree, ObstacleKind::Crate}) {
        if (toString(kind) == text) {
            out = kind;
            return true;
        }
    }
    return false;
}

StageLoadResult loadStageFile(const std::filesystem::path& file) {
    StageLoadResult result;
    const JsonLoadResult loaded = loadJsonFile(file);
    if (!loaded.ok()) {
        result.error = loaded.error;
        return result;
    }

    const json& root = loaded.value;
    const std::string fileName = toUtf8(file.filename());
    try {
        StageDef def;
        def.sourceFile = file;
        def.id = root.at("id").get<std::string>();
        def.displayName = root.value("displayName", def.id);
        def.description = root.value("description", std::string{});
        def.menuOrder = root.value("menuOrder", def.menuOrder);

        const json& size = root.at("size");
        if (!size.is_array() || size.size() != 2) {
            result.error = std::format("{}: \"size\" は [幅, 高さ] で書いてください", fileName);
            return result;
        }
        const float width = std::max(1000.0f, size[0].get<float>());
        const float height = std::max(1000.0f, size[1].get<float>());
        def.bounds = Rectangle{-width * 0.5f, -height * 0.5f, width, height};

        for (const json& item : root.value("obstacles", json::array())) {
            std::string error;
            const std::optional<Shape> shape = parseShape(item, error);
            if (!shape) {
                result.error = std::format("{}: obstacles の {} 番目: {}", fileName, def.obstacles.size() + 1, error);
                return result;
            }
            Obstacle obstacle;
            obstacle.shape = *shape;
            const std::string kind = item.value("kind", std::string("wall"));
            if (!obstacleKindFromString(kind, obstacle.kind)) {
                result.error = std::format("{}: obstacles の {} 番目: \"kind\": \"{}\" は不明です", fileName,
                                           def.obstacles.size() + 1, kind);
                return result;
            }
            def.obstacles.push_back(obstacle);
        }
        for (const json& item : root.value("bushes", json::array())) {
            std::string error;
            const std::optional<Shape> shape = parseShape(item, error);
            if (!shape) {
                result.error = std::format("{}: bushes の {} 番目: {}", fileName, def.bushes.size() + 1, error);
                return result;
            }
            def.bushes.push_back(*shape);
        }
        result.def = std::move(def);
    } catch (const json::exception& e) {
        result.error = std::format("{}: {}", fileName, e.what());
    }
    return result;
}

}  // namespace moba
