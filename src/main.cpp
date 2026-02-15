// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.
//
// EXTIO2 I2C Address Rewrite Tool
// M5Unified でハードウェア初期化＋UI＋スキャン＋書き込みを実行。

#include <M5Unified.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t  TARGET_ADDRESSES[] = { 0x45, 0x49, 0x4D, 0x51 };
static constexpr size_t   NUM_ADDRESSES      = sizeof(TARGET_ADDRESSES) / sizeof(TARGET_ADDRESSES[0]);

static constexpr uint8_t  REG_I2C_ADDRESS    = 0xFF;
static constexpr uint32_t I2C_FREQ           = 100000;

// UI layout (240x320 portrait)
static constexpr int16_t  SCREEN_W           = 240;
static constexpr int16_t  SCREEN_H           = 320;
static constexpr int16_t  BTN_W              = 100;
static constexpr int16_t  BTN_H              = 70;
static constexpr int16_t  BTN_COL[]          = { 15, 125 };   // 2列
static constexpr int16_t  BTN_ROW[]          = { 80, 165 };   // 2行
static constexpr int16_t  TITLE_Y            = 8;
static constexpr int16_t  STATUS_Y           = 40;
static constexpr int16_t  MSG_Y              = 250;
static constexpr int16_t  HELP_Y             = 290;

// Colours
static constexpr uint16_t COL_BG             = TFT_BLACK;
static constexpr uint16_t COL_BTN_ACTIVE     = TFT_GREEN;
static constexpr uint16_t COL_BTN_INACTIVE   = TFT_DARKGREY;
static constexpr uint16_t COL_MSG_OK         = TFT_CYAN;
static constexpr uint16_t COL_MSG_ERR        = TFT_RED;

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

enum class state_t : uint8_t {
  SCANNING, IDLE, WRITING, SUCCESS, ERROR, NO_DEVICE,
};

// ---------------------------------------------------------------------------
// Global variables
// ---------------------------------------------------------------------------

static state_t   _state            = state_t::SCANNING;
static int8_t    _current_index    = -1;
static uint8_t   _current_addr     = 0;
static int8_t    _target_index     = -1;
static uint32_t  _timed_state_ms   = 0;
static bool      _need_redraw      = true;
static char      _msg_buf[64]      = {};
static uint16_t  _msg_color        = COL_MSG_OK;

// ---------------------------------------------------------------------------
// I2C helpers (M5.Ex_I2C — スキャン/読み取り用)
// ---------------------------------------------------------------------------

static bool scan_id(uint8_t addr)
{
  return M5.Ex_I2C.scanID(addr);
}

// ---------------------------------------------------------------------------
// Device scanning
// ---------------------------------------------------------------------------

static bool scan_devices()
{
  Serial.println("[SCAN] Scanning for EXTIO2 devices...");
  _current_index = -1;
  _current_addr  = 0;


  for (size_t i = 0; i < NUM_ADDRESSES; ++i) {
    uint8_t addr = TARGET_ADDRESSES[i];
    if (scan_id(addr)) {
      Serial.printf("[SCAN] EXTIO2 at 0x%02X\n", addr);
      _current_index = (int8_t)i;
      _current_addr  = addr;
      // 注意: readRegister はI2Cバスを壊すため使用しない
      return true;
    }
  }

  // ターゲット以外のアドレスも探す（レスキュー用）
  // readRegister不使用 — scanIDのみでEXTIO2の可能性があるアドレスを検出
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    // 既知の内部デバイスアドレスをスキップ
    if (addr == 0x34 || addr == 0x58 || addr == 0x38 ||
        addr == 0x68 || addr == 0x48) continue;
    // ターゲットアドレスは既にチェック済み
    bool is_target = false;
    for (size_t i = 0; i < NUM_ADDRESSES; ++i) {
      if (addr == TARGET_ADDRESSES[i]) { is_target = true; break; }
    }
    if (is_target) continue;

    if (scan_id(addr)) {
      Serial.printf("[SCAN] Device at non-target 0x%02X (RESCUE)\n", addr);
      _current_index = -1;
      _current_addr  = addr;
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Address writing (M5.Ex_I2C.writeRegister8)
// ---------------------------------------------------------------------------

static bool write_new_address(uint8_t current_addr, uint8_t new_addr)
{
  Serial.printf("[WRITE] 0x%02X -> 0x%02X via writeRegister8\n", current_addr, new_addr);

  // writeRegister8 で [START][addr+W][0xFF][new_addr][STOP] を送信
  bool wr = M5.Ex_I2C.writeRegister8(current_addr, REG_I2C_ADDRESS, new_addr, I2C_FREQ);
  Serial.printf("[WRITE] result: %d\n", wr);

  // デバイスがアドレス変更を処理する時間を確保
  delay(500);

  return true;
}

// ---------------------------------------------------------------------------
// UI drawing
// ---------------------------------------------------------------------------

static void draw_button(int index)
{
  int16_t col = index % 2;
  int16_t row = index / 2;
  int16_t x   = BTN_COL[col];
  int16_t y   = BTN_ROW[row];
  uint16_t color = (index == _current_index) ? COL_BTN_ACTIVE : COL_BTN_INACTIVE;

  M5.Lcd.fillRoundRect(x, y, BTN_W, BTN_H, 6, color);
  M5.Lcd.drawRoundRect(x, y, BTN_W, BTN_H, 6, TFT_WHITE);

  M5.Lcd.setTextColor(TFT_WHITE, color);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFont(&fonts::FreeSansBold12pt7b);

  char buf[8];
  snprintf(buf, sizeof(buf), "0x%02X", TARGET_ADDRESSES[index]);

  int16_t tw = M5.Lcd.textWidth(buf);
  int16_t tx = x + (BTN_W - tw) / 2;
  int16_t ty = y + (BTN_H - 16) / 2;
  M5.Lcd.setCursor(tx, ty);
  M5.Lcd.print(buf);
}

static void draw_screen()
{
  M5.Lcd.fillScreen(COL_BG);

  // Title
  M5.Lcd.setFont(&fonts::FreeSansBold12pt7b);
  M5.Lcd.setTextColor(TFT_WHITE, COL_BG);
  M5.Lcd.setTextDatum(top_center);
  M5.Lcd.drawString("EXTIO2 Addr", SCREEN_W / 2, TITLE_Y);

  // Status
  M5.Lcd.setFont(&fonts::FreeSans9pt7b);
  M5.Lcd.setTextDatum(top_center);
  if (_current_addr != 0) {
    char status[64];
    if (_current_index >= 0) {
      snprintf(status, sizeof(status), "Current: 0x%02X", _current_addr);
      M5.Lcd.setTextColor(TFT_YELLOW, COL_BG);
    } else {
      snprintf(status, sizeof(status), "RESCUE  Addr: 0x%02X", _current_addr);
      M5.Lcd.setTextColor(TFT_MAGENTA, COL_BG);
    }
    M5.Lcd.drawString(status, SCREEN_W / 2, STATUS_Y);
  } else {
    M5.Lcd.setTextColor(TFT_ORANGE, COL_BG);
    M5.Lcd.drawString("No device detected", SCREEN_W / 2, STATUS_Y);
  }

  // Buttons
  for (size_t i = 0; i < NUM_ADDRESSES; ++i) {
    draw_button(i);
  }

  // Message
  if (_msg_buf[0] != '\0') {
    M5.Lcd.setFont(&fonts::FreeSansBold9pt7b);
    M5.Lcd.setTextColor(_msg_color, COL_BG);
    M5.Lcd.setTextDatum(top_center);
    M5.Lcd.drawString(_msg_buf, SCREEN_W / 2, MSG_Y);
  }

  // Help
  M5.Lcd.setFont(&fonts::Font0);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, COL_BG);
  M5.Lcd.setTextDatum(top_center);
  if (_state == state_t::NO_DEVICE) {
    M5.Lcd.drawString("Connect EXTIO2 and tap screen", SCREEN_W / 2, HELP_Y);
  } else if (_state == state_t::IDLE) {
    M5.Lcd.drawString("Tap button to change I2C address", SCREEN_W / 2, HELP_Y);
  }
  M5.Lcd.setTextDatum(top_left);
}

// ---------------------------------------------------------------------------
// Touch hit test
// ---------------------------------------------------------------------------

static int8_t hit_test(int16_t x, int16_t y)
{
  for (size_t i = 0; i < NUM_ADDRESSES; ++i) {
    int16_t bx = BTN_COL[i % 2];
    int16_t by = BTN_ROW[i / 2];
    if (x >= bx && x < bx + BTN_W && y >= by && y < by + BTN_H) {
      return (int8_t)i;
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------

static void handle_scanning()
{
  _msg_buf[0] = '\0';
  _need_redraw = true;

  if (scan_devices()) {
    _state = state_t::IDLE;
  } else {
    _state = state_t::NO_DEVICE;
  }
  _need_redraw = true;
}

static void handle_idle()
{
  auto touch = M5.Touch.getDetail();
  if (touch.wasPressed()) {
    int8_t idx = hit_test(touch.x, touch.y);
    if (idx >= 0 && idx != _current_index) {
      _target_index = idx;
      _state = state_t::WRITING;
      snprintf(_msg_buf, sizeof(_msg_buf), "Writing 0x%02X ...", TARGET_ADDRESSES[_target_index]);
      _msg_color = COL_MSG_OK;
      _need_redraw = true;
    }
  }
}

static void handle_writing()
{
  uint8_t new_addr = TARGET_ADDRESSES[_target_index];
  uint8_t old_addr = _current_addr;
  Serial.printf("[HANDLE] Writing 0x%02X -> 0x%02X\n", old_addr, new_addr);

  write_new_address(old_addr, new_addr);
  delay(500);

  // Verification — フルスキャンで確認
  Serial.println("[VERIFY] Scanning...");
  bool found_new = scan_id(new_addr);
  bool found_old = scan_id(old_addr);
  Serial.printf("[VERIFY] new(0x%02X)=%s old(0x%02X)=%s\n",
                new_addr, found_new ? "YES" : "NO",
                old_addr, found_old ? "YES" : "NO");
  // フルスキャンも表示
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    if (scan_id(a)) Serial.printf("[VERIFY]  0x%02X found\n", a);
  }

  if (found_new) {
    _current_addr  = new_addr;
    _current_index = _target_index;
    snprintf(_msg_buf, sizeof(_msg_buf), "Changed to 0x%02X!", new_addr);
    _msg_color = COL_MSG_OK;
    _state = state_t::SUCCESS;
    _timed_state_ms = millis();
    Serial.printf("[STATE] SUCCESS (0x%02X)\n", new_addr);
  } else if (found_old) {
    snprintf(_msg_buf, sizeof(_msg_buf), "Still at 0x%02X (unchanged)", old_addr);
    _msg_color = COL_MSG_ERR;
    _state = state_t::ERROR;
    _timed_state_ms = millis();
  } else {
    snprintf(_msg_buf, sizeof(_msg_buf), "Device lost!");
    _msg_color = COL_MSG_ERR;
    _state = state_t::ERROR;
    _timed_state_ms = millis();
  }
  _need_redraw = true;
}

static void handle_timed_state()
{
  if (millis() - _timed_state_ms >= 2000) {
    if (_state == state_t::SUCCESS) {
      _msg_buf[0] = '\0';
      _state = state_t::IDLE;
    } else {
      _state = state_t::SCANNING;
    }
    _need_redraw = true;
  }
}

static void handle_no_device()
{
  auto touch = M5.Touch.getDetail();
  if (touch.wasPressed()) {
    _state = state_t::SCANNING;
    _need_redraw = true;
  }
}

// ---------------------------------------------------------------------------
// Arduino setup / loop
// ---------------------------------------------------------------------------

void setup()
{
  auto cfg = M5.config();
  cfg.output_power = true;
  M5.begin(cfg);

  // M5.begin() 後に Serial を再初期化（USB CDC が上書きされる対策）
  Serial.begin(115200);
  delay(100);
  Serial.flush();

  M5.Ex_I2C.begin();

  M5.Lcd.setRotation(0);
  M5.Lcd.fillScreen(COL_BG);
  M5.Lcd.setBrightness(128);

  delay(2000);

  Serial.println("=== EXTIO2 Address Rewrite Tool ===");
  Serial.flush();

  _state = state_t::SCANNING;
  _need_redraw = true;
}

void loop()
{
  M5.update();

  switch (_state) {
    case state_t::SCANNING:   handle_scanning();    break;
    case state_t::IDLE:       handle_idle();         break;
    case state_t::WRITING:    handle_writing();      break;
    case state_t::SUCCESS:
    case state_t::ERROR:      handle_timed_state();  break;
    case state_t::NO_DEVICE:  handle_no_device();    break;
  }

  if (_need_redraw) {
    draw_screen();
    _need_redraw = false;
  }

  delay(20);
}
