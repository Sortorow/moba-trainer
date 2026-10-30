# app モジュール（`src/main.cpp`・`src/app/`）

## 1. 概要

アプリの入口と、ウィンドウ・メインループ・画面の切り替えを担当します。ゲームのルール（`game/`）・画面の部品（`ui/`）・描画（`render/`）・入力（`input/`）をつなぎ合わせる、いちばん上の層です。

| ファイル | 内容 |
|---|---|
| [src/main.cpp](../../src/main.cpp) | `main` 関数。data フォルダを探し、ログ・クラッシュ時の記録・引数を準備して `App` を動かす |
| [src/app/CommandLine.h](../../src/app/CommandLine.h) / [.cpp](../../src/app/CommandLine.cpp) | コマンドライン引数の読み取り（主に自動での動作確認用） |
| [src/app/App.h](../../src/app/App.h) / [.cpp](../../src/app/App.cpp) | アプリ本体。ウィンドウ・メインループ・画面（メニュー / プレイ中 / ポーズ / リザルト）の切り替え |

### 型の一覧

| 型 | 種類 | 継承 | 定義 |
|---|---|---|---|
| `CommandLineOptions` | 構造体 | なし | CommandLine.h |
| `App` | クラス | なし | App.h |
| `App::Screen` | 列挙型（private） | — | App.h |
| `App::PerfCounter` | 構造体（private） | なし | App.h |

---

## 2. `main`（src/main.cpp）

```cpp
int main(int argc, char** argv);
```

**役割**: プログラムの入口。次の順に準備してから `App::run()` を呼びます。

1. `platform::executableDirectory()` で exe のフォルダを得る
2. `locateAppPaths()` で、exe のフォルダから親へ最大 5 段さかのぼって `data/settings.jsonc` を探す。見つからなければエラーのダイアログを出して終了コード 1
3. `log::init()` でログを始める（`logs/` フォルダ）
4. `platform::installCrashHandler()` でクラッシュ時にログへ記録する仕組みを入れる
5. `parseCommandLine()` で引数を読む。読めなければログとダイアログに出して終了コード 1
6. `App` を作り、`run()` の戻り値を終了コードにする
7. `log::shutdown()` でログを閉じる

**戻り値**: 0 = 正常、1 = data フォルダが無い・引数が読めない、2 = 入力スクリプトが読めない（`App::run` から）。

**注意事項**:

- ログを始める前（手順 2）のエラーはログに残せないので、ダイアログだけで知らせます。
- Release ビルドは `/SUBSYSTEM:WINDOWS` ですが、`/ENTRY:mainCRTStartup` を指定しているので、入口は `WinMain` ではなく `main` のままです（[CMakeLists.txt](../../CMakeLists.txt)）。

**背景**（根拠: `Paths.h` のコメント）: ビルドフォルダ（`build/release`）の exe からでもソースの `data/` を使えるよう、data フォルダを親へさかのぼって探します。

---

## 3. コマンドライン引数（CommandLine.h / .cpp）

### 3.1 `CommandLineOptions`（構造体）

**役割**: コマンドライン引数で指定された内容。指定されなかった項目は空（`std::nullopt`）または `false` です。

**背景**（根拠: `CommandLine.h` の先頭「主に自動での動作確認用」、`8c72709`・`0498fab` など）: 入力スクリプトでの動作確認や、メニューを飛ばしてすぐ遊ぶために、手順ごとに引数が足されてきました（`--stage` は手順 3、`--screen` は手順 6）。

| メンバ変数 | 型 | 引数 | 説明 |
|---|---|---|---|
| `characterId` | `optional<string>` | `--character <id>` | メニューを飛ばして、このキャラで始める（例: `like_emma`） |
| `mode` | `optional<string>` | `--mode <name>` | `sandbox` / `dodge` / `shot`。これ以外はエラー |
| `stageId` | `optional<string>` | `--stage <id>` | ステージの id。見つからなければ一覧の最初のステージ |
| `enemyCount` | `optional<int>` | `--enemies <n>` | スキルドッジの敵の数（1〜5） |
| `aiLevel` | `optional<int>` | `--ai <n>` | 敵の賢さ・的の賢さ（1〜5）。**両方に同じ値を入れる** |
| `timeSeconds` | `optional<float>` | `--time <秒>` | 制限時間（秒）。0 より大きい値。メニューと違い 1 分未満も指定できる |
| `seed` | `optional<uint32_t>` | `--seed <n>` | 乱数の種。0 はエラー（0 は「開始時に決める」の意味のため） |
| `noCooldown` | `bool` | `--no-cooldown` | クールダウン「なし」で始める |
| `scriptFile` | `optional<path>` | `--script <file>` | 入力スクリプトで操作する |
| `windowWidth` / `windowHeight` | `optional<int>` | `--window <W>x<H>` | ウィンドウの大きさ（実ピクセル。拡大率を掛けない）。320x240 以上 |
| `hideDebugPanel` | `bool` | `--no-debug-panel` | デバッグパネルを最初から閉じる |
| `resultsDir` | `optional<path>` | `--results-dir <dir>` | リザルトの保存先。入力スクリプトの時は、これを指定した時だけ保存する |
| `screen` | `optional<string>` | `--screen <name>` | プレイせずに `records` / `stats-dodge` / `stats-shot` の画面を開く（動作確認用） |

### 3.2 `parseCommandLine`

```cpp
std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv, std::string& error);
```

| 引数・戻り値 | 説明 |
|---|---|
| `argc` / `argv` | `main` の引数そのまま |
| `error` | 失敗した時の理由（「--enemies の値が読めません: 9」「不明な引数です: --foo」） |
| 戻り値 | 読めた内容。1 つでも解釈できない引数があれば `nullopt` |

**注意事項**:

- 値を取る引数が最後にあって値が無い時（例: 末尾の `--character`）は、「不明な引数です」になります（`hasValue` が `false` なので、どの分岐にも当たらないため）。
- `argv` は `main` が受け取った文字列（Windows のコードページの文字列）をそのまま使います。

### 3.3 `parseNumber`（CommandLine.cpp の中だけ）

```cpp
template <class T> bool parseNumber(std::string_view text, T& out);
```

文字列**全体**を数として読みます（`std::from_chars`）。`"12abc"` のように途中に数字以外があれば失敗します。`int`・`float`・`uint32_t` で使います。

---

## 4. `App`（クラス）

**役割**: アプリ本体。次のものを持ち、1 フレームごとに順に動かします。

- 設定ファイル一式（`GameData`）
- 今のプレイ（`Match`。プレイ中だけ）とその結果
- 保存したリザルト（`ResultStore`）
- メニュー（`MainMenu`）
- 入力の取得元（`InputSource`。実際の操作か入力スクリプト）
- カメラ（`Camera2D`）とカメラの動かし方
- 画面の状態（`Screen`）

**背景**（推測）: 画面の切り替えと、各モジュールの呼び出しを 1 か所に集めるためのクラスです。画面ごとのクラス（状態パターン）には分けず、`switch (screen_)` で切り替えています。画面が 4 つと少ないためと考えられます。

**継承**: なし。

### 4.1 `App::Screen`（列挙型）

| 値 | 意味 | 描くもの |
|---|---|---|
| `Menu` | メインメニュー（記録・統計の画面を含む） | `MainMenu::draw` |
| `Playing` | プレイ中 | ゲーム画面・HUD・練習場のパネル・デバッグパネル |
| `Paused` | ポーズ中（スキルドッジ・スキルショットで Esc） | ゲーム画面・HUD・ポーズのダイアログ。**ゲームの更新はしない** |
| `Result` | リザルト画面 | ゲーム画面（止まったまま）・HUD・リザルト |

遷移は [02-architecture.md の「画面の切り替え」](02-architecture.md#9-画面の切り替え) を参照してください。

### 4.2 定数

| 定数 | 値 | 説明 |
|---|---|---|
| `kFixedDt` | `1.0f / 120.0f` | ゲームの時間を進める刻み（秒）。描画のフレームレートに左右されないように（根拠: App.h のコメント） |

### 4.3 メンバ変数

| 変数 | 型 | 説明 |
|---|---|---|
| `data_` | `GameData` | 設定ファイル一式 |
| `options_` | `CommandLineOptions` | コマンドライン引数 |
| `screen_` | `Screen` | 今の画面。初期値 `Menu` |
| `match_` | `unique_ptr<Match>` | 今のプレイ。メニューでは空 |
| `result_` | `optional<MatchResult>` | 制限時間が来た時の結果（リザルト画面に出す） |
| `results_` | `ResultStore` | 保存したリザルト。保存先は `--results-dir` か `results/` |
| `savedPlayedAt_` | `optional<string>` | 今回の結果を保存した日時。保存しなかった時は空（順位表で強調するため） |
| `menu_` | `MainMenu` | メインメニュー |
| `input_` | `unique_ptr<InputSource>` | 入力の取得元（`LiveInput` か `ScriptInput`） |
| `camera_` | `Camera2D` | raylib の 2D カメラ（注視点・拡大率・画面の中心） |
| `cameraMode_` | `CameraMode` | 今のカメラ（メニューと Y キーで変わる） |
| `settingsCameraMode_` | `CameraMode` | 設定ファイルの `cameraMode`。**設定ファイルの値が変わった時だけ**反映するために覚えておく |
| `cursorConfined_` | `bool` | カーソルをウィンドウの中に閉じ込めているか |
| `pendingInput_` | `InputFrame` | まだゲームの更新に渡していない入力（「押した瞬間」を取りこぼさないため） |
| `accumulator_` | `float` | まだゲームを進めていない時間（秒） |
| `reloadCheckTimer_` | `float` | 設定ファイルの変更を調べるまでの時間。1 秒ごと |
| `scriptClock_` | `float` | 入力スクリプトの時刻。1 フレームごとに 1/60 秒進む |
| `dpiScale_` | `float` | ウィンドウがあるモニターの拡大率（150% なら 1.5） |
| `perf_` | `PerfCounter` | 処理時間の計測 |
| `debugPanelVisible_` | `bool` | デバッグパネルを出すか（F1 で切り替え） |
| `quit_` | `bool` | 終了する（メニューの「終了」・入力スクリプトの `exit`） |
| `screenshotRequest_` | `optional<string>` | 次に描き終えた時に保存するスクリーンショットのファイル名 |

### 4.4 `App::PerfCounter`（構造体）

5 秒ごとに FPS と処理時間をログに出すための集計です（根拠: 指示書「5 秒ごとに FPS と処理時間…をログに記録する」、`712d7d8`）。

| 変数 | 説明 |
|---|---|
| `windowStart` | 集計を始めた時刻（`GetTime()` の秒） |
| `frames` | 集計中のフレーム数 |
| `updateSeconds` | ゲームの更新にかかった時間の合計 |
| `drawSeconds` | 描画にかかった時間の合計（垂直同期の待ち時間を含む） |
| `worstFrame` | 最も遅かった 1 フレームの時間 |

### 4.5 公開メンバ関数

#### `App(AppPaths paths, CommandLineOptions options)`

設定ファイルの場所と引数を覚えるだけで、ファイルの読み込みやウィンドウの作成はしません。`results_` の保存先は `options.resultsDir` があればそれ、無ければ `paths.resultsDir` です。`--no-debug-panel` ならデバッグパネルを閉じた状態にします。

#### `int run()`

| 戻り値 | 説明 |
|---|---|
| 0 | 正常（ウィンドウを閉じた・メニューの終了・入力スクリプトの `exit`） |
| 2 | 入力スクリプトが読めない（ウィンドウを開く前に終わる） |

処理の順番:

1. `data_.loadAll()` → `validateData()`（handler の名前の確認）→ ログのレベルとカメラを設定ファイルに合わせる
2. `results_.load(今日)`（保存したリザルトを読み、180 日より前を消す）
3. `--script` があれば `ScriptInput::load`、無ければ `LiveInput`
4. `initWindow()`
5. `--screen` があればメニューの記録・統計の画面を開く。`--character` / `--script` / `--mode` / `--stage` のどれかがあれば、メニューを飛ばして `startMatch(configFromCommandLine())`
6. ウィンドウが閉じられるか `quit_` になるまで `frame()` を繰り返す
7. `shutdownWindow()`

### 4.6 非公開メンバ関数

| 関数 | 説明 |
|---|---|
| `void initWindow()` | ウィンドウを**隠した状態で**開き（拡大率に合わせて大きさを変える時にちらつかないように）、IME を無効にし、大きさと位置を決めてから表示する。raylib のログをゲームのログに流す。rlImGui・ImPlot を準備し、`imgui.ini` を保存しない設定・キーボードでの UI 操作の無効化（Q/W/E/R が UI に取られないように）・日本語フォントの読み込み・文字の倍率の反映を行う。Esc は `SetExitKey(KEY_NULL)` でウィンドウを閉じるキーから外す |
| `void shutdownWindow()` | カーソルの閉じ込めを解除し、ImPlot・rlImGui を片付けてウィンドウを閉じる |
| `void fitWindowToScreen(int logicalWidth, int logicalHeight)` | 設定の大きさ（拡大率 100% の時の大きさ）に拡大率を掛け、モニターの作業領域の 85% に収め、中央に置く（`ui::fitWindowSize`） |
| `void applyUiScale()` | 文字・ボタンの倍率（拡大率 × 設定の `uiScale`）を `ui::setScale` で反映する |
| `void checkDpiChange()` | 毎フレーム拡大率を調べ、変わっていれば（別のモニターへの移動など）`applyUiScale()` し直す。ウィンドウの大きさは変えない |
| `void frame()` | 1 フレームの処理。[02-architecture.md の「1 フレームの処理」](02-architecture.md#8-1-フレームの処理) の順に行う。入力スクリプトの時は経過時間を 1/60 秒に固定する（毎回同じ結果になるように）。実際の操作の時は経過時間の上限を 0.25 秒にする（ウィンドウの移動中などで止まった後に、ゲームが一気に進まないように） |
| `void recordPerf(double frameStart, double updateEnd, double drawEnd)` | 1 フレームの処理時間を集計し、5 秒たったら DEBUG のログに出して集計をやり直す |
| `void handleGlobalKeys(const InputFrame& input)` | 画面に関係なく効くキーを処理する。F1 = デバッグパネル、F5 = 設定ファイルの再読み込み、Alt + Enter = ボーダーレスの全画面（`ToggleBorderlessWindowed`）、Esc = 画面ごとの処理（下の表）、Y = カメラの切り替え（プレイ中だけ）。入力スクリプトの `resetcd`（自キャラのクールダウンを 0 にする）もここで行う |
| `void reloadData(bool force)` | 設定ファイルを読み直す。`force` が `true`（F5）なら必ず、`false` なら変更があった時だけ。読み直した後、ログのレベル・文字の倍率・カメラ（設定ファイルの `cameraMode` が変わった時だけ）・プレイ中のキャラ（`World::refreshUnitsFromData`）・練習場のダミー（`Match::refreshDummiesIfChanged`）に反映する |
| `void validateData()` | すべてのキャラのすべてのスキルについて、`handler` の名前が実在するか確かめ、無ければエラーとして `GameData` に足す（メニューに赤字で出る） |
| `MatchConfig configFromCommandLine() const` | 引数からプレイの設定を作る。キャラが無ければ一覧の最初のキャラ。クールダウンの倍率は設定ファイルの値 |
| `void startMatch(const MatchConfig& config)` | `Match` を作り、作れなければ（キャラが 1 体も無い）メニューのまま。作れたら結果・入力・時間をリセットし、メニューの初期値を今回の設定にし、カメラを自キャラに合わせて `Playing` にする |
| `void backToMenu()` | `Match` と結果を捨てて `Menu` にする |
| `void pauseOrLeave()` | プレイ中の Esc。制限時間のあるモードは `Paused`、練習場はメニューに戻る |
| `void resume()` | ポーズを解く。ポーズ中の入力とたまった時間は捨てる（ポーズを解いた瞬間に、ポーズ中の操作が効かないように） |
| `void saveResult()` | 結果を `ResultStore` に保存する。入力スクリプトの時は `--results-dir` がある時だけ（普段の記録を汚さないため。根拠: 指示書） |
| `void updateMatch(float frameDt, const InputFrame& input)` | 入力を `pendingInput_` にまとめ、たまった時間の分だけ `Match::update(kFixedDt, ...)` を繰り返す。1 回呼ぶごとに「押した瞬間」の入力を消す。制限時間になったら結果を作って保存し、`Result` にする |
| `void updateCamera(float frameDt, const InputFrame& input)` | カメラの拡大率（画面の縦に `cameraViewHeight` が映るように）と注視点（`nextCameraTarget`）を決める。入力スクリプトの時・ウィンドウが選ばれていない時・UI の上にカーソルがある時は、画面の端でカメラを動かさない |
| `void toggleCameraMode()` | カメラを「自キャラに固定」と「固定しない」で切り替える（Y キー） |
| `void updateCursorConfinement()` | プレイ中で、ウィンドウが選ばれていて、入力スクリプトでない時は、カーソルをウィンドウの中に閉じ込める（ウィンドウの移動に追従するため毎フレーム）。それ以外は解除する |
| `void draw()` | 画面を描く（下の「描画の順番」） |
| `void saveScreenshot(const std::string& file)` | 今の画面を PNG で保存する（入力スクリプトの `screenshot`） |

#### Esc キーの扱い（`handleGlobalKeys`）

| 今の画面 | Esc を押した時 |
|---|---|
| `Menu` | 何もしない |
| `Playing`（スキルドッジ・スキルショット） | `Paused` にする |
| `Playing`（練習場） | メニューに戻る |
| `Paused` | プレイに戻る（「いいえ」と同じ） |
| `Result` | メニューに戻る |

#### 描画の順番（`draw`）

1. 背景を塗る
2. メニュー以外では、`BeginMode2D(camera_)` の中で `drawWorld`（ゲーム画面）
3. `rlImGuiBegin()` の中で、画面ごとの UI
   - `Menu`: `MainMenu::draw`。「開始」で `startMatch`、「終了」で `quit_`
   - `Playing`: `drawHud` → 練習場なら `drawSandboxPanel` と `refreshDummiesIfChanged` → デバッグパネル（F1 で表示中なら）。デバッグパネルでキャラを切り替えたら、新しい乱数の種で `startMatch`
   - `Paused`: `drawHud` → `drawPauseDialog`。はい = メニュー、いいえ = `resume`、リスタート = 新しい乱数の種で `startMatch`
   - `Result`: `drawHud` → `drawResultScreen`。もう一度 = 新しい乱数の種で `startMatch`、メニューへ = `backToMenu`
4. スクリーンショットの依頼があれば保存（描き終えた後、画面に表示する前）
5. `EndDrawing()`

**注意事項**:

- **画面の操作（ボタン）は描画の途中で処理されます**。Dear ImGui は「描く関数が、押されたかどうかを返す」形なので、`draw()` の中で `startMatch` や `backToMenu` が呼ばれます。`match_` を作り直した後は、同じ `case` の中で古い `match_` を使わないようにしています（`break` で抜ける）。
- `frame()` で `InputPollContext` に渡す `io.WantCaptureMouse` は、**前のフレーム**の Dear ImGui の判断です（今のフレームの UI はまだ描いていないため）。Dear ImGui を使うアプリで一般的な遅れです。
- デバッグパネルのキャラの切り替え・リスタート・もう一度は、すべて `seed = 0`（`Match` の中で新しい種を決める）にしてから `startMatch` します。

### 4.7 ファイルの中だけの関数（App.cpp）

| 関数 | 説明 |
|---|---|
| `void raylibLogCallback(int logLevel, const char* text, va_list args)` | raylib のログを、ゲームのログ（分類 `raylib`）に書く。raylib の ERROR 以上 → ERROR、WARNING → WARN、INFO → INFO、それ以外 → DEBUG。1 行 1024 バイトで切る |

### 4.8 背景

- **固定の時間刻み**（根拠: App.h のコメント「描画のフレームレートに左右されないように」）: 弾の当たり判定や AI の判断が、PC の速さで変わらないようにしています。
- **入力スクリプトの時は 1 フレーム = 1/60 秒**（根拠: App.cpp のコメント「毎回同じ結果になるように」）: 実際の経過時間を使うと、PC の速さで結果が変わるためです。
- **カーソルの閉じ込め**（根拠: `0575b7f`、指示書の「操作」）: カメラを固定しない時に、画面の端へカーソルを置いてカメラを動かせるようにするためです。
- **ウィンドウを隠して開く・拡大率を掛ける**（根拠: `9d287c1`、[dpi-scaling.md](../dpi-scaling.md)）: 拡大率 150% の PC で表示が小さくなっていた問題への対応です。
- **カメラの設定ファイルの値は、変わった時だけ反映**（根拠: App.cpp のコメント）: Y キーで切り替えたカメラが、別の項目を保存しただけで設定ファイルの値に戻らないようにしています。
