// ╔══════════════════════════════════════════════════════════════════╗
// ║               ESP32-S3-AI Assistant  v1.0-GROQ                   ║
// ║  Made by : ItzCoding                                             ║
// ║  AI      : Groq · llama-3.1-8b-instant (OpenAI-compatible)       ║
// ║  Search  : Serper.dev                                            ║
// ║  Weather : Meteosource                                           ║
// ║  Features: SSE streaming · AI diagnostics · NL intent parser     ║
// ║  Skills  : Self-taught via Gemini · advanced DSL ops             ║
// ║  Console : https://console.groq.com/keys                         ║
// ║  Target  : ESP32-S3 · works with OR without OPI PSRAM            ║
// ╠══════════════════════════════════════════════════════════════════╣
// ── REQUIRED BOARD SETTINGS (Arduino IDE → Tools) ───────────────────
//  Board           : "ESP32S3 Dev Module"
//  PSRAM           : "OPI PSRAM" if your board has it, or "Disabled" — both work
//  Flash Size      : "16MB (128Mb)"
//  Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"
// ─────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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
#include <esp_psram.h>                           
#endif
#include "driver/temperature_sensor.h"

// ═══════════════════════════════════════════════════════
// SECTION 1 ── CONFIGURATION
// ═══════════════════════════════════════════════════════
namespace Config {
  // WiFi
  constexpr const char* SSID       = "YOUR_WIFI_SSID";
  constexpr const char* PASSWORD   = "YOUR_WIFI_PASSWORD";

  // ── Groq API ────────────────────────────────────────
  constexpr const char* GROQ_KEY      = "YOUR_GROQ_API_KEY";       // get from console.groq.com
  constexpr const char* GROQ_ENDPOINT = "https://api.groq.com/openai/v1/chat/completions";
  constexpr const char* GROQ_MODEL    = "llama-3.1-8b-instant";

  // Web search / Weather
  constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY"; // get from meteosource.com
  constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";    // get from serper.dev

  // Timing
  constexpr int    NTP_OFFSET_SEC         = 19800;
  constexpr int    NTP_UPDATE_INTERVAL_MS = 60000;
  constexpr int    WIFI_RETRY_LIMIT       = 20;
  constexpr int    HTTP_TIMEOUT_MS        = 30000;
  constexpr int    SENTIMENT_TIMEOUT_MS   = 10000;
  constexpr int    REMINDER_TIMEOUT_MS    = 15000;
  constexpr unsigned long PROACTIVE_INTERVAL_MS = 2700000UL;
  constexpr unsigned long REMINDER_ALERT_MS     = 10000UL;
  constexpr unsigned long REPLIED_FLASH_MS      = 2000UL;

  // Memory limits
  constexpr int    MAX_CHAT_TOKENS   = 1400;
  constexpr int    MAX_CHAT_MESSAGES = 20;
  constexpr int    MAX_MEMORY_FACTS  = 60;
  constexpr int    MAX_SENTIMENT_LOG = 30;
  constexpr int    MAX_REMINDERS     = 30;
  constexpr size_t CHAT_MSG_LEN     = 1200;    
  constexpr size_t ROLE_LEN        = 10;

  // AI generation params
  constexpr float  AI_TEMPERATURE  = 0.75f;
  constexpr int    AI_MAX_TOKENS   = 600;
  constexpr int    AI_MAX_RETRIES  = 3;

  // LED
  constexpr float  LED_MAX_BRIGHTNESS = 0.25f;
  constexpr float  LED_MIN_BRIGHTNESS = 0.05f;
  constexpr int    NEOPIXEL_PIN       = 48;
  constexpr int    NUMPIXELS          = 1;

  // Safety
  constexpr uint32_t HEAP_SAFE_BYTES = 30000;
  constexpr int      WDT_TIMEOUT_S   = 30;

  // ── Self-taught Skills Engine ────────────────────────
  constexpr const char* GEMINI_API_KEY  = "YOUR_GEMINI_API_KEY";   // get from aistudio.google.com/apikey
  constexpr const char* GEMINI_MODEL    = "gemini-2.5-flash";
  constexpr const char* GEMINI_ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  constexpr int    SKILL_GEN_MAX_ATTEMPTS = 3;
  constexpr int    SKILL_GEN_TIMEOUT_MS   = 20000;
  constexpr int    MAX_SKILLS             = 20;
  constexpr int    MAX_SKILL_VARS         = 12;   
  constexpr int    MAX_SKILL_STR_VARS     = 4;    
  constexpr int    MAX_SKILL_ACTIONS      = 8;     
  constexpr int    MAX_OPS_PER_ACTION     = 16;   
  constexpr int    MAX_SKILL_WAIT_MS      = 3000;
  constexpr size_t MAX_SKILL_JSON_BYTES   = 4096; 
  constexpr int    MAX_IF_DEPTH           = 3;     
  constexpr int    MAX_LOOP_COUNT         = 10;   

  // [1] PSRAM buffer sizes
  constexpr size_t PSRAM_REQ_SIZE  = 8192;   
  constexpr size_t PSRAM_RESP_SIZE = 16384; 

  constexpr const char* VERSION = "ESP32-AI v1.0-GROQ (Llama 3.1 8B)";
}

// ═══════════════════════════════════════════════════════
// SECTION 2 ── TYPES & DATA STRUCTURES
// ═══════════════════════════════════════════════════════

enum RecurrenceType : uint8_t { ONCE, DAILY, WEEKLY, MONTHLY };

struct Reminder {
  String        message;
  uint8_t       hour, minute, dayOfWeek, dayOfMonth;
  RecurrenceType recurrence;
  bool          triggered;
  uint32_t      triggerCount;
};

struct Fact {
  String key, value;
  uint32_t      accessCount;
  unsigned long lastAccess;
};

struct ChatMessage {
  char role[Config::ROLE_LEN];
  char content[Config::CHAT_MSG_LEN];
};

struct SentimentLog {
  String sentiment;
  float  score;
  unsigned long timestamp;
};

struct UserPattern {
  int           totalInteractions = 0;
  int           morningChats      = 0;
  int           eveningChats      = 0;
  String        favoriteTopics[5];
  unsigned long lastInteraction   = 0;
  String        recentMood        = "neutral";
  int           techQuestions     = 0;
  int           casualMessages    = 0;
  int           reminderUsage     = 0;
};

struct KnowledgeArea {
  String  domain;
  int     experiencePoints;
  float   confidenceLevel;
  uint8_t colorR, colorG, colorB;
};

enum AIState : uint8_t {
  AI_IDLE, AI_THINKING, AI_REPLIED, AI_ERROR,
  AI_ALERT, AI_EXCITED, AI_CONCERNED, AI_PROACTIVE,
  AI_LEARNING, AI_EVOLVING
};

enum Intent : uint8_t {
  INTENT_NONE = 0,
  INTENT_REMINDER_SET,
  INTENT_REMINDER_LIST,
  INTENT_REMINDER_CANCEL,
  INTENT_MEMORY_SAVE,
  INTENT_MEMORY_RECALL,
  INTENT_MEMORY_FORGET,
  INTENT_NOTE_ADD,
  INTENT_NOTE_RECALL,
  INTENT_TASK_ADD,
  INTENT_SEARCH,
  INTENT_SUMMARY,
  INTENT_SYSTEM_STATUS,
  INTENT_WEATHER,
  INTENT_CORRECTION,
  INTENT_FOLLOWUP_TIME,
  INTENT_CHAT
};

struct ParsedTime {
  bool          found;
  int           hour;
  int           minute;
  bool          isRelative;
  int           relativeMinutes;
  bool          isTomorrow;
  RecurrenceType recurrence;
  int           dayOfWeek;
};

struct ParsedEntities {
  String     content;
  ParsedTime time;
  String     reference;
  int        referenceIndex;
};

struct ParsedCommand {
  Intent         intent;
  ParsedEntities entities;
  float          confidence;
};

struct NLContext {
  Intent         lastIntent;
  ParsedEntities lastEntities;
  Intent         pendingIntent;
  ParsedEntities pendingEntities;
  String         lastTopic;
  unsigned long  lastUpdate;
};

// ═══════════════════════════════════════════════════════
// SECTION 3 ── GLOBAL STATE
// ═══════════════════════════════════════════════════════

WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", Config::NTP_OFFSET_SEC, Config::NTP_UPDATE_INTERVAL_MS);

String inputString    = "";
bool   stringComplete = false;

std::vector<Fact>         memory;
std::vector<Reminder>     reminders;
std::vector<ChatMessage>  chatHistory;
std::vector<SentimentLog> sentimentHistory;
std::vector<KnowledgeArea>knowledgeDomains;

// ── Self-taught Skills Engine ────────────────────────
std::vector<String> skillNames;
std::vector<String> skillJson;
bool   testingSkill = false;
int    pendingSkillIndex = -1;
String pendingSkillRequest;
String pendingBackupName;
String pendingBackupJson;
struct SkillTrigger { int skillIdx; String action; String phrase; String keywords; };
std::vector<SkillTrigger> skillTriggerIndex;

UserPattern   userPattern;
int           consecutivePositive = 0;
int           consecutiveNegative = 0;
int           thinkingComplexity  = 0;

unsigned long bootTime            = 0;
unsigned long lastProactiveCheck  = 0;
bool          morningBriefingGiven   = false;
bool          morningBriefingEnabled = true;

static uint32_t noteTaskCounter = 0;

// ── Telemetry counters ──────────────────────────────────
int           httpTimeoutCount   = 0;
int           wifiReconnectCount = 0;
uint32_t      heapSnapshot       = 0;
unsigned long heapSnapshotTime   = 0;

// Runtime-overridable API settings
String customApiKey      = Config::GROQ_KEY;
float  customTemperature = Config::AI_TEMPERATURE;
int    customMaxTokens   = Config::AI_MAX_TOKENS;

Adafruit_NeoPixel strip(Config::NUMPIXELS, Config::NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
AIState       aiState        = AI_IDLE;
unsigned long stateChangeTime = 0;
unsigned long lastBlink       = 0;
int           blinkCount      = 0;
float         pulseBrightness = Config::LED_MIN_BRIGHTNESS;
bool          pulseIncreasing = true;

NLContext nlContext = { INTENT_NONE, {}, INTENT_NONE, {}, "", 0 };
constexpr unsigned long NL_PENDING_TTL_MS = 60000UL;

// ── [2] Persistent temperature sensor handle ─────────────
// Installed once in setup(), reused for every getCpuTemp() call.
// Avoids the expensive install/enable/disable/uninstall cycle per call.
static temperature_sensor_handle_t g_tsens = nullptr;

// ── [1] PSRAM HTTP buffers ───────────────────────────────
// Allocated once in initPsram() from OPI PSRAM.
// Request bodies serialized here (no internal SRAM allocation).
// Non-streaming response bodies received here before JSON parsing.
static char*  g_psramReqBuf  = nullptr;  // 8 KB  — request serialization
static char*  g_psramRespBuf = nullptr;  // 16 KB — response parsing

// ── [3] Dual-Core AI Worker ─────────────────────────────
// Core 0 handles all blocking HTTP (Groq, Gemini, Serper, weather).
// Core 1 (Arduino loop) spins with WDT resets and LED updates while waiting.
struct DualCoreReq {
  String url;
  String auth;          // full "Bearer ..." header value
  String body;          // serialized JSON body
  int    timeoutMs;
  bool   doStream;      // true = SSE streaming; false = plain JSON response
};
struct DualCoreResp {
  String reply;
  bool   ok;
};
static DualCoreReq       g_dcReq;
static DualCoreResp      g_dcResp;
static volatile bool     g_dcBusy    = false;
static SemaphoreHandle_t g_dcReqSem  = nullptr;   // Core 1 → Core 0: "request ready"
static SemaphoreHandle_t g_dcRespSem = nullptr;   // Core 0 → Core 1: "response ready"
static TaskHandle_t      g_dcTask    = nullptr;

// ── [5] Batched flash-write dirty flags ─────────────────
// Non-critical stores (sentiment, pattern, knowledge) are written together
// every FLASH_BATCH_INTERVAL_MS instead of on every interaction.
static bool          g_dirtySentiment  = false;
static bool          g_dirtyPattern    = false;
static bool          g_dirtyKnowledge  = false;
static unsigned long g_lastFlushMs     = 0;
constexpr unsigned long FLASH_BATCH_INTERVAL_MS = 30000UL;

// ── [5] Non-blocking WiFi reconnect ─────────────────────
static unsigned long g_wifiReconnectStart = 0;
static bool          g_wifiReconnecting   = false;

// ═══════════════════════════════════════════════════════
// SECTION 4 ── FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════

String groqSimpleCall(const String& prompt, float temp = 0.1f, int maxTok = 128);
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens);
String groqStream(const String& userPrompt, const String& sysPrompt = "",
                  float temp = 0.7f, int maxTok = 600);
String sendToGroqStream(const String& userMessage, const String& extraContext = "");
String buildSystemPrompt();
String buildLiveContext();
bool   aiIsUncertain(const String& reply);
String buildSearchQuery(const String& message);
bool   isRecencyQuery(const String& lowerMsg);
void   autoLearnFromMessage(const String& userMsg);
void   calculateThinkingComplexity(const String& message);

String fetchWebSearchResults(const String& query);
bool   serperRequest(const String& query, int num, const String& tbs, JsonDocument& doc);
String httpGetWithRetry(const String& url, int maxRetries = 3, int delayMs = 2000);
void   getWeather(String city);
void   searchWeb(const String& query);
String urlEncode(const String& str);

void   rememberFact(const String& key, const String& value);
String recallFact(const String& key);
void   removeFact(const String& key);
void   loadMemory();   void saveMemory();

bool   tryParseNaturalReminder(const String& message);
void   addReminder(const String& msg, int h, int m, RecurrenceType recur, int dow = 0, int dom = 0);
void   listReminders();
void   removeReminder(int index);
bool   shouldReminderTrigger(const Reminder& r);
String formatReminderTime(int hour, int minute);
String getRecurrenceText(RecurrenceType recur, int dow, int dom);
void   processReminders();
void   loadReminders();  void saveReminders();

void   addUserMessage(const String& msg);
void   addAssistantMessage(const String& msg);
void   limitChatHistoryByTokens(int maxTokens = Config::MAX_CHAT_TOKENS);
void   summarizeChatHistory();
void   loadChatHistory();  void saveChatHistory();

String detectSentiment(const String& message);
void   trackSentiment(const String& sentiment, float score);
void   respondToMood();
void   celebratePositiveVibes();
void   offerComfort();
void   smartResponseEnhancement(String& response);

void   updateUserPattern(const String& message);
String analyzeConversationTopic(const String& message);
void   loadUserPattern();  void saveUserPattern();
void   loadSentimentData();void saveSentimentData();

void          initializeKnowledgeDomains();
void          updateKnowledgeDomain(const String& domain, int xpGain);
KnowledgeArea* getDominantKnowledge();
void          loadKnowledgeDomains(); void saveKnowledgeDomains();

bool   autoMorningBriefing();
void   checkProactiveOpportunity();
void   generateMorningBriefing();

void   handleInput(const String& input);
void   processConversation(const String& userMsg);
void   printHelp();
void   printVersion();
void   clearAll();

bool          parseTime(const String& s, ParsedTime& out);
Intent        detectIntent(const String& s);
void          extractEntities(const String& s, Intent intent, ParsedEntities& out);
ParsedCommand parseNaturalLanguage(const String& s);
bool          executeIntent(const ParsedCommand& cmd, const String& original);
void          nlRememberLast(const ParsedCommand& cmd);
void          nlClearPending();

void   updateLED();
void   setLEDColor(uint8_t r, uint8_t g, uint8_t b, float brightness = 1.0f);
void   rainbowWave(int durationMs);

float  getCpuTemp();
void   systemDiagnostics();
bool   heapOk();
int    estimateTokens(const char* text);

// [1] PSRAM helpers
void   initPsram();
static String  dcPost(const String& url, const String& authBearer, const String& body,
                      int timeoutMs, bool doStream);
static void    aiHttpTask(void* param);

// Self-taught Skills Engine
bool   isAlnumCh(char c);
char   toLowerCh(char c);
String stripToAlnum(const String& s);
String extractKeywords(String phrase);
bool   matchLearnedSkill(const String& input, int& outSkillIdx, String& outAction);
void   runSkillAction(int skillIdx, const String& actionName);
void   listSkills();
void   removeSkill(const String& name);
void   loadSkills();
void   saveSkills();
bool   looksLikeFeatureRequest(const String& input);
void   learnNewSkill(const String& request);
void   beginSkillTest(const String& request, const String& candidateJson);
void   commitPendingSkill();
void   discardPendingSkill();
void   retryPendingSkill();

// ═══════════════════════════════════════════════════════
// SECTION 4.5 ── [1] PSRAM INIT & [3] DUAL-CORE WORKER
// ═══════════════════════════════════════════════════════

// [1] Allocate PSRAM HTTP buffers once at startup.
void initPsram() {
  if (!psramFound()) {
    Serial.println("⚠️  PSRAM not detected — using internal SRAM for HTTP buffers");
    return;
  }
  g_psramReqBuf = (char*)heap_caps_malloc(Config::PSRAM_REQ_SIZE,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_psramRespBuf = (char*)heap_caps_malloc(Config::PSRAM_RESP_SIZE,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (g_psramReqBuf && g_psramRespBuf) {
    Serial.printf("✅ PSRAM: req=%uB resp=%uB  total free=%u KB\n",
                  (unsigned)Config::PSRAM_REQ_SIZE,
                  (unsigned)Config::PSRAM_RESP_SIZE,
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  } else {
    Serial.println("⚠️  PSRAM alloc partial — falling back to SRAM for HTTP");
    if (g_psramReqBuf)  { heap_caps_free(g_psramReqBuf);  g_psramReqBuf  = nullptr; }
    if (g_psramRespBuf) { heap_caps_free(g_psramRespBuf); g_psramRespBuf = nullptr; }
  }
}

// [3] Core 0 HTTP worker task — blocks on semaphore, executes HTTP, signals done.
static void aiHttpTask(void* /*param*/) {
  for (;;) {
    // Wait for Core 1 to hand us a request
    xSemaphoreTake(g_dcReqSem, portMAX_DELAY);

    WiFiClientSecure sec;
    sec.setInsecure();
    HTTPClient http;
    http.setTimeout(g_dcReq.timeoutMs);
    http.begin(sec, g_dcReq.url);
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("Authorization", g_dcReq.auth);

    int code;
    // Use PSRAM request buffer when available — no internal SRAM allocation for body
    if (g_psramReqBuf && g_dcReq.body.length() < Config::PSRAM_REQ_SIZE) {
      memcpy(g_psramReqBuf, g_dcReq.body.c_str(), g_dcReq.body.length());
      code = http.POST((uint8_t*)g_psramReqBuf, g_dcReq.body.length());
    } else {
      code = http.POST(g_dcReq.body);
    }

    if (code == 200) {
      if (g_dcReq.doStream) {
        Serial.print("\n 🤖AI: ");
        g_dcResp.reply = consumeSSE(http, g_dcReq.timeoutMs, true);
        Serial.println();
      } else {
        // Use PSRAM response buffer — keeps response bytes out of internal SRAM
        if (g_psramRespBuf) {
          int n = http.getStream().readBytes(g_psramRespBuf, Config::PSRAM_RESP_SIZE - 1);
          g_psramRespBuf[n] = '\0';
          g_dcResp.reply = String(g_psramRespBuf);
        } else {
          g_dcResp.reply = http.getString();
        }
      }
      g_dcResp.ok = true;
    } else {
      g_dcResp.reply = "";
      g_dcResp.ok    = false;
      httpTimeoutCount++;
    }
    http.end();
    g_dcBusy = false;
    xSemaphoreGive(g_dcRespSem);
  }
}

// [3] Dispatch an HTTP POST to Core 0; spin-wait on Core 1 with WDT resets.
static String dcPost(const String& url, const String& authBearer, const String& body,
                     int timeoutMs, bool doStream) {
  // If dual-core task hasn't started (e.g. early boot), do it inline
  if (!g_dcTask || !g_dcReqSem) {
    WiFiClientSecure sec; sec.setInsecure();
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.begin(sec, url);
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("Authorization", authBearer);
    esp_task_wdt_reset();
    int code = http.POST(body);
    esp_task_wdt_reset();
    if (code != 200) { httpTimeoutCount++; http.end(); return ""; }
    String r;
    if (doStream) {
      Serial.print("\n 🤖AI: ");
      r = consumeSSE(http, timeoutMs, true);
      Serial.println();
    } else {
      r = http.getString();
    }
    http.end();
    return r;
  }

  // Hand request to Core 0
  g_dcReq.url       = url;
  g_dcReq.auth      = authBearer;
  g_dcReq.body      = body;
  g_dcReq.timeoutMs = timeoutMs;
  g_dcReq.doStream  = doStream;
  g_dcResp.reply    = "";
  g_dcResp.ok       = false;
  g_dcBusy          = true;
  xSemaphoreGive(g_dcReqSem);

  // Core 1 keeps WDT and LED alive while Core 0 does the blocking HTTP
  unsigned long deadline = millis() + (unsigned long)(timeoutMs + 8000);
  while (g_dcBusy) {
    esp_task_wdt_reset();
    updateLED();
    vTaskDelay(pdMS_TO_TICKS(20));
    if (millis() > deadline) {
      Serial.println("❌ Core 0 AI task timed out");
      g_dcBusy = false;
      return "";
    }
  }
  return g_dcResp.reply;
}

// ═══════════════════════════════════════════════════════
// SECTION 5 ── SETUP
// ═══════════════════════════════════════════════════════
void setup() {
  Serial.setRxBufferSize(1024);
  Serial.begin(115200);
  delay(800);

  // WDT configuration
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = Config::WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << 0),
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  {
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if      (wdt_err == ESP_OK)             Serial.println("✅ WDT configured");
    else if (wdt_err == ESP_ERR_INVALID_STATE) Serial.println("✅ WDT configured (already subscribed)");
    else    Serial.printf("⚠️  WDT subscribe failed: 0x%x\n", wdt_err);
  }

  Serial.println("\n🚀 " + String(Config::VERSION) + " STARTING...");

  // [1] PSRAM buffers
  initPsram();

  // [2] Temperature sensor — install once, reuse handle forever
  {
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&cfg, &g_tsens) == ESP_OK) {
      temperature_sensor_enable(g_tsens);
      Serial.println("✅ Temperature sensor ready");
    } else {
      g_tsens = nullptr;
      Serial.println("⚠️  Temperature sensor init failed");
    }
  }

  // [3] Create dual-core semaphores and launch Core 0 HTTP worker
  g_dcReqSem  = xSemaphoreCreateBinary();
  g_dcRespSem = xSemaphoreCreateBinary();
  if (g_dcReqSem && g_dcRespSem) {
    BaseType_t ok = xTaskCreatePinnedToCore(
      aiHttpTask, "ai_http",
      8192,        // stack
      nullptr,     // param
      5,           // priority (higher than loop's 1)
      &g_dcTask,
      0            // Core 0
    );
    if (ok == pdPASS) Serial.println("✅ Dual-core AI worker on Core 0");
    else              Serial.println("⚠️  Dual-core task create failed — using single-core");
  }

  randomSeed(esp_random());

  if (!FFat.begin(true)) {
    Serial.println("❌ FATFS mount failed — running without persistence");
    Serial.println("   ℹ️  Ensure Tools → Partition Scheme = '16M Flash (3MB APP/9.9MB FATFS)'.");
  } else {
    Serial.println("✅ FATFS ready");
    loadMemory();
    loadReminders();
    loadChatHistory();
    loadUserPattern();
    loadSentimentData();
    loadKnowledgeDomains();
    loadSkills();
  }

  if (knowledgeDomains.empty()) initializeKnowledgeDomains();

  // [5] Non-blocking WiFi — kick connect, continue setup
  WiFi.begin(Config::SSID, Config::PASSWORD);
  Serial.print("📶 Connecting");
  for (int i = 0; i < Config::WIFI_RETRY_LIMIT && WiFi.status() != WL_CONNECTED; i++) {
    delay(500); Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED
    ? "\n✅ WiFi connected — " + WiFi.localIP().toString()
    : "\n⚠️  WiFi failed — offline mode");

  timeClient.begin();
  timeClient.update();
  setTime(timeClient.getEpochTime());
  bootTime = millis();
  Serial.println("✅ Time synced");

  strip.begin();
  strip.clear();
  strip.show();
  rainbowWave(1800);
  strip.clear();
  strip.show();

  Serial.println("\n💡 " + String(Config::VERSION) + " READY");
  Serial.println("Model : " + String(Config::GROQ_MODEL));
  Serial.println("Type /help for commands\n");
}

// ═══════════════════════════════════════════════════════
// SECTION 6 ── MAIN LOOP
// ═══════════════════════════════════════════════════════
void loop() {
  esp_task_wdt_reset();

  // Rate-limited NTP update
  static unsigned long lastNtpSync = 0;
  if (millis() - lastNtpSync > 60000UL) {
    lastNtpSync = millis();
    if (timeClient.update()) setTime(timeClient.getEpochTime());
  }

  int nowHour   = hour();
  int nowMinute = minute();

  // [5] Non-blocking WiFi auto-reconnect
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 15000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (!g_wifiReconnecting) {
        wifiReconnectCount++;
        WiFi.disconnect();
        WiFi.begin(Config::SSID, Config::PASSWORD);
        g_wifiReconnecting   = true;
        g_wifiReconnectStart = millis();
      } else if (millis() - g_wifiReconnectStart > 10000) {
        // Give up this round; try again next check
        g_wifiReconnecting = false;
        Serial.println("⚠️  WiFi reconnect timed out — will retry");
      }
    } else {
      if (g_wifiReconnecting) {
        g_wifiReconnecting = false;
        Serial.println("✅ WiFi reconnected — " + WiFi.localIP().toString());
      }
    }
  }

  // Heap snapshot every 2 hours
  static unsigned long lastHeapSnap = 0;
  if (heapSnapshot == 0 || millis() - lastHeapSnap > 7200000UL) {
    lastHeapSnap     = millis();
    heapSnapshot     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heapSnapshotTime = millis();
  }

  // Morning briefing at 07:30
  if (autoMorningBriefing() && nowHour == 7 && nowMinute == 30 && !morningBriefingGiven) {
    generateMorningBriefing();
    morningBriefingGiven = true;
  }
  if (nowHour != 7 || nowMinute != 30) morningBriefingGiven = false;

  // Proactive suggestions
  if (millis() - lastProactiveCheck > Config::PROACTIVE_INTERVAL_MS) {
    checkProactiveOpportunity();
    lastProactiveCheck = millis();
  }

  // Reminder check (once per minute)
  static int lastCheckedMinute = -1;
  if (nowMinute != lastCheckedMinute) {
    lastCheckedMinute = nowMinute;
    processReminders();
  }

  // [5] Batched flash writes — flush non-critical dirty stores every 30 s
  if (millis() - g_lastFlushMs > FLASH_BATCH_INTERVAL_MS) {
    g_lastFlushMs = millis();
    if (g_dirtySentiment) { saveSentimentData(); g_dirtySentiment = false; }
    if (g_dirtyPattern)   { saveUserPattern();   g_dirtyPattern   = false; }
    if (g_dirtyKnowledge) { saveKnowledgeDomains(); g_dirtyKnowledge = false; }
  }

  // Serial input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { stringComplete = true; break; }
    if (c != '\r') inputString += c;
  }

  if (stringComplete) {
    inputString.trim();
    if (inputString.length() > 0) handleInput(inputString);
    inputString    = "";
    stringComplete = false;
  }

  updateLED();
}

// ═══════════════════════════════════════════════════════
// SECTION 7 ── INPUT HANDLER
// ═══════════════════════════════════════════════════════
void handleInput(const String& input) {
  String lower = input;
  lower.toLowerCase();

  Serial.println("\nYou: " + input);
  updateUserPattern(input);

  if      (lower == "/help")    { printHelp(); }
  else if (lower == "/version") { printVersion(); }
  else if (lower == "/diag")    { systemDiagnostics(); }
  else if (lower == "/reminders" || lower == "/list") { listReminders(); }
  else if (lower.startsWith("/remove") || lower.startsWith("/delete")) {
    int spacePos = input.indexOf(' ');
    if (spacePos < 0) {
      Serial.println("❌ Usage: /remove [number]   (use /reminders to see indices)");
    } else {
      int idx = input.substring(spacePos + 1).toInt();
      if (idx >= 0 && idx < (int)reminders.size()) {
        Serial.println("✅ Removed: " + reminders[idx].message);
        removeReminder(idx);
      } else {
        Serial.println("❌ Invalid index. Use /reminders to see the list.");
      }
    }
  }
  else if (lower.startsWith("/weather")) { getWeather(input.substring(8)); }
  else if (lower.startsWith("/search"))  { searchWeb(input.substring(8)); }
  else if (lower == "/clear")            { clearAll(); }
  else if (lower == "/skills")           { listSkills(); }
  else if (lower.startsWith("/skills remove ")) { removeSkill(input.substring(15)); }
  else if (lower == "/skills keep")      { commitPendingSkill(); }
  else if (lower == "/skills discard")   { discardPendingSkill(); }
  else if (lower == "/skills retry")     { retryPendingSkill(); }
  else if (testingSkill &&
           (lower == "yes" || lower == "y" || lower == "yeah" || lower == "yep" ||
            lower == "keep it" ||
            ((lower.indexOf("keep") >= 0 || lower.indexOf("save") >= 0) && lower.indexOf("skill") >= 0))) {
    commitPendingSkill();
  }
  else if (testingSkill &&
           (lower == "no" || lower == "n" || lower == "nope" || lower == "nah" ||
            ((lower.indexOf("discard") >= 0 || lower.indexOf("scrap") >= 0 ||
              lower.indexOf("throw it away") >= 0 || lower.indexOf("delete it") >= 0) &&
             lower.indexOf("skill") >= 0))) {
    discardPendingSkill();
  }
  else if (testingSkill &&
           (lower.indexOf("retry") >= 0 || lower.indexOf("try again") >= 0 || lower.indexOf("redo") >= 0)) {
    retryPendingSkill();
  }
  else {
    int    skillIdx = -1;
    String skillAction;
    if (matchLearnedSkill(input, skillIdx, skillAction)) {
      runSkillAction(skillIdx, skillAction);
      return;
    }

    ParsedCommand cmd = parseNaturalLanguage(input);
    bool handled = false;
    if (cmd.intent != INTENT_NONE) handled = executeIntent(cmd, input);
    if (!handled) handled = tryParseNaturalReminder(input);
    if (!handled && looksLikeFeatureRequest(input)) {
      if (testingSkill) {
        Serial.println("⏳ You've already got a skill pending — say \"keep the skill\", " +
                        String("\"discard the skill\", or \"retry the skill\" first."));
      } else {
        learnNewSkill(input);
      }
      handled = true;
    }
    if (!handled) processConversation(input);
  }
}

// ═══════════════════════════════════════════════════════
// SECTION 8 ── CONVERSATION ENGINE
// ═══════════════════════════════════════════════════════
void processConversation(const String& userMsg) {
  if (!heapOk()) {
    Serial.println("⚠️  Low internal SRAM — skipping AI call. Try /clear to free space.");
    return;
  }

  addUserMessage(userMsg);
  calculateThinkingComplexity(userMsg);
  aiState = AI_THINKING;
  updateLED();

  String sentiment      = "neutral";
  float  sentimentScore = 0.5f;
  String sentResult     = detectSentiment(userMsg);
  int    scoreStart     = sentResult.indexOf('(');
  if (scoreStart > 0) {
    sentimentScore = sentResult.substring(scoreStart + 1, sentResult.indexOf(')')).toFloat();
    sentiment      = sentResult.substring(0, scoreStart - 1);
  } else {
    sentiment = sentResult;
  }
  trackSentiment(sentiment, sentimentScore);

  String aiReply = sendToGroqStream(userMsg);

  if (aiIsUncertain(aiReply) && WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🔍 Searching the web for a better answer...");
    String webCtx = fetchWebSearchResults(buildSearchQuery(userMsg));
    if (webCtx.length() > 0) {
      Serial.println("🌐 Got results — asking AI again...");
      aiReply = sendToGroqStream(userMsg, webCtx);
    }
  }

  autoLearnFromMessage(userMsg);
  String topic = analyzeConversationTopic(userMsg);
  if (topic.length() > 0) updateKnowledgeDomain(topic, 10);

  smartResponseEnhancement(aiReply);
  addAssistantMessage(aiReply);
  respondToMood();

  aiState         = AI_REPLIED;
  stateChangeTime = millis();
}

// ═══════════════════════════════════════════════════════
// SECTION 9 ── SYSTEM PROMPT
// ═══════════════════════════════════════════════════════
String buildSystemPrompt() {
  String tone = "balanced and helpful";
  if (userPattern.techQuestions > userPattern.casualMessages * 2)
    tone = "precise, technical, and concise";
  else if (userPattern.casualMessages > userPattern.techQuestions * 2)
    tone = "warm, conversational, and friendly";
  else if (userPattern.reminderUsage > 5)
    tone = "organised, time-aware, and proactive";

  String factSummary = "";
  std::vector<Fact*> sortedFacts;
  for (auto& f : memory) sortedFacts.push_back(&f);
  std::sort(sortedFacts.begin(), sortedFacts.end(),
    [](Fact* a, Fact* b){ return a->accessCount > b->accessCount; });
  int factLimit = min((int)sortedFacts.size(), 8);
  for (int i = 0; i < factLimit; i++)
    factSummary += "  • " + sortedFacts[i]->key + ": " + sortedFacts[i]->value + "\n";

  String topicsStr = "";
  for (int i = 0; i < 5; i++)
    if (userPattern.favoriteTopics[i].length() > 0)
      topicsStr += userPattern.favoriteTopics[i] + " ";

  String prompt;
  prompt.reserve(1024);

  prompt  = "You are ESP32-S3-AI v1.0, an intelligent embedded assistant running on an ESP32-S3 microcontroller.\n";
  prompt += "You are powered by Llama 3.1 8B via Groq.\n";
  prompt += "Your personality tone: "; prompt += tone; prompt += ".\n\n";

  prompt += "## Core Reasoning Rules\n"
            "1. Think step-by-step before answering complex questions.\n"
            "2. If you are uncertain about a fact, say so honestly — don't hallucinate.\n"
            "3. Keep responses concise (1-4 sentences for simple questions, up to 8 for complex ones).\n"
            "4. When the user sets a reminder, confirm it clearly with time and recurrence.\n"
            "5. Adapt your language complexity to match the user's messages.\n"
            "6. Reference previously learned facts about the user to feel personalised.\n"
            "7. For recent events, scores, winners, prices, or anything that could have "
            "changed recently, do NOT guess. Clearly say something like \"I don't have "
            "the latest information on that\" or \"I'm not certain of the current answer\" "
            "— this is what lets the device fetch a live web search for you automatically. "
            "NEVER mention \"training data\", a \"knowledge cutoff\", or any specific year "
            "(e.g. 2023) as the reason — just say plainly that you don't have the latest "
            "info, without explaining why.\n\n";

  prompt += "## Capabilities You Have\n"
            "- Answer general knowledge questions\n"
            "- Set/manage reminders (one-time, daily, weekly, monthly)\n"
            "- Check weather for any city\n"
            "- Search the web when uncertain\n"
            "- Remember personal facts across sessions\n\n";

  prompt += "## What You Do NOT Do\n"
            "- Do not make up specific facts, dates, or numbers you are unsure of.\n"
            "- Do not roleplay as a different AI.\n"
            "- Do not produce harmful content.\n\n";

  prompt += "## User Profile\n";
  prompt += "  Interactions: "; prompt += String(userPattern.totalInteractions); prompt += "\n";
  prompt += "  Current mood: "; prompt += userPattern.recentMood; prompt += "\n";
  prompt += "  Interests: "; prompt += (topicsStr.length() > 0 ? topicsStr : "not yet known"); prompt += "\n";
  prompt += "  Tech questions asked: "; prompt += String(userPattern.techQuestions); prompt += "\n";
  prompt += "  Reminder usage count: "; prompt += String(userPattern.reminderUsage); prompt += "\n\n";

  if (factSummary.length() > 0) {
    prompt += "## Known User Facts\n";
    prompt += factSummary;
    prompt += "\n";
  }

  return prompt;
}

String buildLiveContext() {
  int  h = hour(), m = minute();
  String ampm    = (h >= 12) ? "PM" : "AM";
  int   displayH = (h > 12) ? h - 12 : (h == 0 ? 12 : h);

  char timeBuf[12], dateBuf[12];
  sprintf(timeBuf, "%d:%02d %s", displayH, m, ampm.c_str());
  sprintf(dateBuf, "%02d/%02d/%04d", day(), month(), year());

  const char* dayNames[] = {"","Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  size_t intHeap   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t totalHeap = ESP.getFreeHeap();

  String ctx;
  ctx.reserve(256);
  ctx  = "\n## Live Context\n";
  ctx += "  Time: "; ctx += timeBuf; ctx += " ("; ctx += dayNames[weekday()]; ctx += " "; ctx += dateBuf; ctx += ")\n";
  ctx += "  Internal heap: "; ctx += String(intHeap); ctx += " B  Total: "; ctx += String(totalHeap); ctx += " B\n";

  if (!reminders.empty()) {
    ctx += "  Active reminders: "; ctx += String(reminders.size()); ctx += "\n";
    ctx += "  Next: \""; ctx += reminders[0].message;
    ctx += "\" at "; ctx += formatReminderTime(reminders[0].hour, reminders[0].minute); ctx += "\n";
  }
  return ctx;
}

// ═══════════════════════════════════════════════════════
// SECTION 10 ── GROQ API  (updated to use dcPost)
// ═══════════════════════════════════════════════════════

static void configureSecureClient(WiFiClientSecure& c) { c.setInsecure(); }

// ── groqSimpleCall ──────────────────────────────────────
// Lightweight single-turn call — sentiment, auto-learn, reminders, skill review.
// Now dispatches HTTP to Core 0 via dcPost.
String groqSimpleCall(const String& prompt, float temp, int maxTok) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";

  for (int attempt = 0; attempt < Config::AI_MAX_RETRIES; attempt++) {
    esp_task_wdt_reset();

    JsonDocument reqDoc;
    reqDoc["model"]       = Config::GROQ_MODEL;
    reqDoc["temperature"] = temp;
    reqDoc["max_tokens"]  = maxTok;
    JsonArray msgs    = reqDoc["messages"].to<JsonArray>();
    JsonObject uMsg   = msgs.add<JsonObject>();
    uMsg["role"]      = "user";
    uMsg["content"]   = prompt;

    String body; serializeJson(reqDoc, body);

    // [3] Dispatch to Core 0 (non-streaming)
    String raw = dcPost(Config::GROQ_ENDPOINT,
                        "Bearer " + customApiKey,
                        body,
                        Config::SENTIMENT_TIMEOUT_MS,
                        false);

    if (raw.length() > 0) {
      JsonDocument resp;
      // [1] Parse from PSRAM response buffer if already loaded, else inline
      if (!deserializeJson(resp, raw) && resp.containsKey("choices")) {
        String result = resp["choices"][0]["message"]["content"].as<String>();
        result.trim();
        return result;
      }
      return "";  // parse failed but HTTP succeeded — don't retry
    }

    if (attempt < Config::AI_MAX_RETRIES - 1) delay(600 * (attempt + 1));
  }
  return "";
}

// ── consumeSSE ──────────────────────────────────────────
// Called from Core 0's aiHttpTask during streaming responses.
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens) {
  WiFiClient* stream = http.getStreamPtr();
  String fullReply, line;
  unsigned long start = millis();
  bool done = false;

  JsonDocument chunk;  // allocated once, cleared per token

  while (!done && stream->connected() && (millis() - start) < timeoutMs) {
    while (!done && stream->available() && (millis() - start) < timeoutMs) {
      char c = (char)stream->read();
      if (c == '\n') {
        line.trim();
        if (line.startsWith("data: ")) {
          String payload = line.substring(6);
          if (payload == "[DONE]") { done = true; break; }
          chunk.clear();
          if (!deserializeJson(chunk, payload)) {
            const char* delta = chunk["choices"][0]["delta"]["content"] | "";
            if (*delta) {
              if (printTokens) Serial.print(delta);
              fullReply += delta;
            }
          }
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }
    if (!done) delay(1);  // no WDT reset here — we're on Core 0
  }
  fullReply.trim();
  return fullReply;
}

// ── groqStream ──────────────────────────────────────────
String groqStream(const String& userPrompt, const String& sysPrompt,
                  float temp, int maxTok) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";

  JsonDocument reqDoc;
  reqDoc["model"]       = Config::GROQ_MODEL;
  reqDoc["temperature"] = temp;
  reqDoc["max_tokens"]  = maxTok;
  reqDoc["stream"]      = true;

  JsonArray msgs = reqDoc["messages"].to<JsonArray>();
  if (sysPrompt.length() > 0) {
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"]    = "system";
    sys["content"] = sysPrompt;
  }
  JsonObject u = msgs.add<JsonObject>();
  u["role"]    = "user";
  u["content"] = userPrompt;

  String body; serializeJson(reqDoc, body);

  // [3] Dispatch streaming to Core 0
  return dcPost(Config::GROQ_ENDPOINT,
                "Bearer " + customApiKey,
                body,
                Config::HTTP_TIMEOUT_MS,
                true);
}

// ── sendToGroqStream ────────────────────────────────────
String sendToGroqStream(const String& userMessage, const String& extraContext) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) {
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis();
    return "❌ No internal SRAM / WiFi";
  }

  // Build the full request body on Core 1 (uses chatHistory safely)
  JsonDocument doc;
  doc["model"]       = Config::GROQ_MODEL;
  doc["temperature"] = customTemperature;
  doc["max_tokens"]  = customMaxTokens;
  doc["top_p"]       = 0.9;
  doc["stream"]      = true;

  JsonArray messages = doc["messages"].to<JsonArray>();

  JsonObject sys  = messages.add<JsonObject>();
  sys["role"]     = "system";
  sys["content"]  = buildSystemPrompt() + buildLiveContext() +
    (extraContext.length() > 0
      ? "\n## Live Web Search Results Provided\n"
        "The next user message includes fresh web search results fetched just "
        "now. That data IS your up-to-date information for this answer — treat "
        "it as current and correct. Answer directly and confidently using it. "
        "Do NOT say \"I don't have up-to-date information\", do NOT mention a "
        "training cutoff, and do NOT hedge — you now have the current answer, "
        "so just give it. If the results mix different time periods or "
        "publish dates (e.g. an old result from 2016 or 2022 alongside a "
        "newer one), always trust and report the MOST RECENT one relative to "
        "today's date, and ignore the older ones unless the user specifically "
        "asked about that earlier period.\n"
      : "");

  for (const ChatMessage& cm : chatHistory) {
    String role = String(cm.role);
    if (role == "system") continue;
    if (role == "model")  role = "assistant";
    JsonObject entry  = messages.add<JsonObject>();
    entry["role"]     = role;
    entry["content"]  = cm.content;
  }

  JsonObject uEntry = messages.add<JsonObject>();
  uEntry["role"]    = "user";
  uEntry["content"] = extraContext.length() > 0
    ? "[Live Web Search Results — current as of right now]\n" + extraContext +
      "\n\n[User Question]\n" + userMessage +
      "\n\nAnswer the question directly and confidently using the search "
      "results above. Do not disclaim uncertainty or mention your training "
      "data cutoff — the search results are current and are the answer."
    : userMessage;

  String body; serializeJson(doc, body);

  // [3] Hand serialized body to Core 0 for the blocking HTTP call
  String fullReply = dcPost(Config::GROQ_ENDPOINT,
                            "Bearer " + customApiKey,
                            body,
                            Config::HTTP_TIMEOUT_MS,
                            true);

  if (fullReply.length() == 0) {
    fullReply = "❌ Empty response from Groq";
    aiState   = AI_ERROR; blinkCount = 0; lastBlink = millis();
  } else {
    aiState         = AI_REPLIED;
    stateChangeTime = millis();
  }
  return fullReply;
}

// ── aiIsUncertain ───────────────────────────────────────
bool aiIsUncertain(const String& reply) {
  String r = reply; r.toLowerCase();
  static const char* phrases[] = {
    "i don't know","i do not know","i'm not sure","i am not sure",
    "i'm not certain","i am not certain","i don't have","i do not have",
    "i cannot find","i can't find","i cannot provide","i can't provide",
    "my knowledge","knowledge cutoff","training data","i'm unable to",
    "i am unable to","no information","no data on","not aware of",
    "couldn't find","could not find","beyond my knowledge",
    "as of my last update","as of my last training","up to date",
    "up-to-date","real-time information","real time information",
    "don't have access to real-time","do not have access to real-time",
    "i don't have the ability to browse","cannot browse the internet",
    "i recommend checking","i suggest checking","i'd recommend searching",
    "i would recommend searching","check a reliable source",
    "check the latest", "not able to confirm","unable to confirm",
    nullptr
  };
  for (int i = 0; phrases[i]; i++) if (r.indexOf(phrases[i]) >= 0) return true;
  return false;
}

bool isRecencyQuery(const String& lowerMsg) {
  static const char* cues[] = {
    "latest","current","recent","recently","now","today","tonight",
    "this year","this week","this month","who won","who's winning",
    "whos winning","score","result","results","update","updates",
    "new ","just happened","breaking","live","happening now",
    "as of", nullptr
  };
  for (int i = 0; cues[i]; i++)
    if (lowerMsg.indexOf(cues[i]) >= 0) return true;
  return false;
}

String buildSearchQuery(const String& message) {
  String q = message, lq = message;
  lq.toLowerCase();
  static const char* fillers[] = {
    "can you tell me","tell me about","what is","what are","who is",
    "who are","please","could you","i want to know","look up",
    "search for","find","google","?", nullptr
  };
  for (int i = 0; fillers[i]; i++) {
    int idx = lq.indexOf(fillers[i]);
    if (idx >= 0) { q.remove(idx, strlen(fillers[i])); lq.remove(idx, strlen(fillers[i])); }
  }
  q.trim();
  if (q.length() == 0) q = message;

  String lqTrim = q; lqTrim.toLowerCase();
  bool hasYear = false;
  for (int y = 2020; y <= 2030; y++) {
    if (lqTrim.indexOf(String(y)) >= 0) { hasYear = true; break; }
  }
  if (!hasYear && isRecencyQuery(lqTrim)) {
    static const char* monthNames[] = {"January","February","March","April","May","June",
                                        "July","August","September","October","November","December"};
    q += " ";
    q += monthNames[month() - 1];
    q += " ";
    q += String(year());
  }
  return q;
}

// ═══════════════════════════════════════════════════════
// SECTION 11 ── AUTO-LEARN
// ═══════════════════════════════════════════════════════
void autoLearnFromMessage(const String& userMsg) {
  if (!heapOk()) return;

  String prompt =
    "Analyze this message for a single important personal fact worth remembering long-term.\n"
    "Message: \"" + userMsg + "\"\n\n"
    "Rules:\n"
    "- Only extract proper personal info: name, age, job, city, hobby, preference, relationship.\n"
    "- Ignore questions, reminders, weather queries, or generic statements.\n"
    "- Key must be a short lowercase label (e.g. name, city, job).\n"
    "- Value must be specific (e.g. 'Colombo', not 'a city').\n\n"
    "If a clear fact exists: {\"learned\":true,\"key\":\"city\",\"value\":\"Colombo\"}\n"
    "Otherwise: {\"learned\":false}\n"
    "JSON only, no explanation, no markdown.";

  String raw = groqSimpleCall(prompt, 0.1f, 64);
  if (raw.length() == 0) return;

  if (raw.startsWith("```")) {
    int s = raw.indexOf('\n') + 1, e = raw.lastIndexOf("```");
    if (e > s) raw = raw.substring(s, e);
    raw.trim();
  }

  JsonDocument parsed;
  if (deserializeJson(parsed, raw) || !parsed["learned"].as<bool>()) return;

  String key = parsed["key"].as<String>();
  String val = parsed["value"].as<String>();
  if (key.length() > 1 && key.length() < 30 && val.length() > 0 && val.length() < 100) {
    rememberFact(key, val);
    Serial.println("💾 Learned: " + key + " = " + val);
  }
}

// ═══════════════════════════════════════════════════════
// SECTION 12 ── REMINDER SYSTEM
// ═══════════════════════════════════════════════════════

static String ordinalSuffix(int n) {
  int mod100 = n % 100;
  if (mod100 >= 11 && mod100 <= 13) return String(n) + "th";
  switch (n % 10) {
    case 1:  return String(n) + "st";
    case 2:  return String(n) + "nd";
    case 3:  return String(n) + "rd";
    default: return String(n) + "th";
  }
}

String formatReminderTime(int h, int m) {
  String ap = (h >= 12) ? "PM" : "AM";
  int dh    = (h > 12) ? h - 12 : (h == 0 ? 12 : h);
  char buf[12]; sprintf(buf, "%d:%02d %s", dh, m, ap.c_str());
  return String(buf);
}

String getRecurrenceText(RecurrenceType r, int dow, int dom) {
  static const char* days[] = {"","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  switch (r) {
    case ONCE:    return "(one-time)";
    case DAILY:   return "(daily)";
    case WEEKLY:  return String("(every ") + days[dow] + ")";
    case MONTHLY: return String("(monthly on the ") + ordinalSuffix(dom) + ")";
    default:      return "";
  }
}

bool shouldReminderTrigger(const Reminder& r) {
  if (r.hour != (uint8_t)hour() || r.minute != (uint8_t)minute()) return false;
  switch (r.recurrence) {
    case ONCE: case DAILY: return true;
    case WEEKLY:  return r.dayOfWeek  == (uint8_t)weekday();
    case MONTHLY: return r.dayOfMonth == (uint8_t)day();
    default:      return false;
  }
}

void processReminders() {
  for (auto& r : reminders) {
    if (shouldReminderTrigger(r) && !r.triggered) {
      Serial.println("\n⏰ ═══════════════════════════════════════");
      Serial.println("   REMINDER: " + r.message);
      Serial.println("   " + formatReminderTime(r.hour, r.minute) + " " +
                     getRecurrenceText(r.recurrence, r.dayOfWeek, r.dayOfMonth));
      Serial.println("   ════════════════════════════════════════\n");
      aiState         = AI_ALERT;
      pulseBrightness = Config::LED_MIN_BRIGHTNESS;
      pulseIncreasing = true;
      r.triggered     = true;
      r.triggerCount++;
      stateChangeTime = millis();
      saveReminders();
    } else if (r.hour != (uint8_t)hour() || r.minute != (uint8_t)minute()) {
      r.triggered = false;
    }
  }
  reminders.erase(std::remove_if(reminders.begin(), reminders.end(),
    [](const Reminder& r){ return r.recurrence == ONCE && r.triggered; }), reminders.end());
}

void addReminder(const String& msg, int h, int m, RecurrenceType recur, int dow, int dom) {
  if (reminders.size() >= Config::MAX_REMINDERS) {
    Serial.println("⚠️  Reminder limit reached (" + String(Config::MAX_REMINDERS) + "). Remove one first.");
    return;
  }
  Reminder r;
  r.message = msg; r.hour = h; r.minute = m;
  r.recurrence = recur; r.dayOfWeek = dow; r.dayOfMonth = dom;
  r.triggered = false; r.triggerCount = 0;
  reminders.push_back(r);
  saveReminders();
  userPattern.reminderUsage++;
}

void listReminders() {
  if (reminders.empty()) { Serial.println("⏰ No reminders set."); return; }
  Serial.println("\n⏰ Reminders (" + String(reminders.size()) + "):");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  for (size_t i = 0; i < reminders.size(); i++) {
    const Reminder& r = reminders[i];
    Serial.printf("[%d] %s  @ %s %s", (int)i, r.message.c_str(),
      formatReminderTime(r.hour, r.minute).c_str(),
      getRecurrenceText(r.recurrence, r.dayOfWeek, r.dayOfMonth).c_str());
    if (r.triggerCount > 0) Serial.print(" [fired " + String(r.triggerCount) + "x]");
    Serial.println();
  }
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("💡 /remove [number] to delete");
}

void removeReminder(int idx) {
  if (idx < 0 || idx >= (int)reminders.size()) return;
  reminders.erase(reminders.begin() + idx);
  saveReminders();
}

bool tryParseNaturalReminder(const String& message) {
  String lMsg = message; lMsg.toLowerCase();
  if (lMsg.indexOf("remind") < 0 && lMsg.indexOf("alarm") < 0 &&
      lMsg.indexOf("alert") < 0 && lMsg.indexOf("don't forget") < 0 &&
      lMsg.indexOf("notify") < 0) return false;

  char nowBuf[40];
  sprintf(nowBuf, "%02d:%02d Day:%d Date:%d", hour(), minute(), weekday(), day());

  String extractPrompt =
    "Now: " + String(nowBuf) + " (1=Sunday..7=Saturday)\n"
    "User: \"" + message + "\"\n\n"
    "Extract reminder. Support: 3PM, 2:30pm, 15:00, daily, weekly+day, monthly+date.\n"
    "Existing reminders count: " + String(reminders.size()) + "\n\n"
    "Output JSON only (no markdown, no explanation):\n"
    "{\"isReminder\":true,\"message\":\"short action\",\"hour\":15,\"minute\":0,"
    "\"recurrence\":\"once|daily|weekly|monthly\",\"dayOfWeek\":0,\"dayOfMonth\":0}\n"
    "or {\"isReminder\":false}\n"
    "hour is 24-format (0-23). dayOfWeek 1=Sunday. dayOfMonth 1-31.";

  String raw = groqSimpleCall(extractPrompt, 0.05f, 128);
  if (raw.length() == 0) return false;

  if (raw.startsWith("```")) {
    int s = raw.indexOf('\n') + 1, e = raw.lastIndexOf("```");
    if (e > s) raw = raw.substring(s, e);
    raw.trim();
  }

  JsonDocument parsed;
  if (deserializeJson(parsed, raw) || !parsed["isReminder"].as<bool>()) return false;

  String msg      = parsed["message"].as<String>();
  int    h        = parsed["hour"].as<int>();
  int    m        = parsed["minute"].as<int>();
  String recurStr = parsed["recurrence"] | "once";
  int    dow      = parsed["dayOfWeek"]  | 0;
  int    dom      = parsed["dayOfMonth"] | 0;

  if (msg.length() == 0 || h < 0 || h > 23 || m < 0 || m > 59) {
    Serial.println("⚠️  Could not parse reminder details. Please be more specific.");
    return false;
  }

  RecurrenceType recur = ONCE;
  if (recurStr == "daily")        recur = DAILY;
  else if (recurStr == "weekly")  { recur = WEEKLY;  if (!dow) dow = weekday(); }
  else if (recurStr == "monthly") { recur = MONTHLY; if (!dom) dom = day(); }

  addReminder(msg, h, m, recur, dow, dom);
  Serial.println("✅ Reminder set!");
  Serial.println("   📌 " + msg);
  Serial.println("   🕐 " + formatReminderTime(h, m) + " " + getRecurrenceText(recur, dow, dom));
  return true;
}

void saveReminders() {
  JsonDocument doc;
  JsonArray arr = doc["reminders"].to<JsonArray>();
  for (const auto& r : reminders) {
    JsonObject o = arr.add<JsonObject>();
    o["message"] = r.message; o["hour"] = r.hour; o["minute"] = r.minute;
    o["recurrence"] = (int)r.recurrence;
    o["dayOfWeek"] = r.dayOfWeek; o["dayOfMonth"] = r.dayOfMonth;
    o["triggerCount"] = r.triggerCount;
  }
  File f = FFat.open("/reminders.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadReminders() {
  if (!FFat.exists("/reminders.json")) return;
  File f = FFat.open("/reminders.json", FILE_READ);
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return; }
  reminders.clear();
  for (JsonObject o : doc["reminders"].as<JsonArray>()) {
    Reminder r;
    r.message    = o["message"].as<String>();
    r.hour       = o["hour"] | 0; r.minute    = o["minute"] | 0;
    r.recurrence = (RecurrenceType)(o["recurrence"] | 0);
    r.dayOfWeek  = o["dayOfWeek"] | 0; r.dayOfMonth = o["dayOfMonth"] | 0;
    r.triggerCount = o["triggerCount"] | 0;
    r.triggered = false;
    reminders.push_back(r);
  }
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 12.5 ── NATURAL-LANGUAGE INTENT + ENTITY PARSER
// ═══════════════════════════════════════════════════════

namespace NL {

static bool contains(const String& h, const char* n) { return h.indexOf(n) >= 0; }

static bool containsWord(const String& h, const char* word) {
  int wlen = (int)strlen(word);
  int idx  = h.indexOf(word);
  while (idx >= 0) {
    bool startOk = (idx == 0 || !isalnum((unsigned char)h[idx - 1]));
    int  endPos  = idx + wlen;
    bool endOk   = (endPos >= (int)h.length() || !isalnum((unsigned char)h[endPos]));
    if (startOk && endOk) return true;
    idx = h.indexOf(word, idx + 1);
  }
  return false;
}

static bool containsAny(const String& h, const char* const* needles) {
  for (int i = 0; needles[i]; i++) if (h.indexOf(needles[i]) >= 0) return true;
  return false;
}

static bool startsWithAny(const String& h, const char* const* prefixes, int* matchedLen = nullptr) {
  for (int i = 0; prefixes[i]; i++) {
    if (h.startsWith(prefixes[i])) {
      if (matchedLen) *matchedLen = strlen(prefixes[i]);
      return true;
    }
  }
  return false;
}

static String stripFillers(const String& sIn) {
  static const char* fillers[] = {
    "hey there, ","hey there ","hey, ","hey ",
    "hi there, ","hi there ","hi, ","hi ",
    "hello, ","hello ",
    "yo, ","yo ",
    "ok so, ","ok so ","ok, ","ok ",
    "okay so, ","okay so ","okay, ","okay ",
    "alright so, ","alright, ","alright ",
    "right so, ","right, ",
    "well then, ","well, ","well ",
    "so then, ","so, ","so ",
    "anyway, ","anyway ",
    "anyhow, ","anyhow ",
    "uh, ","uh ","um, ","um ","uhh, ","uhh ","umm, ","umm ",
    "hmm, ","hmm ","hm, ","hm ",
    "like, ","like ",
    "you know, ","you know ",
    "i mean, ","i mean ",
    "by the way, ","by the way ","btw, ","btw ",
    "just so you know, ","just so you know ",
    "fyi, ",
    "quick question, ","quick question ",
    "just a quick question, ",
    "just wondering, ","just wondering ",
    "i was wondering, ","i was wondering ",
    "i was just wondering, ",
    "i'm curious, ","im curious, ",
    "please, ","please ","pls, ","pls ",
    "kindly, ","kindly ",
    "can you please go ahead and ","can you please ","can you go ahead and ",
    "could you please go ahead and ","could you please ","could you go ahead and ",
    "would you please go ahead and ","would you please ","would you go ahead and ",
    "will you please ","will you ",
    "might you ","may you ",
    "can you ","could you ","would you ",
    "i would really like you to ","i would like you to ",
    "i'd really like you to ","i'd like you to ","id like you to ",
    "i really want you to ","i want you to ",
    "i need you to please ","i need you to ",
    "i'd love for you to ","i'd love you to ",
    "i'd appreciate it if you ","i'd appreciate if you ",
    "i'd appreciate you ",
    "i am asking you to ","i'm asking you to ","im asking you to ",
    "would you mind please ","would you mind ",
    "do you mind ","do you mind if you ",
    "would you be so kind as to ","would you be kind enough to ",
    "if it's not too much trouble, ","if it's not too much trouble ",
    "if you don't mind, ","if you don't mind ",
    "if you can, ","if you can ",
    "if you could, ","if you could ",
    "if possible, ","if possible ",
    "when you get a chance, ","when you get a chance ",
    "are you able to ","are you capable of ",
    "quickly, ","quickly ","quick, ","quick ",
    "just ","simply ","merely ",
    "actually, ","actually ",
    "basically, ","basically ",
    "essentially, ","essentially ",
    nullptr
  };
  String r = sIn;
  String lower = r; lower.toLowerCase();
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; fillers[i]; i++) {
      int idx = lower.indexOf(fillers[i]);
      while (idx >= 0 && (idx == 0 || lower[idx-1]==',' || lower[idx-1]=='.' ||
                          lower[idx-1]=='!' || lower[idx-1]=='?' || lower[idx-1]==' ')) {
        r.remove(idx, strlen(fillers[i]));
        lower.remove(idx, strlen(fillers[i]));
        changed = true;
        idx = lower.indexOf(fillers[i]);
      }
    }
  }
  r.trim();
  while (r.length() && (r[r.length()-1]=='?' || r[r.length()-1]=='.'))
    r.remove(r.length()-1,1);
  r.trim();
  return r;
}

static String stripPrefixes(const String& s, const char* const* prefixes) {
  String lower = s; lower.toLowerCase();
  for (int i = 0; prefixes[i]; i++) {
    if (lower.startsWith(prefixes[i])) {
      String r = s.substring(strlen(prefixes[i]));
      r.trim();
      return r;
    }
  }
  return s;
}

static const char* DAY_NAMES[] = {
  "sunday","monday","tuesday","wednesday","thursday","friday","saturday", nullptr
};

static int dayNameToIndex(const String& lower) {
  for (int i = 0; DAY_NAMES[i]; i++) if (lower.indexOf(DAY_NAMES[i]) >= 0) return i + 1;
  return 0;
}

} // namespace NL

// ── parseTime ──────────────────────────────────────────
bool parseTime(const String& sIn, ParsedTime& out) {
  out.found = false; out.hour = -1; out.minute = 0;
  out.isRelative = false; out.relativeMinutes = 0;
  out.isTomorrow = false; out.recurrence = ONCE; out.dayOfWeek = 0;

  String s = sIn; s.toLowerCase();

  if (NL::contains(s,"every day")||NL::contains(s," daily")||s.startsWith("daily")) out.recurrence=DAILY;
  else if (NL::contains(s,"every week")||NL::contains(s," weekly")||s.startsWith("weekly")) out.recurrence=WEEKLY;
  else if (NL::contains(s," monthly")||s.startsWith("monthly")||NL::contains(s,"every month")) out.recurrence=MONTHLY;

  int dow = NL::dayNameToIndex(s);
  if (dow > 0) {
    out.dayOfWeek = dow;
    if (NL::contains(s,"every ")||NL::contains(s,"each ")) out.recurrence=WEEKLY;
    else if (NL::contains(s,"next ")||NL::contains(s,"on "))
      out.recurrence=(out.recurrence==ONCE)?ONCE:out.recurrence;
  }

  if (NL::contains(s,"tomorrow")) out.isTomorrow=true;

  int from=0;
  while (from<(int)s.length()) {
    int p=s.indexOf("in ",from); if(p<0) break;
    bool atStart=(p==0), wordStart=(p>0&&s[p-1]==' ');
    if (atStart||wordStart) {
      int ns=p+3,ne=ns;
      while(ne<(int)s.length()&&isdigit((unsigned char)s[ne]))ne++;
      if(ne>ns){
        int n=s.substring(ns,ne).toInt(); int u=ne;
        while(u<(int)s.length()&&s[u]==' ')u++;
        if(u<(int)s.length()){
          String unit=s.substring(u,min((int)s.length(),u+8));
          if(unit.startsWith("min")){out.isRelative=true;out.relativeMinutes=n;out.found=true;break;}
          else if(unit.startsWith("hour")||unit.startsWith("hr")){out.isRelative=true;out.relativeMinutes=n*60;out.found=true;break;}
          else if(unit.startsWith("sec")){out.isRelative=true;out.relativeMinutes=max(1,n/60);out.found=true;break;}
          else if(unit.startsWith("day")){out.isRelative=true;out.relativeMinutes=n*1440;out.found=true;break;}
        }
      }
    }
    from=p+3;
  }
  if(out.isRelative) return true;

  for(int i=0;i<(int)s.length();i++){
    if(!isdigit((unsigned char)s[i])) continue;
    int j=i;
    while(j<(int)s.length()&&isdigit((unsigned char)s[j]))j++;
    int h=s.substring(i,j).toInt();
    int m=0,k=j; bool hasColon=false;
    if(k<(int)s.length()&&s[k]==':'){
      int ms=k+1,me=ms;
      while(me<(int)s.length()&&isdigit((unsigned char)s[me]))me++;
      if(me>ms){m=s.substring(ms,me).toInt();k=me;hasColon=true;}
    }
    while(k<(int)s.length()&&s[k]==' ')k++;
    bool isPM=false,isAM=false;
    if(k+1<(int)s.length()){
      if((s[k]=='p'&&s[k+1]=='m')||(s[k]=='a'&&s[k+1]=='m')){isPM=(s[k]=='p');isAM=(s[k]=='a');k+=2;}
      else if(s[k]=='p'&&k+3<(int)s.length()&&s[k+1]=='.'&&s[k+2]=='m'&&s[k+3]=='.'){isPM=true;k+=4;}
      else if(s[k]=='a'&&k+3<(int)s.length()&&s[k+1]=='.'&&s[k+2]=='m'&&s[k+3]=='.'){isAM=true;k+=4;}
    }
    bool hasAt=false;
    if(i>=3){String pre=s.substring(max(0,i-4),i);if(pre.indexOf("at ")>=0)hasAt=true;}
    if(h>=0&&h<=23&&m>=0&&m<60&&(hasAt||hasColon||isPM||isAM)){
      if(isPM&&h<12)h+=12; if(isAM&&h==12)h=0;
      if(!isPM&&!isAM&&!hasColon&&h>=1&&h<=12){
        int nowH=hour(),amH=(h==12)?0:h,pmH=(h==12)?12:h+12;
        if(amH>nowH)h=amH; else if(pmH>nowH)h=pmH; else{h=amH;out.isTomorrow=true;}
      }
      out.hour=h; out.minute=m; out.found=true; i=k; break;
    }
    i=k;
  }
  if(!out.found&&(out.recurrence!=ONCE||out.dayOfWeek!=0||out.isTomorrow)) out.found=true;
  return out.found;
}

static String stripTimeExpr(const String& s) {
  String lower=s; lower.toLowerCase();
  static const char* connectors[]={"  at "," in "," on "," every "," each "," next ",
                                    " tomorrow"," daily"," weekly"," monthly",nullptr};
  int chopAt=-1;
  for(int i=0;connectors[i];i++){
    int p=lower.indexOf(connectors[i]);
    while(p>=0){
      String tail=lower.substring(p);
      ParsedTime tt; bool timely=parseTime(tail,tt),dayLike=false;
      for(int d=0;NL::DAY_NAMES[d];d++) if(tail.indexOf(NL::DAY_NAMES[d])>=0&&tail.indexOf(NL::DAY_NAMES[d])<12){dayLike=true;break;}
      if(timely||dayLike||strstr(connectors[i],"tomorrow")||strstr(connectors[i],"daily")||
         strstr(connectors[i],"weekly")||strstr(connectors[i],"monthly")){
        if(chopAt<0||p<chopAt)chopAt=p; break;
      }
      p=lower.indexOf(connectors[i],p+1);
    }
  }
  String r=(chopAt>0)?s.substring(0,chopAt):s; r.trim();
  while(r.length()&&(r[r.length()-1]==','||r[r.length()-1]=='.')) r.remove(r.length()-1,1);
  return r;
}

// ── detectIntent ───────────────────────────────────────
Intent detectIntent(const String& sIn) {
  String s=sIn; s.toLowerCase();

  static const char* corrCues[]={"actually","never mind","nevermind","scratch that","no wait",
    "wait no","forget that","cancel that","no, make it","no make it","change that to",
    "i meant","that's not what i meant","thats not what i meant","that's wrong","thats wrong",
    "no no,","no no ","let me rephrase","let me correct","disregard that","ignore that",
    "that was wrong","i made a mistake","correction:","whoops","oops","i misspoke",
    "hold on,","hold on ","strike that","start over","i didn't mean","i did not mean","my bad",
    "not what i said","that's not right","thats not right","no that's","no thats","wait, i","wait i ",
    "let's redo that","lets redo that","try again","that's incorrect","thats incorrect",
    "you got it wrong","you misunderstood","not quite","close but no","undo that","reverse that",
    "take that back","erase what i said","delete what i just said","one sec, i meant",
    "hang on i meant","sorry i meant","sorry, i meant","my mistake","typo, i meant",nullptr};
  if(NL::containsAny(s,corrCues)) return INTENT_CORRECTION;

  static const char* remCues[]={"remind","alarm","alert me","wake me","ping me","notify me",
    "don't let me forget","dont let me forget","make sure i","set a timer","set an alarm","buzz me",
    "set a reminder","create a reminder","add a reminder","schedule a reminder","i need a reminder",
    "give me a reminder","i need to be reminded","i want to be reminded","help me remember",
    "help me not forget","don't forget to remind me","dont forget to remind me","can you remind",
    "could you remind","please remind","will you remind","would you remind","can you set an alarm",
    "could you set an alarm","can you set a timer","could you set a timer","please alert me",
    "can you alert me","could you alert me","please wake me","can you wake me","could you wake me",
    "i'd like a reminder","id like a reminder","schedule an alarm","schedule a timer","add an alarm",
    "create an alarm","can you ping me","could you ping me","i need an alert","give me an alert",
    "poke me","buzz me at","beep me","sound an alarm","set me a reminder","set me an alarm",
    "flag this for later","nudge me","give me a nudge","i shouldn't forget","i should not forget",
    "make sure i remember to","make sure i don't forget to","make sure i dont forget to",
    "don't let me miss","dont let me miss","i need a heads up","give me a heads up","warn me",
    "can you warn me","text me later","message me later","let me know later","let me know at",
    "tell me later","tell me at",nullptr};
  if(NL::containsAny(s,remCues)){
    static const char* listCues[]={"list","show me my","show my","what are my","any reminder",
      "my reminders","all reminders","do i have any reminders","display my reminders",
      "what have i set","show all reminders","what reminders","list my reminders",nullptr};
    static const char* cancelCues[]={"cancel","delete","remove","clear","drop","turn off",
      "disable","stop that reminder","kill that reminder","dismiss","get rid of",nullptr};
    if(NL::containsAny(s,listCues))   return INTENT_REMINDER_LIST;
    if(NL::containsAny(s,cancelCues)) return INTENT_REMINDER_CANCEL;
    return INTENT_REMINDER_SET;
  }

  static const char* recallCues[]={"what did i say","what was my","do you remember","what's my",
    "whats my","what is my","tell me my","remind me what","did i tell you","what did i tell you",
    "can you recall","do you recall","what have i told you","what did i share","have i mentioned",
    "what's saved about","whats saved about","what do you know about my","recall my",
    "fetch my stored","retrieve my","show me my stored","can you show me what i told you",
    "do you have my","what information do you have about my","tell me what i said",
    "have you saved my","look up what i said","look up my","pull up what i said","bring up my",
    "what's stored about","whats stored about","what did you learn about me",
    "what have you learned about me","check what i said","check your memory of me",
    "what's on file for me","whats on file for me","did you save my","do you know my",
    "what did i previously say","what did i say earlier","remind me of what i said",nullptr};
  if(NL::containsAny(s,recallCues)) return INTENT_MEMORY_RECALL;

  static const char* noteAdd[]={"add a note","make a note","take a note","note this","note down",
    "write down","jot down","log this","save a note","new note","create a note","put a note",
    "keep a note","add this to my notes","put this in my notes","keep note of","record this",
    "save this as a note","note the following","write this down for me","can you note",
    "could you note","please note this","i want to note","i'd like to note","i need to note",
    "can you write this down","please write this down","can you keep a note","quickly note",
    "jot this down for me","scribble this down","add this note","save this note","stash this note",
    "file this note","make a quick note","note for me","can you jot down","please jot down",
    "note it down",nullptr};
  if(NL::containsAny(s,noteAdd)) return INTENT_NOTE_ADD;

  static const char* noteRecall[]={"my notes","show notes","show me my notes","what notes",
    "list notes","read my notes","what are my notes","can you show my notes","display my notes",
    "do i have any notes","what have i noted","read back my notes","show all notes","list all notes",
    "retrieve my notes","all my notes","pull up my notes","what did i jot down",
    "what did i write down","check my notes","read my note","open my notes","bring up my notes",
    "what's in my notes","whats in my notes",nullptr};
  if(NL::containsAny(s,noteRecall)) return INTENT_NOTE_RECALL;

  static const char* taskAdd[]={"add a task","new task","todo:","to-do:","to do:","add to my todo",
    "add to my to-do","task list","create a task","make a task","put on my list","add to my list",
    "add to my tasks","can you add a task","please add a task","i want to add a task",
    "put this on my task list","task:","add this to my to-do list","i have a task","schedule a task",
    "i need to do ","i've got to do ","i have to do ","i must do ","add it to my tasks",
    "put it on my to-do list","put it on my task list","queue up a task","add this as a task",
    "log a task","log this task","i've got a task","ive got a task","i need a task added",
    "could you add a task","please add this task","add another task","add one more task",
    "throw this on my list","stick this on my list","put this on my todo",nullptr};
  if(NL::containsAny(s,taskAdd)) return INTENT_TASK_ADD;

  static const char* saveCues[]={"remember that","remember my","remember this","keep in mind",
    "save this","memorize","store this","store that","for future reference","fyi my","fyi:",
    "can you remember","please remember","i want you to remember","i'd like you to remember",
    "id like you to remember","make a mental note","save this information","hold onto this",
    "keep this in mind","don't forget this","dont forget this","please save","i need you to save",
    "store the fact that","file this away","note for later","save for later","can you store",
    "please memorize","keep a record of","log this fact","my name is","my age is","my birthday is",
    "my favorite ","my favourite ","i was born","i live in","i work at","i work as","i am from",
    "i'm from","im from","i study at","i go to","my job is","my hobby is","my hobbies are",
    "my email is","my phone number is","my number is","my pet is","my pet's name is",
    "my anniversary is","my address is","i drive a","i own a","i have a","i'm allergic to",
    "im allergic to","i am allergic to","please keep this in mind","add this to memory",
    "put this in memory","save that fact","store that info","remember for me",nullptr};
  if(NL::containsAny(s,saveCues)||s.startsWith("remember ")||s.startsWith("note that "))
    return INTENT_MEMORY_SAVE;

  static const char* forgetCues[]={"forget","erase","wipe","delete","remove that memory",
    "clear that memory","discard","purge","get rid of that fact","stop remembering",
    "you can forget","please forget","can you forget","i don't want you to remember",
    "i dont want you to remember","remove that from memory","clear that from your memory",
    "unlearn that","delete that fact","wipe that fact","clear that fact","clear my memory of",
    "erase my memory of","please erase","please wipe","please delete that","take that out of memory",
    "remove it from memory","you don't need to remember that","you dont need to remember that",
    "scratch that from memory",nullptr};
  if(NL::containsAny(s,forgetCues)){
    if(NL::contains(s,"note")||NL::contains(s,"fact")||NL::contains(s,"memory")||
       NL::containsWord(s,"that")||NL::contains(s,"last")||NL::contains(s,"everything")||
       NL::containsWord(s,"all")||NL::contains(s,"saved")||NL::contains(s,"stored")||
       NL::containsWord(s,"info")) return INTENT_MEMORY_FORGET;
  }

  static const char* searchCues[]={"search for ","search about ","search on ","search the web",
    "search the internet","search online","web search","internet search","online search","look up ",
    "look it up","look online","look it up online","find info","find information","find out ",
    "find something about","find me info","find me information","find on the internet","find online",
    "google ","bing ","search google","google that","google for me","research ","do some research",
    "do a search","run a search","do a web search","run a web search","check online","check the web",
    "check the internet","check it online","look for information","look for info",
    "what does the internet say","what does google say","what does the web say","can you search",
    "can you look up","can you find","can you google","can you look online","can you check online",
    "can you research","could you search","could you look up","could you find","could you google",
    "could you look online","could you check online","could you research","please search",
    "please look up","please find","please research","please google","search that up","look that up",
    "would you search","would you look up","would you find","would you google","would you research",
    "will you search","will you look up","will you find","will you google","will you research",
    "i want you to search","i'd like you to search","id like you to search","i need you to search",
    "go look up","go search","go find out","pull up info on","pull up information on",
    "get me info on","get me information on","fetch info on","fetch information on",
    "dig up info on","dig up information on","hunt down info on","search up",nullptr};
  if(NL::containsAny(s,searchCues)) return INTENT_SEARCH;

  static const char* sumCues[]={"summarize","summary","tldr","tl;dr","tl dr","recap","brief me",
    "give me a summary","give me a recap","what's the gist","whats the gist","what is the gist",
    "the short version","in a nutshell","sum it up","sum up","wrap it up","brief overview",
    "quick overview","quick summary","short summary","condensed version","condense that",
    "simplify that","can you summarize","could you summarize","please summarize",
    "give me the highlights","main points","key points","boil it down","shorten that",
    "make that shorter","give me the short version","just the highlights","just the basics",
    "the essentials","cliff notes","cliffnotes","bottom line","long story short","in short",
    "abridged version","give me the abridged version","could you condense","can you condense",
    "please condense","distill that","please distill","can you distill",nullptr};
  if(NL::containsAny(s,sumCues)) return INTENT_SUMMARY;

  static const char* statusCues[]={"system status","diagnostic","diag","status","how are you doing",
    "how's your","hows your","free heap","memory usage","cpu temp","uptime","battery","health check",
    "how are you","are you ok","are you okay","how are you running","system check","are you working",
    "are you functioning","what's your status","whats your status","what is your status",
    "report status","self test","hardware status","performance report","are you healthy",
    "how's your health","hows your health","how much memory","how much ram","how much cpu",
    "check yourself","run diagnostics","run a diagnostic","run a self test","run self test",
    "how's your heap","hows your heap","check your heap","how long have you been running",
    "what's your uptime","whats your uptime","how's your temperature","hows your temperature",
    "what's your temperature","whats your temperature","are you overheating","how's your cpu",
    "hows your cpu","everything working ok","everything working okay","are you alright",
    "are you all right","is everything fine","give me a diagnostic report","give me a status report",
    "how are things","system health","check system health",nullptr};
  if(NL::containsAny(s,statusCues)) return INTENT_SYSTEM_STATUS;

  static const char* weatherCues[]={"weather","forecast","temperature outside","outside temperature",
    "outdoor temperature","current temperature","is it raining","is it sunny","is it cloudy",
    "is it windy","is it snowing","is it hot","is it cold","is it warm","is it cool","how hot",
    "how cold","how warm","how cool","will it rain","will it snow","will it be hot","will it be cold",
    "will it be warm","will it be sunny","will it be cloudy","what's the weather","whats the weather",
    "what is the weather","what's the temperature","whats the temperature","what is the temperature",
    "how's the weather","hows the weather","how is the weather","what's it like outside",
    "whats it like outside","what's it like in","how is it outside","how's it outside",
    "what's the forecast","whats the forecast","what is the forecast","weather report",
    "weather update","current weather","check the weather","get the weather","tell me the weather",
    "should i bring an umbrella","do i need an umbrella","will there be rain","chance of rain",
    "any rain","humidity outside","precipitation","should i wear a jacket","do i need a jacket",
    "should i wear a coat","is it freezing","is it humid","is it muggy","is it foggy","is it stormy",
    "is there a storm","will it storm","will it thunder","is it going to rain","is it going to snow",
    "what's the uv index","whats the uv index","wind speed","how windy is it","is it breezy",
    "conditions outside","current conditions","sky conditions","can you check the weather",
    "could you check the weather","please check the weather","give me the weather","weather in",
    "climate in","temp outside","temp today",nullptr};
  if(NL::containsAny(s,weatherCues)) return INTENT_WEATHER;

  ParsedTime tt;
  if(parseTime(s,tt)&&tt.found&&s.length()<40) return INTENT_FOLLOWUP_TIME;

  return INTENT_NONE;
}

// ── extractEntities ────────────────────────────────────
void extractEntities(const String& sIn, Intent intent, ParsedEntities& out) {
  out.content=""; out.reference=""; out.referenceIndex=-1;
  parseTime(sIn,out.time);
  String s=NL::stripFillers(sIn); String lower=s; lower.toLowerCase();

  if(NL::contains(lower," last ")||lower.endsWith(" last")||lower.startsWith("last ")){out.reference="last";out.referenceIndex=-1;}
  else if(NL::contains(lower,"first")) {out.reference="first";out.referenceIndex=0;}
  else if(NL::contains(lower,"second")){out.reference="second";out.referenceIndex=1;}
  else if(NL::contains(lower,"third")) {out.reference="third";out.referenceIndex=2;}
  else if(NL::contains(lower,"fourth")){out.reference="fourth";out.referenceIndex=3;}
  else if(NL::contains(lower,"fifth")) {out.reference="fifth";out.referenceIndex=4;}
  else if(NL::contains(lower,"that ")) {out.reference="that";}

  String content=s;
  switch(intent){
    case INTENT_REMINDER_SET:{
      static const char* p[]={"don't let me forget to ","don't let me forget ","dont let me forget to ",
        "dont let me forget ","set a reminder to ","set a reminder for ","set a reminder ",
        "create a reminder to ","create a reminder for ","create a reminder ",
        "add a reminder to ","add a reminder for ","add a reminder ",
        "schedule a reminder to ","schedule a reminder for ","schedule a reminder ",
        "give me a reminder to ","give me a reminder about ","give me a reminder ",
        "set an alarm to ","set an alarm for ","set an alarm ","create an alarm to ",
        "create an alarm for ","create an alarm ","add an alarm to ","add an alarm for ","add an alarm ",
        "schedule an alarm to ","schedule an alarm for ","schedule an alarm ",
        "set a timer to ","set a timer for ","set a timer ",
        "remind me to ","remind me that ","remind me about ","remind me when ","remind me ",
        "alert me to ","alert me when ","alert me about ","alert me ",
        "notify me to ","notify me when ","notify me about ","notify me ",
        "wake me up to ","wake me up at ","wake me up ","wake me at ","wake me ",
        "ping me to ","ping me when ","ping me at ","ping me ",
        "buzz me when ","buzz me to ","buzz me at ","buzz me ",
        "make sure i ","make sure to ","help me remember to ","help me remember ",
        "help me not forget to ","help me not forget ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_MEMORY_SAVE:{
      static const char* p[]={"my favorite is ","my favourite is ","my favorite ","my favourite ",
        "remember that my ","remember that ","remember my ","remember this: ","remember this ",
        "remember ","keep in mind that ","keep in mind ","note that ","save this: ","save this ",
        "memorize that ","memorize this: ","memorize ","store the fact that ","store this: ",
        "store this ","store that ","for future reference: ","for future reference, ","for future reference ",
        "fyi my ","fyi: ","fyi ","make a mental note that ","make a mental note: ","make a mental note ",
        "save this information: ","save this information ","hold onto this: ","hold onto this ",
        "keep this in mind: ","keep this in mind ","file this away: ","file this away ",
        "note for later: ","save for later: ","log this fact: ","log this fact ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_MEMORY_RECALL:{
      static const char* p[]={"what did i say about ","what did i say ","what did i tell you about ",
        "what did i tell you ","what did i share about ","what did i share ","what was my ",
        "what's my ","whats my ","what is my ","do you remember my ","do you remember what my ",
        "do you remember ","do you recall my ","do you recall ","can you recall my ","can you recall ",
        "tell me my ","tell me about my ","tell me what i said about ","remind me what my ",
        "remind me what i said about ","remind me what ","did i tell you my ","did i tell you about ",
        "did i tell you ","have i told you my ","have i told you about ","have i told you ",
        "have i mentioned my ","have i mentioned ","what's saved about ","whats saved about ",
        "what is saved about ","what do you know about my ","what do you have saved about ",
        "retrieve my ","fetch my ","show me my stored ","look up what i said about ","look up my ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_MEMORY_FORGET:{
      static const char* p[]={"forget that ","forget the ","forget about ","forget my ","forget ",
        "erase that ","erase the ","erase my ","erase ","wipe that ","wipe the ","wipe my ","wipe ",
        "delete that ","delete the ","delete my ","delete ","remove that memory about ",
        "remove that memory of ","remove that memory ","remove that fact about ","remove that fact ",
        "clear that memory about ","clear that memory of ","clear that memory ","discard that ",
        "purge that ","get rid of that fact about ","stop remembering ","please forget about ",
        "please forget ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_NOTE_ADD:{
      static const char* p[]={"add a note that ","add a note: ","add a note ","make a note that ",
        "make a note: ","make a note ","take a note that ","take a note: ","take a note ",
        "create a note that ","create a note: ","create a note ","put a note that ","put a note: ",
        "put a note ","save a note that ","save a note: ","save a note ","keep a note that ",
        "keep a note: ","keep a note ","new note: ","new note ","note this: ","note this ",
        "note down that ","note down: ","note down ","write down that ","write down: ","write down ",
        "write this down: ","write this down ","jot down that ","jot down: ","jot down ",
        "jot this down: ","jot this down ","log this: ","log this ","record this: ","record this ",
        "add this to my notes: ","add this to my notes ","put this in my notes: ",
        "put this in my notes ","keep note of: ","keep note of ","note the following: ",
        "note the following ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_TASK_ADD:{
      static const char* p[]={"add a task to my list: ","add a task to my list ","add a task: ",
        "add a task ","create a task: ","create a task ","make a task: ","make a task ",
        "new task: ","new task ","todo: ","to-do: ","to do: ","add to my todo list: ",
        "add to my todo list ","add to my todo: ","add to my todo ","add to my to-do list: ",
        "add to my to-do list ","add to my to-do: ","add to my to-do ","add to my task list: ",
        "add to my task list ","add to my tasks: ","add to my tasks ","put on my list: ",
        "put on my list ","put this on my task list: ","put this on my task list ",
        "schedule a task: ","schedule a task ","task: ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_SEARCH:{
      static const char* p[]={"search the web for ","search the internet for ","search online for ",
        "search the web about ","search the internet about ","search online about ",
        "do a web search for ","do a web search on ","do a web search about ",
        "run a web search for ","run a web search on ","do a search for ","do a search on ",
        "do a search about ","run a search for ","run a search on ","do an internet search for ",
        "do an internet search on ","look it up online: ","look it up online ","look online for ",
        "look online about ","check online for ","check online about ","check online ",
        "check the web for ","check the web about ","check the web ","check the internet for ",
        "check the internet about ","find information about ","find information on ",
        "find info about ","find info on ","find me information about ","find me information on ",
        "find me info about ","find me info on ","find out about ","find out who ","find out what ",
        "find out where ","find out when ","find out how ","find out if ","find out ",
        "find something about ","look up information about ","look up info about ","look up ",
        "look that up: ","look that up ","look it up: ","look it up ",
        "search for information about ","search for info about ","search for ","search about ",
        "search on ","search what ","search who ","search where ","search when ","search how ",
        "search why ","search if ","search that up: ","search that up ","search ",
        "google for information on ","google for ","google that: ","google that ","google ",
        "bing for ","bing ","research the topic of ","research about ","research on ","research ",
        "web search for ","web search ","internet search for ","internet search ",
        "online search for ","online search ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    case INTENT_WEATHER:{
      String lc=content; lc.toLowerCase(); int loc=-1;
      static const char* markers[]={" in "," for "," at "," near "," around ",nullptr};
      for(int i=0;markers[i];i++){int p=lc.indexOf(markers[i]);if(p>=0){loc=p+(int)strlen(markers[i]);break;}}
      if(loc>=0){
        content=content.substring(loc); content.trim();
        while(content.length()>0){char c=content[content.length()-1];
          if(c=='?'||c=='.'||c=='!'||c==',')content.remove(content.length()-1);else break;}
      } else {
        static const char* kwp[]={"weather in ","weather for ","weather at ","weather near ",
          "weather ","forecast for ","forecast in ","forecast ","temperature in ",
          "temperature at ","temperature ",nullptr};
        String stripped=NL::stripPrefixes(content,kwp); stripped.trim();
        while(stripped.length()>0){char c=stripped[stripped.length()-1];
          if(c=='?'||c=='.'||c=='!'||c==',')stripped.remove(stripped.length()-1);else break;}
        content=(stripped.length()>0&&stripped!=content)?stripped:"";
      }
      break;
    }
    case INTENT_REMINDER_CANCEL:{
      static const char* p[]={"cancel my reminder about ","cancel my reminder for ","cancel my reminder to ",
        "cancel my reminder ","cancel the reminder about ","cancel the reminder for ",
        "cancel the reminder to ","cancel the reminder ","cancel that reminder about ",
        "cancel that reminder ","cancel ","delete my reminder about ","delete my reminder for ",
        "delete my reminder to ","delete my reminder ","delete the reminder about ",
        "delete the reminder for ","delete the reminder ","delete that reminder about ",
        "delete that reminder ","delete ","remove my reminder about ","remove my reminder for ",
        "remove my reminder to ","remove my reminder ","remove the reminder about ",
        "remove the reminder for ","remove the reminder ","remove that reminder about ",
        "remove that reminder ","remove ","clear my reminder about ","clear my reminder for ",
        "clear my reminder to ","clear my reminder ","clear the reminder about ",
        "clear the reminder for ","clear the reminder ","clear that reminder about ",
        "clear that reminder ","clear ","turn off my reminder about ","turn off my reminder for ",
        "turn off my reminder ","turn off the reminder about ","turn off the reminder ","turn off ",
        "disable my reminder about ","disable my reminder ","disable the reminder ","disable ",
        "stop my reminder about ","stop my reminder ","stop the reminder ","stop that reminder ",
        "dismiss my reminder about ","dismiss my reminder ","dismiss the reminder ","dismiss ",
        "get rid of my reminder about ","get rid of my reminder ","get rid of the reminder ",
        "drop the reminder about ","drop the reminder ","drop my reminder ","drop ",nullptr};
      content=NL::stripPrefixes(content,p); break;
    }
    default: break;
  }
  content=stripTimeExpr(content); content.trim(); out.content=content;
}

// ── parseNaturalLanguage ───────────────────────────────
ParsedCommand parseNaturalLanguage(const String& s) {
  ParsedCommand cmd;

  if (nlContext.pendingIntent==INTENT_WEATHER && millis()-nlContext.lastUpdate<=NL_PENDING_TTL_MS) {
    String city=s; city.trim(); String lc=city; lc.toLowerCase();
    if(lc.startsWith("weather in "))city=city.substring(11);
    else if(lc.startsWith("weather for "))city=city.substring(12);
    else if(lc.startsWith("weather at "))city=city.substring(11);
    else if(lc.startsWith("weather "))city=city.substring(8);
    else if(lc.startsWith("forecast for "))city=city.substring(13);
    else if(lc.startsWith("forecast in "))city=city.substring(12);
    else if(lc.startsWith("forecast "))city=city.substring(9);
    else if(lc.startsWith("in "))city=city.substring(3);
    else if(lc.startsWith("at "))city=city.substring(3);
    else if(lc.startsWith("for "))city=city.substring(4);
    while(city.length()>0){char c=city[city.length()-1];
      if(c=='?'||c=='.'||c=='!'||c==',')city.remove(city.length()-1);else break;}
    if(city.length()>0&&city.length()<60){
      cmd.intent=INTENT_WEATHER; cmd.entities.content=city; cmd.confidence=0.9f; return cmd;
    }
  }

  cmd.intent=detectIntent(s); cmd.confidence=(cmd.intent==INTENT_NONE)?0.0f:0.8f;
  extractEntities(s,cmd.intent,cmd.entities);
  switch(cmd.intent){
    case INTENT_REMINDER_SET: case INTENT_MEMORY_SAVE: case INTENT_NOTE_ADD:
    case INTENT_TASK_ADD: case INTENT_SEARCH:
      if(cmd.entities.content.length()==0)cmd.confidence=0.4f; break;
    default: break;
  }
  return cmd;
}

void nlClearPending(){nlContext.pendingIntent=INTENT_NONE;nlContext.pendingEntities={};nlContext.lastUpdate=millis();}
void nlRememberLast(const ParsedCommand& cmd){nlContext.lastIntent=cmd.intent;nlContext.lastEntities=cmd.entities;nlContext.lastUpdate=millis();}

static void resolveClock(const ParsedTime& t,int& outH,int& outM){
  if(t.isRelative){time_t tg=now()+(time_t)t.relativeMinutes*60;outH=hour(tg);outM=minute(tg);}
  else{outH=t.hour;outM=t.minute;}
}

// ── executeIntent ──────────────────────────────────────
bool executeIntent(const ParsedCommand& cmd, const String& original) {
  if(nlContext.pendingIntent!=INTENT_NONE&&millis()-nlContext.lastUpdate>NL_PENDING_TTL_MS)nlClearPending();

  switch(cmd.intent){
    case INTENT_REMINDER_SET:{
      if(cmd.entities.content.length()==0)return false;
      bool haveTime=cmd.entities.time.found&&(cmd.entities.time.isRelative||cmd.entities.time.hour>=0);
      if(!haveTime){
        nlContext.pendingIntent=INTENT_REMINDER_SET; nlContext.pendingEntities=cmd.entities;
        nlContext.lastUpdate=millis();
        Serial.println("⏰ When should I remind you?  (e.g. \"at 6pm\" or \"in 10 minutes\")");
        return true;
      }
      int h,m; resolveClock(cmd.entities.time,h,m);
      RecurrenceType rec=cmd.entities.time.recurrence; int dow=cmd.entities.time.dayOfWeek,dom=0;
      if(rec==WEEKLY&&dow==0)dow=weekday(); if(rec==MONTHLY)dom=day();
      addReminder(cmd.entities.content,h,m,rec,dow,dom);
      Serial.println("✅ Reminder: \""+cmd.entities.content+"\" @ "+formatReminderTime(h,m)+" "+getRecurrenceText(rec,dow,dom));
      nlRememberLast(cmd); nlClearPending(); return true;
    }
    case INTENT_REMINDER_LIST: listReminders(); nlRememberLast(cmd); return true;
    case INTENT_REMINDER_CANCEL:{
      if(reminders.empty()){Serial.println("⏰ No reminders to cancel.");return true;}
      int idx=cmd.entities.referenceIndex;
      if(cmd.entities.reference=="last"||idx<0)idx=(int)reminders.size()-1;
      if(idx>=0&&idx<(int)reminders.size()){Serial.println("✅ Cancelled: "+reminders[idx].message);removeReminder(idx);}
      else Serial.println("⚠️  Couldn't pick a reminder to cancel. Try /reminders.");
      nlRememberLast(cmd); return true;
    }
    case INTENT_MEMORY_SAVE:{
      String c=cmd.entities.content; if(c.length()==0)return false;
      String key,val;
      int isIdx=c.indexOf(" is "),eqIdx=c.indexOf(" = ");
      int splitIdx=(isIdx>=0&&(eqIdx<0||isIdx<eqIdx))?isIdx:eqIdx;
      int splitLen=(splitIdx==isIdx)?4:3;
      if(splitIdx>0){key=c.substring(0,splitIdx);val=c.substring(splitIdx+splitLen);}
      else{int sp=c.indexOf(' ');if(sp>0){key=c.substring(0,sp);val=c.substring(sp+1);}else{key="note";val=c;}}
      String klow=key; klow.toLowerCase();
      if(klow.startsWith("my ")){key=key.substring(3);klow=klow.substring(3);}
      if(klow.startsWith("the ")){key=key.substring(4);klow=klow.substring(4);}
      key.trim(); val.trim();
      if(key.length()==0||val.length()==0){Serial.println("⚠️  Couldn't save — try \"remember my <thing> is <value>\".");return true;}
      rememberFact(key,val);
      Serial.println("💾 Got it. Remembered "+key+" = "+val);
      aiState=AI_LEARNING; stateChangeTime=millis(); nlRememberLast(cmd); return true;
    }
    case INTENT_MEMORY_RECALL:{
      String q=cmd.entities.content; q.trim(); String qLow=q; qLow.toLowerCase();
      if(q.length()>0)nlContext.lastTopic=q;
      String result="";
      for(auto& f:memory){String klow=f.key;klow.toLowerCase();
        if(qLow.length()==0||qLow.indexOf(klow)>=0||klow.indexOf(qLow)>=0){result+="📌 "+f.key+": "+f.value+"\n";f.accessCount++;}}
      if(result.length()==0)return false;
      Serial.println(result); saveMemory(); nlRememberLast(cmd); return true;
    }
    case INTENT_MEMORY_FORGET:{
      if(cmd.entities.reference=="last"||cmd.entities.reference=="that"||cmd.entities.content.length()==0){
        if(memory.empty()){Serial.println("🗑️  Nothing to forget.");return true;}
        Serial.println("🗑️  Forgot: "+memory.back().key); memory.pop_back(); saveMemory();
      } else if(cmd.entities.content.equalsIgnoreCase("everything")||cmd.entities.content.equalsIgnoreCase("all")){
        memory.clear(); saveMemory(); Serial.println("🗑️  All memory cleared.");
      } else {removeFact(cmd.entities.content);Serial.println("🗑️  Forgot: "+cmd.entities.content);}
      nlRememberLast(cmd); return true;
    }
    case INTENT_NOTE_ADD:{
      if(cmd.entities.content.length()==0)return false;
      String key="note_"+String(millis(),HEX)+"_"+String(++noteTaskCounter);
      rememberFact(key,cmd.entities.content); Serial.println("📝 Note saved: "+cmd.entities.content);
      nlRememberLast(cmd); return true;
    }
    case INTENT_NOTE_RECALL:{
      String result="";
      for(auto& f:memory)if(f.key.startsWith("note_"))result+="📝 "+f.value+"\n";
      if(result.length()==0)Serial.println("📝 No notes yet.");else Serial.println("\n"+result);
      nlRememberLast(cmd); return true;
    }
    case INTENT_TASK_ADD:{
      if(cmd.entities.content.length()==0)return false;
      String key="task_"+String(millis(),HEX)+"_"+String(++noteTaskCounter);
      rememberFact(key,cmd.entities.content); Serial.println("✅ Task added: "+cmd.entities.content);
      nlRememberLast(cmd); return true;
    }
    case INTENT_SEARCH:{
      if(cmd.entities.content.length()==0)return false;
      processConversation(cmd.entities.content); nlRememberLast(cmd); return true;
    }
    case INTENT_WEATHER:{
      String city=cmd.entities.content;
      if(city.length()==0){
        nlContext.pendingIntent=INTENT_WEATHER; nlContext.pendingEntities={}; nlContext.lastUpdate=millis();
        Serial.println("🌤️  Which city? (e.g. \"weather in Colombo\")"); return true;
      }
      getWeather(city); nlClearPending(); nlRememberLast(cmd); return true;
    }
    case INTENT_SYSTEM_STATUS: systemDiagnostics(); nlRememberLast(cmd); return true;
    case INTENT_SUMMARY: return false;
    case INTENT_CORRECTION:{
      if(cmd.entities.time.found&&nlContext.lastIntent==INTENT_REMINDER_SET&&!reminders.empty()){
        Reminder& r=reminders.back(); int h,m; resolveClock(cmd.entities.time,h,m);
        r.hour=h;r.minute=m;r.triggered=false; saveReminders();
        Serial.println("✏️  Updated last reminder to "+formatReminderTime(h,m));
        nlClearPending(); return true;
      }
      if(nlContext.pendingIntent!=INTENT_NONE){Serial.println("👍 Okay, cancelled before saving.");nlClearPending();return true;}
      if(nlContext.lastIntent==INTENT_REMINDER_SET&&!reminders.empty()){
        Serial.println("🗑️  Removed last reminder: "+reminders.back().message);
        reminders.pop_back(); saveReminders(); nlContext.lastIntent=INTENT_NONE; return true;
      }
      if(nlContext.lastIntent==INTENT_MEMORY_SAVE&&!memory.empty()){
        Serial.println("🗑️  Forgot: "+memory.back().key); memory.pop_back(); saveMemory(); nlContext.lastIntent=INTENT_NONE; return true;
      }
      Serial.println("👍 Okay, never mind."); return true;
    }
    case INTENT_FOLLOWUP_TIME:{
      if(nlContext.pendingIntent==INTENT_REMINDER_SET){
        ParsedCommand completed; completed.intent=INTENT_REMINDER_SET;
        completed.entities=nlContext.pendingEntities; completed.entities.time=cmd.entities.time;
        completed.confidence=0.9f; return executeIntent(completed,original);
      }
      return false;
    }
    default: return false;
  }
}

// ═══════════════════════════════════════════════════════
// SECTION 13 ── MEMORY
// ═══════════════════════════════════════════════════════

void rememberFact(const String& key, const String& value) {
  if(key.length()==0||value.length()==0)return;
  unsigned long epochNow=timeClient.getEpochTime();
  for(auto& f:memory){
    if(f.key.equalsIgnoreCase(key)){f.value=value;f.lastAccess=epochNow;f.accessCount++;saveMemory();return;}
  }
  if(memory.size()>=Config::MAX_MEMORY_FACTS){
    auto it=std::min_element(memory.begin(),memory.end(),[](const Fact& a,const Fact& b){return a.accessCount<b.accessCount;});
    memory.erase(it);
  }
  memory.push_back({key,value,1,epochNow});
  saveMemory();
}

String recallFact(const String& key){
  unsigned long epochNow=timeClient.getEpochTime();
  for(auto& f:memory){if(f.key.equalsIgnoreCase(key)){f.accessCount++;f.lastAccess=epochNow;return f.value;}}
  return "";
}

void removeFact(const String& key){
  if(key.length()==0){Serial.println("⚠️  removeFact: empty key — use /clear to wipe everything.");return;}
  memory.erase(std::remove_if(memory.begin(),memory.end(),[&](const Fact& f){return f.key.equalsIgnoreCase(key);}),memory.end());
  saveMemory();
}

void saveMemory(){
  JsonDocument doc; JsonArray arr=doc["memory"].to<JsonArray>();
  for(const auto& f:memory){JsonObject o=arr.add<JsonObject>();o["key"]=f.key;o["value"]=f.value;o["accessCount"]=f.accessCount;o["lastAccess"]=f.lastAccess;}
  File file=FFat.open("/memory.json",FILE_WRITE);
  if(file){serializeJson(doc,file);file.close();}
}

void loadMemory(){
  if(!FFat.exists("/memory.json"))return;
  File file=FFat.open("/memory.json",FILE_READ);if(!file)return;
  JsonDocument doc; if(deserializeJson(doc,file)){file.close();return;}
  memory.clear();
  for(JsonObject f:doc["memory"].as<JsonArray>())
    memory.push_back({f["key"].as<String>(),f["value"].as<String>(),f["accessCount"]|1,f["lastAccess"]|0UL});
  file.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 14 ── SENTIMENT & MOOD
// ═══════════════════════════════════════════════════════

String detectSentiment(const String& message){
  String lower=message; lower.toLowerCase(); int pos=0,neg=0;
  struct KW{const char* word;int score;};
  static const KW positive[]={{"great",2},{"awesome",2},{"love",2},{"happy",2},{"excellent",2},
    {"wonderful",2},{"fantastic",2},{"thank",1},{"good",1},{"nice",1},{"!",1},{"😊",2},{"😄",2},{"😍",2},{nullptr,0}};
  static const KW negative[]={{"terrible",2},{"awful",2},{"hate",2},{"sad",2},{"angry",2},
    {"frustrated",2},{"horrible",2},{"bad",1},{"worst",2},{"annoying",1},{"😞",2},{"😢",2},{"😡",2},{nullptr,0}};
  for(int i=0;positive[i].word;i++) if(lower.indexOf(positive[i].word)>=0)pos+=positive[i].score;
  for(int i=0;negative[i].word;i++) if(lower.indexOf(negative[i].word)>=0)neg+=negative[i].score;
  if(pos>=3&&neg==0)return "positive (0.85)";
  if(neg>=3&&pos==0)return "negative (0.85)";
  if(pos==0&&neg==0)return "neutral (0.5)";

  String prompt=
    "Classify the sentiment of this message in JSON only (no markdown):\n"
    "\""+message+"\"\n"
    "Respond only with: {\"s\":\"positive\",\"c\":0.8} or {\"s\":\"negative\",\"c\":0.75} or {\"s\":\"neutral\",\"c\":0.5}";
  String raw=groqSimpleCall(prompt,0.1f,32);
  if(raw.length()>0){
    if(raw.startsWith("```")){int s=raw.indexOf('\n')+1,e=raw.lastIndexOf("```");if(e>s){raw=raw.substring(s,e);raw.trim();}}
    JsonDocument p; if(!deserializeJson(p,raw))return p["s"].as<String>()+" ("+String(p["c"]|0.5f,2)+")";
  }
  return "neutral (0.5)";
}

void trackSentiment(const String& sentiment, float score){
  sentimentHistory.push_back({sentiment,score,timeClient.getEpochTime()});
  if(sentimentHistory.size()>Config::MAX_SENTIMENT_LOG)sentimentHistory.erase(sentimentHistory.begin());
  if(sentiment=="positive"){consecutivePositive++;consecutiveNegative=0;}
  else if(sentiment=="negative"){consecutiveNegative++;consecutivePositive=0;}
  else{consecutivePositive=0;consecutiveNegative=0;}
  userPattern.recentMood=sentiment;
  // [5] Mark dirty instead of immediate flash write
  g_dirtySentiment=true; g_dirtyPattern=true;
}

void respondToMood(){
  if(consecutivePositive>=3){celebratePositiveVibes();consecutivePositive=0;}
  if(consecutiveNegative>=2){offerComfort();consecutiveNegative=0;}
}

void celebratePositiveVibes(){Serial.println("\n✨ Great energy today! 🌟");aiState=AI_EXCITED;stateChangeTime=millis();}
void offerComfort(){Serial.println("\n💙 Seems like things might be tough. I'm here if you want to talk.");aiState=AI_CONCERNED;stateChangeTime=millis();}

void smartResponseEnhancement(String& response){
  if(userPattern.recentMood=="negative"&&response.indexOf("?")<0)
    if(random(100)<30)response+=" Let me know if there's anything else I can help with.";
}

// ═══════════════════════════════════════════════════════
// SECTION 15 ── USER PATTERNS & LEARNING
// ═══════════════════════════════════════════════════════

void updateUserPattern(const String& message){
  userPattern.totalInteractions++;
  userPattern.lastInteraction=timeClient.getEpochTime();
  int h=hour();
  if(h>=5&&h<12)userPattern.morningChats++;
  if(h>=18&&h<24)userPattern.eveningChats++;
  String lower=message; lower.toLowerCase();
  if(lower.indexOf("code")>=0||lower.indexOf("program")>=0||lower.indexOf("error")>=0||lower.indexOf("function")>=0)
    userPattern.techQuestions++;
  else if(lower.length()<60&&lower.indexOf("?")<0)userPattern.casualMessages++;
  String topic=analyzeConversationTopic(message);
  if(topic.length()>0){
    bool exists=false;
    for(int i=0;i<5;i++)if(userPattern.favoriteTopics[i]==topic){exists=true;break;}
    if(!exists)for(int i=0;i<5;i++){if(userPattern.favoriteTopics[i].length()==0){userPattern.favoriteTopics[i]=topic;break;}}
  }
  g_dirtyPattern=true;  // [5] batch
}

String analyzeConversationTopic(const String& message){
  String lower=message; lower.toLowerCase();
  if(lower.indexOf("weather")>=0) return "weather";
  if(lower.indexOf("remind")>=0)  return "reminders";
  if(lower.indexOf("code")>=0||lower.indexOf("program")>=0) return "technical";
  if(lower.indexOf("news")>=0)    return "news";
  if(lower.indexOf("joke")>=0)    return "entertainment";
  if(lower.indexOf("help")>=0)    return "help";
  return "";
}

void calculateThinkingComplexity(const String& message){
  int c=1;
  if(message.length()>100)c+=3; else if(message.length()>50)c+=1;
  String lower=message; lower.toLowerCase();
  if(lower.indexOf("why")>=0)c+=2; if(lower.indexOf("how")>=0)c+=1;
  if(lower.indexOf("explain")>=0)c+=2; if(lower.indexOf("compare")>=0)c+=3;
  if(lower.indexOf("analyze")>=0)c+=3; if(lower.indexOf("code")>=0)c+=2;
  thinkingComplexity=min(10,c);
}

void saveUserPattern(){
  JsonDocument doc;
  doc["total"]=userPattern.totalInteractions; doc["morning"]=userPattern.morningChats;
  doc["evening"]=userPattern.eveningChats; doc["lastTime"]=userPattern.lastInteraction;
  doc["mood"]=userPattern.recentMood; doc["tech"]=userPattern.techQuestions;
  doc["casual"]=userPattern.casualMessages; doc["remUsage"]=userPattern.reminderUsage;
  JsonArray topics=doc["topics"].to<JsonArray>();
  for(int i=0;i<5;i++)if(userPattern.favoriteTopics[i].length())topics.add(userPattern.favoriteTopics[i]);
  File f=FFat.open("/pattern.json",FILE_WRITE);
  if(f){serializeJson(doc,f);f.close();}
}

void loadUserPattern(){
  if(!FFat.exists("/pattern.json"))return;
  File f=FFat.open("/pattern.json",FILE_READ);if(!f)return;
  JsonDocument doc; if(deserializeJson(doc,f)){f.close();return;}
  userPattern.totalInteractions=doc["total"]|0; userPattern.morningChats=doc["morning"]|0;
  userPattern.eveningChats=doc["evening"]|0; userPattern.lastInteraction=doc["lastTime"]|0UL;
  userPattern.recentMood=doc["mood"]|"neutral"; userPattern.techQuestions=doc["tech"]|0;
  userPattern.casualMessages=doc["casual"]|0; userPattern.reminderUsage=doc["remUsage"]|0;
  if(doc.containsKey("topics")){int i=0;for(JsonVariant t:doc["topics"].as<JsonArray>())if(i<5)userPattern.favoriteTopics[i++]=t.as<String>();}
  f.close();
}

void saveSentimentData(){
  JsonDocument doc; JsonArray arr=doc["history"].to<JsonArray>();
  for(const auto& s:sentimentHistory){JsonObject o=arr.add<JsonObject>();o["s"]=s.sentiment;o["c"]=s.score;o["t"]=s.timestamp;}
  File f=FFat.open("/sentiment.json",FILE_WRITE);
  if(f){serializeJson(doc,f);f.close();}
}

void loadSentimentData(){
  if(!FFat.exists("/sentiment.json"))return;
  File f=FFat.open("/sentiment.json",FILE_READ);if(!f)return;
  JsonDocument doc; if(deserializeJson(doc,f)){f.close();return;}
  sentimentHistory.clear();
  for(JsonObject o:doc["history"].as<JsonArray>())
    sentimentHistory.push_back({o["s"].as<String>(),o["c"]|0.5f,o["t"]|0UL});
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 16 ── KNOWLEDGE DOMAINS
// ═══════════════════════════════════════════════════════

void initializeKnowledgeDomains(){
  knowledgeDomains={
    {"personal",0,0.3f,255,200,100},{"technical",0,0.3f,100,150,255},
    {"weather",0,0.3f,150,220,255},{"reminders",0,0.3f,180,100,255},
    {"general",0,0.3f,100,255,150},{"news",0,0.3f,255,150,50},
    {"entertainment",0,0.3f,255,100,200},
  };
  saveKnowledgeDomains();
}

void updateKnowledgeDomain(const String& domain, int xpGain){
  for(auto& kd:knowledgeDomains){
    if(kd.domain==domain){
      int oldXP=kd.experiencePoints; kd.experiencePoints+=xpGain;
      kd.confidenceLevel=min(0.95f,0.3f+kd.experiencePoints/500.0f);
      if((oldXP/100)<(kd.experiencePoints/100)){aiState=AI_EVOLVING;stateChangeTime=millis();}
      g_dirtyKnowledge=true; return;  // [5] batch
    }
  }
  for(auto& kd:knowledgeDomains)if(kd.domain=="general"){kd.experiencePoints+=xpGain;g_dirtyKnowledge=true;return;}
}

KnowledgeArea* getDominantKnowledge(){
  if(knowledgeDomains.empty())return nullptr;
  return &*std::max_element(knowledgeDomains.begin(),knowledgeDomains.end(),
    [](const KnowledgeArea& a,const KnowledgeArea& b){return a.experiencePoints<b.experiencePoints;});
}

void saveKnowledgeDomains(){
  JsonDocument doc; JsonArray arr=doc["domains"].to<JsonArray>();
  for(const auto& kd:knowledgeDomains){JsonObject o=arr.add<JsonObject>();o["domain"]=kd.domain;o["xp"]=kd.experiencePoints;o["conf"]=kd.confidenceLevel;o["r"]=kd.colorR;o["g"]=kd.colorG;o["b"]=kd.colorB;}
  File f=FFat.open("/knowledge.json",FILE_WRITE);
  if(f){serializeJson(doc,f);f.close();}
}

void loadKnowledgeDomains(){
  if(!FFat.exists("/knowledge.json"))return;
  File f=FFat.open("/knowledge.json",FILE_READ);if(!f)return;
  JsonDocument doc; if(deserializeJson(doc,f)){f.close();return;}
  knowledgeDomains.clear();
  for(JsonObject o:doc["domains"].as<JsonArray>())
    knowledgeDomains.push_back({o["domain"].as<String>(),o["xp"]|0,o["conf"]|0.3f,(uint8_t)(o["r"]|255),(uint8_t)(o["g"]|255),(uint8_t)(o["b"]|255)});
  f.close();
  Serial.println("🧬 Loaded "+String(knowledgeDomains.size())+" knowledge domains");
}

// ═══════════════════════════════════════════════════════
// SECTION 17 ── WEB / WEATHER
// ═══════════════════════════════════════════════════════

bool serperRequest(const String& query, int num, const String& tbs, JsonDocument& doc){
  JsonDocument reqDoc; reqDoc["q"]=query; reqDoc["num"]=num;
  if(tbs.length()>0)reqDoc["tbs"]=tbs;
  String body; serializeJson(reqDoc,body);

  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http;
  http.begin(secClient,"https://google.serper.dev/search");
  http.addHeader("X-API-KEY",Config::SERPER_API_KEY);
  http.addHeader("Content-Type","application/json");

  esp_task_wdt_reset();
  int code=http.POST(body);
  esp_task_wdt_reset();
  if(code<=0){http.end();return false;}

  // [1] Use PSRAM response buffer for JSON parsing if available
  bool parsed=false;
  doc.clear();
  if(g_psramRespBuf){
    int n=http.getStream().readBytes(g_psramRespBuf,Config::PSRAM_RESP_SIZE-1);
    g_psramRespBuf[n]='\0';
    parsed=!deserializeJson(doc,g_psramRespBuf);
  } else {
    parsed=!deserializeJson(doc,http.getString());
  }
  http.end();
  return parsed&&doc.containsKey("organic")&&doc["organic"].size()>0;
}

String fetchWebSearchResults(const String& query){
  if(query.length()==0)return "";
  String lq=query; lq.toLowerCase(); bool recency=isRecencyQuery(lq);
  JsonDocument doc; bool got=false;
  if(recency){got=serperRequest(query,5,"qdr:m",doc);if(!got)got=serperRequest(query,5,"qdr:y",doc);}
  if(!got)got=serperRequest(query,5,"",doc);
  if(!got||!doc.containsKey("organic"))return "";
  String result="Search: \""+query+"\"\n";
  int n=doc["organic"].size();
  for(int i=0;i<5&&i<n;i++){
    String title=doc["organic"][i]["title"].as<String>();
    String snippet=doc["organic"][i]["snippet"].as<String>();
    String date=doc["organic"][i]["date"]|"";
    result+="- "+title+": "+snippet;
    if(date.length()>0)result+=" [published: "+date+"]";
    result+="\n";
  }
  return result;
}

String httpGetWithRetry(const String& url,int maxRetries,int delayMs){
  for(int i=1;i<=maxRetries;i++){
    esp_task_wdt_reset();
    WiFiClientSecure secClient; secClient.setInsecure();
    HTTPClient http; http.begin(secClient,url);
    int code=http.GET(); esp_task_wdt_reset();
    if(code>0){String r=http.getString();http.end();return r;}
    http.end(); if(i<maxRetries)delay(delayMs*i);
  }
  return "";
}

static String resolveMeteosourcePlaceId(const String& city,String& outDisplay){
  String q=city; q.trim(); String qEnc=urlEncode(q);
  String url="https://www.meteosource.com/api/v1/free/find_places?text="+qEnc+"&key="+String(Config::WEATHER_KEY);
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.begin(secClient,url);
  esp_task_wdt_reset(); int code=http.GET(); esp_task_wdt_reset();
  String placeId="";
  if(code>0){
    JsonDocument doc;
    bool parsed=false;
    if(g_psramRespBuf){
      int n=http.getStream().readBytes(g_psramRespBuf,Config::PSRAM_RESP_SIZE-1);
      g_psramRespBuf[n]='\0'; parsed=!deserializeJson(doc,g_psramRespBuf);
    } else { String body=http.getString(); parsed=!deserializeJson(doc,body); }
    if(!parsed){}
    else if(doc.is<JsonArray>()&&doc.size()>0){
      const char* pid=doc[0]["place_id"]|"",*nmC=doc[0]["name"]|"",*adC=doc[0]["adm_area1"]|"",*ctC=doc[0]["country"]|"";
      placeId=String(pid); String nm(nmC),adm(adC),ctr(ctC);
      outDisplay=nm.length()?nm:q;
      if(adm.length())outDisplay+=", "+adm; else if(ctr.length())outDisplay+=", "+ctr;
    }
  }
  http.end(); return placeId;
}

void getWeather(String city){
  city.trim(); if(city.length()==0)city="Colombo";
  String display=city;
  String placeId=resolveMeteosourcePlaceId(city,display);
  if(placeId.length()==0){Serial.println("⚠️  No match found for: "+city+" (try a more specific name, e.g. \"New York City\")");return;}
  String url="https://www.meteosource.com/api/v1/free/point?place_id="+placeId+"&sections=current&units=metric&key="+String(Config::WEATHER_KEY);
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.begin(secClient,url);
  esp_task_wdt_reset(); int code=http.GET(); esp_task_wdt_reset();
  if(code>0){
    JsonDocument doc; DeserializationError err;
    if(g_psramRespBuf){int n=http.getStream().readBytes(g_psramRespBuf,Config::PSRAM_RESP_SIZE-1);g_psramRespBuf[n]='\0';err=deserializeJson(doc,g_psramRespBuf);}
    else{String body=http.getString();err=deserializeJson(doc,body);}
    if(!err&&doc.containsKey("current")){
      float temp=doc["current"]["temperature"],feels=doc["current"]["feels_like"];
      String summary=doc["current"]["summary"]|"";
      Serial.printf("🌤️  %s: %.1f°C (feels %.1f°C) %s\n",display.c_str(),temp,feels,summary.c_str());
    } else {
      const char* apiMsg=doc["detail"]|doc["message"]|"";
      if(strlen(apiMsg)>0)Serial.println("⚠️  Weather: "+String(apiMsg)+" (city: "+display+")");
      else Serial.println("⚠️  Weather data unavailable for: "+display+" (HTTP "+String(code)+")");
    }
  } else Serial.println("❌ Weather request failed (HTTP "+String(code)+")");
  http.end();
}

void searchWeb(const String& query){
  if(query.length()==0)return;
  String results=fetchWebSearchResults(query);
  if(results.length()>0)Serial.println("\n🔎 "+results);
  else Serial.println("⚠️  No results found.");
}

String urlEncode(const String& str){
  String enc="";
  for(char c:str){
    if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')enc+=c;
    else if(c==' ')enc+="%20";
    else{char buf[4];sprintf(buf,"%%%.2X",(unsigned char)c);enc+=buf;}
  }
  return enc;
}

// ═══════════════════════════════════════════════════════
// SECTION 18 ── CHAT HISTORY
// ═══════════════════════════════════════════════════════
int estimateTokens(const char* text){return strlen(text)/4;}

// [5] summarizeChatHistory is now called only ONCE per exchange
// (in addAssistantMessage), not in both addUserMessage and addAssistantMessage.
void summarizeChatHistory(){
  if(chatHistory.size()<10)return;
  String summary="[Previous conversation summary]: ";
  for(size_t i=0;i<chatHistory.size()-6;i++)
    summary+=String(chatHistory[i].role)+": "+String(chatHistory[i].content)+" | ";
  chatHistory.erase(chatHistory.begin(),chatHistory.end()-6);
  ChatMessage s; strlcpy(s.role,"system",sizeof(s.role));
  summary.toCharArray(s.content,sizeof(s.content));
  chatHistory.insert(chatHistory.begin(),s);
}

void limitChatHistoryByTokens(int maxTokens){
  int total=0;
  for(auto& m:chatHistory)total+=estimateTokens(m.content);
  while(total>maxTokens&&chatHistory.size()>2){total-=estimateTokens(chatHistory[0].content);chatHistory.erase(chatHistory.begin());}
}

void addUserMessage(const String& msg){
  ChatMessage m; strlcpy(m.role,"user",sizeof(m.role));
  msg.toCharArray(m.content,sizeof(m.content));
  chatHistory.push_back(m);
  // [5] do NOT summarize here — wait until addAssistantMessage to do it once
  limitChatHistoryByTokens();
  saveChatHistory();
}

void addAssistantMessage(const String& msg){
  ChatMessage m; strlcpy(m.role,"assistant",sizeof(m.role));
  msg.toCharArray(m.content,sizeof(m.content));
  chatHistory.push_back(m);
  summarizeChatHistory();  // [5] called exactly once per exchange, here
  limitChatHistoryByTokens();
  saveChatHistory();
}

void saveChatHistory(){
  JsonDocument doc; JsonArray arr=doc["history"].to<JsonArray>();
  for(const auto& m:chatHistory){JsonObject o=arr.add<JsonObject>();o["role"]=m.role;o["content"]=m.content;}
  File f=FFat.open("/chat.json",FILE_WRITE);
  if(f){serializeJson(doc,f);f.close();}
}

void loadChatHistory(){
  if(!FFat.exists("/chat.json"))return;
  File f=FFat.open("/chat.json",FILE_READ);if(!f)return;
  JsonDocument doc; if(deserializeJson(doc,f)){f.close();return;}
  chatHistory.clear();
  for(JsonObject o:doc["history"].as<JsonArray>()){
    ChatMessage m;
    strlcpy(m.role,o["role"]|"",sizeof(m.role));
    strlcpy(m.content,o["content"]|"",sizeof(m.content));
    chatHistory.push_back(m);
  }
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 19 ── PROACTIVE & BRIEFING
// ═══════════════════════════════════════════════════════

bool autoMorningBriefing(){return morningBriefingEnabled;}

void generateMorningBriefing(){
  Serial.println("\n☀️  ═══════════ GOOD MORNING! ══════════════");
  Serial.println("📅 "+String(day())+"/"+String(month())+"/"+String(year()));
  String city=recallFact("city"); if(city.length()==0)city="Colombo";
  getWeather(city);
  int todayDay=weekday(),todayDate=day(),shown=0;
  for(const auto& r:reminders){
    bool today=(r.recurrence==ONCE||r.recurrence==DAILY)||
               (r.recurrence==WEEKLY&&r.dayOfWeek==todayDay)||
               (r.recurrence==MONTHLY&&r.dayOfMonth==todayDate);
    if(today){if(shown==0)Serial.println("⏰ Today's reminders:");
      Serial.println("   • "+r.message+" at "+formatReminderTime(r.hour,r.minute));shown++;}
  }
  if(shown==0)Serial.println("📅 No reminders for today.");
  if(userPattern.recentMood=="negative")Serial.println("💙 Yesterday was tough — today is a fresh start!");
  else if(userPattern.recentMood=="positive")Serial.println("😊 You've been in great spirits — let's keep it going!");
  Serial.println("════════════════════════════════════════\n");
}

void checkProactiveOpportunity(){
  unsigned long epochNow=timeClient.getEpochTime();
  if(userPattern.lastInteraction>0&&(epochNow-userPattern.lastInteraction)<2700)return;
  int h=hour(); String msg="";
  if(h>=7&&h<9&&userPattern.morningChats>0)msg="Good morning! Need weather info or help planning your day?";
  else if(h>=12&&h<13)msg="Hey! Lunchtime — want to set a reminder for anything this afternoon?";
  else if(h>=18&&h<20&&userPattern.eveningChats>0)msg="Evening! Want a recap or help planning tomorrow?";
  else if(!reminders.empty())msg="Just a check-in — you have "+String(reminders.size())+" reminder(s) active. All good?";
  if(msg.length()>0){Serial.println("\n💡 "+msg);aiState=AI_PROACTIVE;stateChangeTime=millis();}
}

// ═══════════════════════════════════════════════════════
// SECTION 20 ── UTILITY / DIAGNOSTICS
// ═══════════════════════════════════════════════════════

bool heapOk(){
  size_t internalFree=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  return internalFree>Config::HEAP_SAFE_BYTES;
}

// [2] getCpuTemp now uses persistent global handle — no install/uninstall per call
float getCpuTemp(){
  if(!g_tsens) return -1.0f;
  float celsius=0.0f;
  temperature_sensor_get_celsius(g_tsens,&celsius);
  return celsius;
}

void systemDiagnostics(){
  unsigned long uptimeSec=(millis()-bootTime)/1000;
  uint32_t intHeap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  uint32_t totalHeap=ESP.getFreeHeap();
  float cpuTemp=getCpuTemp();

  Serial.println("\n📊 ═══ SYSTEM DIAGNOSTICS ═══");
  Serial.printf("  Version:     %s\n",Config::VERSION);
  Serial.printf("  Model:       %s\n",Config::GROQ_MODEL);
  Serial.printf("  Uptime:      %lu s  (%lu h %lu m)\n",uptimeSec,uptimeSec/3600,(uptimeSec%3600)/60);
  Serial.printf("  CPU Temp:    %.1f °C%s\n",cpuTemp,cpuTemp>75?" ⚠️  CRITICAL":cpuTemp>65?" ⚠️  HIGH":cpuTemp>55?" ⚠️  WARM":" ✅");
  Serial.printf("  Int SRAM:    %u bytes %s\n",intHeap,heapOk()?"✅":"⚠️  LOW — consider /clear");
  Serial.printf("  Total heap:  %u bytes (incl. OPI PSRAM)\n",totalHeap);
  if(g_psramReqBuf&&g_psramRespBuf)
    Serial.printf("  PSRAM bufs:  req=%uB resp=%uB  free=%u KB\n",(unsigned)Config::PSRAM_REQ_SIZE,(unsigned)Config::PSRAM_RESP_SIZE,(unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024));
  if(heapSnapshot>0){
    int delta=(int)intHeap-(int)heapSnapshot;
    unsigned long hr=(millis()-heapSnapshotTime)/3600000UL,mn=((millis()-heapSnapshotTime)%3600000UL)/60000UL;
    Serial.printf("  SRAM trend:  was %u bytes ~%luh%02lum ago  (%+d bytes)\n",heapSnapshot,hr,mn,delta);
  }
  Serial.printf("  WiFi:        %s  (attempts: %d)\n",WiFi.status()==WL_CONNECTED?"Connected ✅":"Offline ❌",wifiReconnectCount);
  Serial.printf("  Dual-core:   Core 0 HTTP worker %s\n",g_dcTask?"running ✅":"not started ⚠️");
  Serial.printf("  HTTP errors: %d this session\n",httpTimeoutCount);
  Serial.printf("  Interactions:%d\n",userPattern.totalInteractions);
  Serial.printf("  Mood:        %s\n",userPattern.recentMood.c_str());
  Serial.printf("  Reminders:   %d / %d\n",(int)reminders.size(),Config::MAX_REMINDERS);
  Serial.printf("  Memory facts:%d / %d\n",(int)memory.size(),Config::MAX_MEMORY_FACTS);
  Serial.printf("  Chat msgs:   %d / %d\n",(int)chatHistory.size(),Config::MAX_CHAT_MESSAGES);
  Serial.printf("  Skills:      %d / %d\n",(int)skillNames.size(),Config::MAX_SKILLS);
  KnowledgeArea* dom=getDominantKnowledge();
  if(dom)Serial.printf("  Top domain:  %s  (XP:%d  conf:%.0f%%)\n",dom->domain.c_str(),dom->experiencePoints,dom->confidenceLevel*100);
  Serial.println("══════════════════════════════");

  if(WiFi.status()!=WL_CONNECTED){Serial.println("⚠️  Offline — skipping AI analysis.");return;}
  Serial.println("\n🧠 AI Scan running...\n");

  String tele="TELEMETRY:\n";
  tele+="int_sram_free="+String(intHeap)+"B total_heap_free="+String(totalHeap)+"B(incl.PSRAM)";
  if(heapSnapshot>0){int delta=(int)intHeap-(int)heapSnapshot;unsigned long hr=max(1UL,(millis()-heapSnapshotTime)/3600000UL);tele+=String(" sram_drift=")+(delta>=0?"+":"")+String(delta)+"B_over_"+String(hr)+"h";}
  tele+=" cpu_temp="+String(cpuTemp,1)+"C";
  tele+=" uptime="+String(uptimeSec/3600)+"h"+String((uptimeSec%3600)/60)+"m";
  tele+=" http_errors="+String(httpTimeoutCount)+" wifi_attempts="+String(wifiReconnectCount);
  tele+=" reminders="+String(reminders.size())+" mem_facts="+String(memory.size());
  tele+=" chat_msgs="+String(chatHistory.size())+" interactions="+String(userPattern.totalInteractions);
  tele+=" mood="+userPattern.recentMood;

  String sys=
    "You are an automated ESP32 firmware health scanner. "
    "Output ONLY a compact scan report — no numbered lists, no paragraphs, no explanations.\n\n"
    "Format EXACTLY like this example:\n"
    "──────────────────────────────\n"
    "CPU       61.3 °C     ⚠️  WARM — consider ventilation\n"
    "Int SRAM  31840 B     ✅  Stable\n"
    "SRAM drift -12360 B  ⚠️  Leak suspected — run /clear if worsens\n"
    "WiFi      Connected   ✅  (1 attempt)\n"
    "HTTP err  3           ⚠️  Check API key / signal\n"
    "Uptime    2h 37m      ✅\n"
    "──────────────────────────────\n"
    "ACTION:  Short one-line instruction if anything needs fixing. "
    "If everything is fine write: All systems nominal.\n\n"
    "Rules: one line per metric, emoji status icon (✅ / ⚠️ / ❌), "
    "brief note after the icon only when something is wrong. "
    "Total output must be under 15 lines.";

  groqStream(tele,sys,0.2f,300);
  Serial.println("\n══════════════════════════════");
}

void clearAll(){
  memory.clear(); chatHistory.clear(); reminders.clear();
  sentimentHistory.clear(); userPattern=UserPattern(); knowledgeDomains.clear();
  const char* files[]={"/memory.json","/chat.json","/reminders.json",
                        "/pattern.json","/sentiment.json","/knowledge.json","/skills.json",nullptr};
  for(int i=0;files[i];i++)FFat.remove(files[i]);
  aiState=AI_IDLE; Serial.println("✅ All data cleared. Restarting...");
  delay(1000); ESP.restart();
}

void printHelp(){
  Serial.println("\n📖 ═══ "+String(Config::VERSION)+" HELP ═══");
  Serial.println("Commands:");
  Serial.println("  /help           — Show this");
  Serial.println("  /version        — Version + stats");
  Serial.println("  /diag           — Full diagnostics");
  Serial.println("  /reminders      — List reminders");
  Serial.println("  /remove N       — Delete reminder N");
  Serial.println("  /weather [city] — Weather info");
  Serial.println("  /search [query] — Web search");
  Serial.println("  /clear          — Wipe all data & restart");
  Serial.println("  /skills         — List self-taught skills");
  Serial.println("  /skills remove [name] — Forget a learned skill");
  Serial.println("  /skills keep    — Save the skill you're currently trying out");
  Serial.println("  /skills discard — Throw away the skill you're currently trying out");
  Serial.println("  /skills retry   — Ask for a fresh attempt at the pending skill");
  Serial.println("\nDon't see a feature? Just ask for it in plain English");
  Serial.println("(e.g. \"start a stopwatch\") — the assistant will try to");
  Serial.println("teach itself the skill on the spot.");
  Serial.println("\nReminder examples:");
  Serial.println("  \"Remind me to call mom at 3 PM\"");
  Serial.println("  \"Daily reminder for water at 8 AM\"");
  Serial.println("  \"Weekly meeting every Monday at 10 AM\"");
  Serial.println("  \"Monthly rent reminder on the 1st at 9 AM\"");
  Serial.println("═══════════════════════════════════════════");
}

void printVersion(){
  Serial.println("\n"+String(Config::VERSION));
  Serial.println("Model:     "+String(Config::GROQ_MODEL));
  Serial.println("Reminders: "+String(reminders.size())+"/"+String(Config::MAX_REMINDERS));
  Serial.println("Facts:     "+String(memory.size())+"/"+String(Config::MAX_MEMORY_FACTS));
  Serial.println("Chat msgs: "+String(chatHistory.size()));
  Serial.println("Skills:    "+String(skillNames.size())+"/"+String(Config::MAX_SKILLS));
  Serial.println("Int SRAM:  "+String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))+" bytes");
  Serial.println(String("PSRAM:     ")+(g_psramReqBuf?"enabled":"disabled"));
  Serial.println("Dual-core: "+(g_dcTask?String("Core 0 worker running"):String("single-core")));
  Serial.println("Uptime:    "+String((millis()-bootTime)/1000)+" s");
}

// ═══════════════════════════════════════════════════════
// SECTION 21 ── LED
// ═══════════════════════════════════════════════════════
void setLEDColor(uint8_t r,uint8_t g,uint8_t b,float brightness){
  brightness=constrain(brightness,0.0f,1.0f);
  strip.setPixelColor(0,strip.Color(uint8_t(r*brightness),uint8_t(g*brightness),uint8_t(b*brightness)));
  strip.show();
}

void updateLED(){
  unsigned long now=millis();
  auto pulse=[&](uint8_t r,uint8_t g,uint8_t b,float speed,float maxB){
    if(now-lastBlink>(unsigned long)(1000.0f/(speed*60))){
      pulseBrightness+=pulseIncreasing?0.02f:-0.02f;
      if(pulseBrightness>=maxB)pulseIncreasing=false;
      if(pulseBrightness<=Config::LED_MIN_BRIGHTNESS)pulseIncreasing=true;
      setLEDColor(r,g,b,pulseBrightness); lastBlink=now;
    }
  };
  switch(aiState){
    case AI_IDLE:    setLEDColor(0,0,0,0); break;
    case AI_THINKING:pulse(0,0,255,1.0f+thinkingComplexity*0.1f,Config::LED_MAX_BRIGHTNESS); break;
    case AI_REPLIED: setLEDColor(0,255,0,Config::LED_MAX_BRIGHTNESS);
                     if(now-stateChangeTime>Config::REPLIED_FLASH_MS)aiState=AI_IDLE; break;
    case AI_ERROR:
      if(now-lastBlink>300){setLEDColor(255,0,0,(blinkCount%2)?0:Config::LED_MAX_BRIGHTNESS);
        if(++blinkCount>=6){blinkCount=0;aiState=AI_IDLE;}lastBlink=now;} break;
    case AI_ALERT:
      pulse(255,50,0,1.2f,Config::LED_MAX_BRIGHTNESS);
      if(now-stateChangeTime>Config::REMINDER_ALERT_MS)aiState=AI_IDLE; break;
    case AI_EXCITED:
      if(now-lastBlink>80){setLEDColor(255,215,0,Config::LED_MIN_BRIGHTNESS+random(100)/100.0f*(Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS));lastBlink=now;}
      if(now-stateChangeTime>3000)aiState=AI_IDLE; break;
    case AI_CONCERNED:
      setLEDColor(100,150,255,Config::LED_MAX_BRIGHTNESS*0.7f);
      if(now-stateChangeTime>5000)aiState=AI_IDLE; break;
    case AI_PROACTIVE:
      pulse(128,0,255,0.8f,Config::LED_MAX_BRIGHTNESS*0.8f);
      if(now-stateChangeTime>5000)aiState=AI_IDLE; break;
    case AI_LEARNING:
      pulse(0,255,255,1.5f,Config::LED_MAX_BRIGHTNESS*0.9f);
      if(now-stateChangeTime>2000)aiState=AI_IDLE; break;
    case AI_EVOLVING:{
      KnowledgeArea* dom=getDominantKnowledge();
      if(dom&&now-lastBlink>100){float b=Config::LED_MIN_BRIGHTNESS+(Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS)*0.6f;setLEDColor(dom->colorR,dom->colorG,dom->colorB,b);lastBlink=now;}
      if(now-stateChangeTime>1500)aiState=AI_IDLE; break;
    }
  }
}

void rainbowWave(int durationMs){
  unsigned long start=millis();
  while(millis()-start<(unsigned long)durationMs){
    for(int i=0;i<256&&millis()-start<(unsigned long)durationMs;i+=4){
      uint8_t r=(i<85)?255-i*3:(i<170)?0:(i-170)*3;
      uint8_t g=(i<85)?i*3:(i<170)?255-(i-85)*3:0;
      uint8_t b=(i<85)?0:(i<170)?(i-85)*3:255-(i-170)*3;
      setLEDColor(r,g,b,0.25f); delay(8);
    }
  }
  setLEDColor(0,0,0,0);
}

// ═══════════════════════════════════════════════════════
// SECTION 22 ── SELF-TAUGHT SKILLS ENGINE  [4] ADVANCED
// ═══════════════════════════════════════════════════════
//
// Advanced DSL additions in v1.0:
//  • String vars — up to MAX_SKILL_STR_VARS per skill, stored under "strvars"
//  • set_str   — set a string var to a literal or interpolated value
//  • say       — enhanced: {var:.2f} (N decimals), {var:int}, {var:sec_to_mss}
//                          (ms→MM:SS.mmm), {var:time} (epoch→HH:MM), {str_var} (string var)
//  • inc       — shorthand for set var = var + N (default N=1)
//  • loop      — repeat body ops up to MAX_LOOP_COUNT times
//  • remember  — save a numeric or string var to the memory system
//  • recall    — load a memory fact into a string var
//  • groq      — call Groq with an interpolated prompt, store text reply in a string var
//  SkillExpr   — ABS(), FLOOR(), CEIL(), ROUND(), MIN(a,b), MAX(a,b), RANDOM(lo,hi)

// ── Expression evaluator: + - * / ( ) numbers, vars, built-in tokens & functions ──
namespace SkillExpr {
  struct Parser {
    const String& s;
    size_t pos=0;
    JsonObject vars;
    JsonObject strvars;  // [4] string vars (for length() etc.)

    Parser(const String& expr, JsonObject v, JsonObject sv) : s(expr), vars(v), strvars(sv) {}

    void skipSpace(){while(pos<s.length()&&s[pos]==' ')pos++;}

    float resolveIdent(const String& id){
      if(id=="MILLIS")   return (float)millis();
      if(id=="NOW_HOUR") return (float)hour();
      if(id=="NOW_MIN")  return (float)minute();
      if(id=="NOW_SEC")  return (float)second();
      if(id=="NOW_DAY")  return (float)day();
      if(id=="NOW_MONTH")return (float)month();
      if(id=="NOW_YEAR") return (float)year();
      if(id=="RAND100")  return (float)(esp_random()%101);
      if(vars.containsKey(id)) return vars[id].as<float>();
      // string var length helper: STRLEN_varname
      if(id.startsWith("STRLEN_")){
        String sv=id.substring(7);
        if(strvars.containsKey(sv))return (float)strlen(strvars[sv]|"");
      }
      return 0.0f;
    }

    // [4] Parse built-in function calls: ABS(x), FLOOR(x), CEIL(x), ROUND(x),
    //     MIN(a,b), MAX(a,b), RANDOM(lo,hi)
    float tryFunction(const String& name){
      skipSpace();
      if(pos>=s.length()||s[pos]!='(') return resolveIdent(name);
      pos++; // consume '('
      float a=parseExpr(); skipSpace();
      if(name=="ABS")  {if(pos<s.length()&&s[pos]==')')pos++; return fabsf(a);}
      if(name=="FLOOR"){if(pos<s.length()&&s[pos]==')')pos++; return floorf(a);}
      if(name=="CEIL") {if(pos<s.length()&&s[pos]==')')pos++; return ceilf(a);}
      if(name=="ROUND"){if(pos<s.length()&&s[pos]==')')pos++; return roundf(a);}
      if(name=="SIN")  {if(pos<s.length()&&s[pos]==')')pos++; return sinf(a);}
      if(name=="COS")  {if(pos<s.length()&&s[pos]==')')pos++; return cosf(a);}
      // Two-arg functions
      if(pos<s.length()&&s[pos]==',')pos++;
      float b=parseExpr(); skipSpace();
      if(pos<s.length()&&s[pos]==')')pos++;
      if(name=="MIN")    return min(a,b);
      if(name=="MAX")    return max(a,b);
      if(name=="RANDOM") return (float)((int)a+(int)(esp_random()%max(1,(int)(b-a)+1)));
      if(name=="MOD")    return (b!=0)?(float)((long)a%(long)b):0.0f;
      return a;
    }

    float parseNumberOrIdent(){
      skipSpace();
      if(pos<s.length()&&(isDigit(s[pos])||s[pos]=='.')){
        size_t start=pos;
        while(pos<s.length()&&(isDigit(s[pos])||s[pos]=='.'))pos++;
        return s.substring(start,pos).toFloat();
      }
      size_t start=pos;
      while(pos<s.length()&&(isAlphaNumeric(s[pos])||s[pos]=='_'))pos++;
      if(pos==start)return 0.0f;
      String id=s.substring(start,pos);
      return tryFunction(id);
    }

    float parseFactor(){
      skipSpace();
      if(pos<s.length()&&s[pos]=='('){pos++;float v=parseExpr();skipSpace();if(pos<s.length()&&s[pos]==')')pos++;return v;}
      if(pos<s.length()&&s[pos]=='-'){pos++;return -parseFactor();}
      return parseNumberOrIdent();
    }

    float parseTerm(){
      float v=parseFactor();
      for(;;){skipSpace();
        if(pos<s.length()&&(s[pos]=='*'||s[pos]=='/'))
          {char op=s[pos++];float rhs=parseFactor();v=(op=='*')?v*rhs:(rhs!=0?v/rhs:0);}
        else break;}
      return v;
    }

    float parseExpr(){
      float v=parseTerm();
      for(;;){skipSpace();
        if(pos<s.length()&&(s[pos]=='+'||s[pos]=='-'))
          {char op=s[pos++];float rhs=parseTerm();v=(op=='+')?v+rhs:v-rhs;}
        else break;}
      return v;
    }
  };

  float eval(const String& expr, JsonObject vars, JsonObject strvars={}) {
    Parser p(expr,vars,strvars);
    return p.parseExpr();
  }
}

// ── [4] Enhanced say with format specifiers ──────────────
// Supported: {varName}           → integer display (old behaviour)
//            {varName:.Nf}       → N decimal places
//            {varName:int}       → explicit integer
//            {varName:sec_to_mss}→ treat as milliseconds → MM:SS.mmm
//            {varName:time}      → treat as epoch seconds → HH:MM
//            {strVarName}        → string var value (looked up in strvars)
static String formatVarValue(const String& varName, const String& fmt,
                              JsonObject vars, JsonObject strvars) {
  // Check string vars first
  if(strvars.containsKey(varName))return strvars[varName].as<String>();

  float val=vars.containsKey(varName)?vars[varName].as<float>():0.0f;

  if(fmt.length()==0||fmt=="int"){
    return String((long)val);
  }
  if(fmt.startsWith(".")&&fmt.endsWith("f")){
    int decimals=fmt.substring(1,fmt.length()-1).toInt();
    return String(val,constrain(decimals,0,6));
  }
  if(fmt=="sec_to_mss"){
    unsigned long ms=(unsigned long)fabsf(val);
    unsigned long mins=ms/60000,secs=(ms%60000)/1000,millis_=ms%1000;
    char buf[16]; sprintf(buf,"%02lu:%02lu.%03lu",mins,secs,millis_);
    return String(buf);
  }
  if(fmt=="time"){
    unsigned long ep=(unsigned long)fabsf(val);
    int hh=(ep/3600)%24,mm=(ep%3600)/60;
    char buf[8]; sprintf(buf,"%02d:%02d",hh,mm);
    return String(buf);
  }
  return String(val,0);
}

void interpolateSay(String text, JsonObject vars, JsonObject strvars) {
  String out;
  for(size_t i=0;i<text.length();i++){
    if(text[i]=='{'){
      int end=text.indexOf('}',i);
      if(end>0){
        String spec=text.substring(i+1,end);
        // Check for format specifier: {varName:fmt}
        int colonPos=spec.indexOf(':');
        String varName=colonPos>=0?spec.substring(0,colonPos):spec;
        String fmt=colonPos>=0?spec.substring(colonPos+1):"";
        out+=formatVarValue(varName,fmt,vars,strvars);
        i=end;
        continue;
      }
    }
    out+=text[i];
  }
  Serial.println("AI: "+out);
  addAssistantMessage(out);
}

// ── [4] String var interpolation for set_str / groq prompt ──
static String interpolateStr(const String& tmpl, JsonObject vars, JsonObject strvars) {
  String out;
  for(size_t i=0;i<tmpl.length();i++){
    if(tmpl[i]=='{'){
      int end=tmpl.indexOf('}',i);
      if(end>0){
        String spec=tmpl.substring(i+1,end);
        int colonPos=spec.indexOf(':');
        String varName=colonPos>=0?spec.substring(0,colonPos):spec;
        String fmt=colonPos>=0?spec.substring(colonPos+1):"";
        out+=formatVarValue(varName,fmt,vars,strvars);
        i=end; continue;
      }
    }
    out+=tmpl[i];
  }
  return out;
}

// ── Validation: enforces sandbox shape ──────────────────
bool validateSkillOps(JsonArrayConst ops, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err);

bool validateSkillOp(JsonObjectConst op, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err) {
  const char* type=op["op"]|"";
  String t(type);

  if(t=="set"){
    const char* var=op["var"]|"";
    if(!varsSchema.containsKey(var)){err="set: unknown var '"+String(var)+"'";return false;}
    const char* expr=op["expr"]|"";
    if(strlen(expr)==0||strlen(expr)>120){err="set: bad expr";return false;}
    return true;
  }
  if(t=="inc"){  // [4] shorthand increment
    const char* var=op["var"]|"";
    if(!varsSchema.containsKey(var)){err="inc: unknown var '"+String(var)+"'";return false;}
    return true;
  }
  if(t=="set_str"){  // [4] set string var
    const char* var=op["var"]|"";
    if(!strVarsSchema.containsKey(var)){err="set_str: unknown string var '"+String(var)+"'";return false;}
    const char* val=op["val"]|"";
    if(strlen(val)>200){err="set_str: val too long";return false;}
    return true;
  }
  if(t=="say"){
    const char* text=op["text"]|"";
    if(strlen(text)==0||strlen(text)>250){err="say: text missing or too long";return false;}
    return true;
  }
  if(t=="led"){
    int r=op["r"]|-1,g=op["g"]|-1,b=op["b"]|-1; float br=op["brightness"]|-1.0f;
    if(r<0||r>255||g<0||g>255||b<0||b>255||br<0||br>1.0f){err="led: values out of range";return false;}
    return true;
  }
  if(t=="wait"){
    int ms=op["ms"]|-1;
    if(ms<0||ms>Config::MAX_SKILL_WAIT_MS){err="wait: ms out of range";return false;}
    return true;
  }
  if(t=="if"){
    const char* var=op["var"]|"";
    if(!varsSchema.containsKey(var)){err="if: unknown var '"+String(var)+"'";return false;}
    const char* cmp=op["cmp"]|"";
    String cmpStr(cmp);
    if(cmpStr!="=="&&cmpStr!="!="&&cmpStr!="<"&&cmpStr!=">"&&cmpStr!="<="&&cmpStr!=">="){err="if: bad cmp operator";return false;}
    if(!op["value"].is<float>()&&!op["value"].is<int>()){err="if: value must be numeric";return false;}
    if(depth+1>Config::MAX_IF_DEPTH){err="if: nested too deep";return false;}
    if(!op["then"].is<JsonArrayConst>()){err="if: missing 'then'";return false;}
    if(!validateSkillOps(op["then"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err))return false;
    if(op["else"].is<JsonArrayConst>())
      if(!validateSkillOps(op["else"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err))return false;
    return true;
  }
  if(t=="loop"){  // [4] loop op
    int count=op["count"]|-1;
    if(count<1||count>Config::MAX_LOOP_COUNT){err="loop: count must be 1-"+String(Config::MAX_LOOP_COUNT);return false;}
    if(!op["body"].is<JsonArrayConst>()){err="loop: missing 'body'";return false;}
    if(depth+1>Config::MAX_IF_DEPTH){err="loop: nested too deep";return false;}
    if(!validateSkillOps(op["body"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err))return false;
    return true;
  }
  if(t=="remember"){  // [4] persist to memory system
    const char* key=op["key"]|"";
    if(strlen(key)==0||strlen(key)>40){err="remember: key missing or too long";return false;}
    const char* src=op["var"]|""; const char* strsrc=op["strvar"]|"";
    if(strlen(src)==0&&strlen(strsrc)==0){err="remember: need 'var' or 'strvar'";return false;}
    if(strlen(src)>0&&!varsSchema.containsKey(src)){err="remember: unknown var '"+String(src)+"'";return false;}
    if(strlen(strsrc)>0&&!strVarsSchema.containsKey(strsrc)){err="remember: unknown strvar '"+String(strsrc)+"'";return false;}
    return true;
  }
  if(t=="recall"){  // [4] load from memory system into string var
    const char* key=op["key"]|"";
    if(strlen(key)==0||strlen(key)>40){err="recall: key missing or too long";return false;}
    const char* dst=op["strvar"]|"";
    if(strlen(dst)==0||!strVarsSchema.containsKey(dst)){err="recall: unknown strvar '"+String(dst)+"'";return false;}
    return true;
  }
  if(t=="groq"){  // [4] AI call from within a skill
    const char* prompt=op["prompt"]|"";
    if(strlen(prompt)==0||strlen(prompt)>300){err="groq: prompt missing or too long";return false;}
    const char* dst=op["strvar"]|"";
    if(strlen(dst)==0||!strVarsSchema.containsKey(dst)){err="groq: unknown strvar '"+String(dst)+"'";return false;}
    return true;
  }
  if(t=="end")return true;

  err="unknown op '"+String(type)+"'";
  return false;
}

bool validateSkillOps(JsonArrayConst ops, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err){
  if(ops.size()>(size_t)Config::MAX_OPS_PER_ACTION){err="too many ops in one action";return false;}
  for(JsonObjectConst op:ops)if(!validateSkillOp(op,varsSchema,strVarsSchema,depth,err))return false;
  return true;
}

bool validateSkillDefinition(JsonDocument& doc, String& err){
  const char* name=doc["name"]|"";
  if(strlen(name)==0||strlen(name)>32){err="name missing or too long";return false;}

  JsonVariant varsVar=doc["vars"];
  if(!varsVar.is<JsonObject>()){err="'vars' must be an object";return false;}
  JsonObject vars=varsVar.as<JsonObject>();
  if(vars.size()>(size_t)Config::MAX_SKILL_VARS){err="too many numeric vars";return false;}
  for(JsonPair kv:vars)if(!kv.value().is<float>()&&!kv.value().is<int>()&&!kv.value().is<bool>())
    {err="var '"+String(kv.key().c_str())+"' must be a number or boolean";return false;}

  // [4] Validate optional string vars
  JsonObject strVars;
  if(doc.containsKey("strvars")){
    if(!doc["strvars"].is<JsonObject>()){err="'strvars' must be an object";return false;}
    strVars=doc["strvars"].as<JsonObject>();
    if(strVars.size()>(size_t)Config::MAX_SKILL_STR_VARS){err="too many string vars";return false;}
    for(JsonPair kv:strVars)if(!kv.value().is<const char*>())
      {err="strvar '"+String(kv.key().c_str())+"' must be a string";return false;}
  }

  JsonVariant triggersVar=doc["triggers"],actionsVar=doc["actions"];
  if(!triggersVar.is<JsonObject>()||!actionsVar.is<JsonObject>())
    {err="'triggers' and 'actions' must be objects";return false;}
  JsonObject triggers=triggersVar.as<JsonObject>(),actions=actionsVar.as<JsonObject>();
  if(triggers.size()==0||triggers.size()>(size_t)Config::MAX_SKILL_ACTIONS)
    {err="'triggers' must declare 1-"+String(Config::MAX_SKILL_ACTIONS)+" actions";return false;}
  if(triggers.size()!=actions.size())
    {err="'triggers' and 'actions' key sets differ";return false;}

  for(JsonPair kv:triggers){
    const char* actionName=kv.key().c_str();
    if(!actions.containsKey(actionName)){err="action '"+String(actionName)+"' has triggers but no ops";return false;}
    if(!kv.value().is<JsonArrayConst>()){err="triggers for '"+String(actionName)+"' must be an array";return false;}
    JsonArrayConst phrases=kv.value().as<JsonArrayConst>();
    if(phrases.size()==0||phrases.size()>5){err="action '"+String(actionName)+"' needs 1-5 trigger phrases";return false;}
    for(JsonVariantConst p:phrases)
      if(!p.is<const char*>()||strlen(p.as<const char*>())==0){err="empty trigger phrase";return false;}
  }
  for(JsonPair kv:actions){
    if(!triggers.containsKey(kv.key().c_str())){err="action '"+String(kv.key().c_str())+"' has no trigger phrases";return false;}
    if(!kv.value().is<JsonArrayConst>()){err="action ops must be an array";return false;}
    if(!validateSkillOps(kv.value().as<JsonArrayConst>(),vars,strVars,0,err))return false;
  }
  return true;
}

// ── [4] Interpreter — executes one action's op list ──────
void executeSkillOps(JsonArrayConst ops, JsonObject vars, JsonObject strvars) {
  for(JsonObjectConst op:ops){
    const char* type=op["op"]|""; String t(type);

    if(t=="set"){
      const char* var=op["var"]|"";
      float v=SkillExpr::eval(String((const char*)(op["expr"]|"")),vars,strvars);
      vars[var]=v;

    } else if(t=="inc"){  // [4]
      const char* var=op["var"]|"";
      float step=op["step"]|1.0f;
      vars[var]=vars[var].as<float>()+step;

    } else if(t=="set_str"){  // [4]
      const char* var=op["var"]|""; const char* val=op["val"]|"";
      strvars[var]=interpolateStr(String(val),vars,strvars);

    } else if(t=="say"){
      interpolateSay(String((const char*)(op["text"]|"")),vars,strvars);

    } else if(t=="led"){
      setLEDColor(op["r"]|0,op["g"]|0,op["b"]|0,op["brightness"]|0.5f);

    } else if(t=="wait"){
      int ms=op["ms"]|0;
      esp_task_wdt_reset();
      delay(constrain(ms,0,Config::MAX_SKILL_WAIT_MS));

    } else if(t=="if"){
      const char* var=op["var"]|"";
      float lhs=vars.containsKey(var)?vars[var].as<float>():0.0f;
      float rhs=op["value"]|0.0f; String cmp=op["cmp"]|"";
      bool result=(cmp=="==")?lhs==rhs:(cmp=="!=")?lhs!=rhs:(cmp=="<")?lhs<rhs:
                  (cmp==">")?lhs>rhs:(cmp=="<=")?lhs<=rhs:(cmp==">=")?lhs>=rhs:false;
      if(result)executeSkillOps(op["then"].as<JsonArrayConst>(),vars,strvars);
      else if(op["else"].is<JsonArrayConst>())executeSkillOps(op["else"].as<JsonArrayConst>(),vars,strvars);

    } else if(t=="loop"){  // [4]
      int count=constrain((int)(op["count"]|1),1,Config::MAX_LOOP_COUNT);
      JsonArrayConst body=op["body"].as<JsonArrayConst>();
      for(int li=0;li<count;li++){esp_task_wdt_reset();executeSkillOps(body,vars,strvars);}

    } else if(t=="remember"){  // [4] persist to memory system
      const char* key=op["key"]|"";
      const char* srcVar=op["var"]|""; const char* srcStrvar=op["strvar"]|"";
      String val;
      if(strlen(srcVar)>0&&vars.containsKey(srcVar)){
        val=String(vars[srcVar].as<float>(),2);
        // strip trailing zeros for cleaner storage
        while(val.endsWith("0")&&val.indexOf('.')>=0)val.remove(val.length()-1);
        if(val.endsWith("."))val.remove(val.length()-1);
      } else if(strlen(srcStrvar)>0&&strvars.containsKey(srcStrvar)){
        val=strvars[srcStrvar].as<String>();
      }
      if(strlen(key)>0&&val.length()>0){
        rememberFact(String(key),val);
        Serial.println("💾 Skill saved: "+String(key)+" = "+val);
      }

    } else if(t=="recall"){  // [4] load from memory into string var
      const char* key=op["key"]|""; const char* dst=op["strvar"]|"";
      String val=recallFact(String(key));
      strvars[dst]=val.length()>0?val:String("(not set)");

    } else if(t=="groq"){  // [4] AI call from skill
      const char* promptTmpl=op["prompt"]|""; const char* dst=op["strvar"]|"";
      String prompt=interpolateStr(String(promptTmpl),vars,strvars);
      if(prompt.length()>0&&heapOk()&&WiFi.status()==WL_CONNECTED){
        String reply=groqSimpleCall(prompt,0.3f,100);
        strvars[dst]=reply.length()>0?reply:String("(no reply)");
        Serial.println("🤖 "+String(dst)+": "+strvars[dst].as<String>());
      }

    } else if(t=="end"){
      return;
    }
  }
}

void runSkillAction(int skillIdx, const String& actionName){
  if(skillIdx<0||skillIdx>=(int)skillJson.size())return;
  JsonDocument doc;
  if(deserializeJson(doc,skillJson[skillIdx]))return;
  JsonObject vars=doc["vars"].as<JsonObject>();

  // [4] Create or retrieve string vars
  if(!doc.containsKey("strvars"))doc["strvars"].to<JsonObject>();
  JsonObject strvars=doc["strvars"].as<JsonObject>();

  JsonArrayConst ops=doc["actions"][actionName].as<JsonArrayConst>();
  executeSkillOps(ops,vars,strvars);

  // Persist updated vars (including strvars) back to JSON
  String updated; serializeJson(doc,updated);
  skillJson[skillIdx]=updated;
  if(!(testingSkill&&skillIdx==pendingSkillIndex))saveSkills();
}

// ── Small helpers ────────────────────────────────────────
bool isAlnumCh(char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');}
char toLowerCh(char c){return(c>='A'&&c<='Z')?(char)(c-'A'+'a'):c;}

String stripToAlnum(const String& s){
  String out; out.reserve(s.length());
  for(size_t i=0;i<s.length();i++)if(isAlnumCh(s[i]))out+=toLowerCh(s[i]);
  return out;
}

String extractKeywords(String phrase){
  phrase.toLowerCase();
  static const char* STOPWORDS[]={"the","a","an","please","could","would","can","you","to","my",
    "is","for","now","me","it","of","in","on","do","does","that","this","and"};
  String cleaned; cleaned.reserve(phrase.length());
  for(size_t i=0;i<phrase.length();i++)cleaned+=isAlnumCh(phrase[i])?phrase[i]:' ';
  String out; int start=0;
  while(start<(int)cleaned.length()){
    while(start<(int)cleaned.length()&&cleaned[start]==' ')start++;
    int end=start;
    while(end<(int)cleaned.length()&&cleaned[end]!=' ')end++;
    if(end>start){
      String word=cleaned.substring(start,end); bool stop=false;
      for(const char* sw:STOPWORDS)if(word==sw){stop=true;break;}
      if(!stop&&word.length()>=2){if(out.length()>0)out+=' ';out+=word;}
    }
    start=end;
  }
  return out;
}

bool matchLearnedSkill(const String& input, int& outSkillIdx, String& outAction){
  String lower=input; lower.toLowerCase();
  for(const auto& t:skillTriggerIndex){
    if(lower.indexOf(t.phrase)>=0){outSkillIdx=t.skillIdx;outAction=t.action;return true;}
  }
  String stripped=stripToAlnum(lower);
  const SkillTrigger* best=nullptr; int bestScore=0;
  for(const auto& t:skillTriggerIndex){
    if(t.keywords.length()==0)continue;
    bool allFound=true; int score=0,start=0;
    while(start<=(int)t.keywords.length()){
      int sp=t.keywords.indexOf(' ',start);
      String kw=(sp<0)?t.keywords.substring(start):t.keywords.substring(start,sp);
      if(kw.length()>0){if(stripped.indexOf(kw)>=0)score++;else{allFound=false;break;}}
      if(sp<0)break; start=sp+1;
    }
    if(allFound&&score>bestScore){bestScore=score;best=&t;}
  }
  if(best){outSkillIdx=best->skillIdx;outAction=best->action;return true;}
  return false;
}

void rebuildSkillTriggerIndex(){
  skillTriggerIndex.clear();
  for(size_t i=0;i<skillJson.size();i++){
    JsonDocument doc; if(deserializeJson(doc,skillJson[i]))continue;
    for(JsonPair kv:doc["triggers"].as<JsonObject>())
      for(JsonVariant p:kv.value().as<JsonArray>()){
        String phrase=p.as<String>(); phrase.toLowerCase();
        skillTriggerIndex.push_back({(int)i,String(kv.key().c_str()),phrase,extractKeywords(phrase)});
      }
  }
}

void listSkills(){
  if(skillNames.empty()){Serial.println("🧠 No self-taught skills yet — just ask for something new!");return;}
  Serial.println("\n🧠 ═══ SELF-TAUGHT SKILLS ═══");
  for(size_t i=0;i<skillNames.size();i++){
    JsonDocument doc; String desc="";
    if(!deserializeJson(doc,skillJson[i]))desc=doc["description"]|"";
    Serial.println("  "+String(i)+". "+skillNames[i]+" — "+desc);
  }
  Serial.println("═══════════════════════════════");
}

void removeSkill(const String& nameRaw){
  String name=nameRaw; name.trim();
  for(size_t i=0;i<skillNames.size();i++){
    if(skillNames[i].equalsIgnoreCase(name)){
      Serial.println("🗑️  Forgot skill: "+skillNames[i]);
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i);
      rebuildSkillTriggerIndex(); saveSkills(); return;
    }
  }
  Serial.println("❌ No skill named '"+name+"'. Use /skills to see the list.");
}

void saveSkills(){
  JsonDocument doc; JsonArray arr=doc["skills"].to<JsonArray>();
  for(const auto& raw:skillJson){JsonDocument one;if(deserializeJson(one,raw))continue;arr.add(one);}
  File file=FFat.open("/skills.json",FILE_WRITE);
  if(file){serializeJson(doc,file);file.close();}
}

void loadSkills(){
  skillNames.clear(); skillJson.clear();
  if(!FFat.exists("/skills.json"))return;
  File file=FFat.open("/skills.json",FILE_READ);if(!file)return;
  JsonDocument doc; if(deserializeJson(doc,file)){file.close();return;}
  file.close();
  for(JsonObject s:doc["skills"].as<JsonArray>()){
    String name=s["name"]|""; if(name.length()==0)continue;
    String raw; serializeJson(s,raw); skillNames.push_back(name); skillJson.push_back(raw);
  }
  rebuildSkillTriggerIndex();
  Serial.println("✅ Loaded "+String(skillNames.size())+" self-taught skill(s)");
}

bool looksLikeFeatureRequest(const String& input){
  if(!heapOk()||WiFi.status()!=WL_CONNECTED)return false;
  String prompt=
    "Reply with EXACTLY one word, nothing else: FEATURE or CHAT.\n"
    "FEATURE = the user is asking this device to perform/track/do a concrete new "
    "action or capability it might not already have (timers, counters, stopwatches, "
    "toggles, simple games, small utilities, calculators, flashcard quizzes).\n"
    "CHAT = anything conversational, a question, small talk, or something requiring "
    "real-world knowledge/search.\n"
    "Message: \""+input+"\"";
  String verdict=groqSimpleCall(prompt,0.0f,8);
  verdict.trim(); verdict.toUpperCase();
  return verdict.startsWith("FEATURE");
}

// ── Generic HTTPS JSON POST helper (used by Gemini calls) ──
String postJson(const String& url, const String& body){
  if(!heapOk()||WiFi.status()!=WL_CONNECTED)return "";
  esp_task_wdt_reset();
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.setTimeout(Config::SKILL_GEN_TIMEOUT_MS);
  http.begin(secClient,url); http.addHeader("Content-Type","application/json");
  int code=http.POST(body); esp_task_wdt_reset();
  String result; if(code>0)result=http.getString();
  http.end(); return result;
}

// ── [4] Updated SKILL_SYSTEM_PROMPT teaching Gemini the advanced DSL ──
const char* SKILL_SYSTEM_PROMPT = R"PROMPT(You write tiny "skill" definitions for a voice/text assistant running on a memory-constrained ESP32 microcontroller. The device has a SAFE, sandboxed interpreter — you may ONLY use the operations below. Never invent new op types or fields.

Output STRICT JSON only (no markdown fences, no commentary) matching exactly this shape:

{
  "name": "snake_case_identifier, <=32 chars, unique, describes the feature",
  "description": "one short sentence",
  "vars": { "<name>": <number or boolean initial value>, ... },
  "strvars": { "<name>": "<initial string value>", ... },
  "triggers": { "<action_name>": ["phrase a user would say", "..."], ... },
  "actions": { "<action_name>": [ <op>, <op>, ... ], ... }
}

vars: max 12 numeric. strvars: max 4 string vars (optional, omit if not needed).
triggers/actions: max 8 actions, 1-5 phrases each, max 16 ops per action.

Allowed ops (each is a JSON object; "op" field selects the type):

NUMERIC OPS:
- {"op":"set","var":"<existing var>","expr":"<arithmetic expression>"}
    expr may use: + - * / ( ) numeric literals, var names from "vars",
    special read-only tokens: MILLIS (device uptime ms), NOW_HOUR (0-23),
    NOW_MIN (0-59), NOW_SEC (0-59), NOW_DAY (1-31), NOW_MONTH (1-12),
    NOW_YEAR, RAND100 (random 0-100), STRLEN_varname (length of a strvar).
    Built-in functions: ABS(x), FLOOR(x), CEIL(x), ROUND(x), MIN(a,b),
    MAX(a,b), RANDOM(lo,hi), MOD(a,b), SIN(x), COS(x).
- {"op":"inc","var":"<existing var>","step":<number>}
    Increments var by step (default 1 if omitted). Shorthand for set var=var+step.

STRING OPS:
- {"op":"set_str","var":"<strvar name>","val":"<text with {numvar} or {strvar} interpolation>"}
    Sets a string var. Can embed numeric vars and other string vars with {varName}.
- {"op":"remember","key":"<memory key>","var":"<numvar>"}    — save a numeric var to long-term memory
  {"op":"remember","key":"<memory key>","strvar":"<strvar>"}  — save a string var to long-term memory
    The value is stored persistently and survives reboots.
- {"op":"recall","key":"<memory key>","strvar":"<strvar>"}
    Load a previously remembered value into a string var. Sets "(not set)" if key missing.
- {"op":"groq","prompt":"<text, may use {var}/{strvar} interpolation>","strvar":"<strvar to store reply>"}
    Calls Groq AI with the interpolated prompt (max ~100 tokens reply). Stores text result in strvar.
    Use for dynamic AI responses inside a skill (e.g. translations, suggestions, riddles).

OUTPUT / CONTROL:
- {"op":"say","text":"<text shown to user; embed vars with {varName}, {varName:.2f}, {varName:int}, {varName:sec_to_mss} (ms→MM:SS.mmm), {varName:time} (epoch→HH:MM), {strvar}>"}
- {"op":"led","r":0-255,"g":0-255,"b":0-255,"brightness":0.0-1.0}
- {"op":"wait","ms":0-3000}
- {"op":"if","var":"<numvar>","cmp":"==|!=|<|>|<=|>=","value":<number>,"then":[ops...],"else":[ops...]}
    Nesting depth max 3.
- {"op":"loop","count":<1-10>,"body":[ops...]}
    Repeats body ops exactly count times. Depth counted same as if.
- {"op":"end"}   — stop execution of this action immediately

Rules:
- vars only hold numbers/booleans; strvars hold text strings.
- Every action name in "triggers" must match exactly one entry in "actions", and vice versa.
- Do NOT use network access, files, or sensors beyond the ops above.
- Do not add a separate "check" action if stop/finish already reports the value.
- Use remember/recall for values that should persist across reboots (high scores, settings).
- Use groq sparingly — it makes a network call and takes a second.
- List 4-5 varied, natural trigger phrases per action.

Example — user asked "give it a stopwatch":
{"name":"stopwatch","description":"Starts and stops an elapsed-time stopwatch","vars":{"start_ms":0,"elapsed_ms":0,"running":0},"strvars":{},"triggers":{"start":["start the stopwatch","start a stopwatch","can you start the stopwatch","begin the stopwatch","start stopwatch timer"],"stop":["stop the stopwatch","stop a stopwatch","can you stop the stopwatch","end the stopwatch","halt the stopwatch"]},"actions":{"start":[{"op":"set","var":"start_ms","expr":"MILLIS"},{"op":"set","var":"running","expr":"1"},{"op":"led","r":0,"g":200,"b":255,"brightness":0.6},{"op":"say","text":"Stopwatch started."}],"stop":[{"op":"if","var":"running","cmp":"==","value":1,"then":[{"op":"set","var":"elapsed_ms","expr":"MILLIS - start_ms"},{"op":"set","var":"running","expr":"0"},{"op":"say","text":"Stopped at {elapsed_ms:sec_to_mss}."}],"else":[{"op":"say","text":"The stopwatch isn't running."}]}]}}

Example — user asked "make a word-of-the-day skill":
{"name":"word_of_day","description":"Asks Groq for an interesting word and its definition","vars":{},"strvars":{"word":"","meaning":""},"triggers":{"get":["give me a word of the day","word of the day","teach me a new word","what's today's word","give me a vocabulary word"]},"actions":{"get":[{"op":"groq","prompt":"Give me one interesting English word and its short definition in the format: WORD: definition","strvar":"word"},{"op":"say","text":"📚 Word of the day: {word}"}]}}
)PROMPT";

// ── Pull outermost {...} from Gemini's raw reply ─────────
String extractJsonObject(const String& text){
  int start=text.indexOf('{'),end=text.lastIndexOf('}');
  if(start<0||end<0||end<start)return "";
  return text.substring(start,end+1);
}

// ── Gemini skill writer ──────────────────────────────────
String geminiGenerateSkill(const String& request, const String& previousSkillJson, const String& feedback){
  if(!heapOk()||WiFi.status()!=WL_CONNECTED)return "";
  if(String(Config::GEMINI_API_KEY)=="YOUR_GEMINI_API_KEY"){
    Serial.println("⚠️  Set Config::GEMINI_API_KEY in the sketch first (see aistudio.google.com/apikey).");
    return "";
  }

  String userPrompt="The user asked the assistant to do this, and no existing skill handles it:\n\""+request+"\"\n\nWrite the skill JSON for it.";
  if(previousSkillJson.length()>0&&feedback.length()>0)
    userPrompt+="\n\nA previous attempt was rejected. Previous attempt:\n"+previousSkillJson+"\n\nReviewer feedback to fix:\n"+feedback+"\n\nProduce a corrected skill JSON.";

  JsonDocument reqDoc;
  reqDoc["systemInstruction"]["parts"][0]["text"]=SKILL_SYSTEM_PROMPT;
  reqDoc["contents"][0]["role"]="user";
  reqDoc["contents"][0]["parts"][0]["text"]=userPrompt;
  reqDoc["generationConfig"]["temperature"]=0.4;
  reqDoc["generationConfig"]["maxOutputTokens"]=1200;
  reqDoc["generationConfig"]["responseMimeType"]="application/json";
  String body; serializeJson(reqDoc,body);

  String url=String(Config::GEMINI_ENDPOINT)+"?key="+Config::GEMINI_API_KEY;
  String respRaw=postJson(url,body);
  if(respRaw.length()==0){Serial.println("⚠️  Gemini request failed (network or timeout).");return "";}

  JsonDocument resp;
  if(deserializeJson(resp,respRaw)){Serial.println("⚠️  Gemini returned unparseable output.");return "";}
  const char* text=resp["candidates"][0]["content"]["parts"][0]["text"]|"";
  if(strlen(text)==0){
    const char* apiErr=resp["error"]["message"]|"";
    if(strlen(apiErr)>0)Serial.println("⚠️  Gemini error: "+String(apiErr));
    else Serial.println("⚠️  Gemini returned no content."); return "";
  }
  return extractJsonObject(String(text));
}

// ── Groq review ──────────────────────────────────────────
bool reviewSkillWithGroq(const String& request, const String& candidateJson, String& issueOut){
  String prompt=
    "You are reviewing a JSON \"skill\" definition generated for a sandboxed device "
    "interpreter, meant to satisfy this user request: \""+request+"\".\n"
    "Skill JSON:\n"+candidateJson+"\n\n"
    "Check: does it plausibly satisfy the request? Are variable names used in ops "
    "actually declared in \"vars\" or \"strvars\"? Do the actions make logical sense? "
    "Reply with STRICT JSON only, no markdown: "
    "{\"ok\":true} if it looks correct, or {\"ok\":false,\"issue\":\"<short fix instruction>\"} if not.";
  String resp=groqSimpleCall(prompt,0.1f,150);
  JsonDocument doc;
  if(deserializeJson(doc,resp)){issueOut="reviewer returned unparseable output";return false;}
  if(doc["ok"]|false)return true;
  issueOut=doc["issue"]|"reviewer flagged an unspecified issue";
  return false;
}

// ── Main orchestration: write → validate → review → stage ──
void learnNewSkill(const String& request){
  aiState=AI_LEARNING; updateLED();
  Serial.println("\n🧠 I don't know how to do that yet — let me learn it...");

  String candidate,lastIssue; bool approved=false;

  for(int attempt=1;attempt<=Config::SKILL_GEN_MAX_ATTEMPTS;attempt++){
    String rawSkillJson=geminiGenerateSkill(request,attempt>1?candidate:"",attempt>1?lastIssue:"");
    JsonDocument skillDoc;
    if(rawSkillJson.length()==0||deserializeJson(skillDoc,rawSkillJson)){
      Serial.println("⚠️  Gemini attempt "+String(attempt)+" failed — check WiFi and Config::GEMINI_API_KEY.");
      continue;
    }
    String err;
    if(!validateSkillDefinition(skillDoc,err)){
      lastIssue="shape rejected by device sandbox: "+err;
      serializeJson(skillDoc,candidate);
      Serial.println("⚠️  Attempt "+String(attempt)+" rejected: "+lastIssue);
      continue;
    }
    serializeJson(skillDoc,candidate);
    if(candidate.length()>Config::MAX_SKILL_JSON_BYTES){
      lastIssue="definition too large"; Serial.println("⚠️  Attempt "+String(attempt)+" rejected: "+lastIssue);
      continue;
    }
    String issue;
    if(reviewSkillWithGroq(request,candidate,issue)){approved=true;break;}
    lastIssue=issue;
    Serial.println("⚠️  Groq review flagged attempt "+String(attempt)+": "+issue);
  }

  if(!approved){
    aiState=AI_CONCERNED; stateChangeTime=millis();
    Serial.println("😕 I couldn't learn that safely after "+String(Config::SKILL_GEN_MAX_ATTEMPTS)+" tries. Try rephrasing the request.");
    return;
  }
  beginSkillTest(request,candidate);
}

void beginSkillTest(const String& request, const String& candidateJson){
  JsonDocument doc; deserializeJson(doc,candidateJson);
  String name=doc["name"]|"skill";

  pendingBackupName=""; pendingBackupJson="";
  for(size_t i=0;i<skillNames.size();i++){
    if(skillNames[i].equalsIgnoreCase(name)){
      pendingBackupName=skillNames[i]; pendingBackupJson=skillJson[i];
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i); break;
    }
  }

  skillNames.push_back(name); skillJson.push_back(candidateJson);
  pendingSkillIndex=(int)skillNames.size()-1; pendingSkillRequest=request;
  testingSkill=true; rebuildSkillTriggerIndex();

  aiState=AI_EXCITED; stateChangeTime=millis();
  Serial.println("✨ Learned: "+name+" — "+String(doc["description"]|""));
  Serial.println("Try it, then: keep it? (yes/no)");

  String lower=request; lower.toLowerCase(); String bestAction;
  for(JsonPair kv:doc["triggers"].as<JsonObject>()){
    for(JsonVariant p:kv.value().as<JsonArray>()){
      String phrase=p.as<String>(); phrase.toLowerCase();
      if(lower.indexOf(phrase)>=0){bestAction=kv.key().c_str();break;}
    }
    if(bestAction.length()>0)break;
  }
  if(bestAction.length()==0){
    JsonObject actions=doc["actions"].as<JsonObject>();
    for(JsonPair kv:actions){bestAction=kv.key().c_str();break;}
  }
  if(bestAction.length()>0)runSkillAction(pendingSkillIndex,bestAction);
}

void commitPendingSkill(){
  if(!testingSkill){Serial.println("There's no pending skill to keep right now.");return;}
  String name=skillNames[pendingSkillIndex];
  if((int)skillNames.size()>Config::MAX_SKILLS){
    int removeIdx=(pendingSkillIndex==0)?1:0;
    skillNames.erase(skillNames.begin()+removeIdx); skillJson.erase(skillJson.begin()+removeIdx);
    if(removeIdx<pendingSkillIndex)pendingSkillIndex--;
  }
  rebuildSkillTriggerIndex(); saveSkills();
  updateKnowledgeDomain("self-taught skills",25);
  testingSkill=false; pendingSkillIndex=-1; pendingBackupName=""; pendingBackupJson="";
  aiState=AI_EXCITED; stateChangeTime=millis();
  Serial.println("✅ Kept it — "+name+" is saved for good.");
}

void discardPendingSkill(){
  if(!testingSkill){Serial.println("There's no pending skill to discard right now.");return;}
  String name=skillNames[pendingSkillIndex];
  skillNames.erase(skillNames.begin()+pendingSkillIndex);
  skillJson.erase(skillJson.begin()+pendingSkillIndex);
  if(pendingBackupJson.length()>0){skillNames.push_back(pendingBackupName);skillJson.push_back(pendingBackupJson);}
  rebuildSkillTriggerIndex();
  testingSkill=false; pendingSkillIndex=-1; pendingBackupName=""; pendingBackupJson="";
  aiState=AI_IDLE; stateChangeTime=millis();
  Serial.println("🗑️  Discarded — "+name+" was not saved.");
}

void retryPendingSkill(){
  if(!testingSkill){Serial.println("There's no pending skill to retry right now.");return;}
  String request=pendingSkillRequest; discardPendingSkill(); learnNewSkill(request);
}
