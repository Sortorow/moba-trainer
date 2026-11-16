# game モジュール（1）: ゲームの世界

## 1. 概要

1 回のプレイの中身（キャラ・弾・範囲・設置物・演出・地形）と、それを 1 刻み進める処理です。中心は `World` クラスです。

| ファイル | 内容 |
|---|---|
| [World.h](../../src/game/World.h) / [.cpp](../../src/game/World.cpp) | `World`（中身をまとめて持ち、時間を進める）、`WorldRules` |
| [Unit.h](../../src/game/Unit.h) | `Unit`（キャラ 1 体）、`CastRequest`、`CastState`、`PropTargetKind` |
| [Entities.h](../../src/game/Entities.h) / [.cpp](../../src/game/Entities.cpp) | `Projectile`（弾）、`AreaEffect`（地面の範囲）、`StepTriggers`（踏むと発動する物）、`Prop`（設置物）、`Vfx`（演出） |
| [StatusEffects.h](../../src/game/StatusEffects.h) / [.cpp](../../src/game/StatusEffects.cpp) | `StatusEffects`（状態異常と印） |
| [Terrain.h](../../src/game/Terrain.h) / [.cpp](../../src/game/Terrain.cpp) | `Terrain`（地形の当たり・押し出し・経路探索・ブッシュ） |
| [Vision.h](../../src/game/Vision.h) / [.cpp](../../src/game/Vision.cpp) | `vision::`（視線が通るかの判定と、視界の形の計算）、`VisionRay` |

ヒット率・ドッジ率の集計（`CastStats`・`DodgeCount`）は [18-game-results.md](18-game-results.md)、スキルの発動の流れ（`casting::`）は [15-game-skills.md](15-game-skills.md) にあります。

### 型の一覧

すべて継承はありません。

| 型 | 種類 | 定義 |
|---|---|---|
| `WorldRules` | 構造体 | World.h |
| `HitRecord` | 構造体 | World.h |
| `World` | クラス | World.h |
| `World::CastRecord` | 構造体（private） | World.h |
| `PropTargetKind` | 列挙型 | Unit.h |
| `CastRequest`、`CastState`、`Unit` | 構造体 | Unit.h |
| `ProjectileVisual`、`AreaVisual`、`PropKind`、`VfxKind` | 列挙型 | Entities.h |
| `Projectile`、`AreaEffect`、`StepTriggers`、`Prop`、`Vfx` | 構造体 | Entities.h |
| `StatusEffect` | 構造体 | StatusEffects.h |
| `vision::VisionRay` | 構造体 | Vision.h |
| `StatusEffects` | クラス | StatusEffects.h |
| `Terrain` | クラス | Terrain.h |
| `Terrain::Grid` | 構造体（private） | Terrain.h |

### 設計の考え方

- **キャラはすべて同じ `Unit` 構造体**です。自キャラ・敵キャラ・ダミー・スキルショットの的の違いは、陣営（`team`）・キャラの設定の有無（`characterId`）・HP の有無（`maxHp`）だけです。誰が動かすか（プレイヤー・AI・ダミーの動き）は `World` の外（`Match` が持つ各コントローラー）が決め、`Unit` の `moveTarget` に書きます。
- **弾・範囲の「当たった時の処理」は `std::function` で持ちます**。弾の種類ごとにクラスを作らず、handler（スキルの処理）がラムダで処理を渡します。こうすると、新しいスキルを足しても `World` を変えずに済みます（推測）。
- **1 つのスキルだけの決まりは `World` に書きません**。`World` が持つのは、当たりの判定・命中の数え方など、どのスキルにも共通の決まりです。鉄菱のように「踏むと消える物」は部品（`StepTriggers`。5.3）にし、踏んだ時に何が起きるかはスキルの側（`onStep`）が決めます（背景は 5.3）。
- **弾・範囲・設置物の追加は、刻みの区切りでまとめて行います**（`pending*`）。弾の `onHit` の中で新しい弾を出しても（ブランドの R の跳ね返りなど）、ループ中の配列が変わらないようにするためです（根拠: World.h のコメント「更新中に配列が変わらないように」）。

---

## 2. `World`（クラス）

**役割**: 1 回のプレイの中身をまとめて持ち、時間を進めます。`Match` が 1 つ持ちます。テストでは `World` だけを作って動かします。

**背景**（推測）: 手順 1 で練習場を作った時から、ゲームの中身の中心です。テストで画面を出さずに動かせるよう、描画や入力のことは知りません。

### 2.1 コンストラクタ

| 関数 | 説明 |
|---|---|
| `World(const GameData& data, Rectangle bounds)` | 障害物のないステージで作る（テストと、ステージのファイルが無い時用） |
| `World(const GameData& data, Terrain terrain)` | ステージの地形で作る |

`data` は参照を覚えるだけです。`World` より `GameData` が長く生きる必要があります（`App` が持つので問題ありません）。

### 2.2 `void update(float dt)`

ゲームを `dt` 秒進めます。固定の刻み（1/120 秒）で呼びます。

1. `time_ += dt`
2. すべてのキャラを進める（`updateUnit`）: クールダウン → 被弾の光 → 撃って見えるようになっている時間（`revealTime`） → 詠唱（`casting::update`）→ 移動 → 障害物から押し出す → 状態異常の残り時間を減らす
3. `flushSpawns`: 2 で出た弾・範囲・設置物を加える
4. `updateProjectiles`: 弾を進めて当てる
5. `updateAreas`: 範囲を進めて当てる
6. `updateProps`: 設置物の残り時間
7. `updateVfx`: 演出の残り時間
8. `flushSpawns`: 4〜6 で出た物を加える
9. `removeDead`: 消えた物を取り除き、発動の結果を確定する

**注意事項**: キャラの更新はキャラを加えた順です。上の 2 の中でスキルが出た場合、その弾は同じ刻みの 4 で 1 回目の移動をします。

**注意事項**: 状態異常の残り時間を減らすのは、キャラの更新の最後です。状態異常を付けるのは 4・5（弾と範囲が当たった時）だけなので、付いた効果は、長さが 0 でも次の刻みの詠唱と移動に必ず効きます。AI・入力（`Match::update` で `World::update` より前に動く）と HUD から見た効果とも、同じ刻みで効きます。

**背景**（2026-11-15）: 以前は最初に減らしていたので、長さが 1 刻み（1/120 秒）以下の効果は移動の前に消えていました（鉄菱の `lingerTime` を 0 にすると、範囲の中でもスロウが効かなかった。`lingerTime` は、2026-11-16 に鉄菱を「踏むたびにスロウ」に作り直した時に無くなった）。また、効果が切れる直前の 1 刻みは、AI と HUD からは効いて見えるのに、移動には効いていませんでした。

### 2.3 追加・削除

| 関数 | 説明 |
|---|---|
| `EntityId addUnit(Unit unit)` | キャラを加え、通し番号を振って返す。**すぐに加わる** |
| `void removeUnit(EntityId id)` | キャラを取り除く（練習場のダミーを置き直す時用） |
| `EntityId spawnProjectile(Projectile p)` | 弾を加える（刻みの区切りで実際に加わる）。番号はすぐに振る。発動の記録に「まだ飛んでいる物」として 1 を足す |
| `EntityId spawnArea(AreaEffect a)` | 地面の範囲を加える（同上） |
| `EntityId spawnProp(Prop p)` | 設置物を加える（刻みの区切りで実際に加わる） |
| `void addVfx(Vfx vfx)` | 演出を加える（**すぐに加わる**。当たり判定が無いので、区切りを待たない） |
| `int newCastId()` | 新しい発動の番号（1 回の発動で出た弾・範囲をまとめて集計するため） |

**注意事項**:

- `units_` は `std::vector<Unit>` なので、`addUnit` / `removeUnit` の後は、それまでに得た `Unit*` や `Unit&` は使えません。弾の `onHit` などの中でキャラを足したり消したりしないでください。今はどの handler もしていません。
- `spawnProp` で加えた設置物は、同じ刻みの中では `props()` に見えません（`findProp` でも見つかりません）。

### 2.4 検索

| 関数 | 説明 |
|---|---|
| `Unit* findUnit(EntityId id)`（const 版もある） | 番号でキャラを探す。無ければ `nullptr` |
| `Prop* findProp(EntityId id)` | 番号で設置物を探す |
| `Unit* player()`（const 版もある） | 自キャラ（陣営が `Player` の最初のキャラ） |
| `const CharacterDef* characterOf(const Unit& unit) const` | キャラの設定。**毎回 id で `GameData` を引く**ので、設定ファイルを読み直した後も正しい内容を返す。ダミー・的は `nullptr` |
| `Unit* enemyAt(Vector2 point, Team myTeam, float tolerance)` | `point` の位置にいる敵（当たり判定の縁から `tolerance` まで広げて探す）。最も近いもの。**`myTeam` から見えない敵は選ばない** |
| `Unit* nearestEnemy(Vector2 from, Team myTeam, float maxDistance, string_view preferMark = {})` | `maxDistance` 以内で最も近い敵。`preferMark` の印が付いた敵がいれば、距離に関係なくそちらを先に選ぶ。見えない敵は選ばない（エズリアルの E の追尾弾） |
| `vector<Unit*> enemiesInCircle(Vector2 center, float radius, Team myTeam)` | 円にかかっている敵。**範囲の命中判定用なので、見えない敵も含む** |

### 2.5 視界

| 関数 | 説明 |
|---|---|
| `bool canSee(Team observer, const Unit& target) const` | `observer` の陣営から `target` が見えるか。同じ陣営・撃って見える時間（`revealTime`）が残っている・`canSeePoint` で `target` の中心が見える、のどれかなら見える |
| `bool canSeePoint(Team observer, Vector2 point) const` | `observer` の陣営の、生きているキャラのどれかから、`visionRadius` 以内で視線が通っているか（`vision::lineOfSight`。見る側が入っているブッシュは遮らない） |
| `vector<vision::VisionRay> visionPolygon(const Unit& viewer) const` | `viewer` から見える範囲の形（描画用。`vision::visionPolygon` に `visionRadius` と、`viewer` のいるブッシュを渡す） |
| `void revealAttacker(Unit& caster, const Unit& target)` | 対象指定のスキルを `target` に撃った時に呼ぶ（`casting::runSkill` から）。`caster` が `target` の陣営から見えていなければ、`attackRevealTime` 秒だけ見えるようにする。同じ陣営が対象の時と、もう見えている時は何もしない |

**背景**（根拠: 指示書「視界」「ステージ」、claude_improve_and_answer.txt の 2026/11/03）: MOBA の基本のルールとして、視界の半径（1000。同じ日に 900 → 1150 → 1000 と訂正）の中で、建物・壁・木と、自分が入っていないブッシュに遮られていない所だけが見えます。箱は低い物なので遮りません。ブッシュの中が外から見えない決まり（手順 3）は、「ブッシュも視線を遮る」に含まれます。

撃って見えるようになる決まりは、もとは「ブッシュの中で対象指定のスキルを使うと見える」でしたが、視界を入れた時に「相手から見えていない時に、対象指定のスキルで相手を撃つと見える」にそろえました。将来オートアタックや視界を得る道具を入れた時に、壁の向こうから一方的に殴られた側が相手を見つけられるようにするための決まりです（根拠: 同上）。

弾・範囲は、見えないキャラにも当たります（見えないだけ）。

**全視界**（根拠: claude_improve_and_answer.txt の 2026/11/03）: `WorldRules::fullVision` が `true` の時は、`canSee`・`canSeePoint` がいつも `true` を返します。見えるかどうかを使う所（描画・対象の選択・敵 AI・`revealAttacker`）はすべてこの 2 つを通るので、それぞれの所で全視界かどうかを調べる必要はありません（描画の暗さだけは、`Renderer` が `rules().fullVision` を見て描かない）。視界の仕組みに不具合が出た時の、一時的な逃げ道としても使います。

**注意事項**: 視線は壁に対しては双方向に同じです（こちらから見えなければ、相手からも見えない）。対象指定のスキルの射程（475〜750）は視界の半径より短いので、`revealAttacker` が見えるようにするのは、今はほぼ「ブッシュの中から外の相手を撃った時」だけです。

### 2.6 命中と発動の記録

| 関数 | 説明 |
|---|---|
| `void registerHit(int castId, Team attacker, SkillSlot slot, string_view label, Unit& target)` | スキルが当たった時の共通処理。ヒット率に記録（`stats_.recordHit`）、被弾の光、当たった記録（`recentHits_`）、ログ、命中の観察（テスト用）。**敵の発動が自キャラに当たったら**、その発動で 1 回目なら HP を 1 減らす |
| `const vector<HitRecord>& recentHits() const` | 直前の `update` でスキルが当たった記録（`attacker`・`target`・当たった時の `pos`）。`update` の始めに消える。敵 AI が、見えない自キャラに自分たちのスキルが当たったことを知るために読む（[16-game-ai.md](16-game-ai.md) の 4.1） |
| `void setHitObserver(HitObserver observer)` | 命中のたびに呼ばれる関数を登録する（テストで命中を数える時用） |
| `void beginCast(int castId, Team team, SkillSlot slot, bool probe = false)` | 発動の記録を始める（`casting::runSkill` が handler の前に呼ぶ）。`probe`（敵 AI の確かめ撃ち。`CastRequest::probe`）なら、外れた時はドッジ率に数えず、当たった時は普通の発動と同じく被弾として数える |
| `void endCastIfIdle(int castId)` | 弾も範囲も出さなかった発動（ワープなど）は、ここで記録を閉じる（`runSkill` が handler の後に呼ぶ） |

#### 発動の記録（`CastRecord`）の仕組み

ドッジ率は「敵の 1 回の発動で出た弾・範囲が**すべて消えた**時点で、1 つでも自キャラに当たっていれば被弾、当たっていなければ回避」と数えます（根拠: 指示書「ドッジ率は、敵の 1 回の発動で出た弾や範囲がすべて消えた時点で回避／被弾を確定する」）。敵 AI の確かめ撃ち（`probe` の発動）は、外れた時は結果をログに出すだけで、ドッジ率に数えません。当たった時は、普通の発動と同じく敵の発動 1・被弾 1 として数えます（根拠: 指示書「確かめ撃ちは、外れたものはドッジ率に数えない…」。2026-11-15 に変更。以前は当たっても数えず、被弾の数が HP・ダウンの数と合わないことがあった。外れたものまで数えると、確かめ撃ちをする賢さ 4・5 と隠れる場所のあるステージのドッジ率だけが高く出るので、数えない）。

```mermaid
sequenceDiagram
    participant SC as casting::runSkill
    participant W as World
    participant H as handler

    SC->>W: beginCast(castId)：発動の記録を作る（liveEntities = 0）
    SC->>H: handler(ctx)
    H->>W: spawnProjectile / spawnArea（liveEntities を 1 ずつ足す）
    SC->>W: endCastIfIdle(castId)：liveEntities が 0 なら、すぐ確定
    Note over W: …弾・範囲が飛んでいる間…
    W->>W: registerHit：敵の発動が自キャラに当たれば hitPlayer = true・HP −1
    W->>W: removeDead → releaseCastEntity：消えた弾・範囲ごとに liveEntities −1
    W->>W: 0 になったら finishCastRecord：ドッジ率に加え、ヒット率の「外れ」も確定（stats_.settle）
```

- 弾が当たって次の弾を出す（ブランドの R の跳ね返り）場合、次の弾は前の弾が消える前に加わるので、`liveEntities` が 0 にならず、すべての跳ね返りが終わるまで確定しません。
- 制限時間が来た時に飛んでいる途中の発動は、確定していないので数えません。

### 2.7 その他

| 関数 | 説明 |
|---|---|
| `Vector2 clampToBounds(Vector2 point, float radius) const` | 点をステージの内側（半径 `radius` の円がはみ出さない位置）に収める。障害物は考えない |
| `Vector2 placeable(Vector2 point, float radius) const` | 点の近くで、キャラが立てる位置（`Terrain::pushOut`）。ワープ先・キャラの初期位置に使う |
| `void setCooldownScale(Team team, float scale)` / `float cooldownScale(Team) const` / `float cooldownScaleFor(const Unit&) const` | 陣営ごとのクールダウンの倍率。0 ならクールダウンなし。負の値は 0 にする |
| `void setRules(const WorldRules&)` / `const WorldRules& rules() const` | モードごとのルール |

### 2.8 参照用のアクセサ

`data()`・`settings()`・`bounds()`・`terrain()`・`time()`（始めてからの秒）・`stats()`（ヒット率）・`dodge()`（ドッジ率）・`units()`・`props()`・`projectiles()`・`areas()`・`vfx()`。描画・HUD・AI・テストが使います。`units()` と `props()` は書き換えられる版もあります（スキルの処理が使う）。

### 2.9 非公開のメンバ関数

| 関数 | 説明 |
|---|---|
| `void updateUnit(Unit& unit, float dt)` | キャラ 1 体を進める。**硬直のある詠唱中**（`cast.active && cast.locksMovement`）と、拘束などで動けない間は移動しない（目的地は覚えておき、動けるようになったら再開する）。最後に、ワープなどで障害物にかかっていたら押し出す |
| `void moveAlongPath(Unit& unit, float dt)` | 目的地へ、壁を回り込む道のり（`path`）に沿って進む。目的地が変わった・道のりが無い・次の点へまっすぐ行けなくなった時は、道のりを作り直す。たどり着けなければ移動をやめる。1 刻みで進める距離を使い切るまで、点を順にたどる |
| `void updateProjectiles(float dt)` | 弾を進める。追尾弾は `updateHomingProjectile`。まっすぐの弾は、この刻みの通り道にかかった敵を**進む向きに近い順**に並べて当てる（関数の中の構造体 `Candidate` に、敵と「進む向きに沿った距離」を入れて並べる）。**貫通しない弾は最初の 1 体で止まる**（当たった位置まで戻して消す）。射程の端まで飛んだら `expireProjectile` |
| `void updateHomingProjectile(Projectile& p, float dt)` | 追尾弾を対象へ向けて進める。届けば当てて消える。対象がいなくなったら消える |
| `void hitWithProjectile(Projectile& p, Unit& target)` | 当たった相手を記録し、違う陣営なら `registerHit`、そして弾の `onHit`。**同じ陣営（自分自身）への到達は命中に数えない**（ブランドの R が自分に跳ね返る時） |
| `void expireProjectile(Projectile& p)` | 弾の `onExpire` を呼んで消す |
| `void updateAreas(float dt)` | 範囲を進める。`delay` 秒たったら発動（`onActivate`）。`duration` が 0 なら、発動の瞬間だけ範囲の敵に当てて消える。0 より大きければその間残り、踏むと発動する物（`stepTriggers`）を持っていれば、**発動した刻みから毎刻み、円より物 1 つ分広い範囲の敵に `stepOnTriggers`**（`duration` がたったら、物が残っていても消える） |
| `void hitWithArea(AreaEffect& a, Unit& target)` | 当たった相手を記録し、`registerHit` と `onUnitHit` |
| `void stepOnTriggers(AreaEffect& a, Unit& unit)` | `a.stepTriggers` のうち `unit` が触れている物を、`StepTriggers::consumeTouching` ですべて消す。1 つ以上消したら、その相手が初めて踏んだなら `hitWithArea`（命中は 1 回の発動につき 1 回）、続けて `StepTriggers::onStep(firstStep)` を 1 回 |
| `void updateProps(float dt)` | 設置物の隠れている時間と残り時間を減らし、時間切れなら消す |
| `void updateVfx(float dt)` | 演出の残り時間を減らす |
| `void flushSpawns()` | `pending*` の弾・範囲・設置物を本体の配列へ移す |
| `void removeDead()` | 消えた弾・範囲は `releaseCastEntity` してから取り除く。設置物・演出も取り除く |
| `void releaseCastEntity(int castId)` | 発動で出た物が 1 つ消えた。0 になったら `finishCastRecord` |
| `void finishCastRecord(int castId)` | 発動の結果を確定する。ヒット率の「外れ」を確定（`stats_.settle`）し、敵の発動ならドッジ率に加える（確かめ撃ちは加えない） |
| `void damagePlayer(Unit& player)` | 自キャラの HP を 1 減らす。0 になったらダウン数を増やして全回復（演出「ダウン!」）。`maxHp` が 0（HP を使わないモード）なら何もしない |

### 2.10 メンバ変数

| 変数 | 説明 |
|---|---|
| `data_` | `const GameData*` |
| `terrain_` | 地形 |
| `time_` | 始めてからの秒 |
| `playerCooldownScale_` / `enemyCooldownScale_` | 陣営ごとのクールダウンの倍率（初期値 1） |
| `rules_` | モードごとのルール |
| `lastEntityId_` | 最後に振った通し番号 |
| `lastCastId_` | 最後に振った発動の番号 |
| `stats_` | 自キャラのヒット率（`CastStats`） |
| `dodge_` | ドッジ率（`DodgeCount`） |
| `casts_` | `unordered_map<int, CastRecord>`。結果が確定していない発動 |
| `hitObserver_` | 命中の観察（テスト用） |
| `recentHits_` | 直前の `update` で当たった記録 |
| `units_` / `projectiles_` / `areas_` / `props_` / `vfx_` | 中身 |
| `pendingProjectiles_` / `pendingAreas_` / `pendingProps_` | 刻みの区切りで加える物 |

### 2.11 `World::CastRecord`（構造体）

| メンバ | 説明 |
|---|---|
| `team` | 発動したキャラの陣営 |
| `slot` | キー |
| `liveEntities` | まだ残っている弾・範囲の数 |
| `hitPlayer` | 自キャラに当たったか（敵の発動の時だけ意味がある） |

---

## 3. `WorldRules`（構造体）

| メンバ | 説明 |
|---|---|
| `movementSkillsOnly` | 自キャラは移動系のスキル（`movementSkill: true`）だけ使える |
| `fullVision` | 全視界。`canSee`・`canSeePoint` がいつも `true` を返す（敵も味方も、ステージのすべてが見える）。`Match` が `MatchConfig::fullVision` から決める |

**背景**（根拠: `712d7d8`、`f533230`）: 手順 2 では、スキルドッジで自キャラが移動系のスキルだけ使えるルールでした。その後、エマの R のように他のスキルで置いた物へワープするスキルも回避に使えるため、すべてのスキルを使えるように変えました。**仕組みは残してあり、今はどのモードでも使っていません**（テスト「「移動系だけ使える」ルールを有効にすると…」で動作は確かめています）。

---

## 4. Unit.h

### 4.1 `PropTargetKind`（列挙型）

設置物などを指定するスキル（エマの R）の対象の種類です。`None`・`Dove`（鳩）・`Hat`（帽子）・`Rabbit`（兎にした敵）。`toString` は設定ファイル（params の `targetKinds`）での表記（`"dove"` / `"hat"` / `"rabbit"`）を返します。

### 4.2 `CastRequest`（構造体）: スキル発動の依頼

キーを押した瞬間の情報です。対象指定のスキルでは、発動の流れが対象を選んで書き足します。

| メンバ | 説明 |
|---|---|
| `slot` | 押したキー |
| `cursor` | キーを押した時のカーソル位置（ワールド座標） |
| `targetUnit` | 対象指定のスキルの対象（兎になった敵もここ） |
| `propKind` / `propId` | 鳩・帽子を対象にする場合の種類と番号 |
| `doveCardOrigin` | エマの Q の溜めの間に R で鳩と入れ替わった時の、鳩の元の位置（鳩のカードは入れ替わる前の位置から出る。根拠: `e05707e`） |
| `probe` | 敵 AI の確かめ撃ち（見えない自キャラがいるかを確かめる撃ち方）。外れたらドッジ率に数えず、当たったら被弾として数える（`World::beginCast` の `probe`。根拠: claude_improve_and_answer.txt の 2026/11/03 の Q3、2026-11-15 に当たった時の扱いを変更） |

### 4.3 `CastState`（構造体）: 詠唱中の状態

| メンバ | 説明 |
|---|---|
| `active` | 詠唱中か |
| `request` | 詠唱しているスキルの依頼 |
| `remaining` / `total` | 詠唱の残り時間と、詠唱時間 |
| `locksMovement` | 硬直（詠唱中は移動できない）。スキルの `lockMovement` |

### 4.4 `Unit`（構造体）: キャラ 1 体

| メンバ | 説明 |
|---|---|
| `id` | 通し番号（`addUnit` が振る） |
| `team` | 陣営 |
| `name` | 名前（ログ・画面） |
| `characterId` | キャラの設定の id。**ダミーと的は空** |
| `pos` | 位置 |
| `facing` | 向いている方向（長さ 1） |
| `radius` | 当たり判定の半径 |
| `baseMoveSpeed` | キャラの移動速度 |
| `color` | 体の色 |
| `moveTarget` | 目的地（右クリック・AI・ダミーの動きが書く）。着いたら空になる |
| `path` | 目的地までの道のり（壁を回り込む点の並び）。`World` が `moveTarget` から作る |
| `pathGoal` | `path` を作った時の `moveTarget`（変わったら作り直す） |
| `revealTime` | 視界の外やブッシュの中でも、相手から見える残り時間（対象指定のスキルで撃った後。`World::revealAttacker` が決める） |
| `status` | 状態異常と印 |
| `cast` | 詠唱中の状態 |
| `pendingCast` | 射程外の対象へ近づいている途中のスキル |
| `bufferedCast` | 詠唱中に押されたスキル（詠唱が終わったら発動する。1 つだけ覚える） |
| `cooldowns` | 各キーのクールダウンの残り時間 |
| `hp` / `maxHp` | HP ゲージ（スキルドッジの自キャラ）。`maxHp` が 0 なら HP を使わない |
| `deaths` | HP が 0 になって全回復した回数（ダウン数） |
| `hitFlash` | 被弾した時の光る演出の残り時間 |
| `alive` | 生きているか。**今のゲームでは常に `true`**（誰も倒されない）。`false` のキャラは対象・当たり判定から外れる |
| `float moveSpeed() const` | 今の移動速度（`baseMoveSpeed` × スロウなどの倍率） |

**背景**（根拠: 指示書「被弾時の扱い」「HP が 0 になった後の無敵時間は無い。敵はダメージを受けない（倒せない）」）: HP は 5 段階で、5 回当たるとダウン数が 1 増えて全回復します。敵には HP がありません。

---

## 5. Entities.h / .cpp

### 5.1 `Projectile`（構造体）: 弾

まっすぐ飛ぶ弾と、対象を追尾する弾（`homingTarget` がある）の 2 種類です。

| メンバ | 説明 |
|---|---|
| `id` / `owner` / `team` | 番号・撃ったキャラ・陣営 |
| `castId` / `slot` / `label` | 発動の番号・キー・ログと表示の名前 |
| `pos` / `dir` | 位置と進む向き |
| `speed` | 弾速 |
| `radius` | 当たり判定の半径（幅の半分） |
| `maxDistance` / `traveled` | 飛ぶ距離と、飛んだ距離 |
| `age` | 出てからの時間（的の AI が「気づいたか」に使う） |
| `pierce` | `true` なら当たっても消えずに進む（**地形は常に貫通する**） |
| `homingTarget` | 追尾する対象（0 ならまっすぐ） |
| `visual` / `color` | 見た目の種類と色 |
| `drawOffsetForward` / `drawOffsetSide` | 見た目だけずらして描く量（当たり判定は `pos` のまま）。根拠: `b6c346c`（エマの 2 枚のカードが重なって 1 枚に見えないように） |
| `hitUnits` | 既に当たった相手（同じ相手には 1 回だけ当たる） |
| `onHit` | `function<void(World&, Projectile&, Unit&)>`。当たった時 |
| `onExpire` | `function<void(World&, Projectile&)>`。射程の端まで飛んだ時（今はどの handler も使っていない） |
| `alive` | 消えていないか |
| `bool hasHit(EntityId unit) const` | その相手に既に当たったか |

### 5.2 `AreaEffect`（構造体）: 地面の範囲

`delay` 秒後に発動します。`duration` が 0 なら発動の瞬間だけ判定し、0 より大きければその間残ります。残る範囲は、踏むと発動する物（`stepTriggers`。5.3）を持てます。今ある残る範囲（エマの D の鉄菱）で効くのは、それを踏んだ時だけです。円に入っても、物に触れなければ何も起きません。

| メンバ | 説明 |
|---|---|
| `id` / `owner` / `team` / `castId` / `slot` / `label` | 弾と同じ |
| `origin` | 投げた位置（飛んでいく演出用） |
| `center` / `radius` | 範囲 |
| `delay` / `duration` / `elapsed` | 発動までの時間・残る時間・経過時間 |
| `activated` | 発動したか |
| `visual` / `color` | 見た目 |
| `stepTriggers` | `optional<StepTriggers>`。踏むと発動する物（5.3。エマの D の鉄菱）。残る範囲だけが持てる。無ければ `std::nullopt` |
| `hitUnits` | 当たった相手（`stepTriggers` がある範囲では「一度でも踏んだ相手」） |
| `onActivate` | 発動した瞬間に 1 回（エマの W の帽子を置く・エマの D の着地の光） |
| `onUnitHit` | 相手ごとに 1 回（`stepTriggers` がある範囲では、初めて踏んだ時） |
| `alive` | 消えていないか |
| `bool hasHit(EntityId unit) const` | その相手に既に当たったか |

### 5.3 `StepTriggers`（構造体）: 踏むと発動する物

範囲（`AreaEffect::stepTriggers`）に付ける部品です。今使っているのは、エマの D の鉄菱 12 個です。1 つずつ小さな円の当たり判定を持ち、相手が触れた物は消えます（ほかの相手ももう踏めません）。

| すること | どこで |
|---|---|
| 触れたかを調べ、触れた物を消す | `World::stepOnTriggers` → `consumeTouching` |
| 命中として数える（その相手が初めて踏んだ時。1 回の発動につき 1 回） | `World::stepOnTriggers` → `hitWithArea` |
| 踏んだ時に何が起きるか | `onStep`（スキルの側。鉄菱は `handleCaltrops` のスロウ） |

| メンバ | 説明 |
|---|---|
| `positions` | まだ踏まれていない物の位置。付けた範囲の円の中に置く（`World` は、円より `radius` だけ広い範囲の敵だけを調べる） |
| `radius` | 1 つの当たり判定の半径 |
| `onStep` | `function<void(World&, Unit&, bool firstStep)>`。相手が 1 つ以上踏むたびに 1 回（同時に 2 つ以上踏んでも 1 回）。`firstStep` は、その相手がこの集まりの物を初めて踏んだ時に `true`（エマの D は、初めての時だけ効果の名前の文字を出す） |
| `size_t consumeTouching(Vector2 center, float r)` | 円（`center`, `r`）に触れている物をすべて消し、消した数を返す。同時に 2 つ以上触れていても全部消す（1 つだけ消すと、残りに触れたままなので次の刻みでまた踏んでしまう） |

**背景**（2026-11-17）: 2026-11-16 に鉄菱を作り直した時は、鉄菱の位置を `AreaEffect` に直接持たせ（`pieces`）、「触れたら消える」を `World` の関数（`stepOnPieces`）に書いていました。`World` に 1 つのスキルの決まりが入るので、ユーザーの指摘で部品に分けました。ゲームエンジンのトリガー（Unity の `OnTriggerEnter` など）と同じく、`World` は触れたかを調べて知らせ、何が起きるかは物の側が決めます。決まりは今の鉄菱に要る「触れたら消える」だけです。消えない物などを選べるようにするのは、2 つ目の使い道が出てからにします（使い道が 1 つの時に選択肢を作ると、実際の使い道に合わない作りになりやすいため）。

### 5.4 `Prop`（構造体）: 設置物

エマの鳩・帽子です。エマの R の対象になります。

| メンバ | 説明 |
|---|---|
| `id` / `owner` | 番号・置いたキャラ |
| `team` | 置いたキャラの陣営（相手の陣営の設置物は、視界の中だけ描く） |
| `kind` | `PropKind::Dove`（鳩）か `Hat`（帽子） |
| `pos` | 位置 |
| `lifetime` / `remaining` | 持続時間と残り時間 |
| `hiddenTime` | 0 より大きい間は描かず、対象にもできない（R で飛んで移動している最中） |
| `alive` | 消えていないか |

### 5.5 `Vfx`（構造体）: 演出

当たり判定は無く、残り時間が 0 になったら消えます。

| メンバ | 説明 |
|---|---|
| `kind` | `VfxKind`: `Burst`（光る円）・`Ring`（広がる輪）・`Teleport`（ワープ）・`ClickMarker`（右クリックの地点）・`Text`（文字） |
| `team` | 演出のもとになった行動をした陣営（相手の陣営の演出は、視界の中だけ描く）。状態異常の文字や印の起爆のように、受けた側の位置に出る演出は、受けた側の相手の陣営（`opponentOf`） |
| `pos` / `pos2` | 位置（`Teleport` は移動前と移動後） |
| `radius` / `color` / `text` | 大きさ・色・文字 |
| `duration` / `remaining` | 長さと残り時間 |

`Text` の演出だけは、日本語を出すため HUD（Dear ImGui）が描きます。他は `Renderer` が描きます。

### 5.6 見た目の列挙型と関数

| 名前 | 説明 |
|---|---|
| `ProjectileVisual` | `Orb`（球）・`Ring`（輪）・`Crescent`（半月）・`Card`・`Beam`（光線）・`Bolt`（小さな弾）・`Dove`（鳩）。当たり判定はどれも同じ |
| `ProjectileVisual projectileVisualFromString(string_view)` | `"orb"` などを見た目にする（.cpp の中だけの表 `kProjectileVisuals` を引く）。知らない表記なら警告ログを出して `Orb`（ゲームでは、知らない表記は読み込みの時点でエラーになるので、普通は起きない） |
| `vector<string_view> projectileVisualNames()` | 弾の見た目の表記の一覧（params の `visual` の決まりに使う） |
| `AreaVisual` | `Explosion`（輪だけ）・`Telegraph`（予告円が少しずつ埋まる）・`ThrownHat`（帽子が飛ぶ）・`Caltrops`（鉄菱が飛び、着地後は残っている鉄菱を 1 つずつ描く）。どれを使うかはスキルの処理が決める（設定ファイルの `visual` では選べない。以前は `ground_zone` の `visual` で選べたが、2026-11-16 に鉄菱を作り直した時に、使う所が無くなったので表記の表を消した） |
| `string_view displayName(PropKind)` | 「鳩」/「帽子」（ログ用） |
| `makeBurstVfx(team, pos, radius, color, duration = 0.35)` | 範囲が発動した時の、光って消える円 |
| `makeRingVfx(team, pos, radius, color, duration = 0.4)` | 広がりながら消える輪（命中・ダウン） |
| `makeTeleportVfx(team, from, to, color)` | ワープ（移動前と移動後の輪と、その間の線） |
| `makeClickMarkerVfx(pos)` | 右クリックした地点の、縮んで消える輪（自キャラの陣営） |
| `makeTextVfx(team, pos, text, color)` | 少しずつ上へ浮かんで消える文字（「起爆!」「ダウン!」「スロウ」など） |

---

## 6. StatusEffects.h / .cpp

### 6.1 `StatusEffect`（構造体）

かかっている効果 1 つです。`type`・`remaining`（残り時間）・`duration`（付けた時の長さ）・`amount`（強さ）・`tag`（印の名前）・`source`（付けたキャラ）。

### 6.2 `StatusEffects`（クラス）

**役割**: キャラにかかっている状態異常と印の一覧。「移動できるか」「スキルを使えるか」「移動速度の倍率」を答えます。

| 関数 | 説明 |
|---|---|
| `void apply(const EffectSpec& spec, EntityId source)` | 効果を付ける。**種類・`tag`・強さがすべて同じ**効果が既にあれば、残り時間の長い方に更新する（付け直し）。強さが違えば別の効果として持ち、残り時間をそれぞれ数える。強さが同じものは、残り時間の長い方だけで効き方が決まるので、まとめても効き方は変わらない（範囲の中で毎刻み付け直すスロウで、一覧が増え続けないようにまとめる） |
| `void update(float dt)` | 残り時間を減らし、時間切れの効果を消す。`World::updateUnit` が、詠唱と移動の後に呼ぶ（2.2 の注意事項） |
| `void clear()` | すべて消す |
| `bool has(EffectType) const` | その種類がかかっているか |
| `float remaining(EffectType) const` | 残り時間（最も長いもの）。無ければ 0 |
| `bool hasMark(string_view tag) const` | その印が付いているか |
| `bool consumeMark(string_view tag)` | 印を消費する（起爆した時）。印が無ければ `false` |
| `bool canMove() const` | 移動できるか |
| `bool canCast() const` | スキルを使えるか |
| `bool isAirborne() const` | 打ち上げられているか（描く時に浮かせる） |
| `float moveSpeedMultiplier() const` | 移動速度の倍率。その時に残っているスロウと変身の `amount` のうち**最も強いものだけ**が効く（LoL と同じ。根拠: character_traits.txt の「状態異常」） |
| `const vector<StatusEffect>& list() const` | すべての効果（HUD の表示用） |

#### 効果ごとにできなくなること

| 効果 | 移動 | スキル | 移動速度 |
|---|---|---|---|
| スロウ | できる | できる | `1 − amount` |
| 拘束 | **できない** | できる | — |
| スタン | **できない** | **できない** | — |
| 打ち上げ | **できない** | **できない** | — |
| 変身 | できる | **できない** | `1 − amount` |
| 印 | できる | できる | 変わらない |

**注意事項**: 詠唱中にスタンなどでスキルを使えなくなると、詠唱は中断されます（`casting::update` → `cancelCast`）。

**背景**（2026-11-15）: 以前の `apply` は、同じ種類の効果を 1 つにまとめ、残り時間と強さをそれぞれ大きい方にしていました。そのため、50%・0.5 秒と 40%・1.5 秒を受けると、50% が 1.5 秒続きました。ユーザーの確認で「長さを別々に数え、その時に残っている中で最も強いものが効く」と決まり、強さが違うものはまとめないように直しました。頭の上の表示（`Hud.cpp`）は、強さの違うスロウが並ぶことがあるので、スロウだけ強さも出します（「スロウ 50% 0.3」）。

**背景**（根拠: 指示書「自キャラのスキルによる状態異常（変身・拘束・打ち上げなど）は、敵にもかかる。変身中や打ち上げ中の敵はスキルを撃てない」）: 敵にも自キャラにも同じ仕組みでかかります。

---

## 7. Terrain.h / .cpp

**役割**: ステージの地形（障害物・ブッシュ）。当たりと押し出し、経路探索、ブッシュの判定を行います。視線の計算は [8 章](#8-visionh--cpp) の `vision::` が、この地形を使って行います。

**背景**（根拠: `8c72709`、指示書「ステージ」）: 手順 3 で、路地裏と森のステージのために作りました。

- 壁・建物・木などは**移動と視線だけ**を妨げ、弾や範囲は止めない（視線は 2026/11/03 に追加。箱は視線を遮らない）
- 障害物の向こうを右クリックすると、回り込む道を自動で探して進む（敵も同じ）
- 建物の中など立てない場所を指定したら、その近くの立てる場所まで進む
- 重なり合うブッシュは 1 つのブッシュとして扱う

### 7.1 経路探索の方法

ステージを 40 × 40（`kCellSize`）のマス目に分け、**キャラの半径ごとに「立てるマス目」の表を作って使い回します**（初めて使う時に作る）。その上で 8 方向の A* で道を探し、得られたマス目の並びを「まっすぐ進めるところはまっすぐ進む」ように間引きます。

```mermaid
flowchart TD
    A[findPath from, to, radius] --> B[目的地を pushOut<br/>建物の中なら近くの立てる場所へ]
    B --> C{目的地に立てる?}
    C -->|いいえ| Z[空の道のり<br/>= たどり着けない]
    C -->|はい| D{まっすぐ行ける?<br/>segmentClear}
    D -->|はい| E[目的地 1 点だけの道のり]
    D -->|いいえ| F[半径ごとのマス目 gridFor]
    F --> G[出発・目的地を最寄りの立てるマス目へ]
    G --> H[A* 8 方向<br/>斜めは両隣も立てる時だけ]
    H --> I{見つかった?}
    I -->|いいえ| Z
    I -->|はい| J[マス目の中心の並び<br/>最後を本当の目的地に]
    J --> K[間引き: 今の点から<br/>まっすぐ行ける最も先の点へ]
    K --> L[道のり]
```

### 7.2 メンバ関数

| 関数 | 説明 |
|---|---|
| `Terrain()` | 範囲が空の地形（使う前に、範囲を決めたものを代入する） |
| `explicit Terrain(Rectangle bounds)` | 障害物のない地形 |
| `explicit Terrain(const StageDef& stage)` | ステージの設定から作る（重なり合うブッシュをまとめる） |
| `Rectangle bounds() const` | ステージの範囲 |
| `const vector<Obstacle>& obstacles() const` / `const vector<Shape>& bushes() const` | 描画用 |
| `bool blocked(Vector2 pos, float radius) const` | 円が障害物かステージの外にかかるか。**触れているだけ（0.5 以内）ならかかっていない**とする |
| `Vector2 pushOut(Vector2 pos, float radius) const` | 障害物とステージの外から押し出した位置。障害物が並んでいて押し出した先が別の障害物にかかることがあるので、最大 8 回繰り返す。それでも出られなければ、最も近い立てるマス目の中心 |
| `bool segmentClear(Vector2 a, Vector2 b, float radius) const` | 円が `a` から `b` へまっすぐ進む間に、障害物にかからないか（半径の半分（4〜20）ごとに点を取って `blocked` を調べる） |
| `vector<Vector2> findPath(Vector2 from, Vector2 to, float radius) const` | 道のり（通る点の並び。最後が目的地）。たどり着けなければ空 |
| `int bushAt(Vector2 pos) const` | `pos` を含むブッシュの番号（重なり合うブッシュは同じ番号）。外なら −1 |
| `int bushGroup(std::size_t index) const` | `bushes()` の `index` 番目のブッシュの番号（`bushAt` と同じ番号。視線の計算で、見る側が入っているブッシュを除くのに使う） |
| `const Grid& gridFor(float radius) const`（private） | 半径（切り上げた整数）ごとの立てるマス目。初めて使う時に作り、`grids_` に覚える |
| `Vector2 cellCenter(int col, int row) const`（private） | マス目の中心の座標 |
| `void cellOf(Vector2 pos, int& col, int& row) const`（private） | 座標を含むマス目の番号 |
| `bool nearestFreeCell(const Grid&, int col, int row, int& outCol, int& outRow) const`（private） | 近い順に（四角い輪を広げながら）探して、最初に見つかった立てるマス目 |
| `void groupBushes()`（private） | 重なり合うブッシュに同じ番号を振る（素集合データ構造） |

### 7.3 `Terrain::Grid`（構造体）

`cols` × `rows` のマス目と、立てるかどうかの表（`free`。1 なら立てる）。`bool isFree(int col, int row) const` は範囲の外なら `false`。

### 7.4 .cpp の中だけの関数

| 関数 | 説明 |
|---|---|
| `closestPointInRect` | 長方形の中で、点に最も近い点 |
| `pointInShape` | 点が形の中（縁を含む）にあるか |
| `circleOverlapsShape` | 円が形に**めり込んでいるか**（触れているだけなら `false`） |
| `pushOutOfShape` | 円を形に触れるだけの位置まで押し出す（最も近い外側へ）。中心が長方形の中なら最も近い辺の外へ |
| `shapesOverlap` | 2 つの形が重なっているか（ブッシュをまとめる判断） |

### 7.5 注意事項

- `grids_` は `mutable` で、`const` の関数の中で作ります。複数のスレッドから同時に使うことは想定していません。
- マス目の表は半径ごとに作るので、半径の種類が多いと（ダミーのプリセットでいろいろな大きさを使うなど）メモリと最初の 1 回の時間が増えます。1 つの表は、5000 × 3200 のステージで 125 × 80 = 1 万マスです。
- 「触れているだけならかかっていない」という余裕（`kTouchTolerance = 0.5`）が無いと、壁に沿って歩く時に、押し出された位置がまた「かかっている」と判定されて動けなくなります（推測）。

---

## 8. Vision.h / .cpp

**役割**: 視線が通るかの判定と、視界の形（多角形）の計算です。描画とは切り離してあり、描き方（2D・3D）が変わってもそのまま使えます。

**背景**（根拠: 指示書「視界」、claude_improve_and_answer.txt の 2026/11/03）: 視界の仕組みを入れた時に作りました。視界の形は描画（視界の外を暗くする・相手の陣営の物を視界の中だけ描く）に使い、見えるかどうかの判定（`World::canSee`）には `lineOfSight` を使います。

### 8.1 視線を遮る物

| 物 | 遮るか |
|---|---|
| 建物・壁・木（`ObstacleKind::Building` / `Wall` / `Tree`） | 遮る |
| 箱（`ObstacleKind::Crate`） | 遮らない（低い物） |
| ブッシュ | 見る側が入っているブッシュ（`ignoreBush`。重なり合うブッシュは同じ番号）は遮らない。それ以外は遮る |

### 8.2 関数

| 関数 | 説明 |
|---|---|
| `bool lineOfSight(const Terrain&, Vector2 from, Vector2 to, int ignoreBush)` | `from` から `to` まで視線が通るか。途中か `to` の上（形の縁を含む）に遮る物があれば通らない。距離は考えない（視界の半径は `World::canSeePoint` が見る） |
| `vector<VisionRay> visionPolygon(const Terrain&, Vector2 origin, float radius, int ignoreBush)` | `origin` から見える範囲の形。光線を角度の順（+X から、画面上の時計回り）に並べたもの |

`VisionRay` は、長さ 1 の向き `dir` と、遮る物に当たるまでの距離 `distance`（当たらなければ `radius`）です。光線の先（`origin + dir * distance`）を順に結ぶと、視界の多角形になります。

### 8.3 視界の形の作り方

1. 視界の円にかかる、遮る物だけを集める
2. 光線を出す角度を集める: 1° ごとの 360 本、長方形の 4 つの角、円の 2 本の接線。角と接線は、その向きと、両隣（±0.0001 ラジアン）の 3 本ずつ
3. 角度を並べ、ほぼ同じ角度を 1 つにまとめる
4. 光線ごとに、遮る物に入るまでの距離（長方形はスラブ法、円は 2 次方程式）の最小を求める

角と接線の両隣にも光線を出すので、多角形の頂点は角や接線の位置に合い、影の縁がずれません。1° ごとの光線は、何にも当たらない所で視界の円を多角形で近似するためのものです（視界の半径の既定値 1000 の所で、円との差は約 0.04）。

**注意事項**:

- 円（木・丸いブッシュ）の、見る側に向いた縁は、光線の間を直線で結ぶので、ほんの少し内側に入り込みます（半径 100 の木を距離 500 から見て約 0.1）。`lineOfSight` との食い違いはこの範囲だけです（テスト「視界の形: 路地裏・森で、多角形の内側かどうかと、視線の判定が一致する」で確かめている）。
- 遮る物に囲まれた所（見る側が障害物の中）にいると、すべての光線の距離が 0 になります。キャラは障害物から押し出されるので、普通は起きません。
- 光線の数は、遮る物の数 × 最大 12 本 + 360 本です。毎フレーム自キャラの分を 1 回作ります。路地裏（障害物 25 個ほど）でも、光線 1 本あたり 25 回の交差判定です。

### 8.4 .cpp の中だけの関数

| 関数 | 説明 |
|---|---|
| `entryDistanceRect` / `entryDistanceCircle` / `entryDistance` | 点から向きに進んで、形に入るまでの距離（入らなければ無限大。点が中なら 0） |
| `sightBlockers` | 視線を遮る形の一覧（箱と、`ignoreBush` のブッシュを除く） |
| `shapeWithin` | 形の一部が、点から半径以内にあるか |
| `addEdgeAngles` | 角度と、その両隣を加える |
| `wrapAngle` | 角度を −π 以上 π 未満に収める |
