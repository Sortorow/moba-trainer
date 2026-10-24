# MOBA Trainer

MOBA（LoL・エターナルリターンなど）のスキルショットとスキルドッジを練習するためのソフトです。
要件は [claude_directive.txt](claude_directive.txt)、キャラの仕様は [character_traits.txt](character_traits.txt) にあります。

## 現在の状態（手順 1）

- 練習場（障害物なしのステージ + 動作確認用のダミー 3 体）
- キャラ: like ezreal、like emma
- スキルドッジ / スキルショットのモード、他のステージ、リザルトの保存、統計はこれから

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

## スキルの調整

スキルの数値は `data/characters/*.jsonc` に書かれています（項目の説明はファイルの先頭にあります）。
ゲームを起動したままファイルを保存すると、自動で再読み込みされます。
画面全体の設定（視点の広さ・クールダウン倍率など）は `data/settings.jsonc` です。

## ログ

`logs/` フォルダに出力されます（1 ファイル 20MB まで、作成日時で数えて 10 日分を保持）。
不具合を見つけたら、その時のログファイルを添えて伝えてください。

## 自動での確認

```
build\release\moba_tests.exe                                  # ゲームロジックの自動テスト
build\release\moba_trainer.exe --character like_emma --script tests\scripts\emma_tour.txt
```

`--script` は入力スクリプト（書式は `src/input/ScriptInput.cpp` の先頭）どおりに操作し、
指定した時刻の画面を PNG で保存します。
