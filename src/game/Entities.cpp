#include "game/Entities.h"

#include <utility>

#include "core/Log.h"

namespace moba {

ProjectileVisual projectileVisualFromString(std::string_view text) {
    if (text == "orb") return ProjectileVisual::Orb;
    if (text == "ring") return ProjectileVisual::Ring;
    if (text == "crescent") return ProjectileVisual::Crescent;
    if (text == "card") return ProjectileVisual::Card;
    if (text == "beam") return ProjectileVisual::Beam;
    if (text == "bolt") return ProjectileVisual::Bolt;
    if (text == "dove") return ProjectileVisual::Dove;
    log::warn("data", "弾の見た目 \"{}\" は不明なため \"orb\" を使います", text);
    return ProjectileVisual::Orb;
}

AreaVisual areaVisualFromString(std::string_view text) {
    if (text == "explosion") return AreaVisual::Explosion;
    if (text == "hat") return AreaVisual::ThrownHat;
    if (text == "caltrops") return AreaVisual::Caltrops;
    log::warn("data", "範囲の見た目 \"{}\" は不明なため \"explosion\" を使います", text);
    return AreaVisual::Explosion;
}

std::string_view displayName(PropKind kind) {
    switch (kind) {
        case PropKind::Dove: return "鳩";
        case PropKind::Hat: return "帽子";
    }
    return "?";
}

Vfx makeBurstVfx(Vector2 pos, float radius, Color color, float duration) {
    Vfx vfx;
    vfx.kind = VfxKind::Burst;
    vfx.pos = pos;
    vfx.radius = radius;
    vfx.color = color;
    vfx.duration = vfx.remaining = duration;
    return vfx;
}

Vfx makeRingVfx(Vector2 pos, float radius, Color color, float duration) {
    Vfx vfx = makeBurstVfx(pos, radius, color, duration);
    vfx.kind = VfxKind::Ring;
    return vfx;
}

Vfx makeTeleportVfx(Vector2 from, Vector2 to, Color color) {
    Vfx vfx = makeBurstVfx(from, 60.0f, color, 0.35f);
    vfx.kind = VfxKind::Teleport;
    vfx.pos2 = to;
    return vfx;
}

Vfx makeClickMarkerVfx(Vector2 pos) {
    Vfx vfx = makeBurstVfx(pos, 36.0f, Color{80, 230, 120, 255}, 0.3f);
    vfx.kind = VfxKind::ClickMarker;
    return vfx;
}

Vfx makeTextVfx(Vector2 pos, std::string text, Color color) {
    Vfx vfx = makeBurstVfx(pos, 0.0f, color, 0.9f);
    vfx.kind = VfxKind::Text;
    vfx.text = std::move(text);
    return vfx;
}

}  // namespace moba
