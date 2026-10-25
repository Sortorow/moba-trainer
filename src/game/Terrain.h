#pragma once
// ステージの地形: 通れない場所（建物・壁・木など）との当たりと押し出し、経路探索、ブッシュ
//
// 経路探索は、ステージを kCellSize 四方のマス目に分けた A* で行い、得られた道のりを
// 「まっすぐ進めるところは、まっすぐ進む」ように間引く（キャラの半径ごとに、通れるマス目を作って使い回す）。
// 弾は地形に止められない（壁を通り抜ける）。地形が妨げるのは移動だけ。

#include <map>
#include <vector>

#include "data/StageDef.h"
#include "raylib.h"

namespace moba {

/// ステージの地形（障害物・ブッシュ）。当たりと押し出し、経路探索、ブッシュの判定を行う
class Terrain {
public:
    // 経路探索のマス目の大きさ（ゲーム内単位）
    static constexpr float kCellSize = 40.0f;

    /// 範囲が空の地形（使う前に、範囲を決めたものを代入する）
    Terrain() = default;
    /// 障害物のない地形
    explicit Terrain(Rectangle bounds);
    /// ステージの設定から作る（重なり合うブッシュをまとめる）
    explicit Terrain(const StageDef& stage);

    /// ステージの範囲
    Rectangle bounds() const { return bounds_; }
    /// 通れない場所の一覧（描画用）
    const std::vector<Obstacle>& obstacles() const { return obstacles_; }
    /// ブッシュの一覧（描画用）
    const std::vector<Shape>& bushes() const { return bushes_; }

    /// 中心 pos・半径 radius の円が、障害物かステージの外にかかるか（触れているだけなら、かかっていない）
    bool blocked(Vector2 pos, float radius) const;
    /// 障害物とステージの外から押し出した位置（かかっていなければそのまま）
    Vector2 pushOut(Vector2 pos, float radius) const;
    /// 半径 radius の円が、a から b へまっすぐ進む間に障害物にかからないか
    bool segmentClear(Vector2 a, Vector2 b, float radius) const;
    /// from から to までの道のり（通る点の並び。最後が目的地）。
    /// to に立てない時（建物の中など）は、to に最も近い立てる場所までの道のり。たどり着けなければ空
    std::vector<Vector2> findPath(Vector2 from, Vector2 to, float radius) const;

    /// pos を含むブッシュの番号（重なり合うブッシュは同じ番号）。ブッシュの外なら -1
    int bushAt(Vector2 pos) const;

private:
    /// 半径ごとの「立てるマス目」
    struct Grid {
        int cols = 0;
        int rows = 0;
        std::vector<unsigned char> free;  // cols * rows。1 なら立てる
        /// そのマス目に立てるか（範囲の外なら false）
        bool isFree(int col, int row) const {
            return col >= 0 && row >= 0 && col < cols && row < rows && free[static_cast<std::size_t>(row * cols + col)];
        }
    };

    /// 半径 radius のキャラが立てるマス目（半径ごとに 1 回だけ作って使い回す）
    const Grid& gridFor(float radius) const;
    /// マス目の中心の座標
    Vector2 cellCenter(int col, int row) const;
    /// 座標を含むマス目の番号（範囲の外なら、範囲外の番号になる）
    void cellOf(Vector2 pos, int& col, int& row) const;
    /// (col, row) から近い順に探して、最初に見つかった立てるマス目。無ければ false
    bool nearestFreeCell(const Grid& grid, int col, int row, int& outCol, int& outRow) const;
    /// 重なり合うブッシュに同じ番号を振る
    void groupBushes();

    Rectangle bounds_{};
    std::vector<Obstacle> obstacles_;
    std::vector<Shape> bushes_;
    std::vector<int> bushGroups_;        // bushes_ と同じ並び。重なり合うブッシュは同じ番号
    mutable std::map<int, Grid> grids_;  // 半径（整数に丸めたもの）ごとのマス目。初めて使う時に作る
};

}  // namespace moba
