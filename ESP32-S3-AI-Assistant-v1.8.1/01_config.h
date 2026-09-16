// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 01_config.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 1 ── CONFIGURATION
// ═══════════════════════════════════════════════════════
namespace Config {
  // WiFi
  constexpr const char* SSID       = "YOUR_WIFI_SSID";
  constexpr const char* PASSWORD   = "YOUR_WIFI_PASSWORD";

  // ── OpenRouter API (main AI) ────────────────────────
  constexpr const char* AI_KEY      = "YOUR_OPENROUTER_API_KEY";
  constexpr const char* AI_ENDPOINT = "https://openrouter.ai/api/v1/chat/completions";
  constexpr const char* AI_MODEL    = "nvidia/nemotron-3.5-lightning:free";
  constexpr const char* AI_COMPLEX_MODEL  = "minimax/minimax-m3:free";
  constexpr const char* AI_FALLBACK_MODEL = "openrouter/free";

  // Weather and Web Search
  constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY";
  constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";

  // Timing
  constexpr int    NTP_OFFSET_SEC         = 19800;
  constexpr int    NTP_UPDATE_INTERVAL_MS = 60000;
  constexpr int    WIFI_RETRY_LIMIT       = 20;
  constexpr int    HTTP_TIMEOUT_MS        = 35000;   // v1.7.8: +5s for larger model
  constexpr int    SENTIMENT_TIMEOUT_MS   = 12000;   // v1.7.8: +2s for 70B
  constexpr int    REMINDER_TIMEOUT_MS    = 15000;
  // v1.7.9 FIX: was 45000 ms (45 s) which exceeds the 40 s WDT timeout.
  // postJson() blocks the main loop task (WDT-subscribed) for the full duration.
  // Reduced to 35 s — safely under the watchdog, still long enough for skill generation.
  constexpr int    SKILL_GEN_TIMEOUT_MS   = 35000;
  constexpr unsigned long PROACTIVE_INTERVAL_MS = 2700000UL;
  constexpr unsigned long REMINDER_ALERT_MS     = 10000UL;
  constexpr unsigned long REPLIED_FLASH_MS      = 2000UL;

  // Memory limits — v1.7.8: expanded for 8MB PSRAM boards
  constexpr int    MAX_CHAT_TOKENS   = 3000;   // was 1400
  constexpr int    MAX_CHAT_MESSAGES = 30;     // was 20
  constexpr int    MAX_MEMORY_FACTS  = 80;     // was 60
  constexpr int    MAX_SENTIMENT_LOG = 40;     // was 30
  constexpr int    MAX_REMINDERS     = 30;
  constexpr size_t CHAT_MSG_LEN     = 2000;   // was 1200
  constexpr size_t ROLE_LEN        = 10;
  constexpr size_t MAX_INPUT_LEN   = 512;
  constexpr int    NO_PSRAM_CHAT_MESSAGES = 12;
  constexpr int    NO_PSRAM_CHAT_TOKENS   = 1400;
  constexpr int    NO_PSRAM_MAX_TOKENS    = 512;

  // AI generation params — v1.7.8: longer responses from 70B
  constexpr float  AI_TEMPERATURE  = 0.70f;   // default; overridden by mood engine
  constexpr int    AI_MAX_TOKENS   = 1024;    // was 600
  constexpr int    AI_MAX_RETRIES  = 3;
  constexpr unsigned long SEARCH_CACHE_RECENT_MS  = 15UL * 60UL * 1000UL;
  constexpr unsigned long SEARCH_CACHE_GENERAL_MS = 6UL * 60UL * 60UL * 1000UL;
  constexpr int    MAX_SEARCH_CACHE = 8;

  // LED
  constexpr float  LED_MAX_BRIGHTNESS = 0.25f;
  constexpr float  LED_MIN_BRIGHTNESS = 0.05f;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  constexpr int    NEOPIXEL_PIN       = 48;
#else
  // GPIO 48 does not exist on the original ESP32. GPIO 2 is the common
  // onboard LED/data pin and can be overridden here for a specific board.
  constexpr int    NEOPIXEL_PIN       = 2;
#endif
  constexpr int    NUMPIXELS          = 1;

  // Safety
  constexpr uint32_t HEAP_SAFE_BYTES = 35000;  // was 30000
  constexpr int      WDT_TIMEOUT_S   = 40;     // was 30; 70B needs more time

  // ── Self-taught Skills Engine (OpenRouter) ──────────
  constexpr const char* SKILL_KEY      = "YOUR_OPENROUTER_SKILL_API_KEY"; // may use the same OpenRouter account
  constexpr const char* SKILL_MODEL    = "minimax/minimax-m3:free";             // skill builder (free tier)
  constexpr const char* SKILL_ENDPOINT = "https://openrouter.ai/api/v1/chat/completions";
  constexpr int    MAX_SKILLS             = 20;
  constexpr int    MAX_SKILL_VARS         = 12;
  constexpr int    MAX_SKILL_STR_VARS     = 4;
  constexpr int    MAX_LOOP_COUNT         = 50;

  // ── PSRAM HTTP buffers — v1.7.8: tripled for 70B responses ──
  constexpr size_t PSRAM_REQ_SIZE  = 24576;   // 24 KB (was 8 KB)
  constexpr size_t PSRAM_RESP_SIZE = 65536;   // 64 KB (was 16 KB)

  // ── v1.7.8: CPU frequency tiers ──────────────────────
  constexpr int CPU_FREQ_IDLE   = 80;    // MHz — when waiting for input
  constexpr int CPU_FREQ_ACTIVE = 240;   // MHz — during HTTP calls

  // ── OTA Auto-Update (GitHub Releases) ───────────────
  // Downloads whatever .bin asset is attached to the latest GitHub release.
  // Only bump FIRMWARE_VERSION when you release — nothing else to change.
  constexpr const char* OTA_GITHUB_USER      = "ItzCoding";
  constexpr const char* OTA_GITHUB_REPO      = "ESP32-S3-Ai-Assistant";
  constexpr const char* OTA_API_URL          =
    "https://api.github.com/repos/ItzCoding/ESP32-S3-Ai-Assistant/releases/latest";
  constexpr const char* FIRMWARE_VERSION     = "1.8.1";
  constexpr int         OTA_CHECK_TIMEOUT_MS = 12000;

  constexpr const char* VERSION = "ESP32-AI v1.8.1 (automatic multi-model routing, modular)";
}

