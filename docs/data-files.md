# data フォルダの設定ファイル

ゲームの数値や動きは、`data` フォルダの設定ファイルで調整します。この資料では、各ファイルの用途、書ける項目、そのファイルを読むソースコードをまとめます。

## 目次

1. [全体像](#1-全体像)
2. [共通のルール（書き方・単位・再読み込み）](#2-共通のルール)
3. [settings.jsonc（ゲーム全体の設定）](#3-settingsjsoncゲーム全体の設定)
4. [ai.jsonc（スキルドッジの敵 AI）](#4-aijsoncスキルドッジの敵-ai)
5. [characters/*.jsonc（キャラとスキル）](#5-charactersjsoncキャラとスキル)
6. [handler ごとの params 一覧](#6-handler-ごとの-params-一覧)
7. [効果（onHit など）の書き方](#7-効果onhit-などの書き方)
8. [参照しているソースコードの一覧](#8-参照しているソースコードの一覧)

---

## 1. 全体像

```
data/
├─ settings.jsonc          ゲーム全体の設定（画面・カメラ・クールダウンの初期値・ログ）
├─ ai.jsonc                スキルドッジの敵 AI（賢さ 1〜5 ごとの動き方）
└─ characters/
    ├─ like_ezreal.jsonc   キャラ 1 体につき 1 ファイル（ステータスとスキル）
    ├─ like_emma.jsonc
    └─ like_brand.jsonc
```

| ファイル | 用途 | 読み込むコード | 値を使うコード |
|---|---|---|---|
| `settings.jsonc` | 画面・カメラ・クールダウン倍率の初期値・ログの詳しさ | `src/data/Settings.cpp` | `src/app/App.cpp`、`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ App.cpp から `src/render/Renderer.cpp`、`src/ui/UiScale.cpp`、`src/core/Log.cpp` へ渡される |
| `ai.jsonc` | スキルドッジの敵の強さ・動き方 | `src/data/AiConfig.cpp` | `src/game/Match.cpp`、`src/game/ai/EnemyAI.cpp`、`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ EnemyAI.cpp から `src/game/ai/Aim.cpp` へ、Match.cpp から `src/ui/ResultScreen.cpp` へ渡される |
| `characters/*.jsonc` | キャラのステータスとスキルの数値・動き | `src/data/CharacterDef.cpp` | `src/app/App.cpp`<br>`src/data/GameData.cpp`<br>`src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`、`src/game/SkillCasting.cpp`<br>`src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`、`SkillRegistry.cpp`<br>`src/game/ai/`：`Aim.cpp`、`EnemyAI.cpp`<br>`src/render/Hud.cpp`<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`<br>→ キャラの大きさ・移動速度・色は配置時にキャラの情報（`Unit`）へコピーされ、`src/render/Renderer.cpp` などでも使われる（[5 章](#5-charactersjsoncキャラとスキル)の表を参照） |

3 種類のファイルは、すべて `src/data/GameData.cpp` がまとめて読み込みます。

この資料の「使うコード」には、その値を使うファイルをすべて書いています。値が別のファイルへ渡されてから使われる場合は、「→」で渡し先も書いています。自動テスト（`tests/` フォルダ）での使い方は、[8 章](#8-参照しているソースコードの一覧)にまとめています。

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
| `cameraMode` | `"locked"` | カメラ。今は `"locked"`（自キャラに固定）だけ | `src/data/Settings.cpp`（未対応の値を書いた時の警告だけ。値を使うコードはまだ無い） |
| `cameraViewHeight` | `1500` | 画面の縦方向に映るゲーム内の距離。大きいほど引いた視点（400〜6000） | `src/app/App.cpp`（カメラの拡大率）<br>`src/ui/DebugPanel.cpp`（一時的に変えるスライダー） |
| `cooldownScale` | `0.25` | クールダウン「標準」モードの倍率の初期値（0 以上）。メニューで変えられる | `src/ui/MainMenu.cpp`（メニューの初期値と選択肢）<br>`src/app/App.cpp`（コマンドラインで開始する時の初期値） |
| `showHitboxes` | `true` | 弾・範囲の当たり判定の円を表示するか | `src/app/App.cpp` → `src/render/Renderer.cpp`（描画）<br>`src/ui/DebugPanel.cpp`（一時的に変えるチェックボックス） |
| `uiScale` | `1.0` | 文字・ボタンの大きさの倍率。Windows の拡大率にさらに掛ける（0.5〜3.0）。詳しくは [dpi-scaling.md](dpi-scaling.md) | `src/app/App.cpp` → `src/ui/UiScale.cpp` |
| `window.width` / `window.height` | `1600` / `900` | 起動時のウィンドウの大きさ（Windows の拡大率 100% の時の大きさ。最小 640×360） | `src/app/App.cpp` → `src/ui/UiScale.cpp`（画面に収める計算） |
| `logLevel` | `"debug"` | ログの詳しさ。`"trace"` / `"debug"` / `"info"` / `"warn"` / `"error"` | `src/app/App.cpp` → `src/core/Log.cpp` |

デバッグパネル（`F1`）から「視点の広さ」「当たり判定の円を表示」を一時的に変えられますが、設定ファイルを保存し直すと上書きされます。

---

## 4. ai.jsonc（スキルドッジの敵 AI）

読み込みは `src/data/AiConfig.cpp` です。新しい設定は、**次に開始したスキルドッジから**反映されます。

### 全体の項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `enemyCooldownScale` | `0.5` | 敵のスキルのクールダウン倍率（キャラ設定の値 × この倍率） | `src/game/Match.cpp` → `src/game/World.cpp`（敵のクールダウン倍率として保持） |
| `firstCastDelay` | `1.5` | 開始してから、敵が最初にスキルを撃つまでの時間（秒） | `src/game/ai/EnemyAI.cpp` |
| `maxEngageRange` | `1000` | 敵が自キャラと取ろうとする距離の上限（200 以上） | `src/game/ai/EnemyAI.cpp` |
| `playerReactionTime` | `0.25` | AI が想定する「プレイヤーが弾を見てから避け始めるまでの時間」（秒） | `src/game/ai/EnemyAI.cpp` → `src/game/ai/Aim.cpp` |
| `levels` | （必須） | 賢さ 1〜5 の設定。上から順に 1, 2, 3, 4, 5 | 下の表と、表の後の説明を参照 |

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

---

## 5. characters/*.jsonc（キャラとスキル）

`data/characters` フォルダの `.jsonc` ファイルは、すべて読み込まれます。**ファイルを 1 つ追加すれば、メニューに新しいキャラが増えます**（スキルの処理 `handler` が既にあるものだけで作れる場合）。読み込みは `src/data/CharacterDef.cpp` です。

### キャラの項目

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `id` | （必須） | キャラの ID。ファイル同士で重複してはいけない | `src/data/GameData.cpp`（キャラの一覧・重複のチェック）<br>`src/game/World.cpp`（キャラの設定を ID で探す）<br>`src/game/Match.cpp`、`src/game/Sandbox.cpp`（キャラの配置）<br>`src/app/App.cpp`（コマンドラインでの指定・エラーの表示）<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`（キャラの選択） |
| `displayName` | `id` と同じ | 画面に出す名前（`like ezreal` など） | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラ名としてコピー → `src/render/Hud.cpp` が頭上に表示、ログにも出る）<br>`src/game/Match.cpp` → `src/ui/ResultScreen.cpp`（リザルトの表示）<br>`src/ui/MainMenu.cpp`、`src/ui/DebugPanel.cpp`（キャラの一覧） |
| `origin` | 空 | 元のゲーム（`"LoL"` / `"ER"`）。メニューの表示用 | `src/ui/MainMenu.cpp` |
| `moveSpeed` | `325` | 移動速度（ゲーム内単位 / 秒） | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/game/World.cpp`（移動）、`src/game/ai/EnemyAI.cpp`（狙いの計算）、`src/ui/DebugPanel.cpp`（表示） |
| `hitboxRadius` | `65` | 当たり判定の半径。画面に描く円の大きさでもある | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/game/World.cpp`（当たり判定・カーソル下の敵の選択）、`src/game/SkillCasting.cpp`（移動先の調整・兎の選択）、`src/game/skills/SkillHelpers.cpp`（ワープ先の調整）、`src/game/skills/EmmaSkills.cpp`（R の移動先）、`src/game/ai/EnemyAI.cpp` → `src/game/ai/Aim.cpp`（狙いの計算）、`src/render/Renderer.cpp`（描画）、`src/render/Hud.cpp`（名前の位置・画面外の判定） |
| `color` | 灰色 | キャラの色 | `src/game/Match.cpp`、`src/game/Sandbox.cpp`、`src/game/World.cpp`（キャラの情報にコピー）<br>→ `src/render/Renderer.cpp`（描画）、`src/game/skills/SkillHelpers.cpp`（スキルの `color` を省略した時の弾・範囲の色） |
| `skills` | （必須） | `Q` `W` `E` `R` `D` `F` のスキル。`null` にすると、そのキーにはスキルが無い | 下の表 |

### スキルの項目（`skills.Q` など）

| 項目 | 既定値 | 意味 | 使うコード |
|---|---|---|---|
| `name` | キー名 | 表示名（スキルバー・ログ） | `src/render/Hud.cpp`（スキルバー・詠唱バー）<br>`src/game/SkillCasting.cpp`（ログ）<br>`src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`（弾・範囲の名前、ログ）<br>`src/game/ai/EnemyAI.cpp`（ログ） |
| `handler` | （必須） | スキルの処理の種類（[6 章](#6-handler-ごとの-params-一覧)）。存在しない名前はエラー | `src/game/skills/SkillRegistry.cpp`（名前から処理を探す）<br>`src/game/SkillCasting.cpp`（処理の呼び出し）<br>`src/app/App.cpp`（読み込み時に、存在する名前かを確認） |
| `targeting` | `"direction"` | 対象の取り方（下の表） | `src/game/SkillCasting.cpp`（対象の選択・射程の確認）<br>`src/game/ai/Aim.cpp`（直線か円かの判断） |
| `castTime` | `0` | 詠唱時間（秒）。この間は移動しない | `src/game/SkillCasting.cpp`（→ `src/render/Hud.cpp` が詠唱バーを表示）<br>`src/game/ai/Aim.cpp`（着弾までの時間の計算） |
| `cooldown` | `0` | クールダウン（秒）。実際の値は、メニューで選んだ倍率を掛けたもの | `src/game/SkillCasting.cpp`（クールダウンの設定）<br>`src/game/skills/SkillHelpers.cpp`（命中時のクールダウン短縮）<br>`src/render/Hud.cpp`（残り時間の表示） |
| `range` | `0` | 射程 | `src/game/SkillCasting.cpp`（射程外の判定）<br>`src/game/skills/SkillHelpers.cpp`（地点・ワープ先を射程内に収める）<br>`src/game/skills/CommonSkills.cpp`（弾の飛距離）<br>`src/game/skills/EmmaSkills.cpp`（カードの飛距離）<br>`src/game/ai/Aim.cpp`（狙えるかの判断）<br>`src/game/ai/EnemyAI.cpp`（保つ距離の計算） |
| `countsForHitRate` | `false` | スキルヒット率の集計に含めるか | `src/game/SkillCasting.cpp` → `src/game/CastStats.cpp` |
| `movementSkill` | `false` | 移動系のスキルか（「移動系だけ使える」ルールを有効にした時に使う。今のモードでは使っていない） | `src/game/SkillCasting.cpp`（使えるかの判定）<br>`src/render/Hud.cpp`（使えないスキルを暗く表示） |
| `usedByEnemy` | `false` | スキルドッジで、このキャラが敵として出た時に使うか。1 つでも `true` があるキャラが、敵として登場する | `src/game/ai/EnemyAI.cpp`（使うスキルの選択）<br>`src/data/GameData.cpp`（敵として出られるキャラの一覧 → `src/game/Match.cpp` が敵を選ぶ、`src/ui/MainMenu.cpp` が一覧を表示） |
| `aiPriority` | `1.0` | 敵 AI がスキルを選ぶ時の重み（大きいほど選ばれやすい） | `src/game/ai/EnemyAI.cpp` |
| `usableDuringCast` | なし | このスキルの詠唱中でも、詠唱を中断せずにすぐ使えるスキルのキー（例: `["R"]`）。詠唱時間 0 のスキルだけ | `src/game/SkillCasting.cpp` |
| `params` | `{}` | handler ごとの個別の数値（[6 章](#6-handler-ごとの-params-一覧)） | `src/game/skills/`：`BrandSkills.cpp`、`CommonSkills.cpp`、`EmmaSkills.cpp`、`EzrealSkills.cpp`、`SkillHelpers.cpp`（各 handler の処理）<br>`src/game/SkillCasting.cpp`（`pickRadius`・`targetKinds`）<br>`src/game/ai/Aim.cpp`（`speed`・`width`・`radius`・`delay`・`flightTime`） |

### targeting の値

| 値 | 意味 | 射程外の時 |
|---|---|---|
| `"direction"` | 方向指定（カーソルの方向） | 射程の端まで飛ぶ |
| `"point"` | 地点指定（カーソルの位置） | 射程の端に発動する |
| `"unit"` | 対象指定（カーソルの位置にいる敵） | 射程に入るまで近づいてから発動する |
| `"prop"` | 自分の設置物などを指定（エマの R） | 射程に入るまで近づいてから発動する |
| `"none"` | 対象なし | － |

---

## 6. handler ごとの params 一覧

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
| `onHit` | | 当たった時の効果（[7 章](#7-効果onhit-などの書き方)） |
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
4. この資料の 6 章に、params の表を追加する

---

## 7. 効果（onHit など）の書き方

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

## 8. 参照しているソースコードの一覧

フォルダごとに、そのフォルダにあるファイルをすべて載せています。「関係するファイル」の settings は `settings.jsonc`、ai は `ai.jsonc`、characters は `characters/*.jsonc`（キャラのファイル全部）のことです。
`.h` は、同じ名前の `.cpp` と同じ行にまとめています。

### src/（直下）

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `main.cpp` | 起動時に `data/settings.jsonc` があるかを確認する（無ければエラーを表示） | settings |

### src/app/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `App.cpp` / `App.h` | ウィンドウの大きさ・文字の大きさ・カメラ・ログの詳しさ・当たり判定の円の表示に使う。1 秒ごとに再読み込みを確認する。読み込み時に handler が実在するかを確認する。コマンドラインで指定したキャラを探す | settings、characters |
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
| `GameData.cpp` / `GameData.h` | 3 種類のファイルをまとめて読み込み、変更を検知して再読み込みする。キャラの一覧と、敵として出られるキャラの一覧を作る | すべて |
| `Settings.cpp` / `Settings.h` | `settings.jsonc` を読み、値の範囲をチェックする | settings |
| `AiConfig.cpp` / `AiConfig.h` | `ai.jsonc` を読む | ai |
| `CharacterDef.cpp` / `CharacterDef.h` | キャラのファイルを読む。`params` から値を取り出す関数（`num` / `numOr` / `flagOr` / `text` / `color` / `effects`）もここ | characters |
| `EffectSpec.cpp` / `EffectSpec.h` | 効果の配列を読む | characters |

### src/game/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `Match.cpp` / `Match.h` | スキルドッジでキャラを配置し、敵を選ぶ。敵のクールダウン倍率と、敵 AI の賢さを設定する。リザルトに出すキャラ名を渡す | ai、characters |
| `Sandbox.cpp` / `Sandbox.h` | 練習場でキャラを配置する | characters |
| `World.cpp` / `World.h` | キャラの設定を ID で探す。移動速度・当たり判定の半径を使って移動と当たり判定をする。敵のクールダウン倍率を保持する | ai、characters |
| `SkillCasting.cpp` / `SkillCasting.h` | スキル発動の流れ（詠唱・クールダウン・射程・対象の選択・詠唱中に使えるスキル・移動系だけ使えるルール） | characters |
| `StatusEffects.cpp` / `StatusEffects.h` | スキルの効果（`onHit` などに書いた状態異常）を働かせる | characters |
| `CastStats.cpp` / `CastStats.h` | `countsForHitRate` が `true` のスキルだけを、ヒット率として集計する（判定は `SkillCasting.cpp` から受け取る） | characters |
| `DummyController.cpp` / `DummyController.h` | 使わない | — |
| `Entities.cpp` / `Entities.h` | 使わない | — |
| `MatchConfig.h` | 使わない | — |
| `PlayerController.cpp` / `PlayerController.h` | 使わない | — |
| `Unit.h` | 使わない（キャラの設定からコピーした値を持つだけ） | — |

### src/game/ai/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `EnemyAI.cpp` / `EnemyAI.h` | 敵の動きとスキル選び（`usedByEnemy`・`aiPriority`・射程・賢さごとの設定） | ai、characters |
| `Aim.cpp` / `Aim.h` | 狙いの計算。スキルの `targeting` / `castTime` / `range` と、`params` の `speed` / `width` / `radius` / `delay` / `flightTime` を使う | characters |
| `PlayerObserver.cpp` / `PlayerObserver.h` | 使わない | — |

### src/game/skills/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `SkillRegistry.cpp` | handler の名前から処理を探す | characters |
| `Skill.h` | handler の宣言。スキルの設定を処理に渡す入れ物 | characters |
| `SkillHelpers.cpp` | 弾・範囲・ワープなど、handler で共通の処理。`params` の値と、射程・クールダウン短縮・キャラの色を使う | characters |
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
| `Renderer.cpp` / `Renderer.h` | 当たり判定の円の表示（`showHitboxes`）、キャラの大きさと色 | settings、characters |
| `Hud.cpp` / `Hud.h` | スキルバー（スキル名・クールダウン・使えないスキルを暗く表示）、詠唱バー、キャラ名の表示 | characters |

### src/ui/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `MainMenu.cpp` / `MainMenu.h` | クールダウン倍率の初期値、敵 AI の賢さの名前、キャラの一覧、敵として出るキャラの一覧を表示する | settings、ai、characters |
| `DebugPanel.cpp` / `DebugPanel.h` | 視点の広さ・当たり判定の円の表示を一時的に変える。キャラの選択、敵 AI の賢さの名前、移動速度の表示 | settings、ai、characters |
| `ResultScreen.cpp` / `ResultScreen.h` | リザルトに、キャラ名と敵 AI の賢さの名前を表示する（値は `Match.cpp` から受け取る） | ai、characters |
| `UiScale.cpp` / `UiScale.h` | `uiScale` とウィンドウの大きさから、画面に出す大きさを計算する（値は `App.cpp` から受け取る） | settings |
| `Fonts.cpp` / `Fonts.h` | 使わない | — |

### tests/

| ソースコード | 何をしているか | 関係するファイル |
|---|---|---|
| `test_character_data.cpp` | 設定ファイルがすべて読み込めること、handler が実在することを確認するテスト | すべて |
| `test_world.cpp` | スキルの動きのテスト。期待する値（射程・弾速など）をキャラのファイルから計算する | characters |
| `test_dodge.cpp` | スキルドッジのテスト。設定ファイルを読み込み、`src/game/Match.cpp` を通してキャラと敵 AI の設定を使う | ai、characters |
| `test_geometry.cpp` | 使わない | — |
| `test_log_retention.cpp` | 使わない | — |
| `test_main.cpp` | 使わない | — |
| `test_ui_scale.cpp` | 使わない（`UiScale.cpp` の計算を、テスト用の値で確かめる） | — |
| `scripts/brand_tour.txt`、`scripts/dodge_tour.txt`、`scripts/emma_tour.txt`、`scripts/ezreal_tour.txt` | 使わない（`--script` で使う入力スクリプト） | — |
