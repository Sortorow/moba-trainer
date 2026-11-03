#pragma once
// 視界: 視線が通るかの判定と、視界の形（多角形）の計算
//
// 視線を遮るのは、建物・壁・木（箱は低い物なので遮らない）と、見る側が入っていないブッシュ。
// 見る側が入っているブッシュ（重なり合うブッシュはまとめて 1 つ）は遮らない。
// 描画とは切り離してあり、描き方（2D・3D）が変わってもそのまま使える。

#include <vector>

#include "raylib.h"

namespace moba {

class Terrain;

namespace vision {

/// 視界の形を作る光線 1 本（見る位置から dir の向きに、distance だけ見える）
struct VisionRay {
    Vector2 dir{};          // 長さ 1 の向き
    float distance = 0.0f;  // 遮る物に当たるまでの距離（当たらなければ視界の半径）
};

/// from から to まで視線が通るか（途中か to の上に、視線を遮る物があれば通らない）。距離は考えない
/// @param ignoreBush 遮らないブッシュの番号（Terrain::bushAt の値。見る側が入っているブッシュ）。-1 なら無し
bool lineOfSight(const Terrain& terrain, Vector2 from, Vector2 to, int ignoreBush);

/// origin から見える範囲の形。光線を角度の順（+X 方向から、画面上の時計回り）に並べたもの。
/// 光線は 1° ごとに加えて、遮る物の角と、円の接線の向き（とその両隣）にも出すので、多角形の頂点は角や接線に合う
/// @param ignoreBush 遮らないブッシュの番号（lineOfSight と同じ）
std::vector<VisionRay> visionPolygon(const Terrain& terrain, Vector2 origin, float radius, int ignoreBush);

}  // namespace vision
}  // namespace moba
