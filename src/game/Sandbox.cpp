#include "game/Sandbox.h"

#include "data/CharacterDef.h"
#include "game/DummyController.h"
#include "game/World.h"

namespace moba {
namespace {

constexpr Color kDummyColor{215, 75, 75, 255};
constexpr float kDummyRadius = 65.0f;
constexpr float kDummyMoveSpeed = 300.0f;

EntityId addDummy(World& world, const char* name, Vector2 pos) {
    Unit dummy;
    dummy.team = Team::Enemy;
    dummy.name = name;
    dummy.pos = pos;
    dummy.facing = Vector2{-1.0f, 0.0f};
    dummy.radius = kDummyRadius;
    dummy.baseMoveSpeed = kDummyMoveSpeed;
    dummy.color = kDummyColor;
    return world.addUnit(dummy);
}

}  // namespace

void setupSandbox(World& world, DummyController& dummies, const CharacterDef& character) {
    Unit player;
    player.team = Team::Player;
    player.name = character.displayName;
    player.characterId = character.id;
    player.pos = Vector2{0.0f, 0.0f};
    player.radius = character.hitboxRadius;
    player.baseMoveSpeed = character.moveSpeed;
    player.color = character.color;
    world.addUnit(player);

    dummies.clear();
    const Vector2 a{650.0f, -200.0f};
    dummies.add({addDummy(world, "ダミーA（静止）", a), DummyBehavior::Stationary, a, a});

    const Vector2 bFrom{1000.0f, -550.0f};
    const Vector2 bTo{1000.0f, 450.0f};
    dummies.add({addDummy(world, "ダミーB（往復）", bFrom), DummyBehavior::Patrol, bFrom, bTo});

    const Vector2 c{1450.0f, 250.0f};
    dummies.add({addDummy(world, "ダミーC（静止）", c), DummyBehavior::Stationary, c, c});
}

}  // namespace moba
