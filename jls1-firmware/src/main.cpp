// JLS-1 — Lab Voice Satellite firmware
//
// Push-to-talk audio front end for the Claude/JARVIS lab setup. Captures mic audio
// while the button is held, streams it to the LAB server over a plaintext WebSocket,
// and plays back the server's synthesized reply through the onboard speaker.
//
// Hardware, wiring, and the wire protocol this speaks are specified in:
//   ../docs/lab_audio_satellite_design.md   (architecture, firmware design, LAB-server contract)
//   ../docs/images/jls1_schematic.svg       (schematic)
//
// State machine (mirrors the design doc):
//
//   IDLE --(button down)--> LISTENING --(button up)--> THINKING --(TTS audio)--> SPEAKING --> IDLE
//
// Wire protocol on ws://<LAB_SERVER_HOST>:<LAB_SERVER_PORT><LAB_SERVER_PATH>:
//   Every binary WS frame's first byte is a tag:
//     0x01  TAG_MIC_AUDIO         client -> server, raw 16kHz/16-bit/mono PCM payload
//     0x02  TAG_TTS_AUDIO         server -> client, raw 16kHz/16-bit/mono PCM payload
//     0x03  TAG_END_OF_UTTERANCE  both directions, no payload:
//                                   client -> server: "that's everything I said, go"
//                                   server -> client: "that's the whole reply, done"
//
// This is intentionally a single-loop, synchronous v1 — no FreeRTOS tasks, no
// barge-in, no on-device wake word. See the design doc's "Future improvements"
// section; those are real gaps, just not this file's job yet.

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>

// Copy include/config.h.example to include/config.h and fill in real values —
// config.h is gitignored on purpose (it holds WiFi + LAB auth secrets).
#include "config.h"

// ---------------------------------------------------------------------------
// Pins — must match docs/images/jls1_schematic.svg. If you're on a different
// ESP32-S3 board and any of these land on a flash/PSRAM-strapped GPIO, change
// them here (and re-wire to match) rather than anywhere else in this file.
// ---------------------------------------------------------------------------
static const gpio_num_t MIC_WS_PIN    = GPIO_NUM_16; // INMP441 WS   (I2S0)
static const gpio_num_t MIC_SCK_PIN   = GPIO_NUM_17; // INMP441 SCK  (I2S0)
static const gpio_num_t MIC_SD_PIN    = GPIO_NUM_15; // INMP441 SD   (I2S0 data in)

static const gpio_num_t AMP_LRC_PIN   = GPIO_NUM_5;  // MAX98357A LRC  (I2S1)
static const gpio_num_t AMP_BCLK_PIN  = GPIO_NUM_7;  // MAX98357A BCLK (I2S1)
static const gpio_num_t AMP_DIN_PIN   = GPIO_NUM_6;  // MAX98357A DIN  (I2S1 data out)
static const gpio_num_t AMP_SD_PIN    = GPIO_NUM_4;  // MAX98357A SD   (enable/mute)

static const uint8_t BUTTON_PIN = 18; // push-to-talk, INPUT_PULLUP, active LOW
static const uint8_t LED_PIN    = 8;  // WS2812 status LED
static const uint8_t LED_COUNT  = 1;

static const uint32_t SAMPLE_RATE  = 16000; // matches the LAB-server contract
static const uint32_t DEBOUNCE_MS  = 40;

// INMP441 outputs 24-bit samples left-justified in a 32-bit I2S word. Shifting
// right this many bits converts a raw 32-bit read into a 16-bit PCM sample.
// This is board/gain dependent — if audio is too quiet, lower it; if it's
// clipping, raise it. 14 is a commonly-cited starting point, not gospel.
static const int MIC_SHIFT = 14;

// ---------------------------------------------------------------------------
// Wire protocol tags — see file header.
// ---------------------------------------------------------------------------
enum FrameTag : uint8_t {
  TAG_MIC_AUDIO        = 0x01,
  TAG_TTS_AUDIO         = 0x02,
  TAG_END_OF_UTTERANCE = 0x03,
};

enum class State { IDLE, LISTENING, THINKING, SPEAKING };

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static WebSocketsClient webSocket;
static Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static State state = State::IDLE;
static bool wsConnected = false;

// ---------------------------------------------------------------------------
// Amp enable/mute — kept off except while actually SPEAKING, both to save
// power and as a little insurance against the amp picking up bus noise while
// the mic side is active.
// ---------------------------------------------------------------------------
void setAmpEnabled(bool enabled) {
  digitalWrite(AMP_SD_PIN, enabled ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// Status LED — dim colors on purpose, this thing sits on a bench all day.
// ---------------------------------------------------------------------------
void updateLed() {
  uint32_t color;
  if (!wsConnected) {
    color = pixel.Color(40, 0, 0); // red: no link to the LAB server
  } else {
    switch (state) {
      case State::IDLE:      color = pixel.Color(0, 0, 0);   break;
      case State::LISTENING: color = pixel.Color(0, 0, 40);  break; // blue
      case State::THINKING:  color = pixel.Color(40, 24, 0); break; // amber
      case State::SPEAKING:  color = pixel.Color(0, 40, 0);  break; // green
      default:                color = pixel.Color(40, 0, 40); break; // shouldn't happen
    }
  }
  pixel.setPixelColor(0, color);
  pixel.show();
}

// ---------------------------------------------------------------------------
// I2S setup — mic on I2S0 (RX only), amp on I2S1 (TX only), run concurrently.
// ---------------------------------------------------------------------------
void setupMicI2S() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // matches L/R tied to GND on the mic
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = MIC_SCK_PIN;
  pins.ws_io_num = MIC_WS_PIN;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_SD_PIN;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupAmpI2S() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  // MAX98357A sums L+R internally; sending the same sample on both channels
  // is the standard way to drive it from a mono source.
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = AMP_BCLK_PIN;
  pins.ws_io_num = AMP_LRC_PIN;
  pins.data_out_num = AMP_DIN_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_1, &pins);
  i2s_zero_dma_buffer(I2S_NUM_1);
}

// ---------------------------------------------------------------------------
// Mic capture -> WS, one small chunk per loop() iteration while LISTENING.
// Short read timeout so this never blocks loop() long enough to starve
// webSocket.loop() or trip the watchdog.
// ---------------------------------------------------------------------------
void pumpMicToServer() {
  static int32_t raw[256];
  static uint8_t frame[1 + sizeof(raw) / 2]; // tag byte + room for 16-bit-ified samples

  size_t bytesRead = 0;
  esp_err_t err = i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(20));
  if (err != ESP_OK || bytesRead == 0) return;

  size_t sampleCount = bytesRead / sizeof(int32_t);
  frame[0] = TAG_MIC_AUDIO;
  int16_t* out16 = reinterpret_cast<int16_t*>(frame + 1);
  for (size_t i = 0; i < sampleCount; i++) {
    out16[i] = static_cast<int16_t>(raw[i] >> MIC_SHIFT);
  }

  webSocket.sendBIN(frame, 1 + sampleCount * sizeof(int16_t));
}

// ---------------------------------------------------------------------------
// TTS playback — mono 16-bit PCM in, duplicated to stereo out for the amp.
// Bounded write timeout: if the DMA buffer is backed up (server sending
// faster than we can play), drop the rest of this chunk rather than hang.
// ---------------------------------------------------------------------------
void playTtsChunk(const uint8_t* pcm16Mono, size_t byteLen) {
  static int16_t stereoBuf[512]; // 256 mono samples per pass
  const size_t maxMonoPerPass = sizeof(stereoBuf) / sizeof(stereoBuf[0]) / 2;

  size_t sampleCount = byteLen / sizeof(int16_t);
  const int16_t* mono = reinterpret_cast<const int16_t*>(pcm16Mono);

  size_t offset = 0;
  while (offset < sampleCount) {
    size_t remaining = sampleCount - offset;
    size_t chunk = remaining < maxMonoPerPass ? remaining : maxMonoPerPass;

    for (size_t i = 0; i < chunk; i++) {
      stereoBuf[2 * i] = mono[offset + i];
      stereoBuf[2 * i + 1] = mono[offset + i];
    }

    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(I2S_NUM_1, stereoBuf, chunk * 2 * sizeof(int16_t),
                               &bytesWritten, pdMS_TO_TICKS(100));
    if (err != ESP_OK || bytesWritten == 0) {
      Serial.println("[i2s] amp write timeout, dropping rest of this chunk");
      break;
    }
    offset += chunk;
  }
}

// ---------------------------------------------------------------------------
// Server -> client frame handling
// ---------------------------------------------------------------------------
void handleServerFrame(uint8_t tag, const uint8_t* data, size_t len) {
  switch (tag) {
    case TAG_TTS_AUDIO:
      if (state == State::THINKING) {
        state = State::SPEAKING;
        setAmpEnabled(true);
        Serial.println("[state] THINKING -> SPEAKING");
      }
      if (state == State::SPEAKING && len > 0) {
        playTtsChunk(data, len);
      }
      break;

    case TAG_END_OF_UTTERANCE:
      if (state == State::THINKING || state == State::SPEAKING) {
        setAmpEnabled(false);
        state = State::IDLE;
        Serial.println("[state] -> IDLE (reply complete)");
      }
      break;

    default:
      Serial.printf("[ws] unknown frame tag 0x%02X (len=%u), ignoring\n", tag, (unsigned)len);
      break;
  }
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("[ws] connected");
      break;

    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("[ws] disconnected");
      // Don't strand the satellite mid-conversation if the link drops.
      if (state != State::IDLE) {
        setAmpEnabled(false);
        state = State::IDLE;
      }
      break;

    case WStype_BIN:
      if (length < 1) break;
      handleServerFrame(payload[0], payload + 1, length - 1);
      break;

    case WStype_ERROR:
      Serial.printf("[ws] error, len=%u\n", (unsigned)length);
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Push-to-talk button — simple time-based debounce, active LOW.
// ---------------------------------------------------------------------------
void onButtonPressed() {
  if (state == State::IDLE && wsConnected) {
    state = State::LISTENING;
    Serial.println("[state] IDLE -> LISTENING");
  }
}

void onButtonReleased() {
  if (state == State::LISTENING) {
    uint8_t endTag = TAG_END_OF_UTTERANCE;
    webSocket.sendBIN(&endTag, 1);
    state = State::THINKING;
    Serial.println("[state] LISTENING -> THINKING");
  }
}

void updateButton() {
  static bool lastStable = HIGH;
  static bool lastReading = HIGH;
  static uint32_t lastChangeMs = 0;

  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {
    lastChangeMs = millis();
    lastReading = reading;
  }
  if ((millis() - lastChangeMs) > DEBOUNCE_MS && reading != lastStable) {
    lastStable = reading;
    if (lastStable == LOW) {
      onButtonPressed();
    } else {
      onButtonReleased();
    }
  }
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s", WIFI_SSID);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[wifi] not connected after 30s — will keep retrying in the background");
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nJLS-1 booting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(AMP_SD_PIN, OUTPUT);
  setAmpEnabled(false);

  pixel.begin();
  updateLed();

  setupMicI2S();
  setupAmpI2S();

  connectWifi();

  webSocket.onEvent(onWsEvent);
  webSocket.setExtraHeaders("Authorization: Bearer " LAB_AUTH_TOKEN "\r\n");
  webSocket.begin(LAB_SERVER_HOST, LAB_SERVER_PORT, LAB_SERVER_PATH);
  webSocket.setReconnectInterval(3000);

  Serial.println("JLS-1 ready.");
}

void loop() {
  webSocket.loop();
  updateButton();
  updateLed();

  if (state == State::LISTENING) {
    pumpMicToServer();
  }
}
