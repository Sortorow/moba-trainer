#include "game/DummyController.h"

#include "core/Geometry.h"
#include "core/Log.h"
#include "game/World.h"
#include "raymath.h"

namespace moba {
namespace {

// 往復するダミーが、この秒数のあいだほとんど動けなければ折り返す（押し合いで挟まって進めない時）
constexpr float kPatrolStuckTime = 1.0f;
// 前に数えた位置からこれだけ離れたら「動けた」とみなす（押し合いで小刻みに揺れているだけの時を除くため）
constexpr float kPatrolProgress = 5.0f;

/// 往復のダミーを、次の目的地（headingToPatrolTo が示す側）へ向かわせる
void headToNext(DummyBrain& brain, Unit& unit, float now) {
    unit.moveTarget = brain.headingToPatrolTo ? brain.patrolTo : brain.home;
    brain.headingToPatrolTo = !brain.headingToPatrolTo;
    brain.progressAnchor = unit.pos;
    brain.progressTime = now;
}

}  // namespace

void DummyController::update(World& world) {
    const float now = world.time();
    for (DummyBrain& brain : brains_) {
        Unit* unit = world.findUnit(brain.unitId);
        if (!unit || !unit->alive) continue;

        switch (brain.behavior) {
            case DummyBehavior::Stationary:
                // 元の位置から動かされていたら戻る
                if (!unit->moveTarget && Vector2Distance(unit->pos, brain.home) > 1.0f) unit->moveTarget = brain.home;
                break;
            case DummyBehavior::Patrol: {
                // 目的地に着いたら（moveTarget が空になったら）、反対側へ向かう
                if (!unit->moveTarget) headToNext(brain, *unit, now);
                // 動けているか。スタンなどで動けない間は、止まっていても数えない（効果が切れたら、そのまま進み続ける）
                if (!brain.progressAnchor || Vector2Distance(unit->pos, *brain.progressAnchor) >= kPatrolProgress ||
                    !unit->status.canMove()) {
                    brain.progressAnchor = unit->pos;
                    brain.progressTime = now;
                } else if (now - brain.progressTime >= kPatrolStuckTime) {
                    log::info("sandbox", "{} は {:.1f} 秒進めなかったため、往復を折り返します（位置{}）", unit->name,
                              kPatrolStuckTime, geo::str(unit->pos));
                    headToNext(brain, *unit, now);
                }
                break;
            }
        }
    }
}

}  // namespace moba
