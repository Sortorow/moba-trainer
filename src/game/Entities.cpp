#include "game/Entities.h"

#include <array>
#include <utility>

#include "core/Log.h"

namespace moba {
namespace {

/// 弾の見た目と、設定ファイルでの表記の対応
constexpr std::array<std::pair<std::string_view, ProjectileVisual>, 7> kProjectileVisuals = {{
    {"orb", ProjectileVisual::Orb},
    {"ring", ProjectileVisual::Ring},
    {"crescent", ProjectileVisual::Crescent},
    {"card", ProjectileVisual::Card},
    {"beam", ProjectileVisual::Beam},
    {"bolt", ProjectileVisual::Bolt},
    {"dove", ProjectileVisual::Dove},
}};

/// 範囲の見た目と、設定ファイルでの表記の対応
constexpr std::array<std::pair<std::string_view, AreaVisual>, 4> kAreaVisuals = {{
    {"explosion", AreaVisual::Explosion},
    {"telegraph", AreaVisual::Telegraph},
    {"hat", AreaVisual::ThrownHat},
    {"caltrops", AreaVisual::Caltrops},
}};

}  // namespace

ProjectileVisual projectileVisualFromString(std::string_view text) {
    for (const auto& [name, visual] : kProjectileVisuals) {
        if (name == text) return visual;
    }
    log::warn("data", "弾の見た目 \"{}\" は不明なため \"orb\" を使います", text);
    return ProjectileVisual::Orb;
}

std::vector<std::string_view> projectileVisualNames() {
    std::vector<std::string_view> names;
    for (const auto& entry : kProjectileVisuals) names.push_back(entry.first);
    return names;
}

AreaVisual areaVisualFromString(std::string_view text) {
    for (const auto& [name, visual] : kAreaVisuals) {
        if (name == text) return visual;
    }
    log::warn("data", "範囲の見た目 \"{}\" は不明なため \"explosion\" を使います", text);
    return AreaVisual::Explosion;
}

std::vector<std::string_view> areaVisualNames() {
    std::vector<std::string_view> names;
    for (const auto& entry : kAreaVisuals) names.push_back(entry.first);
    return names;
}

std::string_view displayName(PropKind kind) {
    switch (kind) {
        case PropKind::Dove: return "鳩";
        case PropKind::Hat: return "帽子";
    }
    return "?";
}

Vfx makeBurstVfx(Team team, Vector2 pos, float radius, Color color, float duration) {
    Vfx vfx;
    vfx.kind = VfxKind::Burst;
    vfx.team = team;
    vfx.pos = pos;
    vfx.radius = radius;
    vfx.color = color;
    vfx.duration = vfx.remaining = duration;
    return vfx;
}

Vfx makeRingVfx(Team team, Vector2 pos, float radius, Color color, float duration) {
    Vfx vfx = makeBurstVfx(team, pos, radius, color, duration);
    vfx.kind = VfxKind::Ring;
    return vfx;
}

Vfx makeTeleportVfx(Team team, Vector2 from, Vector2 to, Color color) {
    Vfx vfx = makeBurstVfx(team, from, 60.0f, color, 0.35f);
    vfx.kind = VfxKind::Teleport;
    vfx.pos2 = to;
    return vfx;
}

Vfx makeClickMarkerVfx(Vector2 pos) {
    Vfx vfx = makeBurstVfx(Team::Player, pos, 36.0f, Color{80, 230, 120, 255}, 0.3f);
    vfx.kind = VfxKind::ClickMarker;
    return vfx;
}

Vfx makeTextVfx(Team team, Vector2 pos, std::string text, Color color) {
    Vfx vfx = makeBurstVfx(team, pos, 0.0f, color, 0.9f);
    vfx.kind = VfxKind::Text;
    vfx.text = std::move(text);
    return vfx;
}

}  // namespace moba
