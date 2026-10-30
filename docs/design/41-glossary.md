# 用語集

コードとこの設計書に出てくる言葉です。右の列は、対応するコードの名前です。

## ゲームの言葉

| 言葉 | 意味 | コード |
|---|---|---|
| MOBA | LoL（League of Legends）・エターナルリターン（ER）などの対戦ゲームの分類。右クリックで移動し、Q/W/E/R などでスキルを使う | — |
| like 〇〇 | 「〇〇のような操作ができるキャラ」。本物のキャラではなく、操作感をまねたもの | `CharacterDef::id`（`like_emma` など） |
| 自キャラ | プレイヤーが操作するキャラ | `Team::Player`、`World::player()` |
| 敵（敵キャラ） | スキルドッジで出てくる、AI が操作するキャラ | `Team::Enemy` で `characterId` がある `Unit` |
| ダミー | 練習場の的。スキルを撃たない | `Team::Enemy` で `characterId` が空の `Unit`、`DummyDef` |
| 的 | スキルショットで動き回る的（1 体）。スキルを撃たない | `Team::Enemy` で名前が「的」の `Unit`、`ai::SkillShotTarget` |
| 練習場 | ダミー相手にスキルを試すモード。制限時間なし | `GameMode::Sandbox` |
| スキルドッジ | 敵のスキルを避ける練習のモード | `GameMode::SkillDodge` |
| スキルショット | スキルを当てる練習のモード | `GameMode::SkillShot` |
| ドッジ率 | 敵の発動のうち、1 つも当たらなかった割合 | `DodgeCount::rate()` |
| ヒット率 | 自キャラの発動（集計対象のスキル）のうち、当たった割合 | `HitCount::rate()`、`CastStats` |
| 被弾 | 敵の発動が自キャラに当たること。1 回の発動で何発当たっても 1 回 | `DodgeCount::hit`、`MatchResult::hitsTaken` |
| ダウン | HP が 0 になること。全回復して続ける | `Unit::deaths` |
| 賢さ | 敵・的の AI の難しさ（1〜5） | `MatchConfig::aiLevel` / `targetLevel`、`AiLevel`、`SkillShotLevel` |
| ステージ | 遊ぶ場所（障害物なし・路地裏・森） | `StageDef`、`Terrain` |
| 障害物 | 通れない場所（建物・壁・木・箱）。移動だけを妨げ、弾は通り抜ける | `Obstacle`、`ObstacleKind` |
| ブッシュ | 草むら。中のキャラは外から見えない | `StageDef::bushes`、`Terrain::bushAt`、`World::canSee` |
| リザルト | 制限時間まで遊んだ結果 | `MatchResult`、`SavedResult` |
| 記録 | 保存したリザルトの直近 10 回の順位表 | `drawRecordTable`、`drawRecordsWindow` |
| 統計 | 保存したリザルトを集計したダッシュボード | `StatsDashboard`、`ResultStats` |
| 自己ベスト | 総数 10 以上のプレイの中で、最も成績が高いもの。どれが上かは記録の順位と同じ決まり（割合の高い順、同じなら総数が多い順、さらに同じなら新しい順）で決める | `StatsSummary::best`、`kMinTotalForBest`、`rankedHigher` |
| プリセット | 練習場のダミーの組み合わせ | `SandboxPreset` |

## スキルの言葉

| 言葉 | 意味 | コード |
|---|---|---|
| スキル | Q/W/E/R/D/F のキーで使う技 | `SkillDef`、`SkillSlot` |
| 発動 | スキルが実際に出ること（詠唱が終わった時） | `casting::runSkill`、`castId` |
| 発動の番号（castId） | 1 回の発動に振る番号。その発動で出た弾・範囲をまとめて集計する | `World::newCastId()`、`Projectile::castId` |
| クイックキャスト | キーを押した瞬間にカーソルの方向へ撃つ方式 | `CastMode::Quick` |
| 詠唱（溜め・構え） | キーを押してからスキルが出るまでの時間 | `SkillDef::castTime`、`CastState` |
| 硬直 | 詠唱の間、移動できないこと | `SkillDef::lockMovement`、`CastState::locksMovement` |
| 予約（先行入力） | 詠唱中に押したスキルを、詠唱が終わったら出すこと | `Unit::bufferedCast` |
| 接近 | 射程外の対象を指定した時に、近づいてから発動すること | `Unit::pendingCast` |
| クールダウン（CD） | スキルを使ってから、もう一度使えるまでの時間 | `SkillDef::cooldown`、`Unit::cooldowns` |
| クールダウンの倍率 | クールダウンに掛ける値（「標準」の 4 分の 1 など） | `MatchConfig::cooldownScale`、`World::cooldownScale` |
| CD 短縮 | 命中した時に、そのスキルのクールダウンを本来の○%分短くすること | `skills::refundCooldown`、params の `cooldownRefundOnHit` |
| 射程 | スキルが届く距離 | `SkillDef::range` |
| 方向指定 | カーソルの方向へ撃つスキル（スキルショット） | `Targeting::Direction` |
| 地点指定 | カーソルの位置に出るスキル | `Targeting::Point` |
| 対象指定 | 敵を選んで使うスキル（必ず当たる追尾弾など） | `Targeting::Unit` |
| 設置物指定 | 自分が置いた物などを選んで使うスキル（エマの R） | `Targeting::Prop` |
| handler | スキルの動き（C++ の関数）。設定ファイルの `"handler"` の名前で選ぶ | `skills::SkillHandler`、`SkillRegistry.cpp` |
| params | handler ごとの個別の数値（設定ファイルに書く） | `SkillDef::params` |
| 弾 | 飛んでいく当たり判定 | `Projectile` |
| 追尾弾 | 対象を追いかける弾 | `Projectile::homingTarget` |
| 貫通 | 当たっても消えずに進むこと | `Projectile::pierce` |
| 範囲（地面の範囲効果） | 地面に出る円の当たり判定。遅れて爆発するもの・しばらく残るものがある | `AreaEffect` |
| 予告円 | 遅れて爆発する範囲の、爆発までの表示 | `AreaVisual::Telegraph` |
| 設置物 | エマの鳩・帽子 | `Prop`、`PropKind` |
| 演出 | 当たり判定の無い見た目（光・輪・文字） | `Vfx` |
| ワープ（ブリンク・フラッシュ） | 瞬間移動 | `skills::teleport`、`handleBlink` |

## 状態異常（CC）の言葉

CC（Crowd Control）は、相手の行動を妨げる効果のことです。

| 言葉 | 意味 | コード |
|---|---|---|
| 状態異常 | キャラにかかる効果の総称 | `StatusEffects`、`EffectType` |
| スロウ | 移動速度が下がる | `EffectType::Slow` |
| 拘束（スネア） | 移動できない（スキルは使える） | `EffectType::Snare` |
| スタン | 移動もスキルもできない | `EffectType::Stun` |
| 打ち上げ（ノックアップ） | 移動もスキルもできない（浮いて見える） | `EffectType::Knockup` |
| 変身（兎） | スキルを使えず、移動速度が下がる（エマの E・R） | `EffectType::Polymorph`、`PropTargetKind::Rabbit` |
| 印（マーク） | 付けておき、別のスキルで起爆する（エズリアルの W） | `EffectType::Mark`、`StatusEffect::tag` |
| 起爆 | 印を消費して追加の効果を出すこと | `skills::detonateMark` |

## AI の言葉

| 言葉 | 意味 | コード |
|---|---|---|
| 反応の遅れ | AI が自キャラの動き・弾に気づくまでの時間 | `AiLevel::reactionTime`、`SkillShotLevel::reactionTime` |
| 偏差撃ち | 相手が動き続けると考えて、移動先を狙うこと | `AimWeights::keepMoving`、`AiLevel::predictContinue` |
| 命中見込み | AI が考える「当たりそうな割合」 | `AimResult::expectedHit` |
| 横移動（ストレイフ） | 左右に動くこと | `AiLevel::strafe`、`TargetPattern::Strafe` |
| 囲む動き（フランク） | 他の敵と別の方向から攻めること | `AiLevel::flank` |
| 切り返し | 往復の向きを変えること。接近から急に横へ動くこと | `SkillShotTarget::nextSwing`・`updateApproach` |
| リズムを崩す | 往復の幅をいつもと変えて、読まれにくくすること | `SkillShotLevel::rhythmBreak` |
| パターン | 的の動き方の単位（往復・接近・ランダム・停止） | `TargetPattern` |

## プログラムの言葉

| 言葉 | 意味 | コード |
|---|---|---|
| ワールド座標 | ゲームの世界の座標（ステージの中央が原点） | `Vector2`（`Unit::pos` など） |
| 画面座標 | ウィンドウの中のピクセルの座標 | `GetMousePosition()`、`ImVec2` |
| 刻み（固定の時間刻み） | ゲームを進める単位の時間（1/120 秒） | `App::kFixedDt` |
| 入力スクリプト | 操作を時刻つきで書いたテキストファイル。自動での動作確認に使う | `ScriptInput`、`tests/scripts/` |
| 乱数の種（seed） | 乱数の始まりの値。同じ種なら同じ展開になる | `MatchConfig::seed` |
| 拡大率（DPI） | Windows の「拡大/縮小」の設定（150% など） | `platform::windowDpiScale`、`ui::scale()` |
| 再読み込み | 設定ファイルを読み直すこと（保存すると自動、F5 で手動） | `GameData::reloadIfChanged`、`App::reloadData` |
| 手順 N | 指示書にある開発の段階（1 練習場 → 2 スキルドッジ → 3 ステージ → 4 スキルショット → 5 リザルトの保存 → 6 統計 → 7 キャラの追加） | — |
| 指示書 | `claude_directive.txt`。要件と、開発中に決まった仕様 | — |
| （確認済み）/（Claude 判断） | 指示書・仕様書の印。ユーザーが決めた仕様か、実装時に Claude が決めた仕様か | — |
