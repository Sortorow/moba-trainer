#include "game/Sandbox.h"

#include "core/Log.h"
#include "data/CharacterDef.h"
#include "data/SandboxConfig.h"
#include "game/DummyController.h"
#include "game/World.h"

namespace moba {

Unit makePlayerUnit(const CharacterDef& character) {
    Unit player;
    player.team = Team::Player;
    player.name = character.displayName;
    player.characterId = character.id;
    player.pos = Vector2{0.0f, 0.0f};
    player.radius = character.hitboxRadius;
    player.baseMoveSpeed = character.moveSpeed;
    player.color = character.color;
    return player;
}

void setupSandbox(World& world, DummyController& dummies, const CharacterDef& character, const SandboxPreset* preset) {
    Unit player = makePlayerUnit(character);
    player.pos = world.placeable(player.pos, player.radius);
    world.addUnit(player);
    placeDummies(world, dummies, preset);
}

void placeDummies(World& world, DummyController& dummies, const SandboxPreset* preset) {
    for (const DummyBrain& brain : dummies.brains()) world.removeUnit(brain.unitId);
    dummies.clear();
    if (!preset) {
        log::warn("sandbox", "練習場のダミーのプリセットがないため、ダミーを置きません");
        return;
    }

    for (const DummyDef& def : preset->dummies) {
        Unit dummy;
        dummy.team = Team::Enemy;
        dummy.name = def.name;
        // ステージの建物や木と重なる位置なら、近くの立てる位置へずらす
        dummy.pos = world.placeable(def.position, def.radius);
        dummy.facing = Vector2{-1.0f, 0.0f};
        dummy.radius = def.radius;
        dummy.baseMoveSpeed = def.moveSpeed;
        dummy.color = def.color;
        const EntityId id = world.addUnit(dummy);

        DummyBrain brain;
        brain.unitId = id;
        brain.home = dummy.pos;
        if (def.patrolTo) {
            brain.behavior = DummyBehavior::Patrol;
            brain.patrolTo = world.placeable(*def.patrolTo, def.radius);
        } else {
            brain.behavior = DummyBehavior::Stationary;
            brain.patrolTo = dummy.pos;
        }
        dummies.add(brain);
    }
    log::info("sandbox", "練習場のダミーを「{}」（{} 体）にしました", preset->name, preset->dummies.size());
}

}  // namespace moba
