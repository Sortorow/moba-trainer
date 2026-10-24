#include "game/DummyController.h"

#include "game/World.h"
#include "raymath.h"

namespace moba {

void DummyController::update(World& world) {
    for (DummyBrain& brain : brains_) {
        Unit* unit = world.findUnit(brain.unitId);
        if (!unit || !unit->alive) continue;

        switch (brain.behavior) {
            case DummyBehavior::Stationary:
                // 元の位置から動かされていたら戻る
                if (!unit->moveTarget && Vector2Distance(unit->pos, brain.home) > 1.0f) unit->moveTarget = brain.home;
                break;
            case DummyBehavior::Patrol:
                // 目的地に着いたら（moveTarget が空になったら）、反対側へ向かう
                if (!unit->moveTarget) {
                    unit->moveTarget = brain.headingToPatrolTo ? brain.patrolTo : brain.home;
                    brain.headingToPatrolTo = !brain.headingToPatrolTo;
                }
                break;
        }
    }
}

}  // namespace moba
