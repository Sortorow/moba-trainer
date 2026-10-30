# game モジュール（4）: 1 回のプレイと操作

## 1. 概要

1 回のプレイ（モードの開始から終了まで）を表す `Match` と、その設定（`MatchConfig`）、プレイの中でキャラを動かす部品（自キャラの操作・練習場のダミー）です。

| ファイル | 内容 |
|---|---|
| [MatchConfig.h](../../src/game/MatchConfig.h) | `GameMode`、`CooldownMode`、`MatchConfig`（メニューで選ぶ設定） |
| [Match.h](../../src/game/Match.h) / [.cpp](../../src/game/Match.cpp) | `Match`（1 回のプレイ）、`MatchResult`（結果） |
| [PlayerController.h](../../src/game/PlayerController.h) / [.cpp](../../src/game/PlayerController.cpp) | `PlayerController`（入力 → 自キャラへの命令） |
| [DummyController.h](../../src/game/DummyController.h) / [.cpp](../../src/game/DummyController.cpp) | `DummyController`（練習場のダミーの動き） |
| [Sandbox.h](../../src/game/Sandbox.h) / [.cpp](../../src/game/Sandbox.cpp) | 自キャラの作り方と、練習場の準備（ダミーの配置） |

### 型の一覧

すべて継承はありません。

| 型 | 種類 | 定義 |
|---|---|---|
| `GameMode`・`CooldownMode` | 列挙型 | MatchConfig.h |
| `MatchConfig` | 構造体 | MatchConfig.h |
| `MatchResult` | 構造体 | Match.h |
| `Match` | クラス | Match.h |
| `PlayerController` | クラス | PlayerController.h |
| `DummyBehavior` | 列挙型 | DummyController.h |
| `DummyBrain` | 構造体 | DummyController.h |
| `DummyController` | クラス | DummyController.h |

---

## 2. MatchConfig.h

### 2.1 `GameMode`（列挙型）

| 値 | 画面の名前 | 説明 |
|---|---|---|
| `Sandbox` | 練習場 | ダミー相手にスキルを試す。制限時間なし |
| `SkillDodge` | スキルドッジ | 敵のスキルを避ける |
| `SkillShot` | スキルショット | 動き回る的にスキルを当てる |

`constexpr string_view displayName(GameMode)` が画面とログの名前を返します。

**背景**（根拠: 指示書「指示書のモードに加えて、ダミー相手にスキルを試せる練習場を用意する（Claude 判断）」）: 練習場は指示書の要件にはなく、手順 1 で操作感を確かめるために足したモードです。

### 2.2 `CooldownMode`（列挙型）

| 値 | 説明 |
|---|---|
| `Standard` | 標準: キャラ設定のクールダウン × `cooldownScale` |
| `None` | なし: スキルをすぐに再使用できる |

**背景**（根拠: `46688e0`、指示書「クールダウン」）: 「標準」の倍率の初期値は 4 分の 1 で、メニューで「実戦どおり」「2 分の 1」「4 分の 1」から選べます。**自キャラだけに効き**、敵のクールダウンは `ai.jsonc` の `enemyCooldownScale` です。

### 2.3 `MatchConfig`（構造体）

1 回のプレイの設定です。メニューで選び、`Match` を作る時に渡します。結果と一緒に保存もします。

| メンバ | 既定値 | 説明 |
|---|---|---|
| `mode` | `Sandbox` | モード |
| `characterId` | 空 | 自キャラ |
| `stageId` | 空 | ステージ。空や見つからない時は一覧の最初 |
| `cooldownMode` | `Standard` | クールダウンの扱い |
| `cooldownScale` | 0.25 | 標準の倍率 |
| `enemyCount` | 3 | スキルドッジの敵の数（1〜5） |
| `aiLevel` | 3 | スキルドッジの敵の賢さ（1〜5） |
| `targetLevel` | 3 | スキルショットの的の賢さ（1〜5） |
| `targetRadius` | 65 | 的の大きさ（ai.jsonc の `skillShot.sizes` から選ぶ） |
| `targetMoveSpeed` | 340 | 的の移動速度（`skillShot.moveSpeeds` から選ぶ） |
| `timeLimitSeconds` | 120 | 制限時間 |
| `seed` | 0 | 乱数の種。**0 なら `Match` を作る時に決める**。ログに残るので、同じ展開を再現できる |
| `float playerCooldownScale() const` | — | 自キャラに実際に掛ける倍率（「なし」なら 0） |
| `bool timed() const` | — | 制限時間があるモードか（練習場以外で、制限時間が 0 より大きい） |

**注意事項**: どのモードでも、すべての項目を持っています（スキルドッジでも `targetLevel` を持つ）。使わない項目は無視されます。リザルトの JSON には、モードに関係する項目だけを書きます。

---

## 3. `MatchResult`（構造体）

1 回のプレイの結果です。リザルト画面に出し、`ResultStore` が保存します。

| メンバ | 説明 |
|---|---|
| `config` | プレイの設定（キャラ・ステージが見つからず代わりを使った場合は、直したもの） |
| `characterName` / `stageName` | キャラとステージの表示名（保存した時の名前。後で名前を変えても、保存した結果の表示は変わらない） |
| `aiLevelName` | スキルドッジは敵の賢さの名前、スキルショットは的の賢さの名前 |
| `targetSizeName` | スキルショットの的の大きさの名前（選択肢に無い大きさなら空） |
| `playedSeconds` | 遊んだ時間 |
| `dodge` | スキルドッジのドッジ率の集計（`DodgeCount`） |
| `hitsTaken` | 被弾した回数（= `dodge.hit`。被弾した発動の数） |
| `deaths` | HP が 0 になった回数 |
| `playerHits` | 自キャラのヒット率（集計対象のスキルだけ。制限時間の終了時に飛んでいる途中だった発動は含めない） |
| `playerHitsBySlot` | キーごとのヒット率 |

**注意事項**: スキルドッジでも `playerHits` は記録されます（自キャラが攻撃スキルを使えるため）。リザルト画面や統計では使っていません。

---

## 4. `Match`（クラス）

**役割**: 1 回のプレイ（練習場・スキルドッジ・スキルショット）。`World` と、自キャラ・ダミー・敵 AI・的の AI の操作、制限時間、結果をまとめて持ちます。

**背景**（推測）: 手順 2 でスキルドッジを足した時に、モードごとの準備（敵の配置・AI）と制限時間を `World` の外にまとめるために作られたと考えられます（`World` はモードを知らない）。

**寿命**: `App` が `unique_ptr<Match>` で持ちます。プレイを始めるたびに作り、メニューに戻ると捨てます。「もう一度」「リスタート」も作り直しです。

### 4.1 コンストラクタ `Match(const GameData& data, MatchConfig config)`

プレイを始めます。

1. キャラを探す。見つからなければ一覧の最初のキャラ。**1 体も無ければ `world_` を作らずに終わる**（`valid()` が `false`）
2. 乱数の種が 0 なら `std::random_device` で決める（`| 1u` で 0 にならないようにする）
3. 敵の数・賢さ・的の大きさ・速さを範囲に収める
4. ステージを探す。見つからなければ一覧の最初。ステージのファイルが 1 つも無ければ、障害物のない広場（`kSandboxBounds`）
5. `World` を作り、陣営ごとのクールダウンの倍率を決める
6. モードごとに準備する
   - 練習場: `setupSandbox`（自キャラとダミー）
   - スキルドッジ: `setupSkillDodge`
   - スキルショット: `setupSkillShot`
7. 設定と乱数の種をログに出す

### 4.2 公開メンバ関数

| 関数 | 説明 |
|---|---|
| `bool valid() const` | 開始できたか（キャラの設定が 1 つも無い時は `false`） |
| `void update(float dt, const InputFrame& input)` | ゲームを `dt` 秒進める。自キャラの操作 → ダミー → 敵 AI → 的の AI → `World` の順。制限時間になったら `finished_` にして、結果をログに出す |
| `bool finished() const` | 制限時間になって終わったか |
| `float elapsed() const` | 始めてからの時間 |
| `float remaining() const` | 制限時間の残り（制限なしなら 0） |
| `MatchResult result() const` | 今の結果。**呼ぶたびに `World` の集計から作る** |
| `const MatchConfig& config() const` | 始めた時の設定（直したもの） |
| `World& world()`（const 版もある） | ゲームの中身 |
| `DummyController& dummies()` | 練習場のダミーの動き（デバッグパネルが変える） |
| `const ai::EnemyDirector& enemies() const` / `const ai::SkillShotTarget& target() const` | AI（デバッグパネル・リザルト） |
| `void refreshDummiesIfChanged()` | 練習場で、ダミーの設定（プリセットの選択や sandbox.jsonc の中身）が変わっていれば、ダミーを置き直す。`GameData::sandboxRevision()` と、置いた時の番号を比べる |

### 4.3 非公開メンバ関数

| 関数 | 説明 |
|---|---|
| `void setupSkillDodge(const CharacterDef& character)` | 自キャラ（HP 5）を置き、敵 AI を準備し、自キャラを囲むように敵を置く。敵のキャラは `enemyCandidates()` からランダム。角度は均等＋ばらつき（±0.25 ラジアン）、距離は 1000〜1250。障害物と重なれば近くの立てる位置へ |
| `void setupSkillShot(const CharacterDef& character)` | 自キャラ（HP なし）を置き、的の AI を準備し、自キャラから離れたランダムな方向に的を 1 体置く。的の名前は「的」、色は橙、大きさ・速さはメニューで選んだもの |

### 4.4 メンバ変数

| 変数 | 説明 |
|---|---|
| `data_` | `const GameData&` |
| `config_` | 設定 |
| `world_` | `unique_ptr<World>`（作れなかった時は空） |
| `player_` | `PlayerController` |
| `dummies_` | `DummyController` |
| `enemies_` | `ai::EnemyDirector` |
| `target_` | `ai::SkillShotTarget` |
| `sandboxRevision_` | 今のダミーを置いた時の `GameData::sandboxRevision()` |
| `elapsed_` / `finished_` | 経過時間と、終わったか |

### 4.5 .cpp の中だけの定数

| 定数 | 値 | 説明 |
|---|---|---|
| `kPlayerMaxHp` | 5 | スキルドッジの自キャラの HP（根拠: 指示書「HP ゲージは 5 段階」） |
| `kEnemySpawnMin` / `Max` | 1000 / 1250 | 敵が現れる距離 |
| `kTargetColor` | 橙 | 的の色（根拠: 指示書「名前が「的」の橙色の丸として登場する」） |

### 4.6 注意事項

- `valid()` が `false` の `Match` で `world()` を呼ぶと、空の `unique_ptr` を参照します。`App::startMatch` が `valid()` を確かめてから使っています。
- 使わないモードの部品（練習場の `enemies_` など）も毎刻み `update` されますが、中身が空なので何もしません。
- スキルドッジで、敵として出られるキャラが 1 体もいない時は、エラーをログに出して敵なしで始まります。
- 制限時間の判定は `World::update` の**後**です。最後の刻みで当たった弾も数えます。

---

## 5. `PlayerController`（クラス）

**役割**: 入力を自キャラへの命令（移動・スキル）に変えます。

| 関数 | 説明 |
|---|---|
| `void update(World& world, const InputFrame& input, float dt)` | 右クリックした瞬間は `casting::orderMove` とクリックの印。**押しっぱなしの間は 0.1 秒ごとに**目的地をカーソル位置に更新する。Q〜F を押した瞬間は `casting::requestCast`（カーソルの位置を狙う） |

| メンバ | 説明 |
|---|---|
| `kMoveRepeatInterval = 0.1` | 押しっぱなしの時に目的地を更新する間隔 |
| `moveRepeatTimer_` | 次に更新するまでの時間 |

**背景**（根拠: README「右クリック: 移動（押しっぱなしでカーソルを追いかける）」）: MOBA の右クリックの長押しと同じ操作感にするためです。毎刻み更新しないのは、経路探索の回数を減らすためと考えられます（推測）。

---

## 6. 練習場のダミー（DummyController.h・Sandbox.h）

### 6.1 `DummyBehavior`（列挙型）と `DummyBrain`（構造体）

| 値 | 動き |
|---|---|
| `Stationary` | その場で動かない（押し出されたら元の位置へ戻る） |
| `Patrol` | 2 点の間を往復する |

| `DummyBrain` のメンバ | 説明 |
|---|---|
| `unitId` | ダミーの番号 |
| `behavior` | 動き方 |
| `home` | 静止する位置 / 往復の一方 |
| `patrolTo` | 往復のもう一方 |
| `headingToPatrolTo` | 次に `patrolTo` へ向かうか |

### 6.2 `DummyController`（クラス）

| 関数 | 説明 |
|---|---|
| `void clear()` | すべてのダミーの動きを消す（ダミーのキャラは `World` に残るので、先に取り除いておく） |
| `void add(const DummyBrain&)` | ダミー 1 体の動きを足す |
| `void update(World&)` | 静止のダミーは、元の位置から 1 以上離れていて移動していなければ、元の位置へ向かう。往復のダミーは、目的地に着いたら反対側へ向かう |
| `vector<DummyBrain>& brains()` | すべての動き（デバッグパネルで一時的に変えられる） |

### 6.3 Sandbox.h の関数と定数

| 名前 | 説明 |
|---|---|
| `kSandboxBounds` | `{-2500, -1600, 5000, 3200}`。ステージのファイルが 1 つも無い時の広さ。テストでも、障害物のない `World` を作るのに使う |
| `Unit makePlayerUnit(const CharacterDef&)` | 自キャラを作る（大きさ・移動速度・色を写し、(0, 0) に置く）。**`World` にはまだ加えない**。すべてのモードで使う |
| `void setupSandbox(World&, DummyController&, const CharacterDef&, const SandboxPreset*)` | 練習場の準備: 自キャラとプリセットのダミーを置く。`preset` が `nullptr` ならダミーは置かない |
| `void placeDummies(World&, DummyController&, const SandboxPreset*)` | 今のダミーを `World` から取り除き、プリセットのダミーを置き直す。障害物と重なる位置は近くの立てる位置へずらす（往復先も） |

**背景**（根拠: `3bd20ec`「プリセットはメニューと、練習場の画面右下のパネルで選ぶ。選ぶとすぐにダミーを置き直す」「練習場の途中で sandbox.jsonc を保存した時も、中身が変わっていればダミーを置き直す」）。

**注意事項**: 手順 3 以降、練習場も選んだステージの地形で遊びます。`kSandboxBounds` が使われるのは、ステージのファイルが無い時とテストだけです。
