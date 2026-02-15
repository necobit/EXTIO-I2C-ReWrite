# EXTIO2 I2C Address Rewrite Tool

M5Stack CoreS3 上で動作する、M5Stack Unit EXTIO2 の I2C アドレスを書き換えるツール。

## 概要

KANTAN Play では最大4台の EXTIO2 を同時使用するため、各デバイスに異なる I2C アドレスを割り当てる必要がある。本ツールはタッチ操作で EXTIO2 の I2C アドレスを 4 種類（`0x45`, `0x49`, `0x4D`, `0x51`）のいずれかに変更する。

## 使い方

1. CoreS3 の Port A に EXTIO2 を1台接続する
2. 電源を入れると自動的にデバイスをスキャンし、現在のアドレスが緑色で表示される
3. 変更したいアドレスのボタンをタップする
4. 「Changed to 0xXX!」と表示されれば成功
5. デバイスが見つからない場合は画面タップで再スキャン

## アドレス割り当て

| ボタン位置 | アドレス | 用途 |
|-----------|---------|------|
| 左上 | `0x45` | EXTIO2 デフォルト |
| 右上 | `0x49` | 2台目 |
| 左下 | `0x4D` | 3台目 |
| 右下 | `0x51` | 4台目 |

## レスキューモード

ターゲットアドレス以外に設定されたデバイスも自動検出する（紫色で「RESCUE」と表示）。内部デバイス（AXP2101 `0x34`、AW9523 `0x58` 等）は除外される。

## ビルド

```bash
# ビルド
pio run

# アップロード
pio run -e cores3 -t upload
```

### 動作環境

- **ハードウェア**: M5Stack CoreS3
- **フレームワーク**: Arduino (PlatformIO)
- **依存ライブラリ**: M5Unified

## 技術的な注意事項

### M5GFX I2C ドライバとの共存

CoreS3 では M5GFX が I2C ハードウェアペリフェラルを直接制御（レジスタ直接操作）するため、以下の制約がある:

- **Wire ライブラリは使用不可** — M5GFX と I2C ペリフェラルが競合し `ESP_ERR_INVALID_STATE` になる
- **GPIO ビットバング（ソフトウェア I2C）は LCD 描画後に使用不可** — M5GFX が GPIO マトリックスを保持し、`release()` + `gpio_reset_pin()` でも GPIO 1/2 を解放できない
- **`readRegister` は使用禁止** — M5GFX の I2C ドライバで repeated START + READ を行うと、EXTIO2 がデータを返さずバスがスタックする。以降の全 I2C 操作が失敗する

### 採用した方式

`M5.Ex_I2C.writeRegister8()` を使用。M5GFX 自身の I2C ドライバで書き込みトランザクション（WRITE のみ）を発行する。READ 操作を一切行わないことで I2C バスの安定性を確保している。

### CoreS3 Port A ハードウェア構成

- Port A: GPIO 2 (SDA) / GPIO 1 (SCL)
- M5Unified による初期化が必須（AW9523 バススイッチ + AXP2101 LDO 電源 + レベルシフタ）
- M5Unified 無しでは Port A の GPIO は動作しない（OUTPUT HIGH でも 0 を読む）

## ライセンス

MIT License - Copyright (c) 2025 InstaChord Corp.
