#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Geometry.h"
#include "data/Settings.h"
#include "game/World.h"
#include "rlgl.h"

namespace moba {
namespace {

constexpr Color kGroundColor{34, 52, 42, 255};
constexpr Color kGridMinor{255, 255, 255, 10};
constexpr Color kGridMajor{255, 255, 255, 28};
constexpr Color kHitboxColor{255, 255, 255, 150};

/// 色 a から色 b へ、t（0〜1）の割合で混ぜた色
Color lerpColor(Color a, Color b, float t) {
    auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

/// 頂点の並び順に関係なく三角形を描く（raylib は反時計回りの三角形しか描かないため、両方の順で描く）
void drawTriangleBothSides(Vector2 a, Vector2 b, Vector2 c, Color color) {
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

/// 小さな鳥（鳩）の形。dir の方向を向く
void drawBird(Vector2 pos, Vector2 dir, float size, Color color) {
    const Vector2 side = geo::perpendicular(dir);
    drawTriangleBothSides(pos + dir * size, pos + side * size * 0.9f - dir * size * 0.5f,
                          pos - side * size * 0.9f - dir * size * 0.5f, color);
    DrawCircleV(pos + dir * size * 0.3f, size * 0.35f, color);
}

/// 帽子（エマの W・R）の形を描く
void drawHat(Vector2 pos, float size) {
    DrawCircleV(pos, size * 1.25f, Color{20, 20, 28, 255});
    DrawCircleV(pos, size * 0.8f, Color{45, 45, 60, 255});
    DrawRing(pos, size * 1.25f, size * 1.45f, 0.0f, 360.0f, 32, Color{90, 230, 230, 255});
}

/// 鉄菱 1 つ（四角を 2 つ 45° ずらして重ねた、8 つの角の星形）を描く
/// @param size 角の先までの長さ（当たり判定の半径と同じにする）
void drawCaltrop(Vector2 pos, float size, Color color) {
    DrawPoly(pos, 4, size, 0.0f, color);
    DrawPoly(pos, 4, size, 45.0f, color);
    DrawCircleV(pos, size * 0.35f, Fade(WHITE, 0.8f));  // 真ん中を明るくして、地面の上で見つけやすくする
}

/// 残り時間を円弧で表示する
void drawLifetimeArc(Vector2 pos, float radius, float ratio, Color color) {
    DrawRing(pos, radius, radius + 4.0f, -90.0f, -90.0f + 360.0f * ratio, 32, color);
}

/// ステージの床（マス目の線。細い線は 100、太い線は 500 ごと）と、ステージの外枠を描く
void drawGround(const World& world) {
    const Rectangle b = world.bounds();
    DrawRectangleRec(b, kGroundColor);
    for (float x = std::ceil(b.x / 100.0f) * 100.0f; x <= b.x + b.width; x += 100.0f) {
        const bool major = std::fmod(std::fabs(x), 500.0f) < 0.5f;
        DrawLineEx({x, b.y}, {x, b.y + b.height}, major ? 2.0f : 1.0f, major ? kGridMajor : kGridMinor);
    }
    for (float y = std::ceil(b.y / 100.0f) * 100.0f; y <= b.y + b.height; y += 100.0f) {
        const bool major = std::fmod(std::fabs(y), 500.0f) < 0.5f;
        DrawLineEx({b.x, y}, {b.x + b.width, y}, major ? 2.0f : 1.0f, major ? kGridMajor : kGridMinor);
    }
    DrawRectangleLinesEx(b, 6.0f, Color{120, 140, 130, 255});
}

/// 形（長方形・円）を塗りつぶして描く
void fillShape(const Shape& shape, Color color) {
    if (shape.kind == Shape::Kind::Rect) DrawRectangleRec(shape.rect, color);
    else DrawCircleV(shape.center, shape.radius, color);
}

/// 形（長方形・円）の縁を、太さ thickness の線で描く
void outlineShape(const Shape& shape, float thickness, Color color) {
    if (shape.kind == Shape::Kind::Rect) DrawRectangleLinesEx(shape.rect, thickness, color);
    else DrawRing(shape.center, shape.radius - thickness, shape.radius, 0.0f, 360.0f, 48, color);
}

/// 形を内側へ inset だけ縮めたもの（建物の屋根など）
Shape inset(const Shape& shape, float amount) {
    Shape s = shape;
    if (s.kind == Shape::Kind::Rect) {
        const float x = std::min(amount, s.rect.width * 0.4f);
        const float y = std::min(amount, s.rect.height * 0.4f);
        s.rect = Rectangle{s.rect.x + x, s.rect.y + y, s.rect.width - x * 2.0f, s.rect.height - y * 2.0f};
    } else {
        s.radius = std::max(1.0f, s.radius - amount);
    }
    return s;
}

/// 障害物（どれも通れない。種類ごとに見た目だけを変える）
void drawObstacle(const Obstacle& obstacle) {
    const Shape& s = obstacle.shape;
    switch (obstacle.kind) {
        case ObstacleKind::Building:
            fillShape(s, Color{52, 56, 66, 255});
            fillShape(inset(s, 14.0f), Color{70, 75, 88, 255});  // 屋根
            outlineShape(s, 5.0f, Color{28, 30, 36, 255});
            break;
        case ObstacleKind::Wall:
            fillShape(s, Color{125, 122, 114, 255});
            outlineShape(s, 3.0f, Color{82, 80, 75, 255});
            break;
        case ObstacleKind::Crate:
            fillShape(s, Color{125, 88, 52, 255});
            outlineShape(s, 4.0f, Color{88, 60, 34, 255});
            if (s.kind == Shape::Kind::Rect) {
                const Rectangle& r = s.rect;
                DrawLineEx({r.x, r.y}, {r.x + r.width, r.y + r.height}, 4.0f, Color{88, 60, 34, 255});
                DrawLineEx({r.x + r.width, r.y}, {r.x, r.y + r.height}, 4.0f, Color{88, 60, 34, 255});
            }
            break;
        case ObstacleKind::Tree:
            fillShape(s, Color{24, 70, 36, 255});
            fillShape(inset(s, s.kind == Shape::Kind::Circle ? s.radius * 0.3f : 16.0f), Color{36, 96, 50, 255});
            outlineShape(s, 4.0f, Color{16, 46, 24, 255});
            break;
    }
}

/// ブッシュ（キャラの上に半透明で重ねて描く。中にいるキャラは草に隠れて見える）
void drawBush(const Shape& bush) {
    fillShape(bush, Color{78, 150, 62, 130});
    outlineShape(bush, 4.0f, Color{110, 185, 80, 200});
}

/// 地面の範囲効果を描く（発動前は予告の円と、飛んでいく帽子・鉄菱。発動後は残る範囲）
/// @param showHitboxes 当たり判定の円も描くか
void drawArea(const AreaEffect& area, bool showHitboxes) {
    if (!area.activated) {
        // 発動前: 範囲の輪と、範囲ごとの予告の表示
        const float t = area.delay > 0.0f ? Clamp(area.elapsed / area.delay, 0.0f, 1.0f) : 1.0f;
        DrawCircleV(area.center, area.radius, Fade(area.color, 0.10f));
        DrawRing(area.center, area.radius - 3.0f, area.radius, 0.0f, 360.0f, 64, Fade(area.color, 0.55f));
        switch (area.visual) {
            case AreaVisual::Telegraph: DrawCircleV(area.center, area.radius * t, Fade(area.color, 0.30f)); break;
            case AreaVisual::ThrownHat:
            case AreaVisual::Caltrops: {
                const Vector2 flying =
                    Vector2Lerp(area.origin, area.center, t) - Vector2{0.0f, std::sin(t * PI) * 70.0f};
                if (area.visual == AreaVisual::ThrownHat) drawHat(flying, 22.0f);
                else DrawCircleV(flying, 14.0f, area.color);
                break;
            }
            case AreaVisual::Explosion: break;
        }
        return;
    }
    if (area.visual == AreaVisual::Caltrops) {
        // 円は鉄菱をまいた範囲の目安（当たり判定ではない）。効くのは、残っている鉄菱だけ
        DrawCircleV(area.center, area.radius, Fade(area.color, 0.12f));
        DrawRing(area.center, area.radius - 3.0f, area.radius, 0.0f, 360.0f, 64, Fade(area.color, 0.8f));
        if (area.stepTriggers) {
            for (const Vector2 pos : area.stepTriggers->positions) {
                drawCaltrop(pos, area.stepTriggers->radius, Fade(area.color, 0.9f));
            }
        }
        const float ratio = area.duration > 0.0f ? 1.0f - (area.elapsed - area.delay) / area.duration : 0.0f;
        drawLifetimeArc(area.center, area.radius + 6.0f, Clamp(ratio, 0.0f, 1.0f), Fade(area.color, 0.6f));
    } else if (showHitboxes) {
        DrawCircleLinesV(area.center, area.radius, kHitboxColor);
    }
    // 踏むと発動する物の当たり判定（見た目によらず、まだ踏まれていない物ごとに）
    if (showHitboxes && area.stepTriggers) {
        for (const Vector2 pos : area.stepTriggers->positions) {
            DrawCircleLinesV(pos, area.stepTriggers->radius, kHitboxColor);
        }
    }
}

/// 設置物（鳩・帽子）と、残り時間の円弧を描く。飛んで移動している最中は描かない
void drawProp(const Prop& prop) {
    if (prop.hiddenTime > 0.0f) return;
    const float ratio = prop.lifetime > 0.0f ? prop.remaining / prop.lifetime : 0.0f;
    if (prop.kind == PropKind::Dove) {
        DrawCircleV(prop.pos, 26.0f, Color{170, 120, 255, 70});
        drawBird(prop.pos, Vector2{0.0f, -1.0f}, 18.0f, Color{245, 245, 255, 255});
        drawLifetimeArc(prop.pos, 32.0f, ratio, Color{190, 150, 255, 200});
    } else {
        drawHat(prop.pos, 24.0f);
        drawLifetimeArc(prop.pos, 40.0f, ratio, Color{90, 230, 230, 200});
    }
}

/// 兎（エマの E・R で変身させた敵）の形を描く
void drawRabbit(Vector2 pos, float radius) {
    DrawRing(pos, radius * 0.7f, radius * 0.82f, 0.0f, 360.0f, 32, Color{230, 60, 60, 220});
    DrawEllipse(static_cast<int>(pos.x - radius * 0.22f), static_cast<int>(pos.y - radius * 0.62f), radius * 0.12f,
                radius * 0.32f, RAYWHITE);
    DrawEllipse(static_cast<int>(pos.x + radius * 0.22f), static_cast<int>(pos.y - radius * 0.62f), radius * 0.12f,
                radius * 0.32f, RAYWHITE);
    DrawCircleV(pos, radius * 0.45f, RAYWHITE);
}

/// キャラを描く（影・体・陣営ごとの外周の輪・向き・被弾の光・状態異常の輪・印）
/// @param time 経過時間（印の輪を回す演出に使う）
void drawUnit(const Unit& unit, float time) {
    const bool isPlayer = unit.team == Team::Player;
    const float lift = unit.status.isAirborne() ? 45.0f : 0.0f;  // 打ち上げ中は浮かせて描く
    const Vector2 body = unit.pos - Vector2{0.0f, lift};

    DrawCircleV(unit.pos + Vector2{0.0f, 8.0f}, unit.radius, Color{0, 0, 0, 70});  // 影

    if (unit.status.has(EffectType::Polymorph)) {
        drawRabbit(body, unit.radius);
    } else {
        // 外周の輪で見分ける: 自キャラ = 白、敵キャラ = 赤（太め）、ダミー = 暗い赤
        // 敵キャラは、自キャラと同じキャラでも見分けやすいように体の色を赤寄りにする
        const bool enemyCharacter = !isPlayer && !unit.characterId.empty();
        const Color ring = isPlayer         ? Color{255, 255, 255, 230}
                           : enemyCharacter ? Color{255, 60, 60, 255}
                                            : Color{70, 20, 20, 255};
        const Color bodyColor = enemyCharacter ? lerpColor(unit.color, Color{200, 50, 50, 255}, 0.4f) : unit.color;
        DrawCircleV(body, unit.radius, bodyColor);
        DrawRing(body, unit.radius - (enemyCharacter ? 9.0f : 4.0f), unit.radius, 0.0f, 360.0f, 48, ring);
        DrawLineEx(body, body + unit.facing * unit.radius, 5.0f, Color{255, 255, 255, 200});
    }
    if (unit.hitFlash > 0.0f) DrawCircleV(body, unit.radius, Fade(WHITE, unit.hitFlash / 0.25f * 0.6f));

    // 状態異常
    if (unit.status.has(EffectType::Snare)) {
        DrawRing(unit.pos, unit.radius + 6.0f, unit.radius + 13.0f, 0.0f, 360.0f, 48, Color{175, 95, 255, 230});
    }
    if (unit.status.has(EffectType::Slow)) {
        DrawRing(unit.pos, unit.radius + 15.0f, unit.radius + 18.0f, 0.0f, 360.0f, 48, Color{90, 180, 255, 170});
    }
    for (const StatusEffect& effect : unit.status.list()) {
        if (effect.type != EffectType::Mark) continue;
        for (int i = 0; i < 4; ++i) {
            const float start = time * 160.0f + static_cast<float>(i) * 90.0f;
            DrawRing(body, unit.radius + 4.0f, unit.radius + 11.0f, start, start + 50.0f, 12, Color{255, 210, 80, 235});
        }
    }
}

/// 弾を、見た目の種類（visual）ごとの形で描く。見た目のずれ（drawOffset）を足した位置に描く
/// @param showHitboxes 当たり判定の円も描くか（円は本当の位置に描く）
void drawProjectile(const Projectile& p, bool showHitboxes) {
    const float angle = geo::angleDegrees(p.dir);
    // 見た目だけずらす分を足した位置に描く（当たり判定の円は本当の位置 p.pos に描く）
    const Vector2 pos = p.pos + p.dir * p.drawOffsetForward + geo::perpendicular(p.dir) * p.drawOffsetSide;
    switch (p.visual) {
        case ProjectileVisual::Orb:
            DrawLineEx(pos - p.dir * p.radius * 2.5f, pos, p.radius * 0.9f, Fade(p.color, 0.35f));
            DrawCircleV(pos, p.radius * 0.95f, Fade(p.color, 0.35f));
            DrawCircleV(pos, p.radius * 0.6f, p.color);
            break;
        case ProjectileVisual::Ring:
            DrawRing(pos, p.radius * 0.45f, p.radius * 0.75f, 0.0f, 360.0f, 32, p.color);
            DrawCircleV(pos, p.radius * 0.25f, Fade(p.color, 0.8f));
            break;
        case ProjectileVisual::Crescent:
            DrawRing(pos - p.dir * p.radius * 0.6f, p.radius * 0.55f, p.radius, angle - 75.0f, angle + 75.0f, 32,
                     Fade(p.color, 0.9f));
            break;
        case ProjectileVisual::Card:
            DrawRectanglePro({pos.x, pos.y, 44.0f, 28.0f}, {22.0f, 14.0f}, angle, Color{250, 250, 255, 255});
            DrawRectanglePro({pos.x, pos.y, 36.0f, 20.0f}, {18.0f, 10.0f}, angle, p.color);
            break;
        case ProjectileVisual::Beam:
            DrawLineEx(pos - p.dir * 90.0f, pos, p.radius * 0.8f, Fade(p.color, 0.5f));
            DrawCircleV(pos, p.radius * 0.7f, p.color);
            break;
        case ProjectileVisual::Bolt:
            DrawLineEx(pos - p.dir * 60.0f, pos, p.radius * 0.6f, Fade(p.color, 0.5f));
            DrawCircleV(pos, p.radius * 0.55f, p.color);
            break;
        case ProjectileVisual::Dove:
            DrawLineEx(pos - p.dir * 120.0f, pos, p.radius * 0.5f, Fade(p.color, 0.3f));
            drawBird(pos, p.dir, 22.0f, p.color);
            break;
    }
    if (showHitboxes) DrawCircleLinesV(p.pos, p.radius, kHitboxColor);
}

/// 演出（光る円・広がる輪・ワープ・クリックした地点）を、残り時間に合わせて薄くしながら描く。文字の演出は Hud.cpp で描く
void drawVfx(const Vfx& vfx) {
    const float alpha = vfx.duration > 0.0f ? Clamp(vfx.remaining / vfx.duration, 0.0f, 1.0f) : 0.0f;
    switch (vfx.kind) {
        case VfxKind::Burst:
            DrawCircleV(vfx.pos, vfx.radius, Fade(vfx.color, 0.35f * alpha));
            DrawRing(vfx.pos, vfx.radius - 4.0f, vfx.radius, 0.0f, 360.0f, 48, Fade(vfx.color, 0.8f * alpha));
            break;
        case VfxKind::Ring: {
            const float r = vfx.radius * (1.0f + (1.0f - alpha) * 0.5f);
            DrawRing(vfx.pos, r - 5.0f, r, 0.0f, 360.0f, 48, Fade(vfx.color, alpha));
            break;
        }
        case VfxKind::Teleport:
            DrawLineEx(vfx.pos, vfx.pos2, 6.0f, Fade(vfx.color, 0.35f * alpha));
            DrawRing(vfx.pos, vfx.radius * 0.6f, vfx.radius, 0.0f, 360.0f, 32, Fade(vfx.color, 0.7f * alpha));
            DrawRing(vfx.pos2, vfx.radius * 0.6f, vfx.radius, 0.0f, 360.0f, 32, Fade(vfx.color, 0.7f * alpha));
            break;
        case VfxKind::ClickMarker: {
            const float r = vfx.radius * (0.4f + 0.6f * alpha);
            DrawRing(vfx.pos, r - 4.0f, r, 0.0f, 360.0f, 32, Fade(vfx.color, alpha));
            break;
        }
        case VfxKind::Text: break;  // 文字は Hud.cpp で描く
    }
}

/// 視界の外を覆う三角形（3 つの頂点ずつ並べたもの）。視界の多角形の外側を、光線の間ごとに 2 つの三角形で埋める
/// @param farDistance 三角形の外側の端の距離（画面の隅より遠く）
std::vector<Vector2> outsideTriangles(Vector2 origin, const std::vector<vision::VisionRay>& rays, float farDistance) {
    std::vector<Vector2> triangles;
    triangles.reserve(rays.size() * 6);
    for (std::size_t i = 0; i < rays.size(); ++i) {
        const vision::VisionRay& a = rays[i];
        const vision::VisionRay& b = rays[(i + 1) % rays.size()];
        const Vector2 nearA = origin + a.dir * a.distance;
        const Vector2 nearB = origin + b.dir * b.distance;
        const Vector2 farA = origin + a.dir * farDistance;
        const Vector2 farB = origin + b.dir * farDistance;
        triangles.insert(triangles.end(), {nearA, farA, farB, nearA, farB, nearB});
    }
    return triangles;
}

/// 三角形の並び（outsideTriangles）を塗りつぶす
void fillTriangles(const std::vector<Vector2>& triangles, Color color) {
    for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
        drawTriangleBothSides(triangles[i], triangles[i + 1], triangles[i + 2], color);
}

/// origin から、画面の隅までの最も遠い距離（ワールド座標）。視界の外を覆う三角形を画面の外まで伸ばすのに使う
float distanceToFarthestScreenCorner(Vector2 origin, const Camera2D& camera) {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    float farthest = 0.0f;
    for (const Vector2 corner : {Vector2{0.0f, 0.0f}, Vector2{w, 0.0f}, Vector2{0.0f, h}, Vector2{w, h}}) {
        farthest = std::max(farthest, Vector2Distance(origin, GetScreenToWorld2D(corner, camera)));
    }
    return farthest;
}

/// 描画先 target に drawContents で描き、視界の外（outside の三角形）を消す。
/// target には、色に不透明度を掛けた値（premultiplied alpha）で描く（重ねる時に、半透明の色が薄くならないように）
template <typename DrawContents>
void drawInsideVision(const RenderTexture2D& target, const Camera2D& camera, const std::vector<Vector2>& outside,
                      DrawContents drawContents) {
    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode2D(camera);
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD,
                              RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    drawContents();
    EndBlendMode();
    // 視界の外を、色も不透明度も 0 にする
    rlSetBlendFactors(RL_ZERO, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    fillTriangles(outside, WHITE);
    EndBlendMode();
    EndMode2D();
    EndTextureMode();
}

/// drawInsideVision で描いた描画先を、画面に重ねる（BeginMode2D の外で呼ぶ）
void compositeLayer(const RenderTexture2D& layer) {
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    // 描画先の絵は上下が逆さまなので、高さをマイナスにして読む
    const Rectangle source{0.0f, 0.0f, static_cast<float>(layer.texture.width),
                           -static_cast<float>(layer.texture.height)};
    DrawTextureRec(layer.texture, source, Vector2{0.0f, 0.0f}, WHITE);
    EndBlendMode();
}

}  // namespace

void VisionLayers::ensureSize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (below_.id != 0 && width == width_ && height == height_) return;
    release();
    below_ = LoadRenderTexture(width, height);
    above_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void VisionLayers::release() {
    if (below_.id != 0) UnloadRenderTexture(below_);
    if (above_.id != 0) UnloadRenderTexture(above_);
    below_ = RenderTexture2D{};
    above_ = RenderTexture2D{};
    width_ = height_ = 0;
}

void drawWorld(const World& world, const Camera2D& camera, const RenderOptions& options, VisionLayers& layers) {
    constexpr Team kViewerTeam = Team::Player;
    const auto isMine = [](Team team) { return team == kViewerTeam; };

    // 自キャラの視界の外を覆う三角形（全視界の時と、自キャラがいない時は、視界の外は無い）
    std::vector<Vector2> outside;
    if (const Unit* viewer = world.player(); viewer && !world.rules().fullVision) {
        const float farDistance =
            std::max(distanceToFarthestScreenCorner(viewer->pos, camera), world.settings().visionRadius) + 100.0f;
        outside = outsideTriangles(viewer->pos, world.visionPolygon(*viewer), farDistance);
    }

    // 相手の陣営の範囲・設置物・弾・演出は、別の描画先に描いて、視界の外を消しておく
    layers.ensureSize(GetScreenWidth(), GetScreenHeight());
    drawInsideVision(layers.below(), camera, outside, [&] {
        for (const AreaEffect& area : world.areas()) {
            if (!isMine(area.team)) drawArea(area, options.showHitboxes);
        }
        for (const Prop& prop : world.props()) {
            if (!isMine(prop.team)) drawProp(prop);
        }
    });
    drawInsideVision(layers.above(), camera, outside, [&] {
        for (const Projectile& projectile : world.projectiles()) {
            if (!isMine(projectile.team)) drawProjectile(projectile, options.showHitboxes);
        }
        for (const Vfx& vfx : world.vfx()) {
            if (!isMine(vfx.team)) drawVfx(vfx);
        }
    });

    BeginMode2D(camera);
    drawGround(world);
    for (const Obstacle& obstacle : world.terrain().obstacles()) drawObstacle(obstacle);
    for (const AreaEffect& area : world.areas()) {
        if (isMine(area.team)) drawArea(area, options.showHitboxes);
    }
    for (const Prop& prop : world.props()) {
        if (isMine(prop.team)) drawProp(prop);
    }
    EndMode2D();
    compositeLayer(layers.below());

    BeginMode2D(camera);
    for (const Unit& unit : world.units()) {
        // 自キャラの陣営から見えない敵は描かない
        if (world.canSee(kViewerTeam, unit)) drawUnit(unit, world.time());
    }
    for (const Shape& bush : world.terrain().bushes()) drawBush(bush);
    for (const Projectile& projectile : world.projectiles()) {
        if (isMine(projectile.team)) drawProjectile(projectile, options.showHitboxes);
    }
    for (const Vfx& vfx : world.vfx()) {
        if (isMine(vfx.team)) drawVfx(vfx);
    }
    EndMode2D();
    compositeLayer(layers.above());

    // 視界の外を暗くする（地形も、自分の陣営の物も、視界の外にある物はまとめて暗くなる）
    BeginMode2D(camera);
    fillTriangles(outside, Fade(BLACK, options.fogDarkness));
    EndMode2D();
}

}  // namespace moba
