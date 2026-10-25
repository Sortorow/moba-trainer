# data フォルダの設定ファイル

ゲームの数値や動きは、`data` フォルダの設定ファイルで調整します。この資料では、各ファイルの用途、書ける項目、そのファイルを読むソースコードをまとめます。

## 目次

1. [全体像](#1-全体像)
2. [共通のルール（書き方・単位・再読み込み）](#2-共通のルール)
3. [settings.jsonc（ゲーム全体の設定）](#3-settingsjsoncゲーム全体の設定)
4. [ai.jsonc（スキルドッジの敵 AI・スキルショットの的）](#4-aijsoncスキルドッジの敵-aiスキルショットの的)
5. [sandbox.jsonc（練習場のダミー）](#5-sandboxjsonc練習場のダミー)
6. [stages/*.jsonc（ステージ）](#6-stagesjsoncステージ)
7. [characters/*.jsonc（キャラとスキル）](#7-charactersjsoncキャラとスキル)
8. [handler ごとの params 一覧](#8-handler-ごとの-params-一覧)
9. [効果（onHit など）の書き方](#9-効果onhit-などの書き方)
10. [参照しているソースコードの一覧](#10-参照しているソースコードの一覧)

---

## 1. 全体像

```
data/
├─ settings.jsonc          ゲーム全体の設定（画面・カメラ・クールダウンの初期値・ログ）
├─ ai.jsonc                スキルドッジの敵 AI とスキルショットの的（賢さ 1〜5 ごとの動き方）
├─ sandbox.jsonc           練習場のダミー（大きさ・速さ・数・位置・動き方のプリセット）
├─ stages/
│   ├─ open.jsonc          ステージ 1 つにつき 1 ファイル（広さ・障害物・ブッシュ）。書き方の説明は open.jsonc の先頭
│   ├─ alley.jsonc
│   └─ forest.jsonc
└─ characters/
    ├─ like_ezreal.jsonc   キャラ 1 体につき 1 ファイル（ステータスとスキル）
    ├─ like_emma.jsonc
    └─ like_brand.jsonc
```

| ファイル | 用途 | 読み込むコード | 値を使うコード |
|---|---|---|---|
| `settings.jsonc` | 画面・カメラ・クールダウン倍率の初期値・ログの詳しさ | `src/data/Settings.cpp` | `src/app/App.cpp`、`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ App.cpp から `src/render/Renderer.cpp`、`src/render/CameraControl.cpp`、`src/render/Hud.cpp`、`src/ui/UiScale.cpp`、`src/core/Log.cpp` へ渡される |
| `ai.jsonc` | スキルドッジの敵の強さ・動き方と、スキルショットの的の大きさ・移動速度の選択肢・動き方 | `src/data/AiConfig.cpp` | `src/game/Match.cpp`、`src/game/ai/EnemyAI.cpp`、`src/game/ai/SkillShotTarget.cpp`、`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ EnemyAI.cpp から `src/game/ai/Aim.cpp` へ、Match.cpp から `src/ui/ResultScreen.cpp` へ渡される。メニューで選んだ的の大きさ・移動速度はキャラの情報（`Unit`）へコピーされて `src/game/World.cpp`、`src/render/Renderer.cpp` などで使われる |
| `sandbox.jsonc` | 練習場のダミーのプリセット | `src/data/SandboxConfig.cpp` | `src/data/GameData.cpp`、`src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/ui/SandboxPanel.cpp`<br>→ Sandbox.cpp から `src/game/DummyController.cpp` へ、ダミーの大きさ・移動速度・色はキャラの情報（`Unit`）へコピーされて `src/game/World.cpp`、`src/render/Renderer.cpp` などで使われる（[5 章](#5-sandboxjsonc練習場のダミー)の表を参照）。SandboxPanel.cpp は `src/ui/MainMenu.cpp` と `src/app/App.cpp` から呼ばれる |
| `stages/*.jsonc` | ステージの広さ・障害物・ブッシュ | `src/data/StageDef.cpp` | `src/data/GameData.cpp`、`src/game/Match.cpp`、`src/game/Terrain.cpp`、`src/ui/MainMenu.cpp`<br>→ Terrain.cpp（地形）は `src/game/World.cpp` が持ち、`src/game/SkillCasting.cpp`、`src/game/skills/SkillHelpers.cpp`、`src/game/Sandbox.cpp`、`src/game/ai/EnemyAI.cpp`、`src/render/Renderer.cpp`、`src/render/Hud.cpp`、`src/render/CameraControl.cpp`（`src/app/App.cpp` から）で使われる。ステージの名前は Match.cpp から `src/ui/ResultScreen.cpp` へ渡される（[6 章](#6-stagesjsoncステージ)を参照） |
| `characters/*.jsonc` | キャラのステータスとスキルの数値・動き | `src/data/CharacterDef.cpp` | `src/app/App.cpp`<br>`src/data/GameData.cpp`<br>`src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`、`src/game/SkillCasting.cpp`<br>`src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`、`SkillRegistry.cpp`<br>`src/game/ai/`：`Aim.cpp`、`EnemyAI.cpp`<br>`src/render/Hud.cpp`<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ キャラの大きさ・移動速度・色は配置時にキャラの情報（`Unit`）へコピーされ、`src/render/Renderer.cpp` などでも使われる（[7 章](#7-charactersjsoncキャラとスキル)の表を参照） |

5 種類のファイルは、すべて `src/data/GameData.cpp` がまとめて読み込みます。

この資料の「使うコード」には、その値を使うファイルをすべて書いています。値が別のファイルへ渡されてから使われる場合は、「→」で渡し先も書いています。自動テスト（`tests/` フォルダ）での使い方は、[10 章](#10-参照しているソースコードの一覧)にまとめています。

---

## 2. 共通のルール

### 書き方

- 形式は **JSONC**（コメントを書ける JSON）です。`//` と `/* */` のコメントが使えます。
- 項目を省略すると、それぞれの既定値が使われます（各表の「既定値」の列）。

### 単位

| 種類 | 単位 | 例 |
|---|---|---|
| 距離・射程・幅・半径 | ゲーム内単位（LoL と同じ）。**100 ≒ ER の 1m** | 射程 `1200` = マス目 12 個分 |
| 速度（移動速度・弾速） | ゲーム内単位 / 秒 | `325` = 1 秒にマス目 3.25 個分 |
| 時間 | 秒 | `0.25` = 0.25 秒 |
| 割合 | 0.0〜1.0 | `0.4` = 40% |
| 色 | `[R, G, B]` または `[R, G, B, A]`（各 0〜255） | `[255, 140, 60]` |

練習場の床のマス目は、細い線が 100、太い線が 500 ごとです。

### 再読み込みとエラー

- **ゲームの起動中にファイルを保存すると、1 秒以内に自動で再読み込みされます**（`F5` でもすぐに再読み込みできます）。担当は `GameData::reloadIfChanged()`（`src/data/GameData.cpp`）で、1 秒ごとに `src/app/App.cpp` から呼ばれます。
- 書き方の誤りなどで読み込めなかったファイルは、**前回読み込めた内容を使い続けます**。エラーの内容は、メニュー画面とデバッグパネル（`F1`）に赤字で表示され、ログにも記録されます。
- 必須の項目（下の表で「必須」と書いたもの）が無い時は、既定値を使ってログに警告を 1 回出します。

---

## 3. settings.jsonc（ゲーム全体の設定）

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `castMode` | `"quick"` | スキルの発動方式。今は `"quick"`（キーを押した瞬間に発動）だけ | `src/data/Settings.cpp`（未対応の値を書いた時の警告だけ。値を使うコードはまだ無い） |
| `cameraMode` | `"locked"` | 起動時のカメラ。`"locked"`（自キャラに固定）/ `"free"`（固定しない。画面の端にカーソルを置くとその方向へ動く）。メニューとプレイ中の `Y` キーでも切り替えられる。設定ファイルのこの値を書き換えて保存した時だけ、今のカメラに反映する | `src/app/App.cpp`（今のカメラとして保持する・`Y` キーでの切り替え）<br>→ `src/render/CameraControl.cpp`（カメラの動かし方）、`src/ui/MainMenu.cpp`（メニューでの選択）、`src/render/Hud.cpp`（操作説明に今のカメラを表示） |
| `cameraViewHeight` | `1500` | 画面の縦方向に映るゲーム内の距離。大きいほど引いた視点（400〜6000） | `src/app/App.cpp`（カメラの拡大率）<br>`src/ui/DebugPanel.cpp`（一時的に変えるスライダー） |
| `cameraEdgeScrollSpeed` | `2500` | カメラを固定しない時に、画面の端でカメラが動く速さ（ゲーム内の距離 / 秒。0 以上） | `src/app/App.cpp` → `src/render/CameraControl.cpp` |
| `cameraEdgeMargin` | `12` | カメラを固定しない時に、画面の端からこの幅まで近づくとカメラが動く（Windows の拡大率 100% の時のピクセル。1〜100） | `src/app/App.cpp`（Windows の拡大率と `uiScale` を掛ける）→ `src/render/CameraControl.cpp` |
| `bushRevealTime` | `3` | ブッシュの中で対象指定のスキルを使った時に、外から見えるようになる時間（秒。0〜30）。詳しくは [6 章](#6-stagesjsoncステージ)の「ブッシュの見え方」 | `src/game/World.cpp`（`src/game/SkillCasting.cpp` から呼ばれる） |
| `cooldownScale` | `0.25` | クールダウン「標準」モードの倍率の初期値（0 以上）。メニューで変えられる | `src/ui/MainMenu.cpp`（メニューの初期値と選択肢）<br>`src/app/App.cpp`（コマンドラインで開始する時の初期値） |
| `showHitboxes` | `true` | 弾・範囲の当たり判定の円を表示するか | `src/app/App.cpp` → `src/render/Renderer.cpp`（描画）<br>`src/ui/DebugPanel.cpp`（一時的に変えるチェックボックス） |
| `uiScale` | `1.0` | 文字・ボタンの大きさの倍率。Windows の拡大率にさらに掛ける（0.5〜3.0）。詳しくは [dpi-scaling.md](dpi-scaling.md) | `src/app/App.cpp` → `src/ui/UiScale.cpp` |
| `window.width` / `window.height` | `1600` / `900` | 起動時のウィンドウの大きさ（Windows の拡大率 100% の時の大きさ。最小 640×360） | `src/app/App.cpp` → `src/ui/UiScale.cpp`（画面に収める計算） |
| `logLevel` | `"debug"` | ログの詳しさ。`"trace"` / `"debug"` / `"info"` / `"warn"` / `"error"` | `src/app/App.cpp` → `src/core/Log.cpp` |

デバッグパネル（`F1`）から「視点の広さ」「当たり判定の円を表示」を一時的に変えられますが、設定ファイルを保存し直すと上書きされます。

---

## 4. ai.jsonc（スキルドッジの敵 AI・スキルショットの的）

読み込みは `src/data/AiConfig.cpp` です。新しい設定は、**次に開始したスキルドッジ・スキルショットから**反映されます。前半がスキルドッジの敵、`skillShot` の中がスキルショットの的の設定です。

### 全体の項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `enemyCooldownScale` | `0.5` | 敵のスキルのクールダウン倍率（キャラ設定の値 × この倍率） | `src/game/Match.cpp` → `src/game/World.cpp`（敵のクールダウン倍率として保持） |
| `firstCastDelay` | `1.5` | 開始してから、敵が最初にスキルを撃つまでの時間（秒） | `src/game/ai/EnemyAI.cpp` |
| `maxEngageRange` | `1000` | 敵が自キャラと取ろうとする距離の上限（200 以上） | `src/game/ai/EnemyAI.cpp` |
| `playerReactionTime` | `0.25` | AI が想定する「プレイヤーが弾を見てから避け始めるまでの時間」（秒） | `src/game/ai/EnemyAI.cpp` → `src/game/ai/Aim.cpp` |
| `levels` | （必須） | 賢さ 1〜5 の設定。上から順に 1, 2, 3, 4, 5 | 下の表と、表の後の説明を参照 |
| `skillShot` | （必須） | スキルショットの的の設定 | [skillShot の項目](#skillshot-の項目スキルショットの的)を参照 |

### levels の各項目（賢さ 1 段階分）

| 項目 | 意味 | 値の範囲 |
|---|---|---|
| `name` | 表示名（メニュー・リザルト） | 文字列 |
| `reactionTime` | 自キャラの動きを認識するまでの遅れ（秒）。小さいほど、今の動きに合わせて撃ってくる | 0 以上 |
| `castInterval` | スキルを撃つ間隔 `[最小, 最大]`（秒）。この範囲でランダム | 最小 0.1 以上 |
| `aimError` | 狙いのぶれ（ゲーム内単位）。直線の弾は横方向にだけぶれる | 0 以上 |
| `predict.stay` | 狙う時に「自キャラがその場に留まる」と想定する重み | 0 以上 |
| `predict.continue` | 「今の移動を続ける」と想定する重み（偏差撃ち） | 0 以上 |
| `predict.dodge` | 「弾を見て横に避ける」と想定する重み（避けた先を狙う） | 0 以上 |
| `adaptive` | 自キャラが左右どちらに避けやすいかを学習して、`dodge` の左右の配分を変える | `true` / `false` |
| `minHitChance` | 命中見込みがこれ未満の時は、撃つのを少し待つ（待ちすぎない） | 0〜1 |
| `strafe` | 横移動の多さ | 0〜1 |
| `flank` | 他の敵と離れて、囲むように動く | `true` / `false` |
| `punishCC` | 自キャラが動けない時（拘束・スタン・打ち上げ）は、間隔を待たずにすぐ撃つ | `true` / `false` |
| `engageRangeRatio` | 自キャラとの距離を「使うスキルの最短の射程 × この値」に保とうとする | 0.3〜1.0 |

`predict` の 3 つは割合なので、合計が 1 でなくてもかまいません（重みの比で扱います）。狙いの計算方法は `src/game/ai/Aim.h` の先頭に書いてあります。

**levels を使うコード**

| コード | 使い方 |
|---|---|
| `src/game/Match.cpp` | メニューで選んだ賢さの段階を `src/game/ai/EnemyAI.cpp` に渡す。リザルト用に `name` を取り出す（→ `src/ui/ResultScreen.cpp` が表示） |
| `src/game/ai/EnemyAI.cpp` | すべての項目を、敵の動きとスキル選びに使う |
| `src/ui/MainMenu.cpp` | `name`（敵の賢さのスライダーの表示）と、段階の数 |
| `src/ui/DebugPanel.cpp` | `name`・`reactionTime`・`castInterval`・`aimError`・`predict`・`adaptive` を表示 |

### skillShot の項目（スキルショットの的）

的は 1 体で、スキルを撃ちません。動き方のパターンを組み合わせて動き回り、賢さに応じて自キャラのスキルを避けます。動き方の考え方は `src/game/ai/SkillShotTarget.h` の先頭に書いてあります。

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `sizes` | （必須） | 的の大きさの選択肢。`name`（メニューに出す名前）と `radius`（当たり判定の半径。10〜300）の組の配列。練習場のダミーのプリセット（小さい的 40 / 標準 65 / 大きい的 100）と同じ段階 | `src/ui/MainMenu.cpp`（選択肢の表示）→ 選んだ半径は `src/game/Match.cpp` が的の情報（`Unit`）へコピーし、`src/game/World.cpp`（当たり判定）、`src/game/ai/SkillShotTarget.cpp`（避ける距離）、`src/render/Renderer.cpp`（大きさ）で使われる。`src/game/Match.cpp` はリザルト用に `name` も取り出す（→ `src/ui/ResultScreen.cpp` が表示） |
| `moveSpeeds` | （必須） | 的の移動速度の選択肢（0〜1500）の配列 | `src/ui/MainMenu.cpp`（選択肢の表示）→ 選んだ速度は `src/game/Match.cpp` が的の情報（`Unit`）へコピーし、`src/game/World.cpp`（移動）、`src/game/ai/SkillShotTarget.cpp`（避けるのに間に合うかの見積もり）で使われる |
| `minDistance` | `300` | 自キャラにこれより近づかない | `src/game/ai/SkillShotTarget.cpp` |
| `maxDistance` | `1100` | 自キャラからこれより離れない（11m）。離れすぎていたら、次のパターンは接近になる | `src/game/ai/SkillShotTarget.cpp` |
| `dodgeMargin` | `25` | 避ける時に、弾の幅・範囲の端から、さらにこの距離だけ離れる | `src/game/ai/SkillShotTarget.cpp` |
| `levels` | （必須） | 賢さ 1〜5 の設定。上から順に 1, 2, 3, 4, 5 | 下の表を参照 |

**skillShot.levels の各項目（賢さ 1 段階分）**

賢さには「避け方」と「動き方のパターンの混ぜ方」の 2 つの軸があります。

| 項目 | 意味 | 値の範囲 |
|---|---|---|
| `name` | 表示名（メニュー・リザルト） | 文字列 |
| `reactionTime` | 自キャラの弾・範囲・詠唱に気づいて動き始めるまでの遅れ（秒）。避け始めと、接近からの切り返しに使う | 0.15 以上（小さい値を書いても 0.15 として扱う） |
| `dodgeChance` | 気づいた弾・範囲を避けようとする確率。0 なら避けない | 0〜1 |
| `readCast` | 自キャラの詠唱（構え）を見た時点で、弾・範囲が出る前に、カーソルの方向・位置へ出ると考えて避け始める | `true` / `false` |
| `sideRandom` | 直線の弾を避ける時に、近い側ではなく遠い側へ避ける確率（避ける向きを読まれにくくする） | 0〜1 |
| `patterns.strafe` | 次のパターンに「往復」を選ぶ重み。自キャラから見て左右に、同じ幅で切り返す | 0 以上 |
| `patterns.approach` | 「接近」の重み。自キャラへまっすぐ近づき、撃たれそうになったら（射程に入った・自キャラが止まった・構えた・近づききった）、`reactionTime` の後に急に左右へ 250〜400 動く | 0 以上 |
| `patterns.wander` | 「ランダム」の重み。自キャラの周りのランダムな位置へ動く（障害物・ブッシュの中は選ばない） | 0 以上 |
| `patterns.stop` | 「停止」の重み。`stopTime` の間、立ち止まる | 0 以上 |
| `strafeSwings` | 1 回の往復で切り返す回数 `[最小, 最大]`。同じ値なら毎回同じ回数（リズムを読まれやすい） | 1 以上 |
| `strafeWidth` | 往復の幅 `[最小, 最大]`（100 = 1m）。1 回の往復の間は同じ幅で、往復ごとにこの範囲から選ぶ | 30 以上 |
| `rhythmBreak` | 往復のリズムを崩す確率。2 回切り返した後から、切り返すたびに判定し、崩す時は「いつもより大きく（2.5 倍）動く」か「少し（4 分の 1）だけ動いてすぐ切り返す」 | 0〜1 |
| `stopTime` | 立ち止まる時間 `[最小, 最大]`（秒） | 0.1 以上 |

`patterns` の 4 つは割合なので、合計が 1 でなくてもかまいません（重みの比で扱います）。1 つのパターンが終わるたびに、重みで次のパターンを選びます（避けた後も、新しいパターンを選び直します）。

避けるかどうかは、弾・範囲 1 つ（1 回の発動）につき 1 回だけ `dodgeChance` で決めます。`readCast` で詠唱を見て決めた時は、その詠唱から出た弾・範囲にも同じ判断を使います。追尾する弾（対象指定のスキル）は避けません。避ける先は、自キャラから `maxDistance` 以内に限ります。

弾が速いスキル（エズリアルの Q など）は、弾を見てから（`reactionTime` の後に）避け始めても間に合わないことが多く、`readCast` のある賢さ 4・5 の的だけが避けられます。範囲の真ん中に置かれた時も、発動までに外へ出られなければ当たります。

**skillShot を使うコード**

| コード | 使い方 |
|---|---|
| `src/data/AiConfig.cpp` | 読み込む。`reactionTime` を 0.15 秒（`AiConfig.h` の `kMinTargetReactionTime`）以上に収める |
| `src/game/Match.cpp` | メニューで選んだ大きさ・移動速度で的を置く。的の賢さの段階を `src/game/ai/SkillShotTarget.cpp` に渡す。リザルト用に、賢さの `name` と大きさの `name` を取り出す（→ `src/ui/ResultScreen.cpp` が表示） |
| `src/game/ai/SkillShotTarget.cpp` | `minDistance`・`maxDistance`・`dodgeMargin` と `levels` のすべての項目を、的の移動と回避に使う。詠唱を見る時と、接近で「射程に入った」かを見る時は、キャラの設定（スキルの `targeting` / `castTime` / `range` / `countsForHitRate` と、`params` の `speed` / `width` / `radius` / `delay`）を `src/game/ai/Aim.cpp` を通して使う |
| `src/ui/MainMenu.cpp` | 的の賢さの `name` と段階の数、`sizes`（名前と半径）、`moveSpeeds` を表示して選ぶ |
| `src/ui/DebugPanel.cpp` | `levels` の項目と、的の状態（今のパターン・避けているか・避けた回数）を表示 |

---|---|
| `src/game/Match.cpp` | 的を置く（`targetRadius`・`targetMoveSpeed`）。メニューで選んだ的の賢さの段階を `src/game/ai/SkillShotTarget.cpp` に渡す。リザルト用に `name` を取り出す（→ `src/ui/ResultScreen.cpp` が表示） |
| `src/game/ai/SkillShotTarget.cpp` | すべての項目を、的の移動と回避に使う。詠唱を見る時は、キャラの設定（スキルの `targeting` / `castTime` / `range` と、`params` の `speed` / `width` / `radius` / `delay`）を `src/game/ai/Aim.cpp` を通して使う |
| `src/ui/MainMenu.cpp` | `name`（的の賢さのスライダーの表示）と、段階の数 |
| `src/ui/DebugPanel.cpp` | `levels` のすべての項目と、的の状態（避けているか・避けた回数）を表示 |

---

## 5. sandbox.jsonc（練習場のダミー）

練習場に置くダミー（的）の組み合わせを、**プリセット**として複数書いておくファイルです。読み込みは `src/data/SandboxConfig.cpp` です。

- プリセットは、メニュー（モードで「練習場」を選んだ時）と、練習場の画面右下の「練習場のダミー」で選べます。
- 選ぶとすぐにダミーが置き直され、選んだプリセットの名前がこのファイルの `"preset"` に書き戻されます（次に起動した時も、そのプリセットで始まります）。書き戻すのは `"preset"` の値だけで、コメントや他の行はそのまま残ります。
- 練習場で遊んでいる途中にこのファイルを保存すると、中身が変わっていればダミーが置き直されます。
- デバッグパネル（`F1`）の「ダミー」で、ダミーごとの動き方・移動速度を一時的に変えることもできます（ファイルには保存されません）。

### プリセットの選択と一覧

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `preset` | 最初のプリセット | 今選んでいるプリセットの名前。`presets` に無い名前なら、最初のプリセットを使う | `src/data/SandboxConfig.cpp`（読み込み・値の書き戻し）<br>`src/data/GameData.cpp`（選び直す処理）<br>`src/ui/SandboxPanel.cpp`（選ぶ UI。`src/ui/MainMenu.cpp` と `src/app/App.cpp` から呼ばれる）<br>`src/game/Match.cpp` → `src/game/Sandbox.cpp`（開始時と、選び直した時にダミーを置く） |
| `presets` | （必須） | プリセットの一覧（下の表）。名前が重複したプリセットは、後の方を使わない | `src/data/SandboxConfig.cpp`（読み込み）<br>`src/ui/SandboxPanel.cpp`（選ぶ UI の一覧） |

### プリセットの項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `name` | `プリセット1` など | 名前。メニューと練習場の画面に出る | `src/data/SandboxConfig.cpp`（`preset` との照合）<br>`src/ui/SandboxPanel.cpp`（表示）<br>`src/game/Sandbox.cpp`（ログ） |
| `description` | 空 | 説明。選んでいるプリセットの下に出る | `src/ui/SandboxPanel.cpp` |
| `radius` | `65` | このプリセットのダミーの当たり判定の半径（10〜400） | `src/data/SandboxConfig.cpp`（ダミーごとの `radius` の既定値にする） |
| `moveSpeed` | `300` | このプリセットのダミーの移動速度（0〜2000） | `src/data/SandboxConfig.cpp`（ダミーごとの `moveSpeed` の既定値にする） |
| `color` | `[215, 75, 75]`（赤） | このプリセットのダミーの色 | `src/data/SandboxConfig.cpp`（ダミーごとの `color` の既定値にする） |
| `dummies` | `[]` | ダミーの一覧（下の表）。0 体でもよい | `src/game/Sandbox.cpp`（ダミーを置く）<br>`src/ui/SandboxPanel.cpp`（体数・静止と往復の数・半径をまとめて表示） |

### ダミーの項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `name` | `ダミー1` など | 名前。ダミーの頭上に出る | `src/game/Sandbox.cpp`（キャラの情報にコピー）→ `src/render/Hud.cpp`（頭上に表示）、`src/ui/DebugPanel.cpp`（ダミーの一覧） |
| `position` | `[0, 0]`（書かないと警告） | 置く位置 `[x, y]`。自キャラは (0, 0) から始まる。x は右、y は下がプラス。ステージの外なら端に寄せ、建物や木と重なる位置なら近くの立てる位置へずらす（路地裏・森では、プリセットの位置からずれることがある） | `src/game/Sandbox.cpp`（置く位置・静止する位置。ずらす計算は `src/game/World.cpp` → `src/game/Terrain.cpp`）→ `src/game/DummyController.cpp`（押し出された時に戻る位置・往復の一方） |
| `patrolTo` | なし | 書くと、`position` とこの位置の間を往復する。書かなければ静止する | `src/game/Sandbox.cpp`（動き方を決める）→ `src/game/DummyController.cpp`（往復） |
| `radius` | プリセットの `radius` | このダミーだけの当たり判定の半径 | `src/game/Sandbox.cpp`（キャラの情報にコピー）→ 自キャラ・敵と同じく、キャラの当たり判定の半径として使われる（使うコードは [7 章](#7-charactersjsoncキャラとスキル)の `hitboxRadius` の行と同じ）<br>`src/ui/SandboxPanel.cpp`（半径の表示） |
| `moveSpeed` | プリセットの `moveSpeed` | このダミーだけの移動速度 | `src/game/Sandbox.cpp`（キャラの情報にコピー）→ `src/game/World.cpp`（移動）、`src/ui/DebugPanel.cpp`（一時的に変えるスライダー） |
| `color` | プリセットの `color` | このダミーだけの色 | `src/game/Sandbox.cpp`（キャラの情報にコピー）→ `src/render/Renderer.cpp`（描画） |

---

## 6. stages/*.jsonc（ステージ）

ステージ 1 つにつき 1 ファイルです。今は `open.jsonc`（障害物なし）・`alley.jsonc`（路地裏）・`forest.jsonc`（森）の 3 つがあります。読み込みは `src/data/StageDef.cpp`、地形の計算（当たり・押し出し・経路探索・ブッシュ）は `src/game/Terrain.cpp` です。

- ステージは、メニューの「ステージ」で選びます。練習場・スキルドッジ・スキルショットのどれでも使えます。
- ゲームの起動中にファイルを保存すると再読み込みされ、**次に始めたプレイから**反映されます（遊んでいる途中のステージは変わりません）。
- 地形が妨げるのは**移動だけ**です。弾や範囲は、壁や木を通り抜けます。
- 壁の向こうを右クリックすると、壁を回り込む道のりを自動で探して進みます（敵も同じ）。建物の中など立てない場所を指定した時は、その近くの立てる場所まで進みます。たどり着けない場所なら、その場で止まります。
- ワープ（フラッシュなど）で建物の中を指定した時は、その近くの立てる位置に出ます。

### ステージの項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `id` | （必須） | ステージの ID。ファイル同士で重複してはいけない | `src/data/GameData.cpp`（一覧・重複のチェック）<br>`src/game/Match.cpp`（選んだステージを探す）<br>`src/ui/MainMenu.cpp`（ステージの選択）<br>`src/app/App.cpp`（コマンドラインの `--stage`） |
| `displayName` | `id` と同じ | メニューとリザルトに出る名前 | `src/ui/MainMenu.cpp`（ステージの一覧）<br>`src/game/Match.cpp` → `src/ui/ResultScreen.cpp`（リザルトの表示） |
| `description` | 空 | メニューで名前の横に出る説明 | `src/ui/MainMenu.cpp` |
| `menuOrder` | `1000` | メニューに並べる順（小さいほど上。同じなら `id` の順）。一覧の最初のステージがメニューの初期値になる。今は 障害物なし 1・路地裏 2・森 3 | `src/data/GameData.cpp`（ステージの一覧の並び）→ `src/ui/MainMenu.cpp`、`src/game/Match.cpp`（ステージが見つからない時の代わり） |
| `size` | （必須） | ステージの広さ `[幅, 高さ]`（それぞれ 1000 以上）。中心が (0, 0) で、自キャラは (0, 0) から始まる | `src/data/StageDef.cpp`（ステージの範囲にする）→ `src/game/Terrain.cpp`（ステージの外に出ない・経路探索のマス目）、`src/game/World.cpp`（範囲の内側に収める）、`src/render/Renderer.cpp`（床の描画）、`src/render/CameraControl.cpp`（カメラが動ける範囲） |
| `obstacles` | `[]` | 通れない場所の一覧（下の表） | `src/game/Terrain.cpp`（当たり・押し出し・経路探索）<br>`src/render/Renderer.cpp`（描画） |
| `bushes` | `[]` | ブッシュ（草むら）の一覧。形の書き方は `obstacles` と同じ（`kind` は書かない）。重なっているブッシュは 1 つのブッシュとして扱う | `src/game/Terrain.cpp`（どのブッシュの中か）→ `src/game/World.cpp`（見え方の判定）<br>`src/render/Renderer.cpp`（描画） |

### 障害物の項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `kind` | `"wall"` | 種類。`"building"`（建物）/ `"wall"`（壁・岩）/ `"tree"`（木）/ `"crate"`（箱）。見た目だけが違い、どれも通れない | `src/data/StageDef.cpp`（読み込み）→ `src/render/Renderer.cpp`（種類ごとの見た目） |
| `rect` | — | 長方形 `[左, 上, 幅, 高さ]`。`rect` か `circle` のどちらかを書く | `src/data/StageDef.cpp`（読み込み）→ `src/game/Terrain.cpp`、`src/render/Renderer.cpp` |
| `circle` | — | 円 `[x, y, 半径]` | `src/data/StageDef.cpp`（読み込み）→ `src/game/Terrain.cpp`、`src/render/Renderer.cpp` |

### ブッシュの見え方

| 場面 | 見え方 |
|---|---|
| ブッシュの中のキャラを、ブッシュの外から見る | 見えない（画面に描かれず、名前・画面外の矢印も出ない。対象指定のスキルの対象にも選べない）。弾や範囲は見える |
| 同じブッシュの中にいる | 見える（重なり合うブッシュは同じブッシュ） |
| ブッシュの中で対象指定のスキル（`targeting` が `"unit"`。エマの E など）を使った | `settings.jsonc` の `bushRevealTime` 秒（初期値 3 秒）だけ、外から見える。方向指定・地点指定・設置物指定のスキルでは見えない |
| 自キャラがブッシュの中にいる | 頭上の下に「ブッシュ: 隠れている」または「ブッシュ: 見えている」と出る |
| 敵 AI から自キャラが見えない | 敵は、最後に見えた位置を狙い、そこへ向かう（避け方の学習もしない） |

見え方の判定は `World::canSee`（`src/game/World.cpp`）です。使っているのは `src/render/Renderer.cpp`（描くかどうか）、`src/render/Hud.cpp`（名前・矢印・自キャラの表示）、`src/game/World.cpp`（対象の選択）、`src/game/SkillCasting.cpp`（兎の選択と、対象指定のスキルで見えるようにする処理）、`src/game/ai/EnemyAI.cpp`（敵 AI の認識）です。

---

## 7. characters/*.jsonc（キャラとスキル）

`data/characters` フォルダの `.jsonc` ファイルは、すべて読み込まれます。**ファイルを 1 つ追加すれば、メニューに新しいキャラが増えます**（スキルの処理 `handler` が既にあるものだけで作れる場合）。読み込みは `src/data/CharacterDef.cpp` です。

### キャラの項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `id` | （必須） | キャラの ID。ファイル同士で重複してはいけない | `src/data/GameData.cpp`（キャラの一覧・重複のチェック）<br>`src/game/World.cpp`（キャラの設定を ID で探す）<br>`src/game/Match.cpp`、`src/game/Sandbox.cpp`（キャラの配置）<br>`src/app/App.cpp`（コマンドラインでの指定・エラーの表示）<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`（キャラの選択） |
| `displayName` | `id` と同じ | 画面に出す名前（`like ezreal` など） | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラ名としてコピー → `src/render/Hud.cpp` が頭上に表示、ログにも出る）<br>`src/game/Match.cpp` → `src/ui/ResultScreen.cpp`（リザルトの表示）<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`（キャラの一覧） |
| `origin` | 空 | 元のゲーム（`"LoL"` / `"ER"`）。メニューの表示用 | `src/ui/MainMenu.cpp` |
| `menuOrder` | `1000` | キャラを並べる順（小さいほど先。同じなら `id` の順）。今は エマ 1・エズリアル 2・ブランド 3 で、新しいキャラは今の最後の番号の次を付けて末尾に足す。一覧の最初のキャラがメニューの初期値になる | `src/data/GameData.cpp`（キャラの一覧と、敵として出られるキャラの一覧の並び）→ `src/ui/MainMenu.cpp`（キャラの選択・登場する敵の表示）、`src/ui/DebugPanel.cpp`（キャラの選択）、`src/app/App.cpp`（コマンドラインでキャラを指定しなかった時の初期値）、`src/game/Match.cpp`（キャラが見つからない時の代わり・敵のキャラの選択） |
| `moveSpeed` | `325` | 移動速度（ゲーム内単位 / 秒） | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/game/World.cpp`（移動）、`src/game/ai/EnemyAI.cpp`（狙いの計算）、`src/ui/DebugPanel.cpp`（表示） |
| `hitboxRadius` | `65` | 当たり判定の半径。画面に描く円の大きさでもある | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/game/World.cpp`（当たり判定・カーソル下の敵の選択・地形との当たりと経路探索。地形の計算は `src/game/Terrain.cpp`）、`src/game/SkillCasting.cpp`（移動先の調整・兎の選択）、`src/game/skills/SkillHelpers.cpp`（ワープ先の調整）、`src/game/skills/EmmaSkills.cpp`（R の移動先）、`src/game/ai/EnemyAI.cpp` → `src/game/ai/Aim.cpp`（狙いの計算）、`src/render/Renderer.cpp`（描画）、`src/render/Hud.cpp`（名前の位置・画面外の判定） |
| `color` | 灰色 | キャラの色 | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/render/Renderer.cpp`（描画）、`src/game/skills/SkillHelpers.cpp`（スキルの `color` を省略した時の弾・範囲の色） |
| `skills` | （必須） | `Q` `W` `E` `R` `D` `F` のスキル。`null` にすると、そのキーにはスキルが無い | 下の表 |

### スキルの項目（`skills.Q` など）

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `name` | キー名 | 表示名（スキルバー・ログ） | `src/render/Hud.cpp`（スキルバー・詠唱バー）<br>`src/game/SkillCasting.cpp`（ログ）<br>`src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`（弾・範囲の名前、ログ）<br>`src/game/ai/EnemyAI.cpp`（ログ） |
| `handler` | （必須） | スキルの処理の種類（[8 章](#8-handler-ごとの-params-一覧)）。存在しない名前はエラー | `src/game/skills/SkillRegistry.cpp`（名前から処理を探す）<br>`src/game/SkillCasting.cpp`（処理の呼び出し）<br>`src/app/App.cpp`（読み込み時に、存在する名前かを確認） |
| `targeting` | `"direction"` | 対象の取り方（下の表） | `src/game/SkillCasting.cpp`（対象の選択・射程の確認）<br>`src/game/ai/Aim.cpp`（直線か円かの判断） |
| `castTime` | `0` | 詠唱時間（秒）。キーを押してからスキルが出るまでの時間 | `src/game/SkillCasting.cpp`（→ `src/render/Hud.cpp` が詠唱バーを表示）<br>`src/game/ai/Aim.cpp`（着弾までの時間の計算） |
| `lockMovement` | `true` | 硬直。`true` なら詠唱時間の間は移動できない。`false` なら詠唱中も移動できる（エマの W など） | `src/game/SkillCasting.cpp`（詠唱の状態に写す）→ `src/game/World.cpp`（詠唱中に移動させるか） |
| `cooldown` | `0` | クールダウン（秒）。実際の値は、メニューで選んだ倍率を掛けたもの | `src/game/SkillCasting.cpp`（クールダウンの設定）<br>`src/game/skills/SkillHelpers.cpp`（命中時のクールダウン短縮）<br>`src/render/Hud.cpp`（残り時間の表示） |
| `range` | `0` | 射程 | `src/game/SkillCasting.cpp`（射程外の判定）<br>`src/game/skills/SkillHelpers.cpp`（地点・ワープ先を射程内に収める）<br>`src/game/skills/CommonSkills.cpp`（弾の飛距離）<br>`src/game/skills/EmmaSkills.cpp`（カードの飛距離）<br>`src/game/ai/Aim.cpp`（狙えるかの判断）<br>`src/game/ai/EnemyAI.cpp`（保つ距離の計算） |
| `countsForHitRate` | `false` | スキルヒット率の集計に含めるか | `src/game/SkillCasting.cpp` → `src/game/CastStats.cpp` |
| `movementSkill` | `false` | 移動系のスキルか（「移動系だけ使える」ルールを有効にした時に使う。今のモードでは使っていない） | `src/game/SkillCasting.cpp`（使えるかの判定）<br>`src/render/Hud.cpp`（使えないスキルを暗く表示） |
| `usedByEnemy` | `false` | スキルドッジで、このキャラが敵として出た時に使うか。1 つでも `true` があるキャラが、敵として登場する | `src/game/ai/EnemyAI.cpp`（使うスキルの選択）<br>`src/data/GameData.cpp`（敵として出られるキャラの一覧 → `src/game/Match.cpp` が敵を選ぶ、`src/ui/MainMenu.cpp` が一覧を表示） |
| `aiPriority` | `1.0` | 敵 AI がスキルを選ぶ時の重み（大きいほど選ばれやすい） | `src/game/ai/EnemyAI.cpp` |
| `usableDuringCast` | なし | このスキルの詠唱中でも、詠唱を中断せずにすぐ使えるスキルのキー（例: `["R"]`）。詠唱時間 0 のスキルだけ | `src/game/SkillCasting.cpp` |
| `params` | `{}` | handler ごとの個別の数値（[8 章](#8-handler-ごとの-params-一覧)） | `src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`（各 handler の処理）<br>`src/game/SkillCasting.cpp`（`pickRadius`・`targetKinds`）<br>`src/game/ai/Aim.cpp`（`speed`・`width`・`radius`・`delay`・`flightTime`）<br>`src/game/skills/EmmaSkills.cpp` → `src/render/Renderer.cpp`（`doveCardDrawOffsetForward`・`doveCardDrawOffsetSide`） |

### targeting の値

| 値 | 意味 | 射程外の時 |
|---|---|---|
| `"direction"` | 方向指定（カーソルの方向） | 射程の端まで飛ぶ |
| `"point"` | 地点指定（カーソルの位置） | 射程の端に発動する |
| `"unit"` | 対象指定（カーソルの位置にいる敵） | 射程に入るまで近づいてから発動する |
| `"prop"` | 自分の設置物などを指定（エマの R） | 射程に入るまで近づいてから発動する |
| `"none"` | 対象なし | － |

---

## 8. handler ごとの params 一覧

スキルの `handler` によって、`params` に書ける項目が変わります。表の「必須」は、書かないと既定値を使ってログに警告を出す項目です。処理は `src/game/skills/` のファイルにあります。

### 汎用（どのキャラでも使える）

**`"projectile"`**：直線に飛ぶ弾（`CommonSkills.cpp`）。例：エズリアルの Q・W・R、ブランドの Q

| 項目 | 必須 | 意味 |
|---|---|---|
| `speed` | ○ | 弾速 |
| `width` | ○ | 弾の幅（当たり判定は幅の半分の半径の円） |
| `pierce` | | `true` で貫通（同じ敵には 1 回だけ当たる）。既定は `false`（最初の敵 1 体に当たって消える） |
| `visual` | | 見た目（下の表）。既定は `"orb"` |
| `color` | | 弾の色。既定はキャラの色 |
| `onHit` | | 当たった時の効果（[9 章](#9-効果onhit-などの書き方)） |
| `detonateMark` | | 当たった敵にこの名前の印があれば、消費して起爆する（エズリアルの Q） |

**`"homing_projectile"`**：対象を追いかけて必ず当たる弾（`CommonSkills.cpp`）。例：エマの E。項目は `projectile` と同じです（`pierce` は使いません）。

**`"blink"`**：カーソルの方向へ瞬間移動（`CommonSkills.cpp`）。例：フラッシュ。カーソルが射程内ならその位置、外なら射程の端へ移動します。`color`（演出の色）だけ書けます。

**`"ground_zone"`**：地点に投げて、一定時間残る範囲（`CommonSkills.cpp`）。例：エマの D

| 項目 | 必須 | 意味 |
|---|---|---|
| `flightTime` | ○ | 投げてから着地するまでの時間 |
| `duration` | ○ | 範囲が残る時間 |
| `radius` | ○ | 範囲の半径 |
| `slowAmount` | | 範囲内にいる間かかるスロウの強さ（0 なら無し） |
| `lingerTime` | | 範囲から出た後もスロウが続く時間（既定 0.5） |
| `visual` | | 見た目（範囲の表）。既定は `"caltrops"` |
| `color` / `onHit` | | 色 / 範囲に最初に入った時の効果 |

**`"ground_blast"`**：指定した地点が遅れて爆発する（予告円つき）（`SkillHelpers.cpp`）。例：ブランドの W

| 項目 | 必須 | 意味 |
|---|---|---|
| `delay` | ○ | 詠唱の後、爆発するまでの時間 |
| `radius` | ○ | 爆発の半径 |
| `cooldownRefundOnHit` | | 敵に当たった時、このスキルのクールダウンを本来の値の何割分短くするか（何体に当たっても 1 回） |
| `color` / `onHit` | | 色 / 当たった時の効果 |

**`"targeted_blast"`**：対象の敵とその周りに、すぐに範囲効果（`CommonSkills.cpp`）。例：ブランドの E。`radius`（必須。対象の周りの範囲）、`color`、`onHit` を書けます。

### キャラ固有

**`"ezreal_e"`**：ワープした後、近くの敵に追尾弾を撃つ（`EzrealSkills.cpp`）

| 項目 | 必須 | 意味 |
|---|---|---|
| `boltEnabled` | | 追尾弾を撃つか（既定 `true`） |
| `boltSearchRange` | ○ | 追尾弾の対象を探す距離（移動先から） |
| `boltSpeed` / `boltWidth` | ○ | 追尾弾の弾速 / 幅 |
| `boltPreferMark` | | この印が付いた敵を優先して狙う |
| `boltDetonateMark` | | 追尾弾が当たった時に起爆する印 |
| `color` | | 色 |

**`"emma_q"`**：カードを投げ、自分の位置に鳩を残す。鳩がいれば鳩もカードになって飛ぶ（`EmmaSkills.cpp`）

| 項目 | 必須 | 意味 |
|---|---|---|
| `speed` / `width` | ○ | エマのカードの弾速 / 幅（幅は鳩のカードと共通） |
| `visual` / `color` | | エマのカードの見た目 / 色 |
| `cooldownRefundOnHit` | | エマのカードが当たった時、Q のクールダウンを本来の値の何割分短くするか |
| `doveLifetime` | ○ | 鳩が残る時間 |
| `doveCardSpeed` / `doveCardRange` | | 鳩のカードの弾速 / 飛距離。**省略するとエマ本体の `speed` / `range` と同じ** |
| `doveCardColor` | | 鳩のカードの色 |
| `doveCardDrawOffsetForward` / `doveCardDrawOffsetSide` | | 溜めの間に R で鳩と入れ替わった時に、鳩のカードを**見た目だけ**ずらす量（当たり判定はずらさない。省略すると 0）。2 枚が同じ位置から同じ方向へ飛んで、1 枚に見えてしまうのを防ぐ。Forward は進む方向（マイナスなら後ろ）、Side は横方向（プラスなら進む方向の右） |
| `doubleHitEffects` | | 2 枚とも同じ敵に当たった時の効果 |

**`"emma_w"`**：指定した地点が遅れて爆発し、帽子が残る（`EmmaSkills.cpp`）。`ground_blast` の項目に加えて、`hatLifetime`（必須。帽子が残る時間）を書きます。

**`"emma_r"`**：鳩・帽子・兎を指定して、その位置へ瞬間移動する（`EmmaSkills.cpp`。対象の選び方は `SkillCasting.cpp`）

| 項目 | 必須 | 意味 |
|---|---|---|
| `pickRadius` | | カーソルからこの距離以内の対象を選ぶ（既定 150） |
| `targetKinds` | | 対象にできるもの。`"dove"` / `"hat"` / `"rabbit"` の配列 |
| `dovePathWidth` / `doveSpeed` | ○ | 鳩がエマの元の位置へ飛ぶ時の幅 / 速さ |
| `doveOnHit` | | 鳩の通り道の敵に与える効果 |
| `hatFlightTime` | | 帽子がエマの元の位置へ移るまでの時間（既定 0.15） |
| `hatRadius` / `hatOnHit` | ○ / | 帽子の爆発の半径 / 効果 |
| `rabbitRadius` / `rabbitOnHit` | ○ / | 兎の周りの範囲 / 効果 |
| `color` | | 瞬間移動の演出の色 |

**`"brand_r"`**：対象の敵へ火炎弾を放ち、敵と自分の間を跳ね返る（`BrandSkills.cpp`）

| 項目 | 必須 | 意味 |
|---|---|---|
| `speed` / `width` | ○ | 火炎弾の弾速 / 幅 |
| `bounces` | ○ | 跳ね返る回数（最初の命中は含まない） |
| `maxHitsPerTarget` | ○ | 同じ敵に当たる回数の上限 |
| `bounceRange` | ○ | 次の跳ね返り先を探す距離 |
| `recentWindow` | ○ | この秒数以内に当たった敵は、跳ね返り先として後回しにする |
| `color` / `onHit` | | 色 / 敵に当たった時の効果 |

### 見た目の値

| 弾（`visual`） | 見た目 |
|---|---|
| `"orb"` | 球（尾つき） |
| `"ring"` | 輪 |
| `"crescent"` | 半月（幅の広い弾向き） |
| `"card"` | カード |
| `"beam"` | 光線（尾が長い） |
| `"bolt"` | 小さな弾 |
| `"dove"` | 鳩 |

| 範囲（`visual`） | 見た目 |
|---|---|
| `"explosion"` | 範囲の輪だけ |
| `"telegraph"` | 予告円の内側が少しずつ埋まる |
| `"hat"` | 帽子が飛んでいく |
| `"caltrops"` | 鉄菱が飛んでいき、着地後は範囲に残る |

### 新しい handler を作る時

1. `src/game/skills/` に処理を書く
2. `src/game/skills/Skill.h` に宣言を追加する
3. `src/game/skills/SkillRegistry.cpp` の対応表に、名前と処理を追加する
4. この資料の 8 章に、params の表を追加する

---

## 9. 効果（onHit など）の書き方

`onHit`、`doubleHitEffects`、`doveOnHit` などの効果は、次の形の配列で書きます。読み込みは `src/data/EffectSpec.cpp`、効果の働きは `src/game/StatusEffects.cpp` です。

```jsonc
"onHit": [
  { "type": "slow", "amount": 0.4, "duration": 1.5 },
  { "type": "mark", "tag": "ezreal_w", "duration": 4 }
]
```

| type | 効果 | 使う項目 |
|---|---|---|
| `"slow"` | 移動速度低下。重ねがけせず、最も強いものだけが効く | `amount`（低下率）、`duration` |
| `"snare"` | 移動できない（スキルは使える） | `duration` |
| `"stun"` | 移動もスキルもできない | `duration` |
| `"knockup"` | 打ち上げ。移動もスキルもできない | `duration` |
| `"polymorph"` | 兎に変身。移動以外の行動ができない | `amount`（移動速度の低下率）、`duration` |
| `"mark"` | 印。他のスキルで起爆する（`detonateMark`） | `tag`（印の名前）、`duration` |

---

## 10. 参照しているソースコードの一覧

フォルダごとに、そのフォルダにあるファイルをすべて載せています。「関係するファイル」の settings は `settings.jsonc`、ai は `ai.jsonc`、sandbox は `sandbox.jsonc`、stages は `stages/*.jsonc`（ステージのファイル全部）、characters は `characters/*.jsonc`（キャラのファイル全部）のことです。
`.h` は、同じ名前の `.cpp` と同じ行にまとめています。

### src/（直下）

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `main.cpp` | 起動時に `data/settings.jsonc` があるかを確認する（無ければエラーを表示） | settings |

### src/app/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `App.cpp` / `App.h` | ウィンドウの大きさ・文字の大きさ・カメラ（`cameraMode`・`cameraViewHeight`・`cameraEdgeScrollSpeed`・`cameraEdgeMargin`）・ログの詳しさ・当たり判定の円の表示に使う。1 秒ごとに再読み込みを確認する。読み込み時に handler が実在するかを確認する。コマンドラインで指定したキャラとステージ（`--stage`）を使う。練習場では、ダミーのプリセットを選ぶパネル（`src/ui/SandboxPanel.cpp`）を出し、選び直されたり sandbox.jsonc が書き換えられたりしたらダミーを置き直させる。カメラが動ける範囲として、ステージの範囲を渡す | settings、sandbox、stages、characters |
| `CommandLine.cpp` / `CommandLine.h` | 使わない | — |

### src/core/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `JsonFile.cpp` / `JsonFile.h` | コメント付き JSON を読む。書き方の誤りを、行と列つきのエラーにする | すべて |
| `Log.cpp` / `Log.h` | `logLevel` で決めた詳しさでログを書く（値は `App.cpp` から受け取る） | settings |
| `GameTypes.h` | 使わない | — |
| `Geometry.h` | 使わない | — |
| `LogRetention.cpp` / `LogRetention.h` | 使わない | — |
| `Paths.cpp` / `Paths.h` | 使わない | — |
| `Platform.cpp` / `Platform.h` | 使わない | — |

### src/data/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `GameData.cpp` / `GameData.h` | 5 種類のファイルをまとめて読み込み、変更を検知して再読み込みする。キャラの一覧と、敵として出られるキャラの一覧と、ステージの一覧を作る（どれも `menuOrder` の順）。練習場のダミーのプリセットを選び直して保存する | すべて |
| `Settings.cpp` / `Settings.h` | `settings.jsonc` を読み、値の範囲をチェックする | settings |
| `AiConfig.cpp` / `AiConfig.h` | `ai.jsonc` を読む | ai |
| `StageDef.cpp` / `StageDef.h` | ステージのファイルを読む（広さ・障害物・ブッシュ） | stages |
| `SandboxConfig.cpp` / `SandboxConfig.h` | `sandbox.jsonc` を読む。選んだプリセットの名前を `"preset"` に書き戻す（コメントや他の行は変えない） | sandbox |
| `CharacterDef.cpp` / `CharacterDef.h` | キャラのファイルを読む。`params` から値を取り出す関数（`num` / `numOr` / `flagOr` / `text` / `color` / `effects`）もここ | characters |
| `EffectSpec.cpp` / `EffectSpec.h` | 効果の配列を読む | characters |

### src/game/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `Match.cpp` / `Match.h` | スキルドッジでキャラを配置し、敵を選ぶ。敵のクールダウン倍率と、敵 AI の賢さを設定する。スキルショットで的を置き（メニューで選んだ大きさ・移動速度）、的の AI の賢さを設定する。リザルトに出すキャラ名・ステージ名・賢さの名前・的の大きさの名前を渡す。選んだステージの地形で World を作り、自キャラ・敵を立てる位置に置く。練習場では、選んでいるプリセットのダミーを置き、プリセットが変わったら置き直す | ai、sandbox、stages、characters |
| `Sandbox.cpp` / `Sandbox.h` | 練習場で自キャラとダミーを配置する（ダミーの名前・位置・往復先・大きさ・移動速度・色）。建物や木と重なる位置はずらす | sandbox、stages、characters |
| `World.cpp` / `World.h` | キャラの設定を ID で探す。移動速度・当たり判定の半径を使って移動と当たり判定をする。地形（`Terrain.cpp`）を持ち、道のりに沿って移動させ、障害物から押し出す。ブッシュの見え方を判定する（`bushRevealTime`）。敵のクールダウン倍率を保持する | settings、ai、stages、characters |
| `Terrain.cpp` / `Terrain.h` | 地形の計算: 障害物との当たり・押し出し・まっすぐ進めるかの判定・経路探索（マス目の A*）・どのブッシュの中か | stages |
| `SkillCasting.cpp` / `SkillCasting.h` | スキル発動の流れ（詠唱・クールダウン・射程・対象の選択・詠唱中に使えるスキル・移動系だけ使えるルール）。ブッシュの中で対象指定のスキルを使った時に、見えるようにする。ブッシュに隠れている兎は選ばない | stages、characters |
| `StatusEffects.cpp` / `StatusEffects.h` | スキルの効果（`onHit` などに書いた状態異常）を働かせる | characters |
| `CastStats.cpp` / `CastStats.h` | `countsForHitRate` が `true` のスキルだけを、ヒット率として集計する（判定は `SkillCasting.cpp` から受け取る）。リザルト用に、制限時間の終了時に飛んでいる途中だった発動を除いた数も出す | characters |
| `DummyController.cpp` / `DummyController.h` | ダミーを静止・往復させる（静止する位置・往復先は `Sandbox.cpp` から受け取る） | sandbox |
| `Entities.cpp` / `Entities.h` | 使わない | — |
| `MatchConfig.h` | 使わない | — |
| `PlayerController.cpp` / `PlayerController.h` | 使わない | — |
| `Unit.h` | 使わない（キャラの設定からコピーした値を持つだけ） | — |

### src/game/ai/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `EnemyAI.cpp` / `EnemyAI.h` | 敵の動きとスキル選び（`usedByEnemy`・`aiPriority`・射程・賢さごとの設定）。自キャラがブッシュに隠れている間は、最後に見えた位置を狙う | ai、stages、characters |
| `SkillShotTarget.cpp` / `SkillShotTarget.h` | スキルショットの的の移動（動き方のパターン）と回避（`skillShot` の距離と `levels` の項目）。詠唱を見て避ける時は、スキルの当たり方を `Aim.cpp` の `aimModelFor` で作る。移動先は立てる位置に収め、ランダムな移動ではブッシュの中を選ばない | ai、stages、characters |
| `Aim.cpp` / `Aim.h` | 狙いの計算。スキルの `targeting` / `castTime` / `range` と、`params` の `speed` / `width` / `radius` / `delay` / `flightTime` を使う | characters |
| `PlayerObserver.cpp` / `PlayerObserver.h` | 使わない | — |

### src/game/skills/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `SkillRegistry.cpp` | handler の名前から処理を探す | characters |
| `Skill.h` | handler の宣言。スキルの設定を処理に渡す入れ物 | characters |
| `SkillHelpers.cpp` | 弾・範囲・ワープなど、handler で共通の処理。`params` の値と、射程・クールダウン短縮・キャラの色を使う。ワープ先が建物などの中なら、近くの立てる位置にする | stages、characters |
| `CommonSkills.cpp` | どのキャラでも使える handler の処理。`params` の値と射程を使う | characters |
| `EzrealSkills.cpp` | like ezreal の handler の処理。`params` の値を使う | characters |
| `EmmaSkills.cpp` | like emma の handler の処理。`params` の値と射程を使う | characters |
| `BrandSkills.cpp` | like brand の handler の処理。`params` の値を使う | characters |

### src/input/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `InputFrame.h` | 使わない | — |
| `InputSource.h` | 使わない | — |
| `LiveInput.cpp` | 使わない | — |
| `ScriptInput.cpp` | 使わない | — |

### src/render/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `Renderer.cpp` / `Renderer.h` | 当たり判定の円の表示（`showHitboxes`）、キャラの大きさと色。弾の見た目のずれ（エマの `doveCardDrawOffsetForward` / `doveCardDrawOffsetSide`。値は `EmmaSkills.cpp` が弾に設定する）。ステージの床・障害物（種類ごとの見た目）・ブッシュ。ブッシュに隠れていて見えない敵は描かない | settings、stages、characters |
| `Hud.cpp` / `Hud.h` | スキルバー（スキル名・クールダウン・使えないスキルを暗く表示）、詠唱バー、キャラ名の表示。操作説明に今のカメラ（`cameraMode`。値は `App.cpp` から受け取る）を表示。ブッシュに隠れている敵の名前・矢印は出さず、自キャラがブッシュの中にいる時は見えているかどうかを出す | settings、stages、characters |
| `CameraControl.cpp` / `CameraControl.h` | カメラの動かし方（自キャラに固定 / 固定しない）。`cameraEdgeScrollSpeed`・`cameraEdgeMargin` を使う（値は `App.cpp` から受け取る）。カメラが動ける範囲はステージの範囲（`size`） | settings、stages |

### src/ui/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `MainMenu.cpp` / `MainMenu.h` | クールダウン倍率の初期値、敵 AI の賢さの名前、スキルショットの的の賢さの名前・大きさ・移動速度の選択肢、キャラの一覧、敵として出るキャラの一覧を表示する。カメラ（`cameraMode`。`App.cpp` が持つ今の値）を選ぶ。練習場を選んでいる時は、ダミーのプリセットを選ぶ（`SandboxPanel.cpp` を使う）。ステージの一覧を表示して選ぶ | settings、ai、sandbox、stages、characters |
| `SandboxPanel.cpp` / `SandboxPanel.h` | 練習場のダミーのプリセットを選ぶ UI（メニューと、練習場の画面右下のパネル）。プリセットの名前・説明・体数・半径を表示する | sandbox |
| `DebugPanel.cpp` / `DebugPanel.h` | 視点の広さ・当たり判定の円の表示を一時的に変える。キャラの選択、敵 AI の賢さの名前、スキルショットの的の設定、移動速度の表示 | settings、ai、characters |
| `ResultScreen.cpp` / `ResultScreen.h` | リザルトに、キャラ名・ステージ名・敵 AI（スキルショットでは的）の賢さの名前・的の大きさの名前を表示する（値は `Match.cpp` から受け取る） |
| `PauseDialog.cpp` / `PauseDialog.h` | 使わない（スキルドッジ・スキルショットで Esc を押した時の確認ダイアログ） | — | ai、stages、characters |
| `UiScale.cpp` / `UiScale.h` | `uiScale` とウィンドウの大きさから、画面に出す大きさを計算する（値は `App.cpp` から受け取る） | settings |
| `Fonts.cpp` / `Fonts.h` | 使わない | — |

### tests/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `test_character_data.cpp` | 設定ファイルがすべて読み込めること、handler が実在することを確認するテスト | すべて |
| `test_world.cpp` | スキルの動きのテスト。期待する値（射程・弾速など）をキャラのファイルから計算する | characters |
| `test_dodge.cpp` | スキルドッジのテスト。設定ファイルを読み込み、`src/game/Match.cpp` を通してキャラと敵 AI の設定を使う | ai、characters |
| `test_shot.cpp` | スキルショットのテスト。的の置き方（`sizes`・`moveSpeeds`・`minDistance`・`maxDistance`）、`reactionTime` の下限（ai.jsonc の写しを一時フォルダに作って読む）、動き方のパターン（往復・リズムを崩す・接近）、的の回避（`skillShot.levels` を書き換えて確かめる）、賢さとヒット率、飛んでいる途中の発動を数えないこと | ai、stages、characters |
| `test_geometry.cpp` | 使わない | — |
| `test_log_retention.cpp` | 使わない | — |
| `test_main.cpp` | 使わない | — |
| `test_ui_scale.cpp` | 使わない（`UiScale.cpp` の計算を、テスト用の値で確かめる） | — |
| `test_camera.cpp` | 使わない（`CameraControl.cpp` の計算を、テスト用の値で確かめる） | — |
| `test_sandbox.cpp` | sandbox.jsonc が読み込めること、プリセットを選ぶと `"preset"` だけが書き換わること、練習場のダミーが置き直されることのテスト（data フォルダを一時フォルダへ写して使う） | sandbox、characters |
| `test_stage.cpp` | ステージのファイルが読み込めること、壁を回り込むこと、立てない場所を指定した時・たどり着けない時の動き、ブッシュの見え方、路地裏・森でスキルドッジを遊んでも障害物に入り込まないことのテスト | stages、characters |
| `scripts/brand_tour.txt`、`scripts/camera_tour.txt`、`scripts/dodge_tour.txt`、`scripts/emma_tour.txt`、`scripts/ezreal_tour.txt`、`scripts/shot_tour.txt`、`scripts/stage_tour.txt` | 使わない（`--script` で使う入力スクリプト） | — |
