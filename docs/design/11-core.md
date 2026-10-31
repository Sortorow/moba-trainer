# core モジュール（`src/core/`）

## 1. 概要

どのモジュールからも使う土台です。ゲームのルールや画面のことは知りません。`core` の中のファイル同士以外には依存しません（raylib の `Vector2` の型と数学の関数、nlohmann/json は使います）。

| ファイル | 名前空間 | 内容 |
|---|---|---|
| [GameTypes.h](../../src/core/GameTypes.h) | `moba` | ゲーム全体で使う基本の型（通し番号・陣営・スキルのキー） |
| [Geometry.h](../../src/core/Geometry.h) | `moba::geo` | 2D の幾何計算（当たり判定・向き・距離の制限） |
| [JsonFile.h](../../src/core/JsonFile.h) / [.cpp](../../src/core/JsonFile.cpp) | `moba` | コメント付き JSON（.jsonc）の読み込み |
| [Paths.h](../../src/core/Paths.h) / [.cpp](../../src/core/Paths.cpp) | `moba` | ゲームが使うフォルダとファイルの場所 |
| [Log.h](../../src/core/Log.h) / [.cpp](../../src/core/Log.cpp) | `moba::log` | 開発者用ログ |
| [LogRetention.h](../../src/core/LogRetention.h) / [.cpp](../../src/core/LogRetention.cpp) | `moba::log` | 古いログファイルを消す判断（ファイル操作はしない） |
| [Platform.h](../../src/core/Platform.h) / [.cpp](../../src/core/Platform.cpp) | `moba::platform` | Windows 固有の処理 |

### 型の一覧

| 型 | 種類 | 継承 | 定義 |
|---|---|---|---|
| `EntityId` | 型の別名（`uint32_t`） | — | GameTypes.h |
| `Team` | 列挙型 | — | GameTypes.h |
| `SkillSlot` | 列挙型 | — | GameTypes.h |
| `JsonLoadResult` | 構造体 | なし | JsonFile.h |
| `AppPaths` | 構造体 | なし | Paths.h |
| `log::Level` | 列挙型 | — | Log.h |
| `log::Config` | 構造体 | なし | Log.h |
| `log::State` | 構造体（Log.cpp の中だけ） | なし | Log.cpp |
| `log::Timestamp` | 構造体（Log.cpp の中だけ） | なし | Log.cpp |
| `log::LogFileEntry` | 構造体 | なし | LogRetention.h |
| `platform::ScreenRect` | 構造体 | なし | Platform.h |

---

## 2. GameTypes.h

### 2.1 `EntityId` と `kInvalidId`

```cpp
using EntityId = std::uint32_t;
inline constexpr EntityId kInvalidId = 0;
```

キャラ・弾・範囲・設置物に振る通し番号です。`World` が 1 から順に振ります（`World::lastEntityId_`）。**0 は「無し」**を表します（例: 追尾弾でない弾の `homingTarget`）。キャラと弾と設置物は同じ番号の列から振るので、種類が違っても番号は重なりません。

**注意事項**: 番号はプレイ（`World`）ごとに 1 から振り直します。別のプレイの番号と比べても意味はありません。

### 2.2 `Team`（列挙型）

| 値 | 意味 |
|---|---|
| `Player` | 自キャラの陣営 |
| `Enemy` | 敵の陣営。スキルドッジの敵・スキルショットの的・**練習場のダミーもここ** |

当たり判定は「自分と違う陣営に当たる」で決めています。陣営が 2 つだけなので、ダミーと敵は同じ陣営です。

### 2.3 `SkillSlot`（列挙型）と関連の定数・関数

| 名前 | 説明 |
|---|---|
| `enum class SkillSlot { Q, W, E, R, D, F, Count }` | スキルのキー。`Count` は個数を数えるための値で、キーとしては使わない |
| `kSkillSlotCount` | キーの数（6）。配列の大きさに使う |
| `kAllSkillSlots` | すべてのキーの配列（`for (SkillSlot slot : kAllSkillSlots)` で回すため） |
| `constexpr int index(SkillSlot slot)` | キーを配列の添字（Q = 0 〜 F = 5）にする |
| `constexpr std::string_view toString(SkillSlot slot)` | キーの名前（`"Q"`〜`"F"`）。ログ・画面・設定ファイルのキー名・リザルトの JSON に使う |

**注意事項**: `toString` は `moba` 名前空間に、引数の型違いで複数あります（`toString(CameraMode)`・`toString(EffectType)`・`toString(Targeting)` など）。引数の型で選ばれます。

---

## 3. Geometry.h（`moba::geo`）

**役割**: 当たり判定などで使う 2D の幾何計算。すべて `inline` の関数で、`.cpp` はありません。

**座標系**: 画面と同じで、+X が右、**+Y が下**です。角度は +X 方向が 0 度で、**画面上の時計回りが正**です（raylib の描画関数と同じ）。

| 関数 | 引数 | 戻り値 | 説明 |
|---|---|---|---|
| `float distanceSqToSegment(Vector2 p, Vector2 a, Vector2 b)` | 点 `p`、線分 `ab` | 距離の 2 乗 | 点から線分までの最短距離の **2 乗**。平方根を取らない分だけ速い。比べる相手も 2 乗する。`a` と `b` がほぼ同じ点なら、`p` と `a` の距離 |
| `bool sweptCircleHitsCircle(Vector2 a, Vector2 b, float movingRadius, Vector2 center, float radius)` | 動く円の始点・終点・半径、止まっている円の中心・半径 | 触れるか | 円が `a` から `b` へ動く間に、もう 1 つの円に触れるか。**速い弾が 1 回の刻みで敵をすり抜けないように**、動いた道のり全体で判定する |
| `bool circlesOverlap(Vector2 a, float radiusA, Vector2 b, float radiusB)` | 2 つの円 | 重なるか | ちょうど触れている場合も重なっているとみなす |
| `Vector2 normalizedOr(Vector2 v, Vector2 fallback)` | ベクトル、代わりの向き | 長さ 1 の向き | 長さがほぼ 0（1e-4 以下）で向きが決まらない時は `fallback` を返す。「カーソルが自キャラと重なっていたら、今の向き」のように使う |
| `Vector2 clampDistance(Vector2 from, Vector2 to, float maxDistance)` | 始点、目標、最大距離 | 点 | `to` までの距離が `maxDistance` 以内なら `to`、遠ければ途中の点。射程の端に収める時に使う |
| `Vector2 perpendicular(Vector2 dir)` | 向き | 垂直な向き | `{-dir.y, dir.x}`。進む向きに対して**画面上で右手側**（+Y が下なので） |
| `float angleDegrees(Vector2 dir)` | 向き | 角度（度） | `atan2(dir.y, dir.x)` を度にしたもの |
| `std::string str(Vector2 v)` | 座標 | 文字列 | ログ用の `"(x, y)"`。小数点以下は表示しない |

**背景**: 当たり判定・AI・地形のすべてで同じ計算を使うため、1 か所にまとめています（推測）。`sweptCircleHitsCircle` は、速い弾が 1 回の刻みで敵をすり抜けないようにするためのものです（根拠: Geometry.h のコメント）。例えば弾速 3000 の弾（エマ R の鳩）は 1 刻み（1/120 秒）で 25 進むので、刻みの終わりの位置だけで判定すると、細い弾が小さい的を飛び越えることがあります。

---

## 4. JsonFile.h / .cpp

### 4.1 `JsonLoadResult`（構造体）

| メンバ | 型 | 説明 |
|---|---|---|
| `value` | `nlohmann::json` | 読んだ内容 |
| `error` | `std::string` | 読めなかった理由。空なら成功 |
| `bool ok() const` | — | `error` が空か |

### 4.2 `loadJsonFile`

```cpp
JsonLoadResult loadJsonFile(const std::filesystem::path& file);
```

コメント（`//` と `/* */`）を書ける JSON（.jsonc）を読みます。先頭の UTF-8 の BOM は取り除きます。

| 失敗の理由 | `error` の例 |
|---|---|
| ファイルを開けない | 「ファイルを開けません: (パス)」 |
| 書き方の誤り | 「like_emma.jsonc の書き方に誤りがあります: (nlohmann/json の説明。行と列を含む)」 |

**注意事項**: JSON として読めた後の、項目の型の誤り（数のはずが文字列など）はここでは見ません。各設定の読み込み関数（`loadSettings` など）で `nlohmann::json::exception` を捕まえます。

**背景**（推測）: 設定ファイルにコメントで説明を書けるように、すべての設定ファイルを .jsonc にしています。nlohmann/json の `ignore_comments` を使うための薄い包みです。

---

## 5. Paths.h / .cpp

### 5.1 `AppPaths`（構造体）

ゲームが使うファイルとフォルダの場所です。`makeAppPaths` で、ルートから決まった名前で作ります。

| メンバ | 場所 |
|---|---|
| `root` | data フォルダがある場所（プロジェクトのルート、または exe のフォルダ） |
| `dataDir` | `root/data` |
| `charactersDir` | `root/data/characters` |
| `stagesDir` | `root/data/stages` |
| `settingsFile` | `root/data/settings.jsonc` |
| `aiFile` | `root/data/ai.jsonc` |
| `sandboxFile` | `root/data/sandbox.jsonc` |
| `logsDir` | `root/logs` |
| `resultsDir` | `root/results` |

### 5.2 関数

| 関数 | 説明 |
|---|---|
| `AppPaths makeAppPaths(const path& root)` | `root` を基準に場所を決める。ファイルがあるかは確かめない。テストは `makeAppPaths(MOBA_SOURCE_DIR)` でソースの data を使う |
| `optional<AppPaths> locateAppPaths(const path& startDir, int maxLevelsUp = 5)` | `startDir` から親へ順に `data/settings.jsonc` を探す。見つからなければ `nullopt`。`build/release/` の exe からソースの data を使うため |
| `std::string toUtf8(const path& path)` | パスを UTF-8 の文字列にする。ログや画面に日本語のフォルダ名を出しても文字化けしないように |
| `path pathFromUtf8(std::string_view text)` | UTF-8 の文字列をパスにする（`toUtf8` の逆）。`std::u8string` を通して作る。コマンドライン引数からパスを作る時に使う |

**注意事項**: `toUtf8` は `path::u8string()` を `std::string` に詰め替えるだけです。Windows の `path::string()` はコードページ（Shift_JIS など）の文字列を返すので、ログや画面に出す時は必ず `toUtf8` を使います。逆に、UTF-8 の文字列からパスを作る時は、`path(std::string)`（コードページとして読む）ではなく `pathFromUtf8` を使います。

---

## 6. Log.h / .cpp（`moba::log`）

**役割**: 開発者用のログ。Claude とユーザーが、動かした時の様子を後から調べられるようにします。

**背景**（根拠: 指示書「その他、注意点や要望」）:

- ログの出力は必須。専用のフォルダに保存し、1 ファイル 20MB まで。超えたら新しいファイルにする
- 10 日分を上限にする。11 日目のファイルを作る時に、古い日のファイルを消す
- ファイルの管理は作成日時で行い、ファイル名には必ずタイムスタンプを入れる
- ゲームの起動中でも、エディターなどで読めるようにする（根拠: `41677a1`）

### 6.1 書式

```text
ファイル名: logs/moba_trainer_20261025_223012.log（同じ秒に複数作ったら _2、_3…）
各行:       2026-10-25 22:30:12.345 [INFO ] [skill] like ezreal が [Q] … を発動 castId=3 …
```

文字コードは UTF-8 です。

### 6.2 `Level`（列挙型）

`Trace` < `Debug` < `Info` < `Warn` < `Error` < `Fatal` の順に重要になります。設定ファイルの `logLevel` より低いレベルは書きません。

| レベル | 使いどころ（Log.h の説明より） |
|---|---|
| `Trace` | 毎フレームの細かい様子（右クリック長押しの移動先など） |
| `Debug` | 不具合を調べる時の途中経過（詠唱の開始・AI の判断・FPS など） |
| `Info` | 主な出来事（スキルの発動・命中・プレイの開始と終了） |
| `Warn` | 動きは続けられるが気を付けること（設定の値の誤り） |
| `Error` | うまく動かなかったこと（設定ファイルが読めない） |
| `Fatal` | クラッシュ（`platform` のクラッシュ時の記録だけが使う） |

### 6.3 `Config`（構造体）

| メンバ | 既定値 | 説明 |
|---|---|---|
| `directory` | — | 保存するフォルダ |
| `minLevel` | `Debug` | これより低いレベルは書かない |
| `maxFileBytes` | 20MB | 1 ファイルの上限 |
| `maxDays` | 10 | 残す日数 |
| `filePrefix` | `"moba_trainer"` | ファイル名の先頭 |

### 6.4 公開関数

| 関数 | 説明 |
|---|---|
| `void init(const Config& config)` | ログを始める。フォルダを作り、古いログを消してから、新しいファイルを開く。1 行目に「ログファイルを開始しました（理由: 起動）」、続けて消したファイルを書く |
| `void shutdown()` | 「ログを終了します」と書いてファイルを閉じる |
| `void setMinLevel(Level level)` / `Level minLevel()` | 書く最低レベルの変更と取得（`atomic` なのでロックなしで読める） |
| `bool isEnabled(Level level)` | そのレベルが書かれるか。重い文字列を作る前の確認用 |
| `std::string_view levelName(Level level)` | 表示名（`"INFO"` など） |
| `bool parseLevel(std::string_view text, Level& out)` | 設定ファイルの `"trace"`〜`"error"` を読む。`"fatal"` は受け付けない。知らない文字なら `false` で、`out` は変えない |
| `std::filesystem::path currentFilePath()` | 今書いているファイル（デバッグパネルに出す） |
| `void write(Level level, std::string_view category, std::string_view message)` | 1 行書く。上限を超えそうなら新しいファイルに切り替えてから書く。`init` の前に呼ばれたら、Windows のデバッグ出力（`OutputDebugStringA`）にだけ出す。Debug ビルドでは標準エラーにも出す |
| `template print(Level, category, fmt, args...)` | `std::format` の書式で書く。**レベルが低くて書かない時は、文字列を作らない** |
| `trace` / `debug` / `info` / `warn` / `error`（テンプレート） | `print` のレベルを決めた版。`log::info("skill", "Q を発動 castId={}", castId)` のように使う |

### 6.5 Log.cpp の中だけの型と関数

| 名前 | 説明 |
|---|---|
| `struct State` | ログ全体の状態（設定・開いているファイル・パス・書いた大きさ・最低レベル・初期化したか）。`mutex` で守る |
| `State& state()` | プログラムに 1 つの `State`（関数の中の `static` で、初めて使う時に作る） |
| `struct Timestamp` / `Timestamp now()` | ローカル時刻とミリ秒 |
| `int dateNumber(const std::tm&)` | 日付を `YYYYMMDD` の整数にする |
| `std::string formatLine(...)` | 1 行の文字列を作る |
| `optional<int> fileCreationDate(const path&)` | ファイルの作成日時（ローカル時刻の `YYYYMMDD`）。Windows API（`GetFileAttributesExW`）で取る |
| `void writeRaw(State&, const std::string&)` | ファイルに書いて、**毎行フラッシュ**する（クラッシュしても直前までの内容が残るように） |
| `vector<string> applyRetention(State&, int newFileDate)` | フォルダのログファイルを調べ、作成日時（取れなければファイル名の日付）で `selectLogFilesToDelete` に渡し、選ばれたファイルを消す。結果の説明を返す |
| `void openNewFile(State&, std::string_view reason)` | 今のファイルを閉じ、古いログを消し、新しいファイルを開く。**読み取りは共有し、書き込みだけ禁止する**（`_wfsopen(..., _SH_DENYWR)`） |

### 6.6 注意事項

- 複数のスレッドから書けるよう `mutex` で守っていますが、今のゲームは 1 つのスレッドで動いています。
- 文字列を作る（`formatLine`）のはロックの外、ファイルに書くのはロックの中です。
- `write` は、ロックを取った状態で `openNewFile` を呼ぶことがあります。`openNewFile` の中からは `log::write` を呼ばず `writeRaw` を使うので、二重にロックすることはありません。
- `print` は `if (!isEnabled(level)) return;` で先に抜けますが、**引数の式は呼ぶ前に評価されます**。重い計算を引数に書く時は、呼ぶ側で `isEnabled` を確かめてください。

---

## 7. LogRetention.h / .cpp（`moba::log`）

**役割**: どのログファイルを消すかの判断だけを行います。ファイル操作はしません。

**背景**（根拠: LogRetention.h の先頭）: テストしやすくするため、判断とファイル操作を分けています（[tests/test_log_retention.cpp](../../tests/test_log_retention.cpp)）。

### 7.1 `LogFileEntry`（構造体）

| メンバ | 説明 |
|---|---|
| `path` | ファイルのパス |
| `date` | 作成日（ローカル時刻）の `YYYYMMDD` |

### 7.2 関数

| 関数 | 説明 |
|---|---|
| `bool isLogFileName(const path& file, std::string_view prefix)` | `prefix_YYYYMMDD_...log` の形か（`dateFromLogFileName` が読めるか） |
| `optional<int> dateFromLogFileName(const path& file, std::string_view prefix)` | ファイル名の `prefix_` の後の 8 文字を日付として読む。形が違えば `nullopt` |
| `vector<path> selectLogFilesToDelete(const vector<LogFileEntry>& files, int newFileDate, int maxDays)` | 既存のファイルの日付と、これから作るファイルの日付を合わせた**日付の種類の数**が `maxDays` を超える間、最も古い日のファイルをまとめて消す対象にする。これから作るファイルの日付は消さない |
| `optional<int> parseEightDigits(std::string_view)`（.cpp の中だけ） | 先頭 8 文字を数字として読む |

**注意事項**: 「10 日分」は**暦の 10 日ではなく、ログがある日を 10 日分**として数えます。例えば 1 日目と 20 日目にだけログがあれば、両方残ります（テスト「日付は暦ではなく「ログがある日」で数える」）。リザルトの 180 日（暦で数える）とは数え方が違います。

---

## 8. Platform.h / .cpp（`moba::platform`）

**役割**: Windows 固有の処理。`windows.h` はこのファイルの .cpp（と Log.cpp）の中だけで読みます。

**背景**（根拠: Platform.h の先頭）: `windows.h` は raylib と名前がぶつかる（`DrawText`・`CloseWindow`・`Rectangle` など）ため、他のファイルから見えないように閉じ込めています。

### 8.1 `ScreenRect`（構造体）

モニターの作業領域（タスクバーを除いた範囲。実ピクセル）。`x` `y` `width` `height`。取れなければ大きさ 0。

### 8.2 公開関数

| 関数 | 説明 | 背景 |
|---|---|---|
| `path executableDirectory()` | 実行中の exe があるフォルダ（`GetModuleFileNameW`） | data フォルダを探す起点 |
| `vector<string> commandLineArgs()` | コマンドライン引数（exe の名前を除く）を UTF-8 で返す（`GetCommandLineW` → `CommandLineToArgvW` → UTF-8）。取れなければ空（ERROR のログ） | 根拠: 報告ファイルの B5。`main` の `argv` は Shift_JIS で、書けない文字が `?` になるため |
| `void installCrashHandler()` | 未処理の例外（アクセス違反など）と `std::terminate` の時に、原因をログへ書く仕組みを入れる。起動時に 1 回 | 根拠: Platform.h。クラッシュの原因を後から調べられるように |
| `void disableIme(void* nativeWindowHandle)` | ウィンドウの IME を無効にする（`ImmAssociateContext(hwnd, nullptr)`） | 根拠: 指示書「ゲームのウィンドウでは、日本語入力（IME）を無効にする」。日本語入力がオンのままでも Q/W/E/R が届くように |
| `float windowDpiScale(void* nativeWindowHandle)` | ウィンドウがあるモニターの拡大率（`GetDpiForWindow` ÷ 96） | 根拠: Platform.h。raylib の `GetWindowScaleDPI()` は `FLAG_WINDOW_HIGHDPI` を付けないと常に 1.0 を返すため |
| `ScreenRect monitorWorkArea(void* nativeWindowHandle)` | ウィンドウがあるモニターの作業領域 | ウィンドウを画面に収めるため |
| `void confineCursorToWindow(void* nativeWindowHandle)` | カーソルをウィンドウの中（タイトルバーと枠を除く）に閉じ込める（`ClipCursor`）。ウィンドウの移動に追従するため、閉じ込めたい間は毎フレーム呼ぶ | 根拠: `0575b7f` |
| `void releaseCursor()` | 閉じ込めを解除する | |
| `void showErrorDialog(std::string_view titleUtf8, std::string_view messageUtf8)` | エラーのダイアログを出して、閉じるまで待つ。ウィンドウを開く前の致命的なエラー用 | |

### 8.3 Platform.cpp の中だけの関数

| 関数 | 説明 |
|---|---|
| `std::wstring widen(std::string_view utf8)` | UTF-8 を、Windows の API に渡す UTF-16 にする |
| `std::string narrow(std::wstring_view utf16)` | Windows の API から受け取った UTF-16 を UTF-8 にする（`widen` の逆） |
| `LONG WINAPI onUnhandledException(EXCEPTION_POINTERS*)` | 例外のコード・場所・exe の先頭アドレスを FATAL で書き、ログを閉じる。その後は通常どおりクラッシュさせる（`EXCEPTION_CONTINUE_SEARCH`） |
| `void onTerminate()` | 捕まえられなかった C++ の例外の `what()` を FATAL で書き、ログを閉じて `abort` する |

**注意事項**:

- クラッシュ時に書く「場所」はアドレスだけです。どの関数かを知るには、同じビルドの `.pdb` と「場所 − exe の先頭」の値で調べます。
- `imm.h` は `windows.h` の後に読む必要があるので、空行で区切って clang-format の並べ替えの対象から外しています（根拠: `cdcf40e`）。
