// ╔═════════════════════════════════════════════════════════════════════════╗
// ║               ESP32-S3-AI Assistant  v1.8.0 (modular)                   ║
// ║  AI      : OpenRouter · Nemotron 3.5 Lightning (OpenAI-compatible)      ║
// ║  Search  : Serper.dev        Weather : Meteosource                      ║
// ║  Features: SSE streaming · Function calling · AI summarization          ║
// ║  Skills  : MiniMax M3 generates · chat AI verifies · feedback loop      ║
// ║  v1.8.0  : MODULAR — code split into 28 files (01_config.h .. 28_ota.h) ║
// ║  v1.8.0  : NEW commands /time /forget /wifi /reboot + reset-reason log  ║
// ║  v1.8.0  : Fixed /remove deleting reminder #0 on non-numeric input      ║
// ║  v1.8.0  : Fixed sentiment-parser crash on malformed AI reply           ║  
// ║            coverage (short words like "stop"/"reset"/"check" required)  ║
// ║  v1.8.0  : Switched Groq/Gemini → OpenRouter (Nemotron 3.5 + MiniMax M3)║
// ║  Consoles: https://openrouter.ai/settings/keys                          ║
// ║  Target  : ESP32-S3 · works with OR without OPI PSRAM                   ║
// ╠═════════════════════════════════════════════════════════════════════════╣
// ── REQUIRED BOARD SETTINGS (Arduino IDE → Tools) ───────────────────
//  Select the board's real flash/PSRAM options. See BOARD_SETUP.md.
// ─────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>          // OTA: firmware update over WiFi
#include <Preferences.h>        // Persistent runtime settings and OTA marker
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <FFat.h>
#include <FS.h>
#include <vector>
#include <algorithm>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#ifdef CONFIG_SPIRAM
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_psram.h>
#else
// Arduino-esp32 core 2.x has no esp_psram.h — shim esp_psram_get_size()
#include <esp32-hal-psram.h>
static inline size_t esp_psram_get_size(void) { return ESP.getPsramSize(); }
#endif
#endif
#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "driver/temperature_sensor.h"
#endif


// ==================================================================
//  MODULAR CODEBASE (v1.8.0) - each section now lives in its own file.
//  Edit only the module you need; the includes below MUST stay in this
//  exact order because the modules share one translation unit.
// ==================================================================

#include "01_config.h"
#include "02_types.h"
#include "03_globals.h"
#include "04_declarations.h"
#include "05_tasks_timers.h"
#include "06_psram_worker.h"
#include "07_setup.h"
#include "08_loop.h"
#include "09_state_flush.h"
#include "10_input_handler.h"
#include "11_conversation.h"
#include "12_prompts.h"
#include "13_groq_api.h"
#include "14_language.h"
#include "15_auto_learn.h"
#include "16_reminders.h"
#include "17_nl_parser.h"
#include "18_memory.h"
#include "19_sentiment.h"
#include "20_user_patterns.h"
#include "21_knowledge.h"
#include "22_web_weather.h"
#include "23_chat_history.h"
#include "24_proactive.h"
#include "25_diagnostics.h"
#include "26_led.h"
#include "27_skills.h"
#include "28_ota.h"
