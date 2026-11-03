#pragma once
// 見失った自キャラを探す時の、ステージのエリア分けと探索済みの記録（敵 AI が全員で共有する）
//
// エリア: ブッシュ 1 つ（重なり合うブッシュは 1 つ）と、それ以外の地面を areaSize 四方に分けたマス 1 つ。
// 各エリアに pointSpacing おきの「確かめる点」を置き（敵が立てない所には置かない）、
// エリアの中のすべての点が一度でも敵の陣営の視界に入ったら、そのエリアは探索済みになる。
// ブッシュの中の点は、そのブッシュに入らないと見えないので、ブッシュは中に入った時に探索済みになる。
// 1 か所だけで判定しないのは、マスの中の木の陰などに自キャラが隠れられるため。

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "raylib.h"

namespace moba {
class Terrain;
class World;
struct SearchConfig;
}  // namespace moba

namespace moba::ai {

/// 見失った自キャラを探す時の、ステージのエリア分けと探索済みの記録
class SearchMap {
public:
    /// 確かめる点 1 つ
    struct Point {
        Vector2 pos{};
        int area = -1;      // 属するエリアの番号
        bool seen = false;  // 今の周回で、敵の陣営の視界に一度入ったか
    };

    /// 確かめ撃ちの先（エリアと、狙う点）
    struct ProbeTarget {
        int area = -1;
        Vector2 point{};
    };

    /// ステージをエリアに分け、確かめる点を置く（探索済みの記録は消える）
    /// @param unitRadius 敵の半径（この半径の円が立てない所には点を置かない）
    void build(const Terrain& terrain, const SearchConfig& config, float unitRadius);
    /// build 済みか
    bool built() const { return built_; }
    /// 探索済みの記録と確かめ撃ちの記録を消す（見失った時・全エリアを探し終えて、最初から探し直す時）
    void reset();
    /// 今、敵の陣営の視界に入っている点を「見た」にする（全部見たエリアは探索済みになる）
    void markSeen(const World& world);
    /// たどり着けないエリアをあきらめて、探索済みにする
    void giveUp(int area);

    /// エリアの数
    int areaCount() const { return static_cast<int>(areas_.size()); }
    /// 探索済みのエリアの数
    int exploredCount() const;
    /// すべてのエリアが探索済みか
    bool allExplored() const { return exploredCount() == areaCount(); }
    /// エリアが探索済みか
    bool explored(int area) const { return areas_[static_cast<std::size_t>(area)].explored; }
    /// エリアの、まだ見ていない点の数
    int unseenCount(int area) const { return areas_[static_cast<std::size_t>(area)].unseen; }
    /// エリアのブッシュの番号（Terrain::bushAt と同じ番号。地面のエリアなら -1）
    int bushOf(int area) const { return areas_[static_cast<std::size_t>(area)].bush; }
    /// pos に最も近い確かめる点のエリア（点が無ければ -1）
    int areaNear(Vector2 pos) const;
    /// エリアの説明（ログ用。「ブッシュ 3」「地面 (250, -750) 付近」）
    std::string areaName(int area) const;
    /// すべての確かめる点
    const std::vector<Point>& points() const { return points_; }

    /// 次に探すエリアを選ぶ。未探索のエリアのうち、まだ見ていない点の「from からの距離 + lostAt からの距離」が
    /// 最も小さいエリア（見失った位置の近くから、自分に近い順に探す）
    /// @param avoid 他の敵が向かっているエリア。ほかに未探索のエリアがある限り選ばない
    /// @return エリアの番号。未探索のエリアが無ければ -1
    int chooseArea(Vector2 from, Vector2 lostAt, const std::vector<int>& avoid) const;
    /// エリアのまだ見ていない点のうち、from に最も近い点（すべて見ていれば from）
    Vector2 nearestUnseenPoint(int area, Vector2 from) const;

    /// 確かめ撃ちの先を選ぶ。未探索で、この周回でまだ撃っていないエリアのうち、from から range 以内かつ視界の半径以内で、
    /// 今は敵の陣営から見えていない（ブッシュの中か、何かの陰の）点を持つエリアを候補にする。
    /// 候補のうち lostAt に最も近いエリアを選び、狙う点はその中からランダムに選ぶ
    /// @return 撃つ先。候補が無ければ nullopt
    std::optional<ProbeTarget> chooseProbe(const World& world, Vector2 from, float range, Vector2 lostAt,
                                           std::mt19937& rng) const;
    /// エリアに確かめ撃ちをした（この周回では、もう撃たない）
    void markProbed(int area) { areas_[static_cast<std::size_t>(area)].probed = true; }
    /// この周回で、エリアに確かめ撃ちをしたか
    bool probed(int area) const { return areas_[static_cast<std::size_t>(area)].probed; }

private:
    /// エリア 1 つ（ブッシュ 1 つか、地面のマス 1 つ）
    struct Area {
        int bush = -1;                    // ブッシュの番号（地面のマスなら -1）
        Vector2 center{};                 // 地面のマスの中心（ブッシュなら、点の平均。ログ用）
        std::vector<std::size_t> points;  // このエリアの確かめる点（points_ の番号）
        int unseen = 0;                   // まだ見ていない点の数
        bool explored = false;            // 探索済み（すべての点を見た・あきらめた）
        bool probed = false;              // この周回で確かめ撃ちをした
    };

    std::vector<Point> points_;
    std::vector<Area> areas_;
    bool built_ = false;
};

}  // namespace moba::ai
