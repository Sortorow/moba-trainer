#include "render/Renderer.h"

#include <cmath>

#include "core/Geometry.h"
#include "game/World.h"

namespace moba {
namespace {

constexpr Color kGroundColor{34, 52, 42, 255};
constexpr Color kGridMinor{255, 255, 255, 10};
constexpr Color kGridMajor{255, 255, 255, 28};
constexpr Color kHitboxColor{255, 255, 255, 150};

// 頂点の並び順に関係なく三角形を描く（raylib は反時計回りの三角形しか描かないため、両方の順で描く）
void drawTriangleBothSides(Vector2 a, Vector2 b, Vector2 c, Color color) {
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

// 小さな鳥（鳩）の形。dir の方向を向く
void drawBird(Vector2 pos, Vector2 dir, float size, Color color) {
    const Vector2 side = geo::perpendicular(dir);
    drawTriangleBothSides(pos + dir * size, pos + side * size * 0.9f - dir * size * 0.5f,
                          pos - side * size * 0.9f - dir * size * 0.5f, color);
    DrawCircleV(pos + dir * size * 0.3f, size * 0.35f, color);
}

void drawHat(Vector2 pos, float size) {
    DrawCircleV(pos, size * 1.25f, Color{20, 20, 28, 255});
    DrawCircleV(pos, size * 0.8f, Color{45, 45, 60, 255});
    DrawRing(pos, size * 1.25f, size * 1.45f, 0.0f, 360.0f, 32, Color{90, 230, 230, 255});
}

void drawCaltropStars(Vector2 center, float radius, Color color) {
    DrawPoly(center, 4, 10.0f, 45.0f, color);
    for (int i = 0; i < 7; ++i) {
        const float angle = static_cast<float>(i) * (2.0f * PI / 7.0f);
        const Vector2 offset{std::cos(angle) * radius * 0.6f, std::sin(angle) * radius * 0.6f};
        DrawPoly(center + offset, 4, 10.0f, 45.0f, color);
    }
}

// 残り時間を円弧で表示する
void drawLifetimeArc(Vector2 pos, float radius, float ratio, Color color) {
    DrawRing(pos, radius, radius + 4.0f, -90.0f, -90.0f + 360.0f * ratio, 32, color);
}

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

void drawArea(const AreaEffect& area, bool showHitboxes) {
    if (!area.activated) {
        // 着弾前: 着弾地点の範囲と、飛んでいく物
        const float t = area.delay > 0.0f ? Clamp(area.elapsed / area.delay, 0.0f, 1.0f) : 1.0f;
        DrawCircleV(area.center, area.radius, Fade(area.color, 0.10f));
        DrawRing(area.center, area.radius - 3.0f, area.radius, 0.0f, 360.0f, 64, Fade(area.color, 0.55f));
        const Vector2 flying = Vector2Lerp(area.origin, area.center, t) - Vector2{0.0f, std::sin(t * PI) * 70.0f};
        if (area.visual == AreaVisual::ThrownHat) drawHat(flying, 22.0f);
        else DrawCircleV(flying, 14.0f, area.color);
        return;
    }
    if (area.visual == AreaVisual::Caltrops) {
        DrawCircleV(area.center, area.radius, Fade(area.color, 0.12f));
        DrawRing(area.center, area.radius - 3.0f, area.radius, 0.0f, 360.0f, 64, Fade(area.color, 0.8f));
        drawCaltropStars(area.center, area.radius, Fade(area.color, 0.9f));
        const float ratio = area.duration > 0.0f ? 1.0f - (area.elapsed - area.delay) / area.duration : 0.0f;
        drawLifetimeArc(area.center, area.radius + 6.0f, Clamp(ratio, 0.0f, 1.0f), Fade(area.color, 0.6f));
    } else if (showHitboxes) {
        DrawCircleLinesV(area.center, area.radius, kHitboxColor);
    }
}

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

void drawRabbit(Vector2 pos, float radius) {
    DrawRing(pos, radius * 0.7f, radius * 0.82f, 0.0f, 360.0f, 32, Color{230, 60, 60, 220});
    DrawEllipse(static_cast<int>(pos.x - radius * 0.22f), static_cast<int>(pos.y - radius * 0.62f), radius * 0.12f,
                radius * 0.32f, RAYWHITE);
    DrawEllipse(static_cast<int>(pos.x + radius * 0.22f), static_cast<int>(pos.y - radius * 0.62f), radius * 0.12f,
                radius * 0.32f, RAYWHITE);
    DrawCircleV(pos, radius * 0.45f, RAYWHITE);
}

void drawUnit(const Unit& unit, float time) {
    const bool isPlayer = unit.team == Team::Player;
    const float lift = unit.status.isAirborne() ? 45.0f : 0.0f;  // 打ち上げ中は浮かせて描く
    const Vector2 body = unit.pos - Vector2{0.0f, lift};

    DrawCircleV(unit.pos + Vector2{0.0f, 8.0f}, unit.radius, Color{0, 0, 0, 70});  // 影

    if (unit.status.has(EffectType::Polymorph)) {
        drawRabbit(body, unit.radius);
    } else {
        DrawCircleV(body, unit.radius, unit.color);
        DrawRing(body, unit.radius - 4.0f, unit.radius, 0.0f, 360.0f, 48,
                 isPlayer ? Color{255, 255, 255, 230} : Color{70, 20, 20, 255});
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

void drawProjectile(const Projectile& p, bool showHitboxes) {
    const float angle = geo::angleDegrees(p.dir);
    switch (p.visual) {
        case ProjectileVisual::Orb:
            DrawLineEx(p.pos - p.dir * p.radius * 2.5f, p.pos, p.radius * 0.9f, Fade(p.color, 0.35f));
            DrawCircleV(p.pos, p.radius * 0.95f, Fade(p.color, 0.35f));
            DrawCircleV(p.pos, p.radius * 0.6f, p.color);
            break;
        case ProjectileVisual::Ring:
            DrawRing(p.pos, p.radius * 0.45f, p.radius * 0.75f, 0.0f, 360.0f, 32, p.color);
            DrawCircleV(p.pos, p.radius * 0.25f, Fade(p.color, 0.8f));
            break;
        case ProjectileVisual::Crescent:
            DrawRing(p.pos - p.dir * p.radius * 0.6f, p.radius * 0.55f, p.radius, angle - 75.0f, angle + 75.0f, 32,
                     Fade(p.color, 0.9f));
            break;
        case ProjectileVisual::Card:
            DrawRectanglePro({p.pos.x, p.pos.y, 44.0f, 28.0f}, {22.0f, 14.0f}, angle, Color{250, 250, 255, 255});
            DrawRectanglePro({p.pos.x, p.pos.y, 36.0f, 20.0f}, {18.0f, 10.0f}, angle, p.color);
            break;
        case ProjectileVisual::Beam:
            DrawLineEx(p.pos - p.dir * 90.0f, p.pos, p.radius * 0.8f, Fade(p.color, 0.5f));
            DrawCircleV(p.pos, p.radius * 0.7f, p.color);
            break;
        case ProjectileVisual::Bolt:
            DrawLineEx(p.pos - p.dir * 60.0f, p.pos, p.radius * 0.6f, Fade(p.color, 0.5f));
            DrawCircleV(p.pos, p.radius * 0.55f, p.color);
            break;
        case ProjectileVisual::Dove:
            DrawLineEx(p.pos - p.dir * 120.0f, p.pos, p.radius * 0.5f, Fade(p.color, 0.3f));
            drawBird(p.pos, p.dir, 22.0f, p.color);
            break;
    }
    if (showHitboxes) DrawCircleLinesV(p.pos, p.radius, kHitboxColor);
}

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
        case VfxKind::Text:
            break;  // 文字は Hud.cpp で描く
    }
}

}  // namespace

void drawWorld(const World& world, const RenderOptions& options) {
    drawGround(world);
    for (const AreaEffect& area : world.areas()) drawArea(area, options.showHitboxes);
    for (const Prop& prop : world.props()) drawProp(prop);
    for (const Unit& unit : world.units()) drawUnit(unit, world.time());
    for (const Projectile& projectile : world.projectiles()) drawProjectile(projectile, options.showHitboxes);
    for (const Vfx& vfx : world.vfx()) drawVfx(vfx);
}

}  // namespace moba
