# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 言語設定

ユーザーとのやり取りは日本語で行う

## プロジェクト概要

KANTAN Play Core（かんぷれ）の外部I2C拡張デバイス（`ex_i2c/`）サブシステムの書き換えプロジェクト。親プロジェクトは `/Users/necobut/Documents/仕事/かんぷれ/KANTAN_Play_core`。

## 親プロジェクトとの関係

親プロジェクト KANTAN_Play_core は M5Stack Core2/CoreS3 + 専用ハードウェア「KANTAN Play base」で動作する音楽ガジェットのファームウェア（PlatformIO / Arduino framework / ESP32-S3）。

### 既存の外部I2C設計

書き換え対象の既存コード（`KANTAN_Play_core/main/ex_i2c/`）:

- **`interface_external_t`** (基底クラス): `init()` / `update(uint32_t &button_state)` の仮想関数、`M5.Ex_I2C` 経由のI2C操作ヘルパー（`scanID`, `readRegister`, `writeRegister`）
- **`external_m5extio2_t`**: M5Unit EXTIO2 ドライバ（デフォルトアドレス `0x45`、8ポートGPIO入力、レジスタ `0x20-0x27` 読み取り）
- **`external_m5bytebutton_t`**: M5 Byte Button ドライバ（デフォルトアドレス `0x47`、レジスタ `0x00` で8ビット一括読み取り）

### 統合パターン（`task_port_a.cpp`）

- 各デバイス種別を最大4台（アドレス4刻み）配列で管理
- デバイスは `groups[]` でグループ化、未接続デバイスはローテーション方式で初期化試行
- 各グループの `bitmask`（8ビット）を `system_registry->external_input.setPortABitmask8()` に書き込み
- FreeRTOSタスクとして無限ループで動作

## ビルドコマンド

```bash
# ビルド（デフォルト環境: release_s3）
pio run

# 環境指定ビルド
pio run -e release_s3          # ESP32-S3 リリース
pio run -e esp32s3_arduino     # ESP32-S3 デバッグ

# アップロード（事前に /serial-stop でシリアルモニタを停止すること）
pio run -e release_s3 -t upload
```

## ESP32-S3 (PlatformIO)

ESP32-S3系ボード（M5Stack CoreS3など）でシリアルモニタを使う場合、platformio.iniに以下の設定が必要：

```ini
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
monitor_rts = 0
monitor_dtr = 0
```

## コーディング規約

- 名前空間: 全コード `kanplay_ns` 内
- 定数: `def::command::*`, `def::hw::*`, `def::app::*` に集約（`common_define.hpp`）
- 命名: スネークケース（変数・関数）、パスカルケース + `_t` サフィックス（型名）
- プライベートメンバー: `_` プリフィックス
- ライセンスヘッダー: `// SPDX-License-Identifier: MIT` + `// Copyright (c) 2025 InstaChord Corp.`

## シリアルモニタ（tmux + Slash Commands方式）

**重要**: `pio device monitor` およびBashツールの `run_in_background: true` はどちらもClaudeをフリーズさせるため使わない。

tmuxの別セッションでシリアルモニタを実行し、`tee` でログファイルにも出力する方式を採用。
Claudeはログファイルを読み、ユーザーは `tmux attach -t serial-monitor` でライブ表示を確認可能。

### Slash Commands

| コマンド | 説明 |
|---------|------|
| `/serial-start` | シリアルモニタ開始（ポート自動検出、tmuxセッション作成） |
| `/serial-stop` | シリアルモニタ停止（ファームウェアアップロード前に必須） |
| `/serial-log` | ログ読み取り（引数: 行数 or `clear`） |
| `/serial-send <cmd>` | シリアルコマンド送信 |

## Github
GithubへのコマンドはGithub CLIを使う
