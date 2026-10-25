# MOBA Trainer

MOBA（LoL・エターナルリターンなど）のスキルショットとスキルドッジを練習するためのソフトです。
要件は [claude_directive.txt](claude_directive.txt)、キャラの仕様は [character_traits.txt](character_traits.txt) にあります。

## 現在の状態（手順 2）

- モード
  - 練習場: 障害物なしのステージで、ダミー 3 体を相手にスキルを試す
  - スキルドッジ: 敵（1〜5 体）が撃ってくるスキルを避ける。制限時間（1〜5 分）の後にリザルト画面
- キャラ: like ezreal、like emma、like brand（スキルドッジでは敵としても登場）
- これから: スキルショットモード、路地裏・森のステージ、リザルトの保存、統計

## ビルド

```
build.bat            # Release ビルド → build\release\moba_trainer.exe
build.bat debug      # Debug ビルド   → build\debug\moba_trainer.exe
```

必要なもの: Visual Studio Build Tools（C++ によるデスクトップ開発）。
ライブラリ（raylib / Dear ImGui / ImPlot / nlohmann/json / doctest）は初回ビルド時に自動で取得します。

## 操作

| 操作 | 内容 |
|---|---|
| 右クリック | 移動（押しっぱなしでカーソルを追いかける） |
| Q / W / E / R / D / F | スキル（キーを押した瞬間にカーソル方向へ発動） |
| F1 | デバッグパネルの表示切り替え |
| F5 | 設定ファイルの再読み込み |
| Esc | メニューに戻る |
| Alt + Enter | 全画面の切り替え |

## スキルドッジ

- メニューで「敵の数」（1〜5 体）と「敵の賢さ」（1〜5）、制限時間を選びます
- 自キャラの HP は 5 段階。5 回被弾するとダウン数が 1 増えて全回復します
- 自キャラはすべてのスキルを使えます。攻撃スキルが敵に当たってもダメージはありません（状態異常はかかります）
- ドッジ率 = 敵の 1 回の発動のうち、自キャラに 1 つも当たらなかった割合
  （発動で出た弾・範囲がすべて消えた時点で確定。制限時間の終了時に飛んでいる途中のものは数えない）
- 敵として登場するのは、設定ファイルで `"usedByEnemy": true` のスキルを持つキャラです

## 調整できる設定ファイル

ゲームを起動したままファイルを保存すると、自動で再読み込みされます。

| ファイル | 内容 |
|---|---|
| `data/characters/*.jsonc` | キャラとスキルの数値（項目の説明は like_ezreal.jsonc の先頭） |
| `data/ai.jsonc` | スキルドッジの敵 AI（賢さ 1〜5 ごとの反応速度・狙い方・撃つ間隔など） |
| `data/settings.jsonc` | 文字・ボタンの大きさ（`uiScale`）・視点の広さ・クールダウン倍率の初期値・ログの詳しさなど |

## 資料

| 資料 | 内容 |
|---|---|
| [docs/data-files.md](docs/data-files.md) | data フォルダの設定ファイル（用途・書ける項目・単位・参照しているソースコード） |
| [docs/dpi-scaling.md](docs/dpi-scaling.md) | 画面の文字が小さく表示される問題と、Windows の拡大率への対応（背景・原因・修正方法・今後の注意） |

VSCode では、Markdown ファイルを開いて `Ctrl + Shift + V` を押すと、画像や表が整った状態で読めます。

## ログ

`logs/` フォルダに出力されます（1 ファイル 20MB まで、作成日時で数えて 10 日分を保持）。
スキルドッジを始めた時の「乱数の種」もログに残ります。不具合を見つけたら、その時のログファイルを添えて伝えてください。

## 自動での確認

```
build\release\moba_tests.exe       # ゲームロジックの自動テスト
build\release\moba_trainer.exe --character like_ezreal --mode dodge --enemies 3 --ai 3 --time 12 --seed 42 --script tests\scripts\dodge_tour.txt
```

`--script` は入力スクリプト（書式は `src/input/ScriptInput.cpp` の先頭）どおりに操作し、指定した時刻の画面を PNG で保存します。
コマンドライン引数の一覧は `src/app/CommandLine.h` にあります。
