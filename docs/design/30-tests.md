# テスト（`tests/`）

## 1. 概要

動作の確かめ方は 2 つあります（根拠: 指示書「動作確認について」）。

| 方法 | 何を確かめるか | 実行するもの |
|---|---|---|
| 自動テスト（doctest） | ゲームのルール・計算（画面を出さない） | `build\release\moba_tests.exe` |
| 入力スクリプト | 実際の画面での見た目と操作（スクリーンショットを撮る） | `moba_trainer.exe --script tests\scripts\*.txt` |

**背景**（根拠: `d771af6`）: 手順 1 から、Claude が画面を見ずに動作を確かめられるよう、両方を用意しました。自動テストは本体と同じ `moba_lib` をリンクするので、ゲームと同じコードを確かめます（[02-architecture.md の「ビルドの構成」](02-architecture.md#4-ビルドの構成)）。

## 2. テストの方針

指示書の「動作確認について」に、次の決まりがあります。

1. **期待値は、なるべく設定ファイルの数値から計算する**。数値を調整してもテストが壊れないように（根拠: `27c5357`）。例: `Fixture::timeToReach(slot, distance)` は、設定ファイルの `castTime` と `speed` から、弾が届くまでの時間を計算します。
2. **乱数で結果が揺れるもの（ドッジ率など）は、乱数の種を複数使った平均で比べる**（根拠: `b6c346c`。数値の調整で、種 1 つでは賢さの順位が入れ替わったため）。例: テスト「賢さが高いほど、ランダムに動き回る相手に当てやすい」は、種 7〜10 の 4 つの平均で比べます。
3. テストは、**ソースの `data/` フォルダの本物の設定ファイル**を読みます（`makeAppPaths(MOBA_SOURCE_DIR)`）。ファイルに書き込むテスト（練習場のプリセットの保存・リザルトの保存）は、一時フォルダに写してから使い、本物を書き換えません。

## 3. 実行の仕方

```text
build.bat                                         # ビルド（テストも一緒にできる）
build\release\moba_tests.exe                      # すべてのテスト
build\release\moba_tests.exe -tc="エマ Q*"        # 名前で選ぶ（* は何文字でも）
build\release\moba_tests.exe -sf="*test_shot*"    # ファイルで選ぶ
build\release\moba_tests.exe -s                   # 成功したテストの MESSAGE も出す
```

`-tc`・`-sf`・`-s` は doctest の機能です。テストの中の `MESSAGE(...)`（例: 賢さごとのドッジ率）は、`-s` を付けると見られます。

テストの間もログを書きますが、`log::init` を呼ばないので、ログはファイルではなく Windows のデバッグ出力に出ます（`log::write` の「init の前」の扱い）。

## 4. テストのファイル

| ファイル | 対象 | 主な内容 |
|---|---|---|
| [test_main.cpp](../../tests/test_main.cpp) | — | doctest の `main`（`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`） |
| [test_geometry.cpp](../../tests/test_geometry.cpp) | `core/Geometry.h` | 点と線分の距離・動く円の当たり判定・射程の制限 |
| [test_log_retention.cpp](../../tests/test_log_retention.cpp) | `core/LogRetention` | ファイル名から日付を読む・11 日目で最も古い日を消す・「ログがある日」で数える |
| [test_command_line.cpp](../../tests/test_command_line.cpp) | `app/CommandLine`・`pathFromUtf8` | 引数の読み込みとエラー・日本語や Shift_JIS で書けない文字を含むパスがそのままのパスになる・エラーの文が文字化けしない・`pathFromUtf8` と `toUtf8` が互いに逆で、そのパスでファイルを作れる |
| [test_character_data.cpp](../../tests/test_character_data.cpp) | `data/` | data フォルダがすべて読める（params の確認も通る）・キャラの並び順・すべての handler が実在し params が決まりを満たす（警告も無い）・決まりの細かい判定（0 を許す項目・型違い・選択肢・書き忘れ・効果・整数・負の値・知らない項目名は警告・鉄菱の `rings`）・決まりを満たさないキャラのファイルは使わない（読み直しなら前回の内容、初めてなら選べない）・警告だけなら使えて一覧に出る・設定値が読めている |
| [test_world.cpp](../../tests/test_world.cpp) | `World`・`casting`・各キャラのスキル | 移動・詠唱と予約・硬直・各スキルの命中・印の起爆・エマの Q / W / R の細かい仕様・クールダウンの短縮・クールダウン「なし」・発動の依頼の結果（`CastOutcome`）・貫通するエマの Q でも短縮は 1 回・鉄菱（エマの D）の並び（`rings` のとおりで、投げた向きでは回らない）・踏むとスロウが付いて鉄菱が消え、とどまると `onStep` の長さで切れる・続けて踏むと最後に踏んだ時から数え直す（スロウが移動に効く）・同時に 2 つ触れると 2 つとも消える・着地した時やワープで上に来ても踏む・円の中でも鉄菱の無い所では踏まない・命中は 1 回の発動につき 1 回・エマ自身は踏まない・`duration` で消える（全部踏まれても円は残る）・スキルを使った時に見つかった設定の誤りがエラーの一覧に出る |
| [test_dodge.cpp](../../tests/test_dodge.cpp) | スキルドッジ・`ai::Aim`・ブランドの R | 被弾と HP・確かめ撃ちは外れたら数えず当たったら被弾・回避の確定・ダウン・予告の間に避ける・移動系だけのルール・偏差撃ち・射程外・ブランド R の跳ね返り・賢さとドッジ率・制限時間まで遊ぶ・詠唱時間もクールダウンも 0 のスキルでも撃つ間隔を守る・動けない自キャラにも `punishMinInterval` を空ける・プレイの始めから森のブッシュに隠れたままの自キャラを、開始位置へ追いかけて見つけて撃つ（種 1〜4） |
| [test_shot.cpp](../../tests/test_shot.cpp) | スキルショット・`ai::SkillShotTarget` | 的の置き方・反応の下限・往復と接近のパターン・弾や範囲の回避・詠唱を見て避ける・飛んでいる途中の発動を数えない・賢さとヒット率 |
| [test_stage.cpp](../../tests/test_stage.cpp) | `Terrain`・ステージ | 3 つのステージが読める・半径 65 のキャラが立てる場所にはすべてたどり着ける・壁を回り込む・建物の中をクリック・囲まれた場所へは行けない・ブッシュの見え方・対象指定で見える・障害物に入り込まない |
| [test_vision.cpp](../../tests/test_vision.cpp) | `vision::`・`World` の視界・敵 AI | 建物・壁・木は遮り箱は遮らない・視界の半径・ブッシュの奥・撃った時に見える時間（見えている時は付かない）・近づいている途中で見えなくなったら発動をやめる・見えない刻みに敵は撃ち始めない（ブッシュへ入ってきて見つけた後は撃つ）・敵が保つ距離は視界の内側・全視界（壁・ブッシュ・距離に関係なく見える、敵 AI がブッシュの中の自キャラにも撃つ、モードごとの初期値と `Match` のルール）・視界の形が壁の手前で止まり角に頂点がある・路地裏と森で視界の形と視線の判定が一致する・スキルショットの的の距離の上限は視界の内側・自キャラが離れても的は近づき直す・見失った敵は消えた方向へ追いかけ、ブッシュに入った自キャラを見つけ直す・賢さ 1 は最後に見えた位置、賢さ 5 は chasePredict の先まで追いかけてから探索に移る |
| [test_search.cpp](../../tests/test_search.cpp) | `ai::SearchMap`・`EnemyDirector` の探索と確かめ撃ち | ブッシュは 1 エリア・地面は 500 四方・立てない所に点を置かない・小さいブッシュにも点がある・見えた点だけ見た扱い・探索済みとあきらめ・`reset`・次のエリアは見失った位置の近くで他の敵の行き先を避ける・確かめ撃ちの先は見えない所だけで 1 エリア 1 回・視界の半径より遠い所は選ばない・離れたブッシュの自キャラを探して見つける・建物の陰の自キャラを回り込んで見つける・たどり着けない部屋はあきらめて最初から探し直す・賢さ 4 は確かめ撃ちが当たって向かい、賢さ 3 は撃たない・確かめ撃ちは外れたらドッジ率に数えず、当たったら被弾として数える・見つけたら元の動きに戻る・ブッシュの中の自キャラを見つけたら最も近い敵だけが留まって見失わない・プレイの始めに見えない自キャラは開始位置で見失ったことにして追いかけ（本当の位置へは向かわない）、いなければ探索で見つける |
| [test_sandbox.cpp](../../tests/test_sandbox.cpp) | `SandboxConfig`・練習場 | プリセットが読める・`"preset"` だけが書き換わりコメントが残る・コメント / 文字列 / 内側のオブジェクトの `"preset"` は書き換えない・一番外側に無ければ保存しない・プリセットの切り替えでダミーが置き直される・キャラのファイルを保存した後にプリセットを選んでも、キャラの変更を読んだことにしない |
| [test_results.cpp](../../tests/test_results.cpp) | `ResultStore` | 1 日 20 件・180 日・モードごとの保存・保存して読み直しても同じ・読めないファイルを `.broken` にする・順位 |
| [test_status_effects.cpp](../../tests/test_status_effects.cpp) | `StatusEffects` | スロウを 2 つ受けた時、その時に残っている中で最も強いものが効き、強い方が先に切れたら残っている弱い方が効く（character_traits.txt の例 1〜3）・同じ強さのスロウを付け直すと 1 つにまとまり、最後に付けた時から数え直す（鉄菱を続けて踏んだ時）・同じ強さの付け直しで残り時間が縮まない |
| [test_stats.cpp](../../tests/test_stats.cpp) | `ResultStats` | 絞り込み・平均と自己ベスト・直近 7 日の比較・移動平均（数を合計してから割り、割合の平均にならない）・自己ベストの更新と TOP 3・自己ベストの数字 / TOP 3 / グラフの更新の印が同じプレイを指す・日ごと / 賢さごと / キーごと |
| [test_camera.cpp](../../tests/test_camera.cpp) | `render/CameraControl` | 固定・固定しない・画面の端・ステージの外まで動かない・Space・UI の上 |
| [test_ui_scale.cpp](../../tests/test_ui_scale.cpp) | `ui::scaleForWindowHeight`・`ui::resolutionList`・`ui::fitsInArea`・`ui::startupWindowSize` | 文字の倍率は高さ ÷ 900 × `uiScale`・解像度の一覧はピクセル数の少ない順（1920×1080・2160×1440・2400×1350 を含む）・タイトルバーと枠を足して作業領域に入るか（ちょうどは入る、1 ピクセルはみ出すと入らない）・起動時は入る中で最も大きく、同じ縦横比を優先・どれも入らなければ最小 |
| [test_settings.cpp](../../tests/test_settings.cpp) | `Settings`・`findJsoncValue`・`saveWindowSize`・`GameData::saveWindowSize`・`GameData::hasChangedFiles` | 既定値（縦の見える範囲 2000・ウィンドウ 1920×1080）・入れ子のキーの値をコメント / 文字列 / 別のオブジェクトと区別して探す・`window` の `width`・`height` だけが書き換わりコメントが残る・どちらかが無ければファイルを変えない・保存した値を読み込んだ設定にも反映し、自分の書き込みでは読み直さない・ファイルの変更は読み込まずに分かる（確かめただけでは読み込んだことにならない）・解像度を保存しても、まだ読んでいない別のファイルの変更を読んだことにしない・保存する前に settings.jsonc が書き換えられていたら、保存した後に読み直す |
| [test_ui_text.cpp](../../tests/test_ui_text.cpp) | `ui::escapeFormat` | `%` が `%%` になり、書式に入れても元の文字のまま出る |

`TEST_CASE` の名前は「何を・どうすると・どうなる」の文になっています。名前を上から読むと、仕様の一覧になります。

## 5. テスト用の部品

### 5.1 共通の部品（[tests/TempDataCopy.h](../../tests/TempDataCopy.h)、名前空間 `moba::test`）

| 部品 | 説明 |
|---|---|
| `TempDataCopy` | ソースの data フォルダを、一時フォルダの中の新しいフォルダへ丸ごと写し、終わったら消す。`paths()` で写した data の場所を返す。ファイルに書き込むテスト（プリセットの保存）や、設定ファイルの一部を書き換えて試すテストで使う（本物の data フォルダを書き換えないように） |
| `void markModified(const path& file)` | ファイルの更新日時を 5 秒進める。書き換えた直後のファイルを、更新日時の細かさに左右されずに「変わった」と `GameData` に見つけさせる（test_settings.cpp・test_sandbox.cpp） |
| `string readText(const path& file)` | ファイルの中身をそのまま（文字コードや改行を変えずに）読む。設定ファイルの書き戻しのテストで、前後の中身を比べる時に使う（test_sandbox.cpp・test_settings.cpp） |
| `static int TempDataCopy::rewrite(file, pattern, replacement)` | ファイルの中で正規表現 `pattern` に合う所を、すべて `replacement` に置き換える（文字コードや改行は変えない）。置き換えた数を返すので、テストで `REQUIRE(... == 1)` のように、狙った所だけが変わったことを確かめられる |

**背景**（2026-10-30）: 以前は test_sandbox.cpp と test_dodge.cpp に似た部品がそれぞれあったので、画面の共通の部品（[20-ui.md](20-ui.md) の 5 章・6 章）と同じ考え方で 1 つにまとめました。`readText` も、2026-11-14 に test_settings.cpp で同じものが要るようになったため、test_sandbox.cpp から移しました。

### 5.2 各ファイルの部品（各ファイルの無名名前空間）

| 部品 | ファイル | 説明 |
|---|---|---|
| `Fixture` | test_world.cpp | 障害物のない `World` の (0, 0) に、指定したキャラの自キャラを置く。2 つ目の引数で、読む data フォルダ（`TempDataCopy` で写したもの）を渡せる。`addDummy`・`cast`・`resetCooldowns`・`cooldown`・`skill`（設定ファイルのスキル定義）・`timeToReach`・`findProp`・`data`（読み込んだ設定ファイル）・`run(seconds)`（1/120 秒刻みで進める） |
| `CaltropsValues`・`caltropsValues`・`expectedCaltropOffsets` | test_world.cpp | 鉄菱（エマの D）の設定ファイルの値と、`rings` から計算した鉄菱の位置（ゲームの `ringLayout` とは別に計算する） |
| `latestCaltrops`・`remainingCaltrops`・`throwCaltrops`・`runUntilLanded` | test_world.cpp | 最後に投げた鉄菱の範囲と残りの数・鉄菱を投げて着地の前まで進める・着地した刻みまで進める |
| `touchingCount`・`touchingFromOutside`・`slowedBy` | test_world.cpp | ある位置で触れている鉄菱の数・外側の鉄菱 1 つにだけ円の外から触れる位置・鉄菱のスロウが効いているか |
| `DodgeFixture` | test_dodge.cpp | `Fixture` と同様。HP を持たせるか選べる。`addEnemy`（キャラの設定どおりの敵。AI は付けない）・`cast(caster, ...)` |
| `ShotFixture` | test_shot.cpp | (0, 0) に自キャラ、(distance, 0) に的を置き、的の AI で動かす |
| `standStill()` / `alwaysDodge(readCast)` / `strafeOnly(...)` | test_shot.cpp | テスト用の的の賢さの設定（立ち止まる・必ず避ける・往復だけ） |
| `arrivals(f, seconds)` | test_shot.cpp | 的が移動先に着くたびの位置を記録する |
| `playShot(data, level, seed, seconds)` | test_shot.cpp | スキルショットを遊んだ時のヒット率（自キャラは動かず、Q を撃ち続ける） |
| `StageFixture`・`rect`・`circle`・`makeStage` | test_stage.cpp | テスト用の 2000 × 2000 のステージと、障害物にめり込まなかったかを確かめながら動かす部品 |
| `VisionFixture`・`rectObstacle`・`circle`・`makeStage`・`boundaryDistance` | test_vision.cpp | テスト用の 4000 × 4000 のステージ（障害物の種類を選べる）と、視界の多角形の境目までの距離（多角形の内側かどうかを調べる） |
| `SearchFixture`・`building`・`circle`・`makeStage`・`hideAndSeek` | test_search.cpp | テスト用の正方形のステージと、見えている自キャラを隠れ場所へ消して、敵が見つけ直すまで進める部品 |
| `NoCastTimeData` | test_dodge.cpp | `TempDataCopy` で data を写し、すべてのスキルの `castTime` と ai.jsonc の `enemyCooldownScale` を 0 に書き換える（今の設定ファイルには無い「詠唱もクールダウンも無いスキル」を作るため） |
| `dummyUnits` | test_sandbox.cpp | 練習場のダミーの一覧 |
| `slice` | test_settings.cpp | 文字列の中の `TextRange` の部分 |
| `TempDir`・`dodgeResult`・`shotResult`・`at` | test_results.cpp | 一時フォルダと、テスト用の結果・日時 |
| `containsText`・`realSkill` | test_character_data.cpp | 文の一覧に、ある文字列を含む文があるか・本物の設定ファイルのスキル定義の写し（params を書き換えて決まりの判定を試す） |
| `makeResult`・`pointers` | test_stats.cpp | テスト用の保存した結果 |
| `slow`・`advance`・`kQDoubleHit`・`kCaltrops` | test_status_effects.cpp | スロウの効果（エマの Q の 2 枚当て・鉄菱を 1 つ踏んだ時）と、World と同じく 1 刻みずつ残り時間を減らして進める |
| `inputAt` | test_camera.cpp | カーソルの位置だけを決めた `CameraControlInput` |
| `makeFiles`・`contains` | test_log_retention.cpp | テスト用のログファイルの一覧 |

**テストの書き方の型**:

```cpp
TEST_CASE("エズリアル Q: 正面の敵に当たり、外せば当たらない") {
    Fixture f("like_ezreal");                       // 自キャラを置く
    const EntityId dummy = f.addDummy({600, 0});    // 的を置く
    f.cast(SkillSlot::Q, {600, 0});                 // スキルを使う
    f.run(f.timeToReach(SkillSlot::Q, 600) + 0.1f); // 設定ファイルの値から計算した時間だけ進める
    CHECK(f.world().stats().slot(SkillSlot::Q).hits == 1);
}
```

（上は書き方の例です。実際のテストの中身は [test_world.cpp](../../tests/test_world.cpp) を見てください。）

## 6. 入力スクリプト（`tests/scripts/`）

書式は [13-input.md](13-input.md) にあります。スクリーンショットは、実行した時のフォルダ（カレントディレクトリ）に保存されます。

| ファイル | 確かめること | 実行例（各ファイルの先頭のコメントより） |
|---|---|---|
| [ezreal_tour.txt](../../tests/scripts/ezreal_tour.txt) | like ezreal の各スキル（練習場） | `--character like_ezreal --script ...` |
| [emma_tour.txt](../../tests/scripts/emma_tour.txt) | like emma の各スキル（練習場） | `--character like_emma --script ...` |
| [brand_tour.txt](../../tests/scripts/brand_tour.txt) | like brand の各スキル（練習場） | `--character like_brand --script ...` |
| [dodge_tour.txt](../../tests/scripts/dodge_tour.txt) | スキルドッジ | `--character like_ezreal --mode dodge --enemies 3 --ai 3 --time 12 --seed 42 --script ...` |
| [shot_tour.txt](../../tests/scripts/shot_tour.txt) | スキルショット | `--character like_ezreal --mode shot --ai 3 --time 8 --seed 42 --script ...` |
| [camera_tour.txt](../../tests/scripts/camera_tour.txt) | カメラの「固定しない」 | `--character like_ezreal --script ...` |
| [stage_tour.txt](../../tests/scripts/stage_tour.txt) | 壁の回り込みとブッシュ | `--character like_emma --stage alley --script ...`（`forest` も） |
| [vision_tour.txt](../../tests/scripts/vision_tour.txt) | 視界（視界の外の暗さ・壁やブッシュの陰・視界の外の敵や弾が描かれない） | `--character like_emma --mode dodge --stage alley --enemies 3 --ai 3 --time 12 --seed 42 --script ...`（`forest` も） |

入力スクリプトで動かす時は、

- 時間は 1 フレーム = 1/60 秒で進むので、PC の速さに関係なく同じ結果になります（`--seed` も指定した場合）
- カーソルの閉じ込めと、画面の端のカメラは効きません
- リザルトは `--results-dir` を指定した時だけ保存します
- `exit` 命令が無いと、ウィンドウを閉じるまで終わりません

## 7. テストを足す時

1. 対象に合うファイルに `TEST_CASE` を足す（新しいファイルを作った時は、`CMakeLists.txt` の `moba_tests` に足す）
2. 期待値は設定ファイルから計算する（`f.skill(slot).range` など）
3. 乱数が絡む時は、種を決める（`config.seed`）か、複数の種の平均で比べる
4. ファイルに書く時や、設定ファイルの一部を変えて試す時は、一時フォルダを使う（`test::TempDataCopy`、リザルトは `TempDir`）
5. 画面の部品（`ui/`・`render/Hud`・`Renderer`）は自動テストしていません。テストしたい計算は、画面のファイルから切り出してからテストします（[02-architecture.md の「分け方の意図」](02-architecture.md#分け方の意図)）
