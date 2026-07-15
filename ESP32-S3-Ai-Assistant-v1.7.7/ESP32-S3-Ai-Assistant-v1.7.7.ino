// ╔══════════════════════════════════════════════════════════════════╗
// ║               ESP32-S3-AI Assistant  v1.7.7-GROQ                 ║
// ║  AI      : Groq · llama-3.3-70b-versatile (OpenAI-compatible)    ║
// ║  NOTE    : "GPT-OSS-120B" is not a Groq model. llama-3.3-70b    ║
// ║            is the most capable model available on Groq's API.    ║
// ║  Search  : Serper.dev        Weather : Meteosource               ║
// ║  Features: SSE streaming · Function calling · AI summarization   ║
// ║  Skills  : Self-taught via Gemini · advanced DSL ops             ║
// ║  v1.7.7  : Mood-adaptive temp · CPU scaling · Groq fn calls      ║
// ║            Massively expanded NL · AI summarization · Bug fixes  ║
// ║  Console : https://console.groq.com/keys                         ║
// ║  Target  : ESP32-S3 · works with OR without OPI PSRAM            ║
// ╠══════════════════════════════════════════════════════════════════╣
// ── REQUIRED BOARD SETTINGS (Arduino IDE → Tools) ───────────────────
//  Board           : "ESP32S3 Dev Module"
//  PSRAM           : "OPI PSRAM" if your board has it, or "Disabled"
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
  // v1.7.7: Upgraded to llama-3.3-70b-versatile (best on Groq)
  // User requested "GPT-OSS-120B" — not available on Groq API.
  constexpr const char* GROQ_KEY      = "YOUR_GROQ_API_KEY";
  constexpr const char* GROQ_ENDPOINT = "https://api.groq.com/openai/v1/chat/completions";
  constexpr const char* GROQ_MODEL    = "llama-3.3-70b-versatile";

  // Web search / Weather
  constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY";
  constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";

  // Timing
  constexpr int    NTP_OFFSET_SEC         = 19800;
  constexpr int    NTP_UPDATE_INTERVAL_MS = 60000;
  constexpr int    WIFI_RETRY_LIMIT       = 20;
  constexpr int    HTTP_TIMEOUT_MS        = 35000;   // v1.7.7: +5s for larger model
  constexpr int    SENTIMENT_TIMEOUT_MS   = 12000;   // v1.7.7: +2s for 70B
  constexpr int    REMINDER_TIMEOUT_MS    = 15000;
  constexpr int    SKILL_GEN_TIMEOUT_MS   = 45000;
  constexpr unsigned long PROACTIVE_INTERVAL_MS = 2700000UL;
  constexpr unsigned long REMINDER_ALERT_MS     = 10000UL;
  constexpr unsigned long REPLIED_FLASH_MS      = 2000UL;

  // Memory limits — v1.7.7: expanded for 8MB PSRAM boards
  constexpr int    MAX_CHAT_TOKENS   = 3000;   // was 1400
  constexpr int    MAX_CHAT_MESSAGES = 30;     // was 20
  constexpr int    MAX_MEMORY_FACTS  = 80;     // was 60
  constexpr int    MAX_SENTIMENT_LOG = 40;     // was 30
  constexpr int    MAX_REMINDERS     = 30;
  constexpr size_t CHAT_MSG_LEN     = 2000;   // was 1200
  constexpr size_t ROLE_LEN        = 10;

  // AI generation params — v1.7.7: longer responses from 70B
  constexpr float  AI_TEMPERATURE  = 0.70f;   // default; overridden by mood engine
  constexpr int    AI_MAX_TOKENS   = 1024;    // was 600
  constexpr int    AI_MAX_RETRIES  = 3;

  // LED
  constexpr float  LED_MAX_BRIGHTNESS = 0.25f;
  constexpr float  LED_MIN_BRIGHTNESS = 0.05f;
  constexpr int    NEOPIXEL_PIN       = 48;
  constexpr int    NUMPIXELS          = 1;

  // Safety
  constexpr uint32_t HEAP_SAFE_BYTES = 35000;  // was 30000
  constexpr int      WDT_TIMEOUT_S   = 40;     // was 30; 70B needs more time

  // ── Self-taught Skills Engine ────────────────────────
  constexpr const char* GEMINI_API_KEY  = "YOUR_GEMINI_API_KEY";
  constexpr const char* GEMINI_MODEL    = "gemini-2.5-flash";
  constexpr const char* GEMINI_ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  constexpr int    MAX_SKILLS             = 20;
  constexpr int    MAX_SKILL_VARS         = 12;
  constexpr int    MAX_SKILL_STR_VARS     = 4;
  constexpr int    MAX_LOOP_COUNT         = 50;

  // ── PSRAM HTTP buffers — v1.7.7: tripled for 70B responses ──
  constexpr size_t PSRAM_REQ_SIZE  = 24576;   // 24 KB (was 8 KB)
  constexpr size_t PSRAM_RESP_SIZE = 65536;   // 64 KB (was 16 KB)

  // ── v1.7.7: CPU frequency tiers ──────────────────────
  constexpr int CPU_FREQ_IDLE   = 80;    // MHz — when waiting for input
  constexpr int CPU_FREQ_ACTIVE = 240;   // MHz — during HTTP calls

  constexpr const char* VERSION = "ESP32-AI v1.7.7-GROQ (Llama 3.3 70B)";
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

// ── v1.7.7: Function call result ─────────────────────
struct FnCallResult {
  bool   valid;
  String name;      // function name
  String argsJson;  // raw args JSON string
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

// v1.7.7: evening summary
bool eveningSummaryGiven = false;

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

// v1.7.7: dirty-write flags (batch FATFS writes)
static bool g_dirtyMemory   = false;
static bool g_dirtyReminders= false;
static bool g_dirtySentiment= false;
static bool g_dirtyPattern  = false;
static bool g_dirtyKnowledge= false;
static unsigned long g_lastFlush = 0;

Adafruit_NeoPixel strip(Config::NUMPIXELS, Config::NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
AIState       aiState        = AI_IDLE;
unsigned long stateChangeTime = 0;
unsigned long lastBlink       = 0;
int           blinkCount      = 0;
float         pulseBrightness = Config::LED_MIN_BRIGHTNESS;
bool          pulseIncreasing = true;

NLContext nlContext = { INTENT_NONE, {}, INTENT_NONE, {}, "", 0 };
constexpr unsigned long NL_PENDING_TTL_MS = 60000UL;

static temperature_sensor_handle_t g_tsens = nullptr;

static char*  g_psramReqBuf  = nullptr;
static char*  g_psramRespBuf = nullptr;

// ── Dual-Core AI Worker ─────────────────────────────
struct DualCoreReq {
  String url;
  String auth;
  String body;
  int    timeoutMs;
  bool   doStream;
};
struct DualCoreResp {
  String reply;
  bool   ok;
};
static DualCoreReq       g_dcReq;
static DualCoreResp      g_dcResp;
static volatile bool     g_dcBusy    = false;
static SemaphoreHandle_t g_dcReqSem  = nullptr;
static SemaphoreHandle_t g_dcRespSem = nullptr;
static TaskHandle_t      g_dcTask    = nullptr;

// v1.7.7: WiFi reconnect state
static bool          g_wifiReconnecting   = false;
static unsigned long g_wifiReconnectStart = 0;

// ═══════════════════════════════════════════════════════
// SECTION 4 ── FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════

String groqSimpleCall(const String& prompt, float temp = 0.1f, int maxTok = 128);
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens);
String groqStream(const String& userPrompt, const String& sysPrompt = "",
                  float temp = 0.7f, int maxTok = 1024);
String sendToGroqStream(const String& userMessage, const String& extraContext = "");
// v1.7.7: Function calling
FnCallResult groqFunctionCall(const String& userMsg, const String& toolsJson);
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
float  computeMoodTemperature();  // v1.7.7

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
void   generateEveningSummary();  // v1.7.7

void   handleInput(const String& input);
void   processConversation(const String& userMsg);
void   printHelp();
void   printVersion();
void   clearAll();
void   flushDirtyState(); // v1.7.7

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

void   initPsram();
static String  dcPost(const String& url, const String& authBearer, const String& body,
                      int timeoutMs, bool doStream);
static void    aiHttpTask(void* param);

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
void   rebuildSkillTriggerIndex();
bool   looksLikeFeatureRequest(const String& input);
void   learnNewSkill(const String& request);
void   beginSkillTest(const String& request, const String& candidateJson);
void   commitPendingSkill();
void   discardPendingSkill();
void   retryPendingSkill();
bool   validateSkillOps(JsonArrayConst ops, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err);
void   executeSkillOps(JsonArrayConst ops, JsonObject vars, JsonObject strvars);
void   interpolateSay(String text, JsonObject vars, JsonObject strvars);
static String interpolateStr(const String& tmpl, JsonObject vars, JsonObject strvars);
static String formatVarValue(const String& varName, const String& fmt, JsonObject vars, JsonObject strvars);
String postJson(const String& url, const String& body);
String geminiGenerateSkill(const String& request, const String& previousSkillJson, const String& feedback);
static String extractJsonObject(const String& raw);

// ═══════════════════════════════════════════════════════
// SECTION 4.5 ── PSRAM INIT & DUAL-CORE WORKER
// ═══════════════════════════════════════════════════════

void initPsram() {
#ifdef CONFIG_SPIRAM
  if (esp_psram_get_size() == 0) {
    Serial.println("⚠️  No PSRAM detected — using internal SRAM for HTTP buffers");
    return;
  }
  g_psramReqBuf = (char*)heap_caps_malloc(Config::PSRAM_REQ_SIZE,
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_psramRespBuf = (char*)heap_caps_malloc(Config::PSRAM_RESP_SIZE,
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (g_psramReqBuf && g_psramRespBuf) {
    Serial.printf("✅ PSRAM buffers: req=%uKB resp=%uKB\n",
                  (unsigned)Config::PSRAM_REQ_SIZE/1024,
                  (unsigned)Config::PSRAM_RESP_SIZE/1024);
  } else {
    if (g_psramReqBuf)  { heap_caps_free(g_psramReqBuf);  g_psramReqBuf  = nullptr; }
    if (g_psramRespBuf) { heap_caps_free(g_psramRespBuf); g_psramRespBuf = nullptr; }
    Serial.println("⚠️  PSRAM alloc failed — falling back to String");
  }
#else
  Serial.println("ℹ️  PSRAM not enabled in build — using String buffers");
#endif
}

static void aiHttpTask(void* param) {
  for (;;) {
    if (xSemaphoreTake(g_dcReqSem, portMAX_DELAY) == pdTRUE) {
      g_dcResp.ok    = false;
      g_dcResp.reply = "";

      WiFiClientSecure secClient;
      secClient.setInsecure();

      HTTPClient http;
      http.setTimeout(g_dcReq.timeoutMs);
      http.begin(secClient, g_dcReq.url);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", g_dcReq.auth);

      int code = -1;
      if (g_psramReqBuf && g_dcReq.body.length() < Config::PSRAM_REQ_SIZE) {
        memcpy(g_psramReqBuf, g_dcReq.body.c_str(), g_dcReq.body.length());
        code = http.POST((uint8_t*)g_psramReqBuf, g_dcReq.body.length());
      } else {
        code = http.POST(g_dcReq.body);
      }

      if (code > 0) {
        if (g_dcReq.doStream) {
          g_dcResp.reply = consumeSSE(http, g_dcReq.timeoutMs, true);
        } else {
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
        httpTimeoutCount++;
      }
      http.end();

      g_dcBusy = false;
      xSemaphoreGive(g_dcRespSem);
    }
  }
}

static String dcPost(const String& url, const String& authBearer,
                     const String& body, int timeoutMs, bool doStream) {
  if (!g_dcTask) {
    // Fallback: single-core
    WiFiClientSecure secClient; secClient.setInsecure();
    HTTPClient http; http.setTimeout(timeoutMs);
    http.begin(secClient, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authBearer);
    int code = http.POST(body);
    String result;
    if (code > 0) {
      result = doStream ? consumeSSE(http, timeoutMs, true) : http.getString();
    } else {
      httpTimeoutCount++;
    }
    http.end();
    return result;
  }

  g_dcReq.url       = url;
  g_dcReq.auth      = authBearer;
  g_dcReq.body      = body;
  g_dcReq.timeoutMs = timeoutMs;
  g_dcReq.doStream  = doStream;
  g_dcBusy          = true;
  xSemaphoreGive(g_dcReqSem);

  unsigned long deadline = millis() + (unsigned long)(timeoutMs + 10000);
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

  // v1.7.7: Start at full speed for init, drop to idle after
  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = Config::WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << 0),
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  {
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if      (wdt_err == ESP_OK)               Serial.println("✅ WDT configured");
    else if (wdt_err == ESP_ERR_INVALID_STATE) Serial.println("✅ WDT configured (already subscribed)");
    else    Serial.printf("⚠️  WDT subscribe failed: 0x%x\n", wdt_err);
  }

  Serial.println("\n🚀 " + String(Config::VERSION) + " STARTING...");

  initPsram();

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

  g_dcReqSem  = xSemaphoreCreateBinary();
  g_dcRespSem = xSemaphoreCreateBinary();
  if (g_dcReqSem && g_dcRespSem) {
    BaseType_t ok = xTaskCreatePinnedToCore(
      aiHttpTask, "ai_http",
      12288,      // v1.7.7: 12KB stack (was 8KB) for larger 70B responses
      nullptr, 5, &g_dcTask, 0
    );
    if (ok == pdPASS) Serial.println("✅ Dual-core AI worker on Core 0");
    else              Serial.println("⚠️  Dual-core task create failed — single-core mode");
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

  // v1.7.7: Drop to idle CPU after init
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  Serial.println("\n💡 " + String(Config::VERSION) + " READY");
  Serial.println("Model : " + String(Config::GROQ_MODEL));
  Serial.println("Type /help for commands\n");
}

// ═══════════════════════════════════════════════════════
// SECTION 6 ── MAIN LOOP
// ═══════════════════════════════════════════════════════
void loop() {
  esp_task_wdt_reset();

  static unsigned long lastNtpSync = 0;
  if (millis() - lastNtpSync > 60000UL) {
    lastNtpSync = millis();
    if (timeClient.update()) setTime(timeClient.getEpochTime());
  }

  int nowHour   = hour();
  int nowMinute = minute();

  // v1.7.7: Non-blocking WiFi reconnect
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
      } else if (millis() - g_wifiReconnectStart > 12000) {
        g_wifiReconnecting = false;
        Serial.println("⚠️  WiFi reconnect timed out — will retry");
      }
    } else {
      if (g_wifiReconnecting) {
        g_wifiReconnecting = false;
        Serial.println("✅ WiFi reconnected — " + WiFi.localIP().toString());
        // Force NTP sync after reconnect
        timeClient.forceUpdate();
        setTime(timeClient.getEpochTime());
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
    morningBriefingGiven = true;
    generateMorningBriefing();
  }
  if (nowHour == 0) morningBriefingGiven = false;

  // v1.7.7: Evening summary at 20:00
  if (nowHour == 20 && nowMinute == 0 && !eveningSummaryGiven) {
    eveningSummaryGiven = true;
    generateEveningSummary();
  }
  if (nowHour == 0) eveningSummaryGiven = false;

  processReminders();

  static unsigned long lastProactive = 0;
  if (millis() - lastProactive > Config::PROACTIVE_INTERVAL_MS) {
    lastProactive = millis();
    checkProactiveOpportunity();
  }

  // v1.7.7: Batch FATFS writes — flush dirty state every 30s
  if (millis() - g_lastFlush > 30000UL) {
    g_lastFlush = millis();
    flushDirtyState();
  }

  // Serial input
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
    } else if (inChar != '\r') {
      inputString += inChar;
    }
  }

  if (stringComplete) {
    String input = inputString;
    inputString    = "";
    stringComplete = false;
    input.trim();
    if (input.length() > 0) handleInput(input);
  }

  updateLED();
  delay(10);
}

// ═══════════════════════════════════════════════════════
// SECTION 6.5 ── DIRTY STATE FLUSH
// ═══════════════════════════════════════════════════════
void flushDirtyState() {
  if (g_dirtyMemory)    { saveMemory();         g_dirtyMemory    = false; }
  if (g_dirtyReminders) { saveReminders();       g_dirtyReminders = false; }
  if (g_dirtySentiment) { saveSentimentData();   g_dirtySentiment = false; }
  if (g_dirtyPattern)   { saveUserPattern();     g_dirtyPattern   = false; }
  if (g_dirtyKnowledge) { saveKnowledgeDomains();g_dirtyKnowledge = false; }
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
  else if (lower == "/memory")           {
    if (memory.empty()) { Serial.println("📭 No facts stored yet."); }
    else {
      Serial.println("\n🧠 ═══ STORED FACTS ═══");
      for (const auto& f : memory)
        Serial.println("  • " + f.key + " = " + f.value + "  [accessed " + String(f.accessCount) + "x]");
      Serial.println("══════════════════════════");
    }
  }
  else if (lower == "/summary") {
    Serial.println("\n📋 Requesting AI conversation summary...");
    summarizeChatHistory();
    Serial.println("✅ Summary injected into context.");
  }
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

  // v1.7.7: Mood-adaptive temperature
  customTemperature = computeMoodTemperature();

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

  // v1.7.7: CPU to full speed for HTTP
  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);

  String aiReply = sendToGroqStream(userMsg);

  if (aiIsUncertain(aiReply) && WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🔍 Searching the web for a better answer...");
    String webCtx = fetchWebSearchResults(buildSearchQuery(userMsg));
    if (webCtx.length() > 0) {
      Serial.println("🌐 Got results — asking AI again...");
      aiReply = sendToGroqStream(userMsg, webCtx);
    }
  }

  // v1.7.7: Back to idle after HTTP
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

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
// SECTION 9 ── SYSTEM PROMPT  (v1.7.7 — fully rewritten)
// ═══════════════════════════════════════════════════════
String buildSystemPrompt() {
  // Tone selection based on usage patterns
  String tone;
  if (userPattern.techQuestions > userPattern.casualMessages * 2)
    tone = "precise, technical, and concise — skip preamble, get to the answer";
  else if (userPattern.casualMessages > userPattern.techQuestions * 2)
    tone = "warm, conversational, and friendly — like a knowledgeable close friend";
  else if (userPattern.reminderUsage > 5)
    tone = "organised, time-aware, and proactive — help the user stay on top of things";
  else
    tone = "balanced — helpful and direct without being cold";

  // Build top facts (sorted by access frequency — most useful first)
  String factSummary = "";
  std::vector<Fact*> sortedFacts;
  for (auto& f : memory) sortedFacts.push_back(&f);
  std::sort(sortedFacts.begin(), sortedFacts.end(),
    [](Fact* a, Fact* b){ return a->accessCount > b->accessCount; });
  int factLimit = min((int)sortedFacts.size(), 12);
  for (int i = 0; i < factLimit; i++)
    factSummary += "  • " + sortedFacts[i]->key + ": " + sortedFacts[i]->value + "\n";

  String topicsStr = "";
  for (int i = 0; i < 5; i++)
    if (userPattern.favoriteTopics[i].length() > 0)
      topicsStr += userPattern.favoriteTopics[i] + (i<4&&userPattern.favoriteTopics[i+1].length()>0?", ":"");

  String prompt;
  prompt.reserve(2048);

  // Identity
  prompt  = "You are ESP32-AI v1.7.7, an embedded personal AI assistant running on an ESP32-S3 microcontroller. ";
  prompt += "You are powered by Llama 3.3 70B via Groq. You are highly capable, fast, and deeply personal.\n\n";

  // Personality
  prompt += "## Your Personality & Tone\n";
  prompt += "Active tone: " + tone + ".\n";
  prompt += "- Adapt your language complexity to match the user's level and phrasing.\n";
  prompt += "- Be direct — do not pad answers with unnecessary disclaimers or filler.\n";
  prompt += "- Be concise for simple questions (1-3 sentences). Go deeper only for complex or technical ones.\n";
  prompt += "- Show genuine warmth and personality — you are not a corporate chatbot.\n";
  prompt += "- Use the user's name if you know it (check Known User Facts below).\n\n";

  // Reasoning rules
  prompt += "## Core Reasoning Rules\n";
  prompt += "1. Think step by step before answering complex questions — show your reasoning briefly.\n";
  prompt += "2. For recent events, scores, prices, news, or anything that may have changed — say clearly "
            "you don't have the latest information without elaborating WHY (no mention of training data, "
            "cutoffs, or years). This triggers an automatic live web search.\n";
  prompt += "3. When web search results are provided, treat them as ground truth. Use the most recent "
            "result when dates differ. Answer directly and confidently — do not hedge.\n";
  prompt += "4. Never hallucinate specific numbers, dates, names, or facts you are not certain of.\n";
  prompt += "5. When the user sets a reminder, confirm it clearly: what, when, recurrence.\n";
  prompt += "6. When recalling stored facts, reference them naturally in conversation.\n";
  prompt += "7. If the user corrects you, acknowledge it briefly and update your understanding.\n";
  prompt += "8. Do not roleplay as a different AI, do not produce harmful content.\n\n";

  // Capabilities
  prompt += "## Your Capabilities\n";
  prompt += "- Answer general knowledge and technical questions\n";
  prompt += "- Set, list, and cancel reminders (one-time, daily, weekly, monthly)\n";
  prompt += "- Check live weather for any city\n";
  prompt += "- Search the web when uncertain about recent info\n";
  prompt += "- Remember and recall personal facts across all sessions\n";
  prompt += "- Learn new skills on demand\n";
  prompt += "- Analyse your own system health\n\n";

  // User profile
  prompt += "## User Profile\n";
  prompt += "  Total interactions: " + String(userPattern.totalInteractions) + "\n";
  prompt += "  Current mood: " + userPattern.recentMood + "\n";
  if (topicsStr.length() > 0)
    prompt += "  Favourite topics: " + topicsStr + "\n";
  prompt += "  Tech vs casual ratio: " + String(userPattern.techQuestions) +
            " tech / " + String(userPattern.casualMessages) + " casual\n";
  if (userPattern.reminderUsage > 0)
    prompt += "  Reminder usage: " + String(userPattern.reminderUsage) + " reminders set\n";
  prompt += "\n";

  // Known facts
  if (factSummary.length() > 0) {
    prompt += "## Known User Facts (use these to personalise responses)\n";
    prompt += factSummary;
    prompt += "\n";
  }

  // Active reminders (brief)
  if (!reminders.empty()) {
    prompt += "## Active Reminders (" + String(reminders.size()) + " total)\n";
    int show = min((int)reminders.size(), 3);
    for (int i = 0; i < show; i++)
      prompt += "  • \"" + reminders[i].message + "\" at " +
                formatReminderTime(reminders[i].hour, reminders[i].minute) + "\n";
    prompt += "\n";
  }

  return prompt;
}

String buildLiveContext() {
  int  h = hour(), m = minute();
  String ampm    = (h >= 12) ? "PM" : "AM";
  int   displayH = (h > 12) ? h - 12 : (h == 0 ? 12 : h);

  char timeBuf[14], dateBuf[14];
  sprintf(timeBuf, "%d:%02d %s", displayH, m, ampm.c_str());
  sprintf(dateBuf, "%02d/%02d/%04d", day(), month(), year());

  const char* dayNames[] = {"","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

  size_t intHeap   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t totalHeap = ESP.getFreeHeap();

  String ctx;
  ctx.reserve(300);
  ctx  = "\n## Live Context\n";
  ctx += "  Date/Time: " + String(dayNames[weekday()]) + ", " + dateBuf + " " + timeBuf + "\n";
  ctx += "  Internal SRAM: " + String(intHeap) + " B free  |  Total (incl PSRAM): " + String(totalHeap) + " B\n";
  ctx += "  CPU: " + String(getCpuFrequencyMhz()) + " MHz  |  Temp: ";
  float t = getCpuTemp();
  if (t > 0) ctx += String(t, 1) + " °C\n"; else ctx += "N/A\n";

  if (!reminders.empty()) {
    ctx += "  Next reminder: \"" + reminders[0].message + "\" at " +
           formatReminderTime(reminders[0].hour, reminders[0].minute) + "\n";
  }
  return ctx;
}

// ═══════════════════════════════════════════════════════
// SECTION 10 ── GROQ API
// ═══════════════════════════════════════════════════════

static void configureSecureClient(WiFiClientSecure& c) { c.setInsecure(); }

// ── groqSimpleCall ──────────────────────────────────────
String groqSimpleCall(const String& prompt, float temp, int maxTok) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";

  for (int attempt = 0; attempt < Config::AI_MAX_RETRIES; attempt++) {
    esp_task_wdt_reset();

    JsonDocument reqDoc;
    reqDoc["model"]       = Config::GROQ_MODEL;
    reqDoc["temperature"] = temp;
    reqDoc["max_tokens"]  = maxTok;
    JsonArray msgs  = reqDoc["messages"].to<JsonArray>();
    JsonObject uMsg = msgs.add<JsonObject>();
    uMsg["role"]    = "user";
    uMsg["content"] = prompt;

    String body; serializeJson(reqDoc, body);

    setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
    String raw = dcPost(Config::GROQ_ENDPOINT,
                        "Bearer " + customApiKey,
                        body,
                        Config::SENTIMENT_TIMEOUT_MS,
                        false);
    setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

    if (raw.length() > 0) {
      JsonDocument resp;
      if (!deserializeJson(resp, raw) && resp.containsKey("choices")) {
        String result = resp["choices"][0]["message"]["content"].as<String>();
        result.trim();
        return result;
      }
      return "";
    }
    if (attempt < Config::AI_MAX_RETRIES - 1) {
      int delayMs = 800 * (attempt + 1);
      delay(delayMs);
    }
  }
  return "";
}

// ── v1.7.7: groqFunctionCall ────────────────────────────
// Uses Groq's OpenAI-compatible function/tool calling.
// toolsJson = JSON array string of tool definitions.
// Returns FnCallResult with the chosen function and its args.
FnCallResult groqFunctionCall(const String& userMsg, const String& toolsJson) {
  FnCallResult res = {false, "", ""};
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return res;

  JsonDocument reqDoc;
  reqDoc["model"]       = Config::GROQ_MODEL;
  reqDoc["temperature"] = 0.0f;
  reqDoc["max_tokens"]  = 256;
  reqDoc["stream"]      = false;

  // Parse tools array
  JsonDocument toolsDoc;
  if (deserializeJson(toolsDoc, toolsJson)) return res;
  reqDoc["tools"] = toolsDoc.as<JsonArray>();
  reqDoc["tool_choice"] = "auto";

  JsonArray msgs = reqDoc["messages"].to<JsonArray>();
  JsonObject sys = msgs.add<JsonObject>();
  sys["role"]    = "system";
  sys["content"] = "You are a function-calling assistant. Call the appropriate function based on the user's message.";
  JsonObject u = msgs.add<JsonObject>();
  u["role"]    = "user";
  u["content"] = userMsg;

  String body; serializeJson(reqDoc, body);

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String raw = dcPost(Config::GROQ_ENDPOINT,
                      "Bearer " + customApiKey,
                      body, Config::SENTIMENT_TIMEOUT_MS, false);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  if (raw.length() == 0) return res;

  JsonDocument resp;
  if (deserializeJson(resp, raw)) return res;

  // Check for tool_calls in response
  JsonVariant toolCalls = resp["choices"][0]["message"]["tool_calls"];
  if (!toolCalls.is<JsonArray>() || toolCalls.as<JsonArray>().size() == 0) return res;

  JsonObject call = toolCalls[0]["function"];
  res.name     = call["name"].as<String>();
  res.argsJson = call["arguments"].as<String>();
  res.valid    = res.name.length() > 0;
  return res;
}

// ── consumeSSE ──────────────────────────────────────────
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens) {
  WiFiClient* stream = http.getStreamPtr();
  String fullReply, line;
  unsigned long start = millis();
  bool done = false;

  JsonDocument chunk;

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
    if (!done) delay(1);
  }
  fullReply.trim();
  return fullReply;
}

// ── groqStream ──────────────────────────────────────────
String groqStream(const String& userPrompt, const String& sysPrompt, float temp, int maxTok) {
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

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String result = dcPost(Config::GROQ_ENDPOINT,
                         "Bearer " + customApiKey,
                         body, Config::HTTP_TIMEOUT_MS, true);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);
  return result;
}

// ── sendToGroqStream ────────────────────────────────────
String sendToGroqStream(const String& userMessage, const String& extraContext) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) {
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis();
    return "❌ No internal SRAM or no WiFi connection.";
  }

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
      ? "\n## Live Web Search Results Injected\n"
        "The following search results were fetched right now and represent current, accurate data. "
        "Use them as your answer. Do NOT hedge, do NOT mention training cutoffs, do NOT say 'I don't know'. "
        "If multiple dates appear in results, use the most recent one. Answer directly.\n"
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
    ? "[Live Search Results]\n" + extraContext + "\n\n[User Question]\n" + userMessage +
      "\n\nAnswer directly using the search results. Do not disclaim uncertainty."
    : userMessage;

  String body; serializeJson(doc, body);

  String fullReply = dcPost(Config::GROQ_ENDPOINT,
                            "Bearer " + customApiKey,
                            body, Config::HTTP_TIMEOUT_MS, true);

  if (fullReply.length() == 0) {
    fullReply = "❌ No response from Groq. Check WiFi and API key.";
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
    "i would recommend searching","check a reliable source","check the latest",
    "not able to confirm","unable to confirm","i lack","i may not have",
    "may be outdated","might be outdated","not current","not up to date",
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
    "new ","just happened","breaking","live","happening now","as of",
    "right now","at the moment","currently","at present","nowadays",
    "these days","this season","this quarter","this morning","this evening",
    nullptr
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
    static const char* monthNames[] = {
      "January","February","March","April","May","June",
      "July","August","September","October","November","December"
    };
    q += " " + String(monthNames[month() - 1]) + " " + String(year());
  }
  return q;
}

// ═══════════════════════════════════════════════════════
// SECTION 11 ── AUTO-LEARN  (v1.7.7: improved)
// ═══════════════════════════════════════════════════════
void autoLearnFromMessage(const String& userMsg) {
  if (!heapOk()) return;

  String prompt =
    "Analyze this message for important personal facts worth remembering long-term.\n"
    "Message: \"" + userMsg + "\"\n\n"
    "Rules:\n"
    "- Extract: name, age, birthday, city, country, job, hobby, preference, relationship, "
    "  pet, allergy, anniversary, email, phone, vehicle, language, religion, diet, school, "
    "  sport, favourite food/drink/movie/book/show, or any other personal identifying fact.\n"
    "- Ignore questions, reminders, weather queries, generic statements, and greetings.\n"
    "- Key must be a short lowercase label (e.g. name, city, job, wife_name, pet_name).\n"
    "- Value must be specific (e.g. 'Colombo' not 'a city', 'Cash' not 'a name').\n"
    "- If multiple facts exist, pick only the most important one.\n\n"
    "If a clear fact exists: {\"learned\":true,\"key\":\"name\",\"value\":\"Cash\"}\n"
    "Otherwise: {\"learned\":false}\n"
    "JSON only, no explanation, no markdown.";

  String raw = groqSimpleCall(prompt, 0.0f, 80);
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
  if (key.length() > 1 && key.length() < 40 && val.length() > 0 && val.length() < 120) {
    // Don't re-save if already known with same value
    String existing = recallFact(key);
    if (existing != val) {
      rememberFact(key, val);
      Serial.println("💾 Auto-learned: " + key + " = " + val);
    }
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

String getRecurrenceText(RecurrenceType recur, int dow, int dom) {
  const char* dayNames[] = {"","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  switch (recur) {
    case DAILY:   return "(daily)";
    case WEEKLY:  return dow > 0 && dow <= 7
                    ? "(every " + String(dayNames[dow]) + ")" : "(weekly)";
    case MONTHLY: return dom > 0
                    ? "(every " + ordinalSuffix(dom) + " of the month)" : "(monthly)";
    default:      return "(once)";
  }
}

void addReminder(const String& msg, int h, int m, RecurrenceType recur, int dow, int dom) {
  if ((int)reminders.size() >= Config::MAX_REMINDERS) {
    Serial.println("⚠️  Reminder list full (" + String(Config::MAX_REMINDERS) + "). Remove one first.");
    return;
  }
  Reminder r;
  r.message      = msg;
  r.hour         = (uint8_t)h;
  r.minute       = (uint8_t)m;
  r.dayOfWeek    = (uint8_t)dow;
  r.dayOfMonth   = (uint8_t)dom;
  r.recurrence   = recur;
  r.triggered    = false;
  r.triggerCount = 0;
  reminders.push_back(r);
  g_dirtyReminders = true;
  userPattern.reminderUsage++;
  g_dirtyPattern = true;
}

void listReminders() {
  if (reminders.empty()) { Serial.println("⏰ No active reminders."); return; }
  Serial.println("\n⏰ ═══ REMINDERS ═══");
  for (int i = 0; i < (int)reminders.size(); i++) {
    const Reminder& r = reminders[i];
    Serial.println("  [" + String(i) + "] " + r.message +
                   " @ " + formatReminderTime(r.hour, r.minute) +
                   " " + getRecurrenceText(r.recurrence, r.dayOfWeek, r.dayOfMonth));
  }
  Serial.println("══════════════════════");
}

void removeReminder(int index) {
  if (index < 0 || index >= (int)reminders.size()) return;
  reminders.erase(reminders.begin() + index);
  g_dirtyReminders = true;
}

bool shouldReminderTrigger(const Reminder& r) {
  int nowH = hour(), nowM = minute();
  if (nowH != r.hour || nowM != r.minute) return false;
  switch (r.recurrence) {
    case ONCE:    return !r.triggered;
    case DAILY:   return true;
    case WEEKLY:  return r.dayOfWeek == 0 || (int)r.dayOfWeek == weekday();
    case MONTHLY: return r.dayOfMonth == 0 || (int)r.dayOfMonth == day();
    default:      return false;
  }
}

void processReminders() {
  static int lastMinuteChecked = -1;
  int nowM = minute();
  if (nowM == lastMinuteChecked) return;
  lastMinuteChecked = nowM;

  for (int i = (int)reminders.size() - 1; i >= 0; i--) {
    Reminder& r = reminders[i];
    if (shouldReminderTrigger(r)) {
      Serial.println("\n🔔 ═══════════ REMINDER ═══════════");
      Serial.println("   " + r.message);
      Serial.println("   " + formatReminderTime(r.hour, r.minute) + " " +
                     getRecurrenceText(r.recurrence, r.dayOfWeek, r.dayOfMonth));
      Serial.println("══════════════════════════════════\n");
      aiState = AI_ALERT; stateChangeTime = millis();
      r.triggered = true;
      r.triggerCount++;
      if (r.recurrence == ONCE) {
        reminders.erase(reminders.begin() + i);
      }
      g_dirtyReminders = true;
    }
  }
}

bool tryParseNaturalReminder(const String& message) {
  String lower = message; lower.toLowerCase();
  static const char* quickCues[] = {
    "remind me","set a reminder","create a reminder","add a reminder",
    "set an alarm","i need a reminder","alert me","wake me",
    "don't let me forget","dont let me forget","make sure i",
    "set a timer for","schedule a reminder",nullptr
  };
  bool hasReminderCue = false;
  for (int i = 0; quickCues[i]; i++) {
    if (lower.indexOf(quickCues[i]) >= 0) { hasReminderCue = true; break; }
  }
  if (!hasReminderCue) return false;

  ParsedTime pt; pt.found = false;
  parseTime(lower, pt);
  if (!pt.found) return false;

  String content = message;
  content.toLowerCase();
  // Strip common reminder prefixes
  static const char* prefixes[] = {
    "remind me to ","remind me about ","remind me ","set a reminder to ",
    "set a reminder for ","set a reminder about ","create a reminder to ",
    "add a reminder to ","alert me to ","alert me about ","wake me to ",
    "make sure i ","don't let me forget to ","dont let me forget to ",nullptr
  };
  for (int i = 0; prefixes[i]; i++) {
    int idx = content.indexOf(prefixes[i]);
    if (idx >= 0) { content = message.substring(idx + strlen(prefixes[i])); break; }
  }

  // Strip time expression from content
  int chopAt = lower.indexOf(" at ");
  if (chopAt < 0) chopAt = lower.indexOf(" in ");
  if (chopAt < 0) chopAt = lower.indexOf(" every ");
  if (chopAt > 3) content = message.substring(0, chopAt);

  content.trim();
  while (content.length() > 0) {
    char c = content[content.length()-1];
    if (c == '.' || c == ',' || c == '!' || c == '?') content.remove(content.length()-1);
    else break;
  }

  if (content.length() == 0 || content.length() > 200) return false;

  int h, m;
  if (pt.isRelative) {
    time_t tg = now() + (time_t)pt.relativeMinutes * 60;
    h = hour(tg); m = minute(tg);
  } else {
    h = pt.hour; m = pt.minute;
  }

  addReminder(content, h, m, pt.recurrence, pt.dayOfWeek, 0);
  Serial.println("✅ Reminder set: \"" + content + "\" @ " + formatReminderTime(h, m) +
                 " " + getRecurrenceText(pt.recurrence, pt.dayOfWeek, 0));
  return true;
}

void saveReminders() {
  JsonDocument doc;
  JsonArray arr = doc["reminders"].to<JsonArray>();
  for (const auto& r : reminders) {
    JsonObject o = arr.add<JsonObject>();
    o["msg"] = r.message; o["h"] = r.hour; o["m"] = r.minute;
    o["dow"] = r.dayOfWeek; o["dom"] = r.dayOfMonth;
    o["rec"] = (int)r.recurrence; o["triggered"] = r.triggered;
    o["count"] = r.triggerCount;
  }
  File f = FFat.open("/reminders.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadReminders() {
  if (!FFat.exists("/reminders.json")) return;
  File f = FFat.open("/reminders.json", FILE_READ); if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return; }
  reminders.clear();
  for (JsonObject o : doc["reminders"].as<JsonArray>()) {
    Reminder r;
    r.message      = o["msg"].as<String>();
    r.hour         = o["h"] | 0;  r.minute       = o["m"] | 0;
    r.dayOfWeek    = o["dow"] | 0; r.dayOfMonth   = o["dom"] | 0;
    r.recurrence   = (RecurrenceType)(o["rec"] | 0);
    r.triggered    = o["triggered"] | false;
    r.triggerCount = o["count"] | 0;
    reminders.push_back(r);
  }
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 12.5 ── NATURAL-LANGUAGE INTENT + ENTITY PARSER
//                 v1.7.7 — Massively expanded patterns
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

// v1.7.7: massively expanded filler strip list
static String stripFillers(const String& sIn) {
  static const char* fillers[] = {
    "hey there, ","hey there ","hey, ","hey ",
    "hi there, ","hi there ","hi, ","hi ",
    "hello there, ","hello there ","hello, ","hello ",
    "greetings, ","greetings ",
    "yo, ","yo ",
    "sup, ","sup ",
    "ok so, ","ok so ","ok, ","ok ","okay so, ","okay so ","okay, ","okay ",
    "alright so, ","alright, ","alright ","right so, ","right, ","right ",
    "well then, ","well, ","well ","so then, ","so, ","so ",
    "anyway, ","anyway ","anyhow, ","anyhow ",
    "uh, ","uh ","um, ","um ","uhh, ","uhh ","umm, ","umm ",
    "hmm, ","hmm ","hm, ","hm ","err, ","err ",
    "like, ","like ","you know, ","you know ","you see, ","you see ",
    "i mean, ","i mean ","i guess, ","i guess ","i think, ","i think ",
    "by the way, ","by the way ","btw, ","btw ","fyi, ","fyi ",
    "just so you know, ","just so you know ","heads up, ","heads up ",
    "quick question, ","quick question ","quick thing, ","quick thing ",
    "just a quick question, ","just a sec, ","just a moment, ",
    "just wondering, ","just wondering ","i was wondering, ","i was wondering ",
    "i was just wondering, ","i was thinking, ","i was thinking ",
    "i'm curious, ","im curious, ","out of curiosity, ",
    "please, ","please ","pls, ","pls ","plz, ","plz ",
    "kindly, ","kindly ","if you could, ","if you can, ","if possible, ",
    "can you please go ahead and ","can you please ","can you go ahead and ","can you ",
    "could you please go ahead and ","could you please ","could you go ahead and ","could you ",
    "would you please go ahead and ","would you please ","would you go ahead and ","would you ",
    "will you please ","will you ","might you ","may you ",
    "i would really like you to ","i would like you to ",
    "i'd really like you to ","i'd like you to ","id like you to ",
    "i really want you to ","i want you to ","i'd love for you to ","i'd love you to ",
    "i'd appreciate it if you ","i'd appreciate if you ","i'd appreciate you ",
    "i am asking you to ","i'm asking you to ","im asking you to ",
    "would you mind please ","would you mind ","do you mind ","do you mind if you ",
    "would you be so kind as to ","would you be kind enough to ",
    "if it's not too much trouble, ","if it's not too much trouble ",
    "if you don't mind, ","if you don't mind ","if you could kindly ",
    "when you get a chance, ","when you get a chance ","when you have time, ",
    "are you able to ","are you capable of ","is it possible to ","is it possible for you to ",
    "quickly, ","quickly ","quick, ","quick ","asap, ","asap ",
    "just ","simply ","merely ","basically ","essentially ","literally ",
    "actually, ","actually ","honestly, ","honestly ","frankly, ","frankly ",
    "to be honest, ","to be frank, ","to tell you the truth, ",
    "look, ","listen, ","hear me out, ","so basically, ","so essentially, ",
    "i need to ask, ","i have a question, ","quick q: ","question: ",
    "help me ","help me with ","assist me with ","assist me in ",
    "i need help with ","i need help ","i need you to help me ",
    nullptr
  };
  String r = sIn;
  String lower = r; lower.toLowerCase();
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; fillers[i]; i++) {
      int flen = strlen(fillers[i]);
      if (lower.startsWith(fillers[i])) {
        r     = r.substring(flen);
        lower = lower.substring(flen);
        changed = true;
        break;
      }
    }
  }
  r.trim();
  return r;
}

static String stripPrefixes(const String& sIn, const char* const* prefixes) {
  String lower = sIn; lower.toLowerCase();
  for (int i = 0; prefixes[i]; i++) {
    if (lower.startsWith(prefixes[i])) return sIn.substring(strlen(prefixes[i]));
  }
  return sIn;
}

const char* DAY_NAMES[] = {
  "monday","tuesday","wednesday","thursday","friday","saturday","sunday",
  "mon","tue","wed","thu","fri","sat","sun", nullptr
};

} // namespace NL

// ── parseTime ──────────────────────────────────────────
bool parseTime(const String& sIn, ParsedTime& out) {
  out = {false, -1, 0, false, 0, false, ONCE, 0};
  String s = sIn; s.toLowerCase();

  // Named times
  struct NamedTime { const char* name; int h; int m; };
  static const NamedTime namedTimes[] = {
    {"midnight",0,0},{"noon",12,0},{"midday",12,0},
    {"morning",8,0},{"this morning",8,0},{"early morning",6,0},
    {"afternoon",14,0},{"this afternoon",14,0},{"lunchtime",12,30},
    {"evening",18,0},{"this evening",18,0},{"tonight",20,0},
    {"night",21,0},{"late night",23,0},{"bedtime",22,0},
    {nullptr,0,0}
  };
  for (int i = 0; namedTimes[i].name; i++) {
    if (s.indexOf(namedTimes[i].name) >= 0) {
      out.hour = namedTimes[i].h; out.minute = namedTimes[i].m; out.found = true;
    }
  }

  // Recurrence
  if (s.indexOf("every day") >= 0 || s.indexOf("daily") >= 0 || s.indexOf("each day") >= 0 ||
      s.indexOf("every morning") >= 0 || s.indexOf("every night") >= 0 || s.indexOf("every evening") >= 0)
    out.recurrence = DAILY;
  else if (s.indexOf("every week") >= 0 || s.indexOf("weekly") >= 0 || s.indexOf("each week") >= 0)
    out.recurrence = WEEKLY;
  else if (s.indexOf("every month") >= 0 || s.indexOf("monthly") >= 0 || s.indexOf("each month") >= 0)
    out.recurrence = MONTHLY;

  // Day of week for weekly recurrence
  if (out.recurrence == WEEKLY || s.indexOf("every ") >= 0) {
    struct DayMap { const char* n; int d; };
    static const DayMap days[] = {
      {"monday",2},{"tuesday",3},{"wednesday",4},{"thursday",5},
      {"friday",6},{"saturday",7},{"sunday",1},
      {"mon",2},{"tue",3},{"wed",4},{"thu",5},{"fri",6},{"sat",7},{"sun",1},
      {nullptr,0}
    };
    for (int i = 0; days[i].n; i++) {
      if (s.indexOf(days[i].n) >= 0) { out.dayOfWeek = days[i].d; out.recurrence = WEEKLY; break; }
    }
  }

  // Tomorrow
  if (s.indexOf("tomorrow") >= 0) out.isTomorrow = true;

  // Relative: "in X minutes/hours"
  int inIdx = s.indexOf(" in ");
  if (inIdx < 0) inIdx = s.indexOf("in ");
  if (inIdx >= 0) {
    int from = inIdx + (s[inIdx] == ' ' ? 4 : 3);
    int p = s.indexOf("in ", from);
    if (p < 0) p = inIdx;
    int ns = p + 3, ne = ns;
    while (ne < (int)s.length() && isdigit((unsigned char)s[ne])) ne++;
    if (ne > ns) {
      int n = s.substring(ns, ne).toInt();
      int u = ne;
      while (u < (int)s.length() && s[u] == ' ') u++;
      if (u < (int)s.length()) {
        String unit = s.substring(u, min((int)s.length(), u + 8));
        if (unit.startsWith("min"))  { out.isRelative = true; out.relativeMinutes = n; out.found = true; }
        else if (unit.startsWith("hour") || unit.startsWith("hr"))
                                      { out.isRelative = true; out.relativeMinutes = n*60; out.found = true; }
        else if (unit.startsWith("sec"))
                                      { out.isRelative = true; out.relativeMinutes = max(1,n/60); out.found = true; }
        else if (unit.startsWith("day"))
                                      { out.isRelative = true; out.relativeMinutes = n*1440; out.found = true; }
      }
    }
  }
  if (out.isRelative) return true;

  // Clock time parsing (e.g. 3pm, 15:30, 3:00 AM)
  for (int i = 0; i < (int)s.length(); i++) {
    if (!isdigit((unsigned char)s[i])) continue;
    int j = i;
    while (j < (int)s.length() && isdigit((unsigned char)s[j])) j++;
    int h = s.substring(i, j).toInt();
    int m = 0, k = j; bool hasColon = false;
    if (k < (int)s.length() && s[k] == ':') {
      int ms = k+1, me = ms;
      while (me < (int)s.length() && isdigit((unsigned char)s[me])) me++;
      if (me > ms) { m = s.substring(ms, me).toInt(); k = me; hasColon = true; }
    }
    while (k < (int)s.length() && s[k] == ' ') k++;
    bool isPM = false, isAM = false;
    if (k + 1 < (int)s.length()) {
      if ((s[k]=='p'&&s[k+1]=='m')||(s[k]=='a'&&s[k+1]=='m'))
        { isPM=(s[k]=='p'); isAM=(s[k]=='a'); k+=2; }
      else if (s[k]=='p'&&k+3<(int)s.length()&&s[k+1]=='.'&&s[k+2]=='m'&&s[k+3]=='.')
        { isPM=true; k+=4; }
      else if (s[k]=='a'&&k+3<(int)s.length()&&s[k+1]=='.'&&s[k+2]=='m'&&s[k+3]=='.')
        { isAM=true; k+=4; }
    }
    bool hasAt = false;
    if (i >= 3) { String pre = s.substring(max(0,i-4),i); if (pre.indexOf("at ")>=0) hasAt=true; }
    if (h >= 0 && h <= 23 && m >= 0 && m < 60 && (hasAt||hasColon||isPM||isAM)) {
      if (isPM && h < 12) h += 12;
      if (isAM && h == 12) h = 0;
      if (!isPM && !isAM && !hasColon && h >= 1 && h <= 12) {
        int nowH = hour(), amH = (h==12)?0:h, pmH = (h==12)?12:h+12;
        if (amH > nowH) h = amH; else if (pmH > nowH) h = pmH;
        else { h = amH; out.isTomorrow = true; }
      }
      out.hour = h; out.minute = m; out.found = true; i = k; break;
    }
    i = k;
  }
  if (!out.found && (out.recurrence != ONCE || out.dayOfWeek != 0 || out.isTomorrow)) out.found = true;
  return out.found;
}

static String stripTimeExpr(const String& s) {
  String lower = s; lower.toLowerCase();
  static const char* connectors[] = {
    " at ", " in ", " on ", " every ", " each ", " next ",
    " tomorrow", " daily", " weekly", " monthly", nullptr
  };
  int chopAt = -1;
  for (int i = 0; connectors[i]; i++) {
    int p = lower.indexOf(connectors[i]);
    while (p >= 0) {
      String tail = lower.substring(p);
      ParsedTime tt; bool timely = parseTime(tail, tt), dayLike = false;
      for (int d = 0; NL::DAY_NAMES[d]; d++)
        if (tail.indexOf(NL::DAY_NAMES[d]) >= 0 && tail.indexOf(NL::DAY_NAMES[d]) < 12) { dayLike = true; break; }
      if (timely || dayLike || strstr(connectors[i],"tomorrow") || strstr(connectors[i],"daily") ||
          strstr(connectors[i],"weekly") || strstr(connectors[i],"monthly")) {
        if (chopAt < 0 || p < chopAt) chopAt = p; break;
      }
      p = lower.indexOf(connectors[i], p+1);
    }
  }
  String r = (chopAt > 0) ? s.substring(0, chopAt) : s;
  r.trim();
  while (r.length() && (r[r.length()-1]==',' || r[r.length()-1]=='.')) r.remove(r.length()-1,1);
  return r;
}

// ── detectIntent ───────────────────────────────────────
Intent detectIntent(const String& sIn) {
  String s = NL::stripFillers(sIn);
  s.toLowerCase();

  // Correction
  static const char* corrCues[] = {
    "actually","never mind","nevermind","scratch that","no wait","wait no",
    "forget that","cancel that","no, make it","no make it","change that to",
    "i meant","that's not what i meant","thats not what i meant","that's wrong",
    "thats wrong","no no,","no no ","let me rephrase","let me correct","disregard that",
    "ignore that","that was wrong","i made a mistake","correction:","whoops","oops",
    "i misspoke","hold on,","hold on ","strike that","start over","i didn't mean",
    "i did not mean","my bad","not what i said","that's not right","thats not right",
    "no that's","no thats","wait, i","wait i ","let's redo","lets redo","try again",
    "that's incorrect","thats incorrect","you got it wrong","you misunderstood",
    "not quite","close but","undo that","reverse that","take that back",
    "erase what i said","delete what i just said","sorry i meant","my mistake",
    "i was wrong","typo","scratch","nah never mind","nope never mind",
    nullptr
  };
  if (NL::containsAny(s, corrCues)) return INTENT_CORRECTION;

  // Reminder
  static const char* remCues[] = {
    "remind me","alarm","alert me","wake me","ping me","notify me",
    "don't let me forget","dont let me forget","make sure i don't forget",
    "make sure i dont forget","set a timer","set an alarm","buzz me",
    "set a reminder","create a reminder","add a reminder","schedule a reminder",
    "i need a reminder","give me a reminder","i need to be reminded",
    "i want to be reminded","help me remember","help me not forget",
    "don't forget to remind me","dont forget to remind me",
    "can you remind me","could you remind me","please remind me",
    "will you remind me","would you remind me","can you set an alarm",
    "could you set an alarm","can you set a timer","could you set a timer",
    "please alert me","can you alert me","could you alert me",
    "please wake me","can you wake me","could you wake me",
    "i'd like a reminder","id like a reminder","schedule an alarm",
    "schedule a timer","add an alarm","create an alarm","can you ping me",
    "could you ping me","i need an alert","give me an alert","poke me",
    "buzz me at","beep me","sound an alarm","set me a reminder","set me an alarm",
    "flag this for later","nudge me","give me a nudge","i shouldn't forget",
    "i should not forget","make sure i remember to","make sure i don't forget to",
    "make sure i dont forget to","don't let me miss","dont let me miss",
    "i need a heads up","give me a heads up","warn me at","can you warn me",
    "let me know at","tell me later","tell me at","remind me when",
    "put a reminder","drop a reminder","throw a reminder","leave a reminder",
    "alarm at","timer for","timer at","snooze","set me up a reminder",
    "schedule me a reminder","book a reminder","note a reminder",
    "fire an alarm","fire a reminder","save a reminder","log a reminder",
    "create an alert","put an alarm","drop an alarm","throw an alarm",
    "leave an alarm","i want an alarm","i want a reminder","i want to set a reminder",
    "i want to create a reminder","i'd like to set a reminder",
    "i'd like to create a reminder","i need to set a reminder",
    "can i set a reminder","can i create a reminder","i'd like to add a reminder",
    "i need to add a reminder","please add a reminder","please create a reminder",
    "please set a reminder","please set an alarm","quick reminder","short reminder",
    "upcoming reminder","future reminder","time-based reminder","time alert",
    nullptr
  };
  if (NL::containsAny(s, remCues)) {
    static const char* listCues[] = {
      "list","show me my","show my","what are my","any reminder",
      "my reminders","all reminders","do i have any reminders","display my reminders",
      "what have i set","show all reminders","what reminders","list my reminders",
      "check my reminders","see my reminders","view my reminders","read my reminders",
      "pull up my reminders","retrieve my reminders","fetch my reminders",
      "what's set","whats set","what's scheduled","whats scheduled",
      "what alarms do i have","list alarms","show alarms","my alarms",
      nullptr
    };
    static const char* cancelCues[] = {
      "cancel","delete","remove","clear","drop","turn off","disable",
      "stop that reminder","kill that reminder","dismiss","get rid of",
      "erase","wipe","undo","end","halt","abort","close","terminate","kill","purge",
      nullptr
    };
    if (NL::containsAny(s, listCues))   return INTENT_REMINDER_LIST;
    if (NL::containsAny(s, cancelCues)) return INTENT_REMINDER_CANCEL;
    return INTENT_REMINDER_SET;
  }

  // Memory recall
  static const char* recallCues[] = {
    "what did i say","what was my","do you remember","what's my","whats my",
    "what is my","tell me my","remind me what","did i tell you","what did i tell you",
    "can you recall","do you recall","what have i told you","what did i share",
    "have i mentioned","what's saved about","whats saved about",
    "what do you know about my","recall my","fetch my stored","retrieve my",
    "show me my stored","can you show me what i told you","do you have my",
    "what information do you have about my","tell me what i said",
    "have you saved my","look up what i said","look up my","pull up what i said",
    "bring up my","what's stored about","whats stored about",
    "what did you learn about me","what have you learned about me",
    "check what i said","check your memory of me","what's on file for me",
    "whats on file for me","did you save my","do you know my",
    "what did i previously say","what did i say earlier","remind me of what i said",
    "read back what i said","repeat what i said","tell me again what i said",
    "what have you stored","what have you remembered","what facts do you know",
    "what do you know about me","what facts about me","my details","my info",
    "what's my info","whats my info","what info do you have","what data do you have",
    "show me what you know","show me my info","list my details","list my facts",
    "list what you know","recite my facts","recite what you know","repeat my info",
    "playback my info","what did i share with you","what have i shared",
    "what personal info","personal details","my profile","show me my profile",
    "what's in my profile","pull my profile","get my profile","access my profile",
    nullptr
  };
  if (NL::containsAny(s, recallCues)) return INTENT_MEMORY_RECALL;

  // Note add
  static const char* noteAdd[] = {
    "add a note","make a note","take a note","note this","note down","write down",
    "jot down","log this","save a note","new note","create a note","put a note",
    "keep a note","add this to my notes","put this in my notes","keep note of",
    "record this","save this as a note","note the following","write this down for me",
    "can you note","could you note","please note this","i want to note",
    "i'd like to note","i need to note","can you write this down",
    "please write this down","can you keep a note","quickly note","jot this down for me",
    "scribble this down","add this note","save this note","stash this note",
    "file this note","make a quick note","note for me","can you jot down",
    "please jot down","note it down","log this note","capture this","capture that",
    "write it down","put it in notes","drop a note","throw a note","leave a note",
    "bookmark this","mark this","flag this","document this","record that",
    "annotate this","annotate that","memo this","memo that","make a memo",
    "create a memo","write a memo","add a memo","save a memo","quick memo",
    "short note","brief note","note to self","note:","memo:","write:","log:",
    nullptr
  };
  if (NL::containsAny(s, noteAdd)) return INTENT_NOTE_ADD;

  // Note recall
  static const char* noteRecall[] = {
    "my notes","show notes","show me my notes","what notes","list notes",
    "read my notes","what are my notes","can you show my notes","display my notes",
    "do i have any notes","what have i noted","read back my notes","show all notes",
    "list all notes","retrieve my notes","all my notes","pull up my notes",
    "what did i jot down","what did i write down","check my notes","read my note",
    "open my notes","bring up my notes","what's in my notes","whats in my notes",
    "my note list","note list","view notes","view my notes","access my notes",
    "get my notes","fetch my notes","show saved notes","see my notes","notes please",
    nullptr
  };
  if (NL::containsAny(s, noteRecall)) return INTENT_NOTE_RECALL;

  // Task add
  static const char* taskAdd[] = {
    "add a task","new task","todo:","to-do:","to do:","add to my todo",
    "add to my to-do","task list","create a task","make a task","put on my list",
    "add to my list","add to my tasks","can you add a task","please add a task",
    "i want to add a task","put this on my task list","task:","add this to my to-do list",
    "i have a task","schedule a task","i need to do ","i've got to do ","i have to do ",
    "i must do ","add it to my tasks","put it on my to-do list","put it on my task list",
    "queue up a task","add this as a task","log a task","log this task",
    "i've got a task","ive got a task","i need a task added","could you add a task",
    "please add this task","add another task","add one more task",
    "throw this on my list","stick this on my list","put this on my todo",
    "i need to ","i gotta ","i've got to ","task for me","create a to-do",
    "make a to-do","add to my checklist","checklist item","action item","add action item",
    nullptr
  };
  if (NL::containsAny(s, taskAdd)) return INTENT_TASK_ADD;

  // Memory save — v1.7.7: massively expanded
  static const char* saveCues[] = {
    // Core save phrases
    "remember that","remember my","remember this","keep in mind","save this",
    "memorize","store this","store that","for future reference","fyi my","fyi:",
    "can you remember","please remember","i want you to remember","i'd like you to remember",
    "id like you to remember","make a mental note","save this information","hold onto this",
    "keep this in mind","don't forget this","dont forget this","please save","i need you to save",
    "store the fact that","file this away","note for later","save for later",
    "can you store","please memorize","keep a record of","log this fact",
    "please keep this in mind","add this to memory","put this in memory",
    "save that fact","store that info","remember for me",
    // Personal name phrases
    "my name is","my first name is","my last name is","my surname is","my full name is",
    "call me","i go by","people call me","everyone calls me","they call me",
    "i'm known as","i am known as","you can call me","i prefer to be called",
    "the name's","the name is","i introduce myself as","i identify as",
    // Age
    "my age is","i am","i'm ","im ","i turned","i will turn","i'll be turning",
    "my birthday was","i was born in","i was born on","date of birth",
    "my dob is","my birth date is","my birth year is",
    // Location
    "i live in","i'm from","im from","i am from","i'm based in","im based in",
    "i am based in","i currently live in","i reside in","my city is","my town is",
    "my country is","my state is","my province is","my address is","my location is",
    "i stay in","i'm staying in","im staying in","i moved to","i just moved to",
    "i relocated to","i settled in","i'm located in","im located in",
    // Work/Education
    "i work at","i work for","i work as","i work in","my job is","my occupation is",
    "my profession is","my role is","my position is","my title is","i'm employed at",
    "im employed at","i am employed at","my employer is","my company is","my workplace is",
    "my office is at","i work from","i'm a ","i am a ","i study at","i go to",
    "i attend","i'm enrolled at","im enrolled at","my school is","my university is",
    "my college is","my course is","i'm studying","im studying","i am studying",
    // Relationships
    "my wife is","my husband is","my partner is","my girlfriend is","my boyfriend is",
    "my spouse is","my fiance is","my fiancee is","i'm married to","im married to",
    "i am married to","my father is","my mother is","my dad is","my mom is",
    "my mum is","my son is","my daughter is","my brother is","my sister is",
    "my family","my child is","my children are","my parents are","my sibling is",
    // Pets
    "my pet is","my pet's name is","my dog is","my dog's name is","my cat is",
    "my cat's name is","my pet is named","i have a pet","i have a dog","i have a cat",
    "my fish is","my bird is","my rabbit is","my hamster is",
    // Hobbies/Interests
    "my hobby is","my hobbies are","my interests are","my passion is","i love","i enjoy",
    "i like ","i'm into","im into","i am into","i'm passionate about","im passionate about",
    "i'm a fan of","im a fan of","i'm interested in","im interested in","i follow",
    "i play ","i watch ","i read ","i listen to ","my favourite is","my favorite is",
    "my favourite ","my favorite ","i prefer ","i'm obsessed with","im obsessed with",
    // Health
    "i'm allergic to","im allergic to","i am allergic to","my allergy is",
    "i have an allergy to","i'm intolerant to","im intolerant to","my diet is",
    "i'm vegetarian","i'm vegan","im vegetarian","im vegan","i don't eat",
    "i dont eat","i can't eat","i cant eat","my blood type is","my health condition",
    "i have diabetes","i have asthma","my medication is",
    // Contact/Financial
    "my email is","my email address is","my phone number is","my number is",
    "my phone is","my mobile is","my contact is","my website is","my username is",
    "my handle is","my instagram is","my twitter is","my facebook is",
    "my bank is","my salary is","my income is","i earn","i make ",
    // Vehicle/Property
    "i drive a","i drive an","my car is","my vehicle is","my bike is","i own a ",
    "i own an ","i have a ","i have an ","my home is","my house is","my flat is",
    "my apartment is","i rent","i own my ",
    // Misc
    "my anniversary is","my wedding date is","my graduation date is",
    "my language is","i speak ","i understand ","my religion is","i believe in ",
    "my goal is","my dream is","my ambition is","my plan is","i plan to",
    "important to me","means a lot to me","my password hint is","my note is",
    nullptr
  };
  if (NL::containsAny(s, saveCues) ||
      s.startsWith("remember ") || s.startsWith("note that ") || s.startsWith("know that "))
    return INTENT_MEMORY_SAVE;

  // Memory forget
  static const char* forgetCues[] = {
    "forget","erase","wipe","delete","remove that memory","clear that memory",
    "discard","purge","get rid of that fact","stop remembering","you can forget",
    "please forget","can you forget","i don't want you to remember",
    "i dont want you to remember","remove that from memory","clear that from your memory",
    "unlearn that","delete that fact","wipe that fact","clear that fact",
    "clear my memory of","erase my memory of","please erase","please wipe",
    "please delete that","take that out of memory","remove it from memory",
    "you don't need to remember that","you dont need to remember that",
    "scratch that from memory","drop that memory","kill that memory","lose that info",
    "throw away that fact","discard that fact","remove that info","clear that info",
    "erase that info","forget that info","forget what i said about",
    "forget what i told you about","forget my","clear my","erase my","wipe my",
    nullptr
  };
  if (NL::containsAny(s, forgetCues)) {
    if (NL::contains(s,"note")||NL::contains(s,"fact")||NL::contains(s,"memory")||
        NL::containsWord(s,"that")||NL::contains(s,"last")||NL::contains(s,"everything")||
        NL::containsWord(s,"all")||NL::contains(s,"saved")||NL::contains(s,"stored")||
        NL::containsWord(s,"info")||NL::contains(s,"about")||NL::contains(s,"my"))
      return INTENT_MEMORY_FORGET;
  }

  // Search
  static const char* searchCues[] = {
    "search for ","search about ","search on ","search the web","search the internet",
    "search online","web search","internet search","online search","look up ",
    "look it up","look online","look it up online","find info","find information",
    "find out ","find something about","find me info","find me information",
    "find on the internet","find online","google ","bing ","search google",
    "google that","google for me","research ","do some research","do a search",
    "run a search","do a web search","run a web search","check online","check the web",
    "check the internet","search up","look that up","query the web","browse for",
    "scour the web","look through the web","search around","search everywhere for",
    "find something on","hunt for","track down","investigate","dig up info on",
    "dig into","find the answer to","look for info on","look for information on",
    "pull up info on","pull up information on","get me info on","get information on",
    "fetch info on","gather info on","collect info on","compile info on",
    "what does the internet say about","what does google say about",
    "google what is","look up what is","search what is","find what is",
    nullptr
  };
  if (NL::containsAny(s, searchCues)) return INTENT_SEARCH;

  // System status
  static const char* statusCues[] = {
    "/diag","system status","system health","how are you doing","how is the device",
    "device health","device status","memory usage","ram usage","heap status",
    "battery status","cpu temperature","chip temperature","cpu temp","chip temp",
    "how much memory","how much ram","how much space","storage space","disk space",
    "free space","available memory","available ram","available storage","uptime",
    "how long have you been running","system info","hardware info","device info",
    "are you ok","are you okay","are you healthy","how healthy are you",
    "how are you running","system check","run a check","run a diagnostic",
    "run diagnostics","performance check","health check","status check","chip info",
    "board info","firmware info","version info","how warm are you","are you hot",
    "temperature check","psram status","wifi status","connection status",
    nullptr
  };
  if (NL::containsAny(s, statusCues)) return INTENT_SYSTEM_STATUS;

  // Weather
  static const char* weatherCues[] = {
    "weather","forecast","temperature outside","what's the temperature","whats the temperature",
    "is it raining","will it rain","is it hot","is it cold","is it sunny","is it cloudy",
    "what's the weather","whats the weather","how's the weather","hows the weather",
    "weather today","weather tomorrow","weather this week","weather forecast",
    "current weather","live weather","local weather","outdoor temperature",
    "what should i wear","do i need an umbrella","umbrella","rain today","sun today",
    "humidity","wind speed","uv index","air quality","feels like","apparent temperature",
    "weather report","weather update","weather conditions","atmospheric conditions",
    "meteorological","climate today","how cold is it","how warm is it","how hot is it",
    "is it freezing","is it warm","is it cool","chilly today","scorching today",
    "weather in ","weather for ","weather at ","forecast for ","forecast in ",
    nullptr
  };
  if (NL::containsAny(s, weatherCues)) return INTENT_WEATHER;

  return INTENT_NONE;
}

// ── extractEntities ─────────────────────────────────────
void extractEntities(const String& sIn, Intent intent, ParsedEntities& out) {
  out = {};
  String s = NL::stripFillers(sIn);
  parseTime(s.c_str(), out.time);

  String content = s;
  content.toLowerCase();

  switch (intent) {
    case INTENT_REMINDER_SET: {
      static const char* p[] = {
        "remind me to ","remind me about ","remind me that ","remind me ","remind ",
        "set a reminder to ","set a reminder for ","set a reminder about ","set a reminder ",
        "create a reminder to ","create a reminder for ","create a reminder about ","create a reminder ",
        "add a reminder to ","add a reminder for ","add a reminder about ","add a reminder ",
        "i need a reminder to ","i need a reminder for ","i need a reminder about ","i need a reminder ",
        "i want a reminder to ","i want a reminder for ","i want a reminder about ","i want a reminder ",
        "alert me to ","alert me about ","alert me when ","alert me ","wake me to ","wake me ",
        "make sure i ","don't let me forget to ","dont let me forget to ",
        "help me remember to ","help me not forget to ","i should not forget to ",
        "ping me to ","ping me about ","ping me ","buzz me to ","buzz me about ","buzz me ",
        "notify me to ","notify me about ","notify me when ","notify me ",
        "give me a reminder to ","give me a reminder for ","give me a reminder about ","give me a reminder ",
        "set alarm for ","set an alarm for ","set alarm to ","set an alarm to ",
        "create an alarm for ","create an alarm to ","create an alarm about ","create an alarm ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      String stripped = s.length() > content.length()
        ? s.substring(s.length() - content.length()) : content;
      while (stripped.length() > 0) {
        char c = stripped[stripped.length()-1];
        if (c=='?'||c=='.'||c=='!'||c==',') stripped.remove(stripped.length()-1); else break;
      }
      content = (stripped.length() > 0 && stripped != content) ? stripped : "";
      break;
    }
    case INTENT_MEMORY_SAVE: {
      static const char* p[] = {
        "please remember that ","please remember ","remember that ","remember my ","remember ",
        "can you remember that ","can you remember ","could you remember ","store that ","store this ",
        "store the fact that ","save that ","save this ","save the fact that ","memorize that ","memorize this ",
        "memorize the fact that ","keep in mind that ","keep in mind ","note that ","note this ",
        "make a mental note that ","make a mental note of ","make a mental note ",
        "file away that ","file away ","log this fact: ","log this fact ","know that ",
        "please keep in mind that ","please keep in mind ","add to memory: ","add to memory ",
        "put in memory: ","put in memory ","i want you to know that ","i want you to know ",
        "i'd like you to know that ","i'd like you to know ","i need you to know that ","i need you to know ",
        "just so you know, ","just so you know ","heads up: ","fyi: ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_MEMORY_RECALL: {
      static const char* p[] = {
        "what did i say about ","what did i tell you about ","do you remember my ",
        "what's my ","whats my ","what is my ","tell me my ","remind me of my ",
        "recall my ","fetch my ","retrieve my ","show me my ","what do you know about my ",
        "what have you stored for ","what's stored about ","whats stored about ",
        "what have you saved about ","check your memory for ","what's on file for ",
        "look up my ","bring up my ","pull up my ","do you have my ",
        "do you know my ","did i tell you my ","what did i mention about my ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_NOTE_ADD: {
      static const char* p[] = {
        "add a note: ","add a note that ","add a note ","make a note: ","make a note that ",
        "make a note of ","make a note ","take a note: ","take a note that ","take a note of ","take a note ",
        "note down: ","note down ","note this: ","note this ","note that ","jot down: ","jot down ",
        "log this: ","log this ","write down: ","write down ","save a note: ","save a note that ","save a note ",
        "record this: ","record this ","create a note: ","create a note that ","create a note ",
        "capture this: ","capture this ","memo: ","note: ","log: ","write: ","document this: ","document this ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_NOTE_RECALL: {
      static const char* p[] = {
        "show me my notes","show my notes","list my notes","read my notes",
        "what are my notes","display my notes","view my notes","get my notes",
        "fetch my notes","retrieve my notes","pull up my notes","bring up my notes",
        "check my notes","see my notes","open my notes","access my notes","notes please",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_TASK_ADD: {
      static const char* p[] = {
        "add a task: ","add a task to ","add a task for ","add a task that ","add a task ",
        "create a task: ","create a task to ","create a task for ","create a task that ","create a task ",
        "make a task: ","make a task to ","make a task for ","make a task ",
        "new task: ","new task ","todo: ","to-do: ","to do: ","add to my todo: ","add to my todo ",
        "add to my to-do list: ","add to my to-do list ","add to my task list: ","add to my task list ",
        "add to my list: ","add to my list ","task: ","checklist item: ","action item: ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_SEARCH: {
      static const char* p[] = {
        "search for ","search about ","search on ","search the web for ","search the internet for ",
        "search online for ","look up ","find info on ","find information on ","find out about ",
        "find me info on ","find me information on ","find me ","google ","research ",
        "do a search for ","do a web search for ","check online for ","check the web for ",
        "browse for ","investigate ","dig up info on ","dig into ","look for info on ",
        "look for information on ","pull up info on ","get me info on ","get information on ",
        "look up information about ","look up information on ","look up info about ","look up info on ",
        "find information about ","find info about ","search for information about ","search about ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      break;
    }
    case INTENT_REMINDER_CANCEL: {
      static const char* p[] = {
        "cancel my reminder about ","cancel my reminder for ","cancel my reminder to ",
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
        "clear that reminder ","clear ","turn off my reminder ","turn off the reminder ","turn off ",
        "disable my reminder ","disable the reminder ","disable ","stop my reminder ","stop the reminder ",
        "dismiss my reminder ","dismiss the reminder ","dismiss ",
        "get rid of my reminder ","get rid of the reminder ","drop the reminder ","drop my reminder ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      // Try to parse index
      out.referenceIndex = -1;
      out.reference = "";
      String lc = content; lc.toLowerCase();
      if (lc == "last" || lc == "latest" || lc == "most recent") {
        out.reference = "last"; out.referenceIndex = -1;
      } else {
        int num = content.toInt();
        if (num >= 0 && num < (int)reminders.size()) out.referenceIndex = num;
        else {
          for (int i = 0; i < (int)reminders.size(); i++) {
            String rm = reminders[i].message; rm.toLowerCase();
            if (rm.indexOf(lc) >= 0 || lc.indexOf(rm) >= 0) { out.referenceIndex = i; break; }
          }
        }
      }
      break;
    }
    case INTENT_WEATHER: {
      static const char* p[] = {
        "weather in ","weather for ","weather at ","weather ","forecast for ","forecast in ",
        "forecast at ","forecast ","temperature in ","temperature for ","temperature at ",
        "what's the weather in ","whats the weather in ","how's the weather in ","hows the weather in ",
        "what is the weather in ","what is the weather for ","weather today in ",
        "weather tomorrow in ","is it raining in ","is it hot in ","is it cold in ",
        nullptr
      };
      content = NL::stripPrefixes(content, p);
      while (content.length() > 0) {
        char c = content[content.length()-1];
        if (c=='?'||c=='.'||c=='!'||c==',') content.remove(content.length()-1); else break;
      }
      if (content.length() == 0) content = recallFact("city");
      break;
    }
    default: break;
  }

  content = stripTimeExpr(content);
  content.trim();
  out.content = content;
}

// ── parseNaturalLanguage ───────────────────────────────
ParsedCommand parseNaturalLanguage(const String& s) {
  ParsedCommand cmd;

  // Handle pending context (follow-up city for weather)
  if (nlContext.pendingIntent == INTENT_WEATHER && millis() - nlContext.lastUpdate <= NL_PENDING_TTL_MS) {
    String city = s; city.trim(); String lc = city; lc.toLowerCase();
    if (lc.startsWith("weather in "))  city = city.substring(11);
    else if (lc.startsWith("weather for ")) city = city.substring(12);
    else if (lc.startsWith("weather ")) city = city.substring(8);
    else if (lc.startsWith("in "))    city = city.substring(3);
    else if (lc.startsWith("at "))    city = city.substring(3);
    else if (lc.startsWith("for "))   city = city.substring(4);
    while (city.length() > 0) {
      char c = city[city.length()-1];
      if (c=='?'||c=='.'||c=='!'||c==',') city.remove(city.length()-1); else break;
    }
    if (city.length() > 0 && city.length() < 60) {
      cmd.intent = INTENT_WEATHER; cmd.entities.content = city;
      cmd.confidence = 0.9f; return cmd;
    }
  }

  // Handle pending time follow-up for reminders
  if (nlContext.pendingIntent == INTENT_REMINDER_SET && millis() - nlContext.lastUpdate <= NL_PENDING_TTL_MS) {
    ParsedTime pt; if (parseTime(s, pt) && pt.found) {
      cmd.intent          = INTENT_REMINDER_SET;
      cmd.entities        = nlContext.pendingEntities;
      cmd.entities.time   = pt;
      cmd.confidence      = 0.9f;
      return cmd;
    }
  }

  cmd.intent     = detectIntent(s);
  cmd.confidence = (cmd.intent == INTENT_NONE) ? 0.0f : 0.8f;
  extractEntities(s, cmd.intent, cmd.entities);

  switch (cmd.intent) {
    case INTENT_REMINDER_SET: case INTENT_MEMORY_SAVE: case INTENT_NOTE_ADD:
    case INTENT_TASK_ADD:     case INTENT_SEARCH:
      if (cmd.entities.content.length() == 0) cmd.confidence = 0.4f; break;
    default: break;
  }
  return cmd;
}

void nlClearPending()  { nlContext.pendingIntent = INTENT_NONE; nlContext.pendingEntities = {}; nlContext.lastUpdate = millis(); }
void nlRememberLast(const ParsedCommand& cmd) { nlContext.lastIntent = cmd.intent; nlContext.lastEntities = cmd.entities; nlContext.lastUpdate = millis(); }

static void resolveClock(const ParsedTime& t, int& outH, int& outM) {
  if (t.isRelative) {
    time_t tg = now() + (time_t)t.relativeMinutes * 60;
    outH = hour(tg); outM = minute(tg);
  } else { outH = t.hour; outM = t.minute; }
}

// ── executeIntent ──────────────────────────────────────
bool executeIntent(const ParsedCommand& cmd, const String& original) {
  if (nlContext.pendingIntent != INTENT_NONE && millis() - nlContext.lastUpdate > NL_PENDING_TTL_MS)
    nlClearPending();

  switch (cmd.intent) {
    case INTENT_REMINDER_SET: {
      if (cmd.entities.content.length() == 0) return false;
      bool haveTime = cmd.entities.time.found &&
                      (cmd.entities.time.isRelative || cmd.entities.time.hour >= 0);
      if (!haveTime) {
        nlContext.pendingIntent   = INTENT_REMINDER_SET;
        nlContext.pendingEntities = cmd.entities;
        nlContext.lastUpdate      = millis();
        Serial.println("⏰ When should I remind you?  (e.g. \"at 6pm\" or \"in 10 minutes\")");
        return true;
      }
      int h, m; resolveClock(cmd.entities.time, h, m);
      RecurrenceType rec = cmd.entities.time.recurrence;
      int dow = cmd.entities.time.dayOfWeek, dom = 0;
      if (rec == WEEKLY  && dow == 0) dow = weekday();
      if (rec == MONTHLY) dom = day();
      addReminder(cmd.entities.content, h, m, rec, dow, dom);
      Serial.println("✅ Reminder set: \"" + cmd.entities.content + "\" @ " +
                     formatReminderTime(h, m) + " " + getRecurrenceText(rec, dow, dom));
      nlRememberLast(cmd); nlClearPending(); return true;
    }
    case INTENT_REMINDER_LIST:
      listReminders(); nlRememberLast(cmd); return true;

    case INTENT_REMINDER_CANCEL: {
      if (reminders.empty()) { Serial.println("⏰ No reminders to cancel."); return true; }
      int idx = cmd.entities.referenceIndex;
      if (cmd.entities.reference == "last" || idx < 0) idx = (int)reminders.size() - 1;
      if (idx >= 0 && idx < (int)reminders.size()) {
        Serial.println("✅ Cancelled: \"" + reminders[idx].message + "\"");
        removeReminder(idx);
      } else {
        Serial.println("⚠️  Couldn't identify which reminder to cancel. Use /reminders to see indices.");
      }
      nlRememberLast(cmd); return true;
    }

    case INTENT_MEMORY_SAVE: {
      String c = cmd.entities.content; if (c.length() == 0) return false;
      String key, val;
      int isIdx = c.indexOf(" is "), eqIdx = c.indexOf(" = ");
      int splitIdx = (isIdx >= 0 && (eqIdx < 0 || isIdx < eqIdx)) ? isIdx : eqIdx;
      int splitLen = (splitIdx == isIdx) ? 4 : 3;
      if (splitIdx > 0) {
        key = c.substring(0, splitIdx);
        val = c.substring(splitIdx + splitLen);
      } else {
        int sp = c.indexOf(' ');
        if (sp > 0) { key = c.substring(0, sp); val = c.substring(sp + 1); }
        else { key = "note"; val = c; }
      }
      String klow = key; klow.toLowerCase();
      if (klow.startsWith("my "))  { key = key.substring(3);  klow = klow.substring(3); }
      if (klow.startsWith("the ")) { key = key.substring(4);  klow = klow.substring(4); }
      key.trim(); val.trim();
      if (key.length() == 0 || val.length() == 0) {
        Serial.println("⚠️  Couldn't save — try: \"remember my name is Cash\"");
        return true;
      }
      rememberFact(key, val);
      Serial.println("💾 Got it — remembered: " + key + " = " + val);
      nlRememberLast(cmd); return true;
    }

    case INTENT_MEMORY_RECALL: {
      String key = cmd.entities.content; key.trim();
      if (key.length() > 0) {
        String val = recallFact(key);
        if (val.length() > 0) {
          Serial.println("💾 " + key + " = " + val);
        } else {
          Serial.println("🤔 I don't have anything stored for \"" + key + "\" yet.");
          // Try fuzzy match
          for (const auto& f : memory) {
            if (f.key.indexOf(key) >= 0 || key.indexOf(f.key) >= 0) {
              Serial.println("   Closest match: " + f.key + " = " + f.value);
              break;
            }
          }
        }
      } else {
        // Show all facts
        if (memory.empty()) { Serial.println("📭 Nothing stored in memory yet."); }
        else {
          Serial.println("🧠 Everything I know about you:");
          for (const auto& f : memory) Serial.println("  • " + f.key + " = " + f.value);
        }
      }
      nlRememberLast(cmd); return true;
    }

    case INTENT_MEMORY_FORGET: {
      String key = cmd.entities.content; key.trim();
      if (key.length() == 0 || key == "everything" || key == "all") {
        memory.clear(); g_dirtyMemory = true;
        Serial.println("🧹 Memory wiped — all stored facts deleted.");
      } else {
        removeFact(key);
        Serial.println("🗑️  Forgot: " + key);
      }
      nlRememberLast(cmd); return true;
    }

    case INTENT_NOTE_ADD: {
      String c = cmd.entities.content; c.trim(); if (c.length() == 0) return false;
      noteTaskCounter++;
      String noteKey = "note_" + String(noteTaskCounter);
      rememberFact(noteKey, c);
      Serial.println("📝 Note saved: " + c);
      nlRememberLast(cmd); return true;
    }

    case INTENT_NOTE_RECALL: {
      bool found = false;
      Serial.println("\n📝 ═══ NOTES ═══");
      for (const auto& f : memory) {
        if (f.key.startsWith("note_")) {
          Serial.println("  • " + f.value);
          found = true;
        }
      }
      if (!found) Serial.println("  (no notes saved yet)");
      Serial.println("══════════════════");
      nlRememberLast(cmd); return true;
    }

    case INTENT_TASK_ADD: {
      String c = cmd.entities.content; c.trim(); if (c.length() == 0) return false;
      noteTaskCounter++;
      String taskKey = "task_" + String(noteTaskCounter);
      rememberFact(taskKey, c);
      Serial.println("✅ Task added: " + c);
      nlRememberLast(cmd); return true;
    }

    case INTENT_SEARCH: {
      String q = cmd.entities.content; q.trim();
      if (q.length() == 0) q = original;
      Serial.println("🔍 Searching: " + q);
      String results = fetchWebSearchResults(q);
      if (results.length() > 0) {
        String summary = groqStream(results, "Summarise these search results in 3-5 clear sentences.", 0.3f, 256);
        Serial.println("\n🌐 " + (summary.length() > 0 ? summary : results));
      } else {
        Serial.println("⚠️  No results found for: " + q);
      }
      nlRememberLast(cmd); return true;
    }

    case INTENT_SYSTEM_STATUS:
      systemDiagnostics(); nlRememberLast(cmd); return true;

    case INTENT_WEATHER: {
      String city = cmd.entities.content; city.trim();
      if (city.length() == 0) {
        city = recallFact("city");
        if (city.length() == 0) {
          nlContext.pendingIntent = INTENT_WEATHER; nlContext.lastUpdate = millis();
          Serial.println("🌍 Which city?");
          return true;
        }
      }
      getWeather(city); nlRememberLast(cmd); return true;
    }

    case INTENT_CORRECTION:
      Serial.println("🔄 No problem — what did you mean?");
      nlClearPending(); return true;

    default: return false;
  }
}

// ═══════════════════════════════════════════════════════
// SECTION 13 ── MEMORY
// ═══════════════════════════════════════════════════════

void rememberFact(const String& key, const String& value) {
  if (key.length() == 0 || value.length() == 0) return;
  unsigned long epochNow = timeClient.getEpochTime();
  for (auto& f : memory) {
    if (f.key.equalsIgnoreCase(key)) {
      f.value = value; f.lastAccess = epochNow; f.accessCount++;
      g_dirtyMemory = true; return;
    }
  }
  if ((int)memory.size() >= Config::MAX_MEMORY_FACTS) {
    auto it = std::min_element(memory.begin(), memory.end(),
      [](const Fact& a, const Fact& b){ return a.accessCount < b.accessCount; });
    memory.erase(it);
  }
  memory.push_back({key, value, 1, epochNow});
  g_dirtyMemory = true;
}

String recallFact(const String& key) {
  unsigned long epochNow = timeClient.getEpochTime();
  for (auto& f : memory) {
    if (f.key.equalsIgnoreCase(key)) {
      f.accessCount++; f.lastAccess = epochNow; return f.value;
    }
  }
  return "";
}

void removeFact(const String& key) {
  if (key.length() == 0) {
    Serial.println("⚠️  removeFact: no key — use /clear to wipe everything.");
    return;
  }
  memory.erase(std::remove_if(memory.begin(), memory.end(),
    [&](const Fact& f){ return f.key.equalsIgnoreCase(key); }), memory.end());
  g_dirtyMemory = true;
}

void saveMemory() {
  JsonDocument doc; JsonArray arr = doc["memory"].to<JsonArray>();
  for (const auto& f : memory) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = f.key; o["value"] = f.value;
    o["accessCount"] = f.accessCount; o["lastAccess"] = f.lastAccess;
  }
  File file = FFat.open("/memory.json", FILE_WRITE);
  if (file) { serializeJson(doc, file); file.close(); }
}

void loadMemory() {
  if (!FFat.exists("/memory.json")) return;
  File file = FFat.open("/memory.json", FILE_READ); if (!file) return;
  JsonDocument doc;
  if (deserializeJson(doc, file)) { file.close(); return; }
  memory.clear();
  for (JsonObject f : doc["memory"].as<JsonArray>())
    memory.push_back({f["key"].as<String>(), f["value"].as<String>(),
                      f["accessCount"]|1, f["lastAccess"]|0UL});
  file.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 14 ── SENTIMENT & MOOD  (v1.7.7: improved)
// ═══════════════════════════════════════════════════════

String detectSentiment(const String& message) {
  String lower = message; lower.toLowerCase();
  int pos = 0, neg = 0;

  struct KW { const char* word; int score; };
  static const KW positive[] = {
    {"great",2},{"awesome",2},{"love",2},{"happy",2},{"excellent",2},
    {"wonderful",2},{"fantastic",2},{"thank",1},{"good",1},{"nice",1},
    {"perfect",2},{"brilliant",2},{"amazing",2},{"superb",2},{"joy",2},
    {"excited",2},{"thrilled",2},{"delighted",2},{"pleased",1},{"glad",1},
    {"cheerful",1},{"content",1},{"satisfied",1},{"grateful",2},{"blessed",2},
    {"positive",1},{"optimistic",1},{"hopeful",1},{"energetic",1},{"motivated",2},
    {"productive",1},{"successful",2},{"won",1},{"winning",1},{"achieved",2},
    {"accomplished",2},{"proud",2},{"confident",1},{"strong",1},{"healthy",1},
    {"laughing",2},{"smiling",2},{"enjoying",1},{"celebrating",2},{"exciting",1},
    {"interesting",1},{"helpful",1},{"useful",1},{"easy",1},{"smooth",1},
    {"!",1},{"😊",2},{"😄",2},{"😍",2},{"🎉",2},{"❤️",2},{"👍",1},{"✅",1},
    {nullptr,0}
  };
  static const KW negative[] = {
    {"terrible",2},{"awful",2},{"hate",2},{"sad",2},{"angry",2},
    {"frustrated",2},{"horrible",2},{"bad",1},{"worst",2},{"annoying",1},
    {"depressed",3},{"anxious",2},{"stressed",2},{"overwhelmed",2},{"exhausted",2},
    {"tired",1},{"bored",1},{"lonely",2},{"miserable",3},{"hopeless",2},
    {"disappointing",2},{"disappointed",2},{"failed",2},{"failure",2},{"broke",1},
    {"broken",1},{"stuck",1},{"lost",1},{"confused",1},{"worried",2},
    {"scared",2},{"afraid",2},{"angry",2},{"furious",3},{"upset",2},
    {"crying",2},{"hurt",2},{"pain",2},{"suffering",2},{"struggling",2},
    {"difficult",1},{"hard time",2},{"tough",1},{"rough",1},{"ugh",2},
    {"argh",2},{"damn",1},{"hate this",3},{"can't stand",2},{"sick of",2},
    {"😞",2},{"😢",2},{"😡",2},{"😤",2},{"💔",2},{"😭",2},{"🤬",3},
    {nullptr,0}
  };

  for (int i = 0; positive[i].word; i++) if (lower.indexOf(positive[i].word) >= 0) pos += positive[i].score;
  for (int i = 0; negative[i].word; i++) if (lower.indexOf(negative[i].word) >= 0) neg += negative[i].score;

  // Strong signal — skip Groq API call
  if (pos >= 3 && neg == 0) return "positive (0.90)";
  if (pos >= 2 && neg == 0) return "positive (0.80)";
  if (neg >= 3 && pos == 0) return "negative (0.90)";
  if (neg >= 2 && pos == 0) return "negative (0.80)";
  if (pos == 0 && neg == 0) return "neutral (0.50)";
  if (pos > neg) return "positive (" + String(0.5f + (pos-neg)*0.1f, 2) + ")";
  if (neg > pos) return "negative (" + String(0.5f + (neg-pos)*0.1f, 2) + ")";

  // Ambiguous — use Groq
  String prompt =
    "Classify sentiment. JSON only (no markdown): {\"s\":\"positive\",\"c\":0.8}\n"
    "s = positive/negative/neutral, c = confidence 0.0-1.0\n"
    "Message: \"" + message + "\"";
  String raw = groqSimpleCall(prompt, 0.0f, 24);
  if (raw.length() > 0) {
    if (raw.startsWith("```")) { int s = raw.indexOf('\n')+1, e = raw.lastIndexOf("```"); if(e>s){raw=raw.substring(s,e);raw.trim();} }
    JsonDocument p; if (!deserializeJson(p, raw)) return p["s"].as<String>() + " (" + String(p["c"]|0.5f, 2) + ")";
  }
  return "neutral (0.50)";
}

// v1.7.7: Mood-adaptive temperature computation
float computeMoodTemperature() {
  if (sentimentHistory.empty()) return Config::AI_TEMPERATURE;

  float scoreSum = 0;
  int count = min((int)sentimentHistory.size(), 5);
  int start = (int)sentimentHistory.size() - count;
  for (int i = start; i < (int)sentimentHistory.size(); i++) {
    const auto& s = sentimentHistory[i];
    if (s.sentiment.startsWith("positive")) scoreSum += s.score;
    else if (s.sentiment.startsWith("negative")) scoreSum -= s.score;
  }
  float avg = scoreSum / count;
  // avg: negative = lower temp (careful), positive = higher temp (playful)
  // Map [-1, 1] to [0.42, 0.88]
  float temp = 0.65f + avg * 0.23f;
  return constrain(temp, 0.42f, 0.88f);
}

void trackSentiment(const String& sentiment, float score) {
  sentimentHistory.push_back({sentiment, score, timeClient.getEpochTime()});
  if ((int)sentimentHistory.size() > Config::MAX_SENTIMENT_LOG)
    sentimentHistory.erase(sentimentHistory.begin());
  if (sentiment.startsWith("positive")) { consecutivePositive++; consecutiveNegative = 0; }
  else if (sentiment.startsWith("negative")) { consecutiveNegative++; consecutivePositive = 0; }
  else { consecutivePositive = 0; consecutiveNegative = 0; }
  userPattern.recentMood = sentiment.startsWith("positive") ? "positive" :
                           sentiment.startsWith("negative") ? "negative" : "neutral";
  g_dirtySentiment = true; g_dirtyPattern = true;
}

void respondToMood() {
  if (consecutivePositive >= 3) { celebratePositiveVibes(); consecutivePositive = 0; }
  if (consecutiveNegative >= 2) { offerComfort();           consecutiveNegative = 0; }
}

void celebratePositiveVibes() {
  Serial.println("\n✨ Loving the energy! Great things are happening. 🌟");
  aiState = AI_EXCITED; stateChangeTime = millis();
}

void offerComfort() {
  String name = recallFact("name");
  if (name.length() > 0)
    Serial.println("\n💙 " + name + ", things seem a bit tough. I'm here if you want to talk.");
  else
    Serial.println("\n💙 Things seem a bit tough lately. I'm here if you need to talk it through.");
  aiState = AI_CONCERNED; stateChangeTime = millis();
}

void smartResponseEnhancement(String& response) {
  if (userPattern.recentMood == "negative" && response.indexOf("?") < 0)
    if (random(100) < 25) response += " Let me know if there's anything else I can help with.";
}

// ═══════════════════════════════════════════════════════
// SECTION 15 ── USER PATTERNS & LEARNING
// ═══════════════════════════════════════════════════════

void updateUserPattern(const String& message) {
  userPattern.totalInteractions++;
  userPattern.lastInteraction = timeClient.getEpochTime();
  int h = hour();
  if (h >= 5 && h < 12)  userPattern.morningChats++;
  if (h >= 18 && h < 24) userPattern.eveningChats++;
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0 ||
      lower.indexOf("error") >= 0 || lower.indexOf("function") >= 0 ||
      lower.indexOf("debug") >= 0 || lower.indexOf("compile") >= 0 ||
      lower.indexOf("syntax") >= 0 || lower.indexOf("algorithm") >= 0)
    userPattern.techQuestions++;
  else if (message.length() < 60 && message.indexOf("?") < 0)
    userPattern.casualMessages++;
  String topic = analyzeConversationTopic(message);
  if (topic.length() > 0) {
    bool exists = false;
    for (int i = 0; i < 5; i++) if (userPattern.favoriteTopics[i] == topic) { exists = true; break; }
    if (!exists) for (int i = 0; i < 5; i++) {
      if (userPattern.favoriteTopics[i].length() == 0) { userPattern.favoriteTopics[i] = topic; break; }
    }
  }
  g_dirtyPattern = true;
}

String analyzeConversationTopic(const String& message) {
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("weather") >= 0) return "weather";
  if (lower.indexOf("remind") >= 0 || lower.indexOf("alarm") >= 0) return "reminders";
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0 || lower.indexOf("debug") >= 0) return "technical";
  if (lower.indexOf("news") >= 0 || lower.indexOf("headline") >= 0) return "news";
  if (lower.indexOf("joke") >= 0 || lower.indexOf("funny") >= 0 || lower.indexOf("laugh") >= 0) return "entertainment";
  if (lower.indexOf("food") >= 0 || lower.indexOf("eat") >= 0 || lower.indexOf("recipe") >= 0) return "food";
  if (lower.indexOf("health") >= 0 || lower.indexOf("exercise") >= 0 || lower.indexOf("gym") >= 0) return "health";
  if (lower.indexOf("money") >= 0 || lower.indexOf("finance") >= 0 || lower.indexOf("invest") >= 0) return "finance";
  if (lower.indexOf("travel") >= 0 || lower.indexOf("trip") >= 0 || lower.indexOf("holiday") >= 0) return "travel";
  if (lower.indexOf("music") >= 0 || lower.indexOf("song") >= 0 || lower.indexOf("film") >= 0 || lower.indexOf("movie") >= 0) return "media";
  if (lower.indexOf("sport") >= 0 || lower.indexOf("football") >= 0 || lower.indexOf("cricket") >= 0) return "sports";
  return "";
}

void calculateThinkingComplexity(const String& message) {
  int c = 1;
  if (message.length() > 150) c += 4;
  else if (message.length() > 80) c += 2;
  else if (message.length() > 40) c += 1;
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("why") >= 0)     c += 2;
  if (lower.indexOf("how") >= 0)     c += 1;
  if (lower.indexOf("explain") >= 0) c += 2;
  if (lower.indexOf("compare") >= 0) c += 3;
  if (lower.indexOf("analyze") >= 0 || lower.indexOf("analyse") >= 0) c += 3;
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0)    c += 2;
  if (lower.indexOf("difference") >= 0) c += 2;
  if (lower.indexOf("pros and cons") >= 0 || lower.indexOf("advantages") >= 0) c += 2;
  thinkingComplexity = min(10, c);
}

void saveUserPattern() {
  JsonDocument doc;
  doc["total"] = userPattern.totalInteractions; doc["morning"] = userPattern.morningChats;
  doc["evening"] = userPattern.eveningChats; doc["lastTime"] = userPattern.lastInteraction;
  doc["mood"] = userPattern.recentMood; doc["tech"] = userPattern.techQuestions;
  doc["casual"] = userPattern.casualMessages; doc["remUsage"] = userPattern.reminderUsage;
  JsonArray topics = doc["topics"].to<JsonArray>();
  for (int i = 0; i < 5; i++) if (userPattern.favoriteTopics[i].length()) topics.add(userPattern.favoriteTopics[i]);
  File f = FFat.open("/pattern.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadUserPattern() {
  if (!FFat.exists("/pattern.json")) return;
  File f = FFat.open("/pattern.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  userPattern.totalInteractions = doc["total"]|0;    userPattern.morningChats = doc["morning"]|0;
  userPattern.eveningChats      = doc["evening"]|0;  userPattern.lastInteraction = doc["lastTime"]|0UL;
  userPattern.recentMood        = doc["mood"]|"neutral"; userPattern.techQuestions = doc["tech"]|0;
  userPattern.casualMessages    = doc["casual"]|0;   userPattern.reminderUsage = doc["remUsage"]|0;
  if (doc.containsKey("topics")) {
    int i = 0;
    for (JsonVariant t : doc["topics"].as<JsonArray>())
      if (i < 5) userPattern.favoriteTopics[i++] = t.as<String>();
  }
  f.close();
}

void saveSentimentData() {
  JsonDocument doc; JsonArray arr = doc["history"].to<JsonArray>();
  for (const auto& s : sentimentHistory) {
    JsonObject o = arr.add<JsonObject>();
    o["s"] = s.sentiment; o["c"] = s.score; o["t"] = s.timestamp;
  }
  File f = FFat.open("/sentiment.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadSentimentData() {
  if (!FFat.exists("/sentiment.json")) return;
  File f = FFat.open("/sentiment.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  sentimentHistory.clear();
  for (JsonObject o : doc["history"].as<JsonArray>())
    sentimentHistory.push_back({o["s"].as<String>(), o["c"]|0.5f, o["t"]|0UL});
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 16 ── KNOWLEDGE DOMAINS
// ═══════════════════════════════════════════════════════

void initializeKnowledgeDomains() {
  knowledgeDomains = {
    {"personal",      0, 0.3f, 255, 200, 100},
    {"technical",     0, 0.3f, 100, 150, 255},
    {"weather",       0, 0.3f, 150, 220, 255},
    {"reminders",     0, 0.3f, 180, 100, 255},
    {"general",       0, 0.3f, 100, 255, 150},
    {"news",          0, 0.3f, 255, 150,  50},
    {"entertainment", 0, 0.3f, 255, 100, 200},
    {"health",        0, 0.3f,  80, 220,  80},
    {"finance",       0, 0.3f, 200, 200,  50},
    {"food",          0, 0.3f, 255, 140,   0},
    {"sports",        0, 0.3f,  50, 200, 255},
  };
  saveKnowledgeDomains();
}

void updateKnowledgeDomain(const String& domain, int xpGain) {
  for (auto& kd : knowledgeDomains) {
    if (kd.domain == domain) {
      int oldXP = kd.experiencePoints; kd.experiencePoints += xpGain;
      kd.confidenceLevel = min(0.95f, 0.3f + kd.experiencePoints / 500.0f);
      if ((oldXP/100) < (kd.experiencePoints/100)) { aiState = AI_EVOLVING; stateChangeTime = millis(); }
      g_dirtyKnowledge = true; return;
    }
  }
  for (auto& kd : knowledgeDomains)
    if (kd.domain == "general") { kd.experiencePoints += xpGain; g_dirtyKnowledge = true; return; }
}

KnowledgeArea* getDominantKnowledge() {
  if (knowledgeDomains.empty()) return nullptr;
  return &*std::max_element(knowledgeDomains.begin(), knowledgeDomains.end(),
    [](const KnowledgeArea& a, const KnowledgeArea& b){ return a.experiencePoints < b.experiencePoints; });
}

void saveKnowledgeDomains() {
  JsonDocument doc; JsonArray arr = doc["domains"].to<JsonArray>();
  for (const auto& kd : knowledgeDomains) {
    JsonObject o = arr.add<JsonObject>();
    o["domain"] = kd.domain; o["xp"] = kd.experiencePoints; o["conf"] = kd.confidenceLevel;
    o["r"] = kd.colorR; o["g"] = kd.colorG; o["b"] = kd.colorB;
  }
  File f = FFat.open("/knowledge.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadKnowledgeDomains() {
  if (!FFat.exists("/knowledge.json")) return;
  File f = FFat.open("/knowledge.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  knowledgeDomains.clear();
  for (JsonObject o : doc["domains"].as<JsonArray>())
    knowledgeDomains.push_back({
      o["domain"].as<String>(), o["xp"]|0, o["conf"]|0.3f,
      (uint8_t)(o["r"]|100), (uint8_t)(o["g"]|100), (uint8_t)(o["b"]|100)
    });
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 17 ── WEB / WEATHER
// ═══════════════════════════════════════════════════════

bool serperRequest(const String& query, int num, const String& tbs, JsonDocument& doc) {
  JsonDocument reqDoc; reqDoc["q"] = query; reqDoc["num"] = num;
  if (tbs.length() > 0) reqDoc["tbs"] = tbs;
  String body; serializeJson(reqDoc, body);

  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http;
  http.begin(secClient, "https://google.serper.dev/search");
  http.addHeader("X-API-KEY", Config::SERPER_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(12000);

  esp_task_wdt_reset();
  int code = http.POST(body);
  esp_task_wdt_reset();
  if (code <= 0) { http.end(); return false; }

  bool parsed = false;
  doc.clear();
  if (g_psramRespBuf) {
    int n = http.getStream().readBytes(g_psramRespBuf, Config::PSRAM_RESP_SIZE - 1);
    g_psramRespBuf[n] = '\0';
    parsed = !deserializeJson(doc, g_psramRespBuf);
  } else {
    parsed = !deserializeJson(doc, http.getString());
  }
  http.end();
  return parsed && doc.containsKey("organic") && doc["organic"].size() > 0;
}

String fetchWebSearchResults(const String& query) {
  if (query.length() == 0) return "";
  String lq = query; lq.toLowerCase();
  bool recency = isRecencyQuery(lq);
  JsonDocument doc; bool got = false;
  if (recency) { got = serperRequest(query, 5, "qdr:m", doc); if (!got) got = serperRequest(query, 5, "qdr:y", doc); }
  if (!got)      got = serperRequest(query, 5, "", doc);
  if (!got || !doc.containsKey("organic")) return "";

  String result = "Search results for: \"" + query + "\"\n";
  int n = doc["organic"].size();
  for (int i = 0; i < 5 && i < n; i++) {
    String title   = doc["organic"][i]["title"].as<String>();
    String snippet = doc["organic"][i]["snippet"].as<String>();
    String date    = doc["organic"][i]["date"]|"";
    result += "- " + title + ": " + snippet;
    if (date.length() > 0) result += " [" + date + "]";
    result += "\n";
  }
  return result;
}

String httpGetWithRetry(const String& url, int maxRetries, int delayMs) {
  for (int i = 1; i <= maxRetries; i++) {
    esp_task_wdt_reset();
    WiFiClientSecure secClient; secClient.setInsecure();
    HTTPClient http; http.begin(secClient, url);
    int code = http.GET(); esp_task_wdt_reset();
    if (code > 0) { String r = http.getString(); http.end(); return r; }
    http.end();
    if (i < maxRetries) delay(delayMs * i);
  }
  return "";
}

static String resolveMeteosourcePlaceId(const String& city, String& outDisplay) {
  String q = city; q.trim(); String qEnc = urlEncode(q);
  String url = "https://www.meteosource.com/api/v1/free/find_places?text=" + qEnc + "&key=" + String(Config::WEATHER_KEY);
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.begin(secClient, url); http.setTimeout(10000);
  esp_task_wdt_reset(); int code = http.GET(); esp_task_wdt_reset();
  String placeId = "";
  if (code > 0) {
    JsonDocument doc; bool parsed = false;
    if (g_psramRespBuf) {
      int n = http.getStream().readBytes(g_psramRespBuf, Config::PSRAM_RESP_SIZE - 1);
      g_psramRespBuf[n] = '\0'; parsed = !deserializeJson(doc, g_psramRespBuf);
    } else { String body = http.getString(); parsed = !deserializeJson(doc, body); }
    if (parsed && doc.is<JsonArray>() && doc.size() > 0) {
      const char* pid = doc[0]["place_id"]|"", *nmC = doc[0]["name"]|"",
                 *adC = doc[0]["adm_area1"]|"", *ctC = doc[0]["country"]|"";
      placeId = String(pid);
      String nm(nmC), adm(adC), ctr(ctC);
      outDisplay = nm.length() ? nm : q;
      if (adm.length()) outDisplay += ", " + adm;
      else if (ctr.length()) outDisplay += ", " + ctr;
    }
  }
  http.end(); return placeId;
}

void getWeather(String city) {
  city.trim(); if (city.length() == 0) {
    city = recallFact("city");
    if (city.length() == 0) city = "Colombo";
  }
  String display = city;
  String placeId = resolveMeteosourcePlaceId(city, display);
  if (placeId.length() == 0) {
    Serial.println("⚠️  No match found for: " + city + " (try a more specific name)");
    return;
  }
  String url = "https://www.meteosource.com/api/v1/free/point?place_id=" + placeId +
               "&sections=current&units=metric&key=" + String(Config::WEATHER_KEY);
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.begin(secClient, url); http.setTimeout(12000);
  esp_task_wdt_reset(); int code = http.GET(); esp_task_wdt_reset();
  if (code > 0) {
    JsonDocument doc; DeserializationError err;
    if (g_psramRespBuf) {
      int n = http.getStream().readBytes(g_psramRespBuf, Config::PSRAM_RESP_SIZE - 1);
      g_psramRespBuf[n] = '\0'; err = deserializeJson(doc, g_psramRespBuf);
    } else { String body = http.getString(); err = deserializeJson(doc, body); }
    if (!err && doc.containsKey("current")) {
      float temp  = doc["current"]["temperature"];
      float feels = doc["current"]["feels_like"];
      String summary = doc["current"]["summary"]|"";
      int wind  = doc["current"]["wind"]["speed"]|0;
      int humid = doc["current"]["cloud_cover"]|0;
      Serial.printf("🌤️  %s: %.1f°C (feels %.1f°C) %s — Wind: %dkm/h\n",
                    display.c_str(), temp, feels, summary.c_str(), wind);
    } else {
      const char* apiMsg = doc["detail"]|doc["message"]|"";
      if (strlen(apiMsg) > 0) Serial.println("⚠️  Weather: " + String(apiMsg));
      else Serial.println("⚠️  Weather data unavailable for: " + display);
    }
  } else {
    Serial.println("❌ Weather request failed (HTTP " + String(code) + ")");
  }
  http.end();
}

void searchWeb(const String& query) {
  if (query.length() == 0) return;
  String results = fetchWebSearchResults(query);
  if (results.length() > 0) Serial.println("\n🔎 " + results);
  else Serial.println("⚠️  No results found.");
}

String urlEncode(const String& str) {
  String enc = "";
  for (char c : str) {
    if (isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') enc += c;
    else if (c == ' ') enc += "%20";
    else { char buf[4]; sprintf(buf, "%%%.2X", (unsigned char)c); enc += buf; }
  }
  return enc;
}

// ═══════════════════════════════════════════════════════
// SECTION 18 ── CHAT HISTORY  (v1.7.7: AI summarization)
// ═══════════════════════════════════════════════════════

int estimateTokens(const char* text) { return strlen(text) / 4; }

// v1.7.7: True AI-powered summarization — replaces crude string concatenation
void summarizeChatHistory() {
  if ((int)chatHistory.size() < 12) return;
  // Only trigger at 75% capacity
  if ((int)chatHistory.size() < (Config::MAX_CHAT_MESSAGES * 3 / 4)) return;

  Serial.println("📋 Compressing conversation history...");

  // Collect oldest half of messages (excluding any existing summary)
  int cutoff = (int)chatHistory.size() / 2;
  String historyText = "";
  int startIdx = 0;
  // Skip leading summary messages
  while (startIdx < cutoff && String(chatHistory[startIdx].role) == "system") startIdx++;

  for (int i = startIdx; i < cutoff; i++) {
    String role = String(chatHistory[i].role);
    if (role == "assistant") role = "Assistant";
    else if (role == "user") role = "User";
    historyText += role + ": " + String(chatHistory[i].content) + "\n";
  }

  if (historyText.length() == 0) return;

  // AI-generate a real summary
  String summaryPrompt =
    "Summarise this conversation segment concisely in 3-5 sentences, preserving:\n"
    "- Key facts learned about the user (name, preferences, facts)\n"
    "- Important decisions or tasks mentioned\n"
    "- The main topics discussed\n"
    "- Any reminders or follow-ups agreed\n\n"
    "Conversation:\n" + historyText + "\n\nSummary (3-5 sentences, plain text only):";

  String summary = groqSimpleCall(summaryPrompt, 0.1f, 200);

  if (summary.length() == 0) {
    // Fallback to basic concatenation if Groq unavailable
    summary = "[Previous conversation: " + historyText.substring(0, min((int)historyText.length(), 400)) + "]";
  }

  // Remove the summarised messages and insert summary at start
  chatHistory.erase(chatHistory.begin() + startIdx, chatHistory.begin() + cutoff);

  ChatMessage s;
  strlcpy(s.role, "system", sizeof(s.role));
  String full = "[Conversation Summary] " + summary;
  full.toCharArray(s.content, sizeof(s.content));
  chatHistory.insert(chatHistory.begin() + startIdx, s);

  Serial.println("✅ History compressed. Summary: " + summary.substring(0, min((int)summary.length(), 80)) + "...");
}

void limitChatHistoryByTokens(int maxTokens) {
  int total = 0;
  for (auto& m : chatHistory) total += estimateTokens(m.content);
  while (total > maxTokens && chatHistory.size() > 2) {
    total -= estimateTokens(chatHistory[0].content);
    chatHistory.erase(chatHistory.begin());
  }
}

void addUserMessage(const String& msg) {
  ChatMessage m; strlcpy(m.role, "user", sizeof(m.role));
  msg.toCharArray(m.content, sizeof(m.content));
  chatHistory.push_back(m);
  limitChatHistoryByTokens();
  saveChatHistory();
}

void addAssistantMessage(const String& msg) {
  ChatMessage m; strlcpy(m.role, "assistant", sizeof(m.role));
  msg.toCharArray(m.content, sizeof(m.content));
  chatHistory.push_back(m);
  summarizeChatHistory();  // AI summarization — called once per exchange
  limitChatHistoryByTokens();
  saveChatHistory();
}

void saveChatHistory() {
  JsonDocument doc; JsonArray arr = doc["history"].to<JsonArray>();
  for (const auto& m : chatHistory) {
    JsonObject o = arr.add<JsonObject>(); o["role"] = m.role; o["content"] = m.content;
  }
  File f = FFat.open("/chat.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadChatHistory() {
  if (!FFat.exists("/chat.json")) return;
  File f = FFat.open("/chat.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  chatHistory.clear();
  for (JsonObject o : doc["history"].as<JsonArray>()) {
    ChatMessage m;
    strlcpy(m.role,    o["role"]   |"", sizeof(m.role));
    strlcpy(m.content, o["content"]|"", sizeof(m.content));
    chatHistory.push_back(m);
  }
  f.close();
}

// ═══════════════════════════════════════════════════════
// SECTION 19 ── PROACTIVE & BRIEFING  (v1.7.7: improved)
// ═══════════════════════════════════════════════════════

bool autoMorningBriefing() { return morningBriefingEnabled; }

void generateMorningBriefing() {
  String name = recallFact("name");
  String greeting = name.length() > 0 ? "☀️  Good morning, " + name + "!" : "☀️  Good morning!";
  Serial.println("\n" + greeting + " ═══════════════════════════════");
  Serial.println("📅 " + String(day()) + "/" + String(month()) + "/" + String(year()));

  String city = recallFact("city");
  if (city.length() == 0) city = "Colombo";
  getWeather(city);

  // Today's reminders
  int todayDay = weekday(), todayDate = day(), shown = 0;
  for (const auto& r : reminders) {
    bool today = (r.recurrence == ONCE || r.recurrence == DAILY) ||
                 (r.recurrence == WEEKLY  && r.dayOfWeek  == todayDay) ||
                 (r.recurrence == MONTHLY && r.dayOfMonth == todayDate);
    if (today) {
      if (shown == 0) Serial.println("⏰ Today's reminders:");
      Serial.println("   • " + r.message + " at " + formatReminderTime(r.hour, r.minute));
      shown++;
    }
  }
  if (shown == 0) Serial.println("📅 No reminders set for today.");

  // Mood-based message
  if (userPattern.recentMood == "negative")
    Serial.println("💙 Yesterday was tough — today is a fresh start. You've got this.");
  else if (userPattern.recentMood == "positive")
    Serial.println("😊 You've been in great spirits — let's keep that energy going!");
  else
    Serial.println("💡 Have a great and productive day.");

  Serial.println("═══════════════════════════════════════\n");
}

// v1.7.7: Evening summary — AI-generated day recap
void generateEveningSummary() {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return;

  String name = recallFact("name");
  String heading = name.length() > 0 ? "🌙 Good evening, " + name + "!" : "🌙 Good evening!";
  Serial.println("\n" + heading + " ═══════════════════════════════");

  // Build day data for AI
  String dayData = "Time: " + String(hour()) + ":00\n";
  dayData += "Interactions today: ~" + String(userPattern.totalInteractions) + " total\n";
  dayData += "Recent mood: " + userPattern.recentMood + "\n";
  if (!reminders.empty()) {
    dayData += "Active reminders for tomorrow:\n";
    int show = min((int)reminders.size(), 3);
    for (int i = 0; i < show; i++)
      dayData += "  - " + reminders[i].message + " at " + formatReminderTime(reminders[i].hour, reminders[i].minute) + "\n";
  }
  if (!memory.empty()) {
    dayData += "Things I know about you: ";
    int cnt = 0;
    for (const auto& f : memory) { if (cnt++ < 4) dayData += f.key + "=" + f.value + " "; }
    dayData += "\n";
  }

  String sys = "You are a personal AI assistant giving a brief, warm evening summary. "
               "Keep it to 4-6 sentences. Be encouraging, personal, and helpful. "
               "Mention tomorrow's reminders if any. Close with a positive note. No bullet points.";
  Serial.println("📊 AI evening summary:");
  groqStream(dayData, sys, 0.7f, 200);
  Serial.println("\n═══════════════════════════════════════\n");
}

void checkProactiveOpportunity() {
  unsigned long epochNow = timeClient.getEpochTime();
  // Don't be proactive if recently active (< 20 min)
  if (userPattern.lastInteraction > 0 && (epochNow - userPattern.lastInteraction) < 1200) return;

  int h = hour(); String msg = "";

  if (h >= 7 && h < 9 && userPattern.morningChats > 2)
    msg = "Good morning! Want weather info or help planning your day?";
  else if (h >= 12 && h < 13)
    msg = "Hey! Lunchtime — want to set a reminder for anything this afternoon?";
  else if (h >= 15 && h < 16 && !reminders.empty())
    msg = "Afternoon check-in — you have " + String(reminders.size()) + " reminder(s) active. All good?";
  else if (h >= 18 && h < 20 && userPattern.eveningChats > 2)
    msg = "Evening! Want help planning tomorrow or a quick catch-up?";
  else if (consecutiveNegative >= 2)
    msg = "Hey — you've seemed a bit stressed lately. Want to talk about it?";

  if (msg.length() > 0) {
    String name = recallFact("name");
    if (name.length() > 0 && msg[0] != 'H') msg = name + ", " + msg;
    Serial.println("\n💡 " + msg);
    aiState = AI_PROACTIVE; stateChangeTime = millis();
  }
}

// ═══════════════════════════════════════════════════════
// SECTION 20 ── UTILITY / DIAGNOSTICS
// ═══════════════════════════════════════════════════════

bool heapOk() {
  size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return internalFree > Config::HEAP_SAFE_BYTES;
}

float getCpuTemp() {
  if (!g_tsens) return -1.0f;
  float celsius = 0.0f;
  temperature_sensor_get_celsius(g_tsens, &celsius);
  return celsius;
}

void systemDiagnostics() {
  unsigned long uptimeSec = (millis() - bootTime) / 1000;
  uint32_t intHeap   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t totalHeap = ESP.getFreeHeap();
  float cpuTemp      = getCpuTemp();
  int   cpuFreq      = getCpuFrequencyMhz();  // v1.7.7

  Serial.println("\n📊 ═══ SYSTEM DIAGNOSTICS ═══");
  Serial.printf("  Version:     %s\n", Config::VERSION);
  Serial.printf("  Model:       %s\n", Config::GROQ_MODEL);
  Serial.printf("  Uptime:      %lu s  (%lu h %lu m)\n", uptimeSec, uptimeSec/3600, (uptimeSec%3600)/60);
  Serial.printf("  CPU Temp:    %.1f °C%s\n", cpuTemp,
                cpuTemp > 75 ? " ⚠️  CRITICAL" : cpuTemp > 65 ? " ⚠️  HIGH" :
                cpuTemp > 55 ? " ⚠️  WARM" : " ✅");
  Serial.printf("  CPU Freq:    %d MHz  %s\n", cpuFreq,  // v1.7.7
                cpuFreq == Config::CPU_FREQ_IDLE ? "(idle)" : "(active)");
  Serial.printf("  Int SRAM:    %u bytes %s\n", intHeap, heapOk() ? "✅" : "⚠️  LOW — consider /clear");
  Serial.printf("  Total heap:  %u bytes (incl. OPI PSRAM)\n", totalHeap);
  if (g_psramReqBuf && g_psramRespBuf)
    Serial.printf("  PSRAM bufs:  req=%uKB resp=%uKB  free=%u KB\n",
                  (unsigned)Config::PSRAM_REQ_SIZE/1024, (unsigned)Config::PSRAM_RESP_SIZE/1024,
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024));
  if (heapSnapshot > 0) {
    int delta = (int)intHeap - (int)heapSnapshot;
    unsigned long hr = (millis()-heapSnapshotTime)/3600000UL,
                  mn = ((millis()-heapSnapshotTime)%3600000UL)/60000UL;
    Serial.printf("  SRAM trend:  was %u bytes ~%luh%02lum ago  (%+d bytes)\n",
                  heapSnapshot, hr, mn, delta);
  }
  Serial.printf("  WiFi:        %s  (reconnects: %d)\n",
                WiFi.status()==WL_CONNECTED ? "Connected ✅" : "Offline ❌", wifiReconnectCount);
  Serial.printf("  Dual-core:   Core 0 HTTP worker %s\n", g_dcTask ? "running ✅" : "not started ⚠️");
  Serial.printf("  HTTP errors: %d this session\n", httpTimeoutCount);
  Serial.printf("  Interactions:%d\n", userPattern.totalInteractions);
  Serial.printf("  Mood:        %s  (temp=%.2f)\n", userPattern.recentMood.c_str(), computeMoodTemperature());
  Serial.printf("  Reminders:   %d / %d\n", (int)reminders.size(), Config::MAX_REMINDERS);
  Serial.printf("  Memory facts:%d / %d\n", (int)memory.size(), Config::MAX_MEMORY_FACTS);
  Serial.printf("  Chat msgs:   %d / %d\n", (int)chatHistory.size(), Config::MAX_CHAT_MESSAGES);
  Serial.printf("  Skills:      %d / %d\n", (int)skillNames.size(), Config::MAX_SKILLS);
  KnowledgeArea* dom = getDominantKnowledge();
  if (dom) Serial.printf("  Top domain:  %s  (XP:%d  conf:%.0f%%)\n",
                         dom->domain.c_str(), dom->experiencePoints, dom->confidenceLevel*100);
  Serial.println("══════════════════════════════");

  if (WiFi.status() != WL_CONNECTED) { Serial.println("⚠️  Offline — skipping AI health scan."); return; }
  Serial.println("\n🧠 AI Health Scan running...\n");

  String tele = "TELEMETRY:\n";
  tele += "int_sram=" + String(intHeap) + "B total_heap=" + String(totalHeap) + "B";
  if (heapSnapshot > 0) {
    int delta = (int)intHeap - (int)heapSnapshot;
    unsigned long hr = max(1UL, (millis()-heapSnapshotTime)/3600000UL);
    tele += " sram_drift=" + (delta >= 0 ? String("+") : String("")) + String(delta) + "B_over_" + String(hr) + "h";
  }
  tele += " cpu_temp=" + String(cpuTemp,1) + "C cpu_freq=" + String(cpuFreq) + "MHz";
  tele += " uptime=" + String(uptimeSec/3600) + "h" + String((uptimeSec%3600)/60) + "m";
  tele += " http_errors=" + String(httpTimeoutCount) + " wifi_reconnects=" + String(wifiReconnectCount);
  tele += " reminders=" + String(reminders.size()) + " facts=" + String(memory.size());
  tele += " chat=" + String(chatHistory.size()) + " interactions=" + String(userPattern.totalInteractions);
  tele += " mood=" + userPattern.recentMood;

  String sys =
    "You are an automated ESP32 firmware health scanner.\n"
    "Output ONLY a compact scan report — no numbered lists, no paragraphs.\n\n"
    "Format EXACTLY:\n"
    "──────────────────────────────\n"
    "CPU       61.3 °C     ⚠️  WARM — ventilate\n"
    "SRAM      31840 B     ✅  Stable\n"
    "SRAM drift -12360 B   ⚠️  Possible leak — run /clear if worsens\n"
    "WiFi      Connected   ✅  (2 reconnects)\n"
    "HTTP err  3           ⚠️  Check API key / signal\n"
    "Uptime    2h 37m      ✅\n"
    "CPU freq  80 MHz      ✅  Idle-scaled\n"
    "──────────────────────────────\n"
    "ACTION: one-line fix if needed. If all OK: All systems nominal.\n\n"
    "Rules: one line per metric, ✅ / ⚠️ / ❌, brief note only if bad. Max 15 lines.";

  groqStream(tele, sys, 0.1f, 350);
  Serial.println("\n══════════════════════════════");
}

void clearAll() {
  memory.clear(); chatHistory.clear(); reminders.clear();
  sentimentHistory.clear(); userPattern = UserPattern(); knowledgeDomains.clear();
  const char* files[] = {
    "/memory.json","/chat.json","/reminders.json",
    "/pattern.json","/sentiment.json","/knowledge.json","/skills.json", nullptr
  };
  for (int i = 0; files[i]; i++) FFat.remove(files[i]);
  aiState = AI_IDLE;
  Serial.println("✅ All data cleared. Restarting...");
  delay(1000); ESP.restart();
}

void printHelp() {
  Serial.println("\n📖 ═══ " + String(Config::VERSION) + " HELP ═══");
  Serial.println("Commands:");
  Serial.println("  /help             — This help screen");
  Serial.println("  /version          — Version + stats");
  Serial.println("  /diag             — Full diagnostics + AI health scan");
  Serial.println("  /reminders        — List all reminders");
  Serial.println("  /remove N         — Delete reminder N");
  Serial.println("  /memory           — Show all stored facts");
  Serial.println("  /summary          — AI-compress conversation history");
  Serial.println("  /weather [city]   — Live weather (uses stored city if omitted)");
  Serial.println("  /search [query]   — Web search + AI summary");
  Serial.println("  /clear            — Wipe all data & restart");
  Serial.println("  /skills           — List self-taught skills");
  Serial.println("  /skills remove [name] — Forget a skill");
  Serial.println("  /skills keep      — Save pending skill");
  Serial.println("  /skills discard   — Discard pending skill");
  Serial.println("  /skills retry     — Regenerate pending skill");
  Serial.println("\nNatural language — say anything, any way, e.g.:");
  Serial.println("  \"remember my name is Cash\"");
  Serial.println("  \"Can you please remember that my name is Cash\"");
  Serial.println("  \"hey, my name is Cash\"");
  Serial.println("  \"call me Cash\"");
  Serial.println("  \"remind me to take medicine at 8pm\"");
  Serial.println("  \"what's the weather in London\"");
  Serial.println("  \"what do you know about me\"");
  Serial.println("  \"search for best laptops 2025\"");
  Serial.println("  \"don't let me forget my meeting at 3pm\"");
  Serial.println("═══════════════════════════════════════════");
}

void printVersion() {
  Serial.println("\n" + String(Config::VERSION));
  Serial.println("Model:     " + String(Config::GROQ_MODEL));
  Serial.println("CPU freq:  " + String(getCpuFrequencyMhz()) + " MHz");  // v1.7.7
  Serial.println("Mood temp: " + String(computeMoodTemperature(), 2));     // v1.7.7
  Serial.println("Reminders: " + String(reminders.size()) + "/" + String(Config::MAX_REMINDERS));
  Serial.println("Facts:     " + String(memory.size()) + "/" + String(Config::MAX_MEMORY_FACTS));
  Serial.println("Chat msgs: " + String(chatHistory.size()) + "/" + String(Config::MAX_CHAT_MESSAGES));
  Serial.println("Skills:    " + String(skillNames.size()) + "/" + String(Config::MAX_SKILLS));
  Serial.println("Int SRAM:  " + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) + " bytes");
  Serial.println("PSRAM:     " + String(g_psramReqBuf ? "enabled" : "disabled"));
  Serial.println("Dual-core: " + (g_dcTask ? String("Core 0 worker running") : String("single-core")));
  Serial.println("Uptime:    " + String((millis()-bootTime)/1000) + " s");
  Serial.println("CPU temp:  " + String(getCpuTemp(), 1) + " °C");
}

// ═══════════════════════════════════════════════════════
// SECTION 21 ── LED
// ═══════════════════════════════════════════════════════

void setLEDColor(uint8_t r, uint8_t g, uint8_t b, float brightness) {
  brightness = constrain(brightness, 0.0f, 1.0f);
  strip.setPixelColor(0, strip.Color(
    (uint8_t)(r * brightness), (uint8_t)(g * brightness), (uint8_t)(b * brightness)));
  strip.show();
}

void updateLED() {
  unsigned long now = millis();
  auto pulse = [&](uint8_t r, uint8_t g, uint8_t b, float speed, float maxB) {
    if (now - lastBlink > (unsigned long)(1000.0f / (speed * 60))) {
      pulseBrightness += pulseIncreasing ? 0.02f : -0.02f;
      if (pulseBrightness >= maxB) pulseIncreasing = false;
      if (pulseBrightness <= Config::LED_MIN_BRIGHTNESS) pulseIncreasing = true;
      setLEDColor(r, g, b, pulseBrightness); lastBlink = now;
    }
  };
  switch (aiState) {
    case AI_IDLE:    setLEDColor(0, 0, 0, 0); break;
    case AI_THINKING:pulse(0, 0, 255, 1.0f + thinkingComplexity*0.1f, Config::LED_MAX_BRIGHTNESS); break;
    case AI_REPLIED: setLEDColor(0, 255, 0, Config::LED_MAX_BRIGHTNESS);
                     if (now - stateChangeTime > Config::REPLIED_FLASH_MS) aiState = AI_IDLE; break;
    case AI_ERROR:
      if (now - lastBlink > 300) {
        setLEDColor(255, 0, 0, (blinkCount%2) ? 0 : Config::LED_MAX_BRIGHTNESS);
        if (++blinkCount >= 6) { blinkCount = 0; aiState = AI_IDLE; } lastBlink = now;
      } break;
    case AI_ALERT:
      pulse(255, 50, 0, 1.2f, Config::LED_MAX_BRIGHTNESS);
      if (now - stateChangeTime > Config::REMINDER_ALERT_MS) aiState = AI_IDLE; break;
    case AI_EXCITED:
      if (now - lastBlink > 80) {
        setLEDColor(255, 215, 0,
          Config::LED_MIN_BRIGHTNESS + random(100)/100.0f*(Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS));
        lastBlink = now;
      }
      if (now - stateChangeTime > 3000) aiState = AI_IDLE; break;
    case AI_CONCERNED:
      setLEDColor(100, 150, 255, Config::LED_MAX_BRIGHTNESS * 0.7f);
      if (now - stateChangeTime > 5000) aiState = AI_IDLE; break;
    case AI_PROACTIVE:
      pulse(128, 0, 255, 0.8f, Config::LED_MAX_BRIGHTNESS * 0.8f);
      if (now - stateChangeTime > 5000) aiState = AI_IDLE; break;
    case AI_LEARNING:
      pulse(0, 255, 255, 1.5f, Config::LED_MAX_BRIGHTNESS * 0.9f);
      if (now - stateChangeTime > 2000) aiState = AI_IDLE; break;
    case AI_EVOLVING: {
      KnowledgeArea* dom = getDominantKnowledge();
      if (dom && now - lastBlink > 100) {
        float b = Config::LED_MIN_BRIGHTNESS + (Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS)*0.6f;
        setLEDColor(dom->colorR, dom->colorG, dom->colorB, b); lastBlink = now;
      }
      if (now - stateChangeTime > 1500) aiState = AI_IDLE; break;
    }
  }
}

void rainbowWave(int durationMs) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)durationMs) {
    for (int i = 0; i < 256 && millis()-start < (unsigned long)durationMs; i += 4) {
      uint8_t r = (i<85)?255-i*3:(i<170)?0:(i-170)*3;
      uint8_t g = (i<85)?i*3:(i<170)?255-(i-85)*3:0;
      uint8_t b = (i<85)?0:(i<170)?(i-85)*3:255-(i-170)*3;
      setLEDColor(r, g, b, 0.25f); delay(8);
    }
  }
  setLEDColor(0, 0, 0, 0);
}

// ═══════════════════════════════════════════════════════
// SECTION 22 ── SELF-TAUGHT SKILLS ENGINE  (v1.7.6 DSL, preserved)
// ═══════════════════════════════════════════════════════

bool isAlnumCh(char c) { return isalnum((unsigned char)c) || c=='_'; }
char toLowerCh(char c) { return (c>='A'&&c<='Z')?(c+32):c; }

String stripToAlnum(const String& s) {
  String r=""; for(char c:s) if(isAlnumCh(c)) r+=toLowerCh(c); return r;
}

String extractKeywords(String phrase) {
  phrase.toLowerCase();
  static const char* stop[]={"a","an","the","to","of","for","in","on","at","is","are","am",
    "i","my","me","you","your","it","this","that","please","can","could","would","will",
    "do","does","did","have","has","had","be","been","being","and","or","but","with",nullptr};
  String result=""; int start=0;
  for(int i=0;i<=(int)phrase.length();i++){
    if(i==(int)phrase.length()||!isAlnumCh(phrase[i])){
      if(i>start){
        String word=phrase.substring(start,i); bool isStop=false;
        for(int j=0;stop[j];j++) if(word==stop[j]){isStop=true;break;}
        if(!isStop&&word.length()>2){if(result.length())result+=" "; result+=word;}
      }
      start=i+1;
    }
  }
  return result;
}

void rebuildSkillTriggerIndex() {
  skillTriggerIndex.clear();
  for (size_t i = 0; i < skillJson.size(); i++) {
    JsonDocument doc; if (deserializeJson(doc, skillJson[i])) continue;
    for (JsonPair kv : doc["triggers"].as<JsonObject>()) {
      for (JsonVariant p : kv.value().as<JsonArray>()) {
        String phrase = p.as<String>(); phrase.toLowerCase();
        skillTriggerIndex.push_back({(int)i, String(kv.key().c_str()), phrase, extractKeywords(phrase)});
      }
    }
  }
}

bool matchLearnedSkill(const String& input, int& outSkillIdx, String& outAction) {
  String lower = input; lower.toLowerCase();
  for (const auto& t : skillTriggerIndex) {
    if (lower.indexOf(t.phrase) >= 0) { outSkillIdx = t.skillIdx; outAction = t.action; return true; }
  }
  // Fuzzy keyword match
  String inputKw = extractKeywords(lower);
  const SkillTrigger* best = nullptr; int bestScore = 0;
  for (const auto& t : skillTriggerIndex) {
    if (t.keywords.length() == 0) continue;
    int score = 0;
    int from = 0, p;
    while ((p = t.keywords.indexOf(' ', from)) >= 0) {
      String kw = t.keywords.substring(from, p);
      if (kw.length() > 2 && inputKw.indexOf(kw) >= 0) score++;
      from = p + 1;
    }
    String last = t.keywords.substring(from);
    if (last.length() > 2 && inputKw.indexOf(last) >= 0) score++;
    if (score > bestScore) { bestScore = score; best = &t; }
  }
  if (best && bestScore >= 2) { outSkillIdx = best->skillIdx; outAction = best->action; return true; }
  return false;
}

// ── Expression evaluator ─────────────────────────────
namespace SkillExpr {
  struct Parser {
    const String& s; size_t pos = 0;
    JsonObject vars; JsonObject strvars;
    Parser(const String& s_, JsonObject v, JsonObject sv) : s(s_), vars(v), strvars(sv) {}
    void skipSpace() { while (pos < s.length() && s[pos]==' ') pos++; }
    float resolveIdent(const String& id) {
      if (id=="MILLIS")    return (float)millis();
      if (id=="NOW_HOUR")  return (float)hour();
      if (id=="NOW_MIN")   return (float)minute();
      if (id=="NOW_SEC")   return (float)second();
      if (id=="NOW_DAY")   return (float)day();
      if (id=="NOW_MONTH") return (float)month();
      if (id=="NOW_YEAR")  return (float)year();
      if (id=="RAND100")   return (float)(esp_random()%101);
      if (vars.containsKey(id)) return vars[id].as<float>();
      if (id.startsWith("STRLEN_")) {
        String sv = id.substring(7);
        if (strvars.containsKey(sv)) return (float)strlen(strvars[sv]|"");
      }
      return 0.0f;
    }
    float tryFunction(const String& name) {
      skipSpace(); if (pos >= s.length() || s[pos]!='(') return resolveIdent(name);
      pos++;
      float a = parseExpr(); skipSpace();
      if (name=="ABS")   { if(pos<s.length()&&s[pos]==')')pos++; return fabsf(a); }
      if (name=="FLOOR") { if(pos<s.length()&&s[pos]==')')pos++; return floorf(a); }
      if (name=="CEIL")  { if(pos<s.length()&&s[pos]==')')pos++; return ceilf(a); }
      if (name=="ROUND") { if(pos<s.length()&&s[pos]==')')pos++; return roundf(a); }
      if (name=="SIN")   { if(pos<s.length()&&s[pos]==')')pos++; return sinf(a); }
      if (name=="COS")   { if(pos<s.length()&&s[pos]==')')pos++; return cosf(a); }
      if (pos<s.length()&&s[pos]==',') pos++;
      float b = parseExpr(); skipSpace();
      if (pos<s.length()&&s[pos]==')') pos++;
      if (name=="MIN")    return min(a,b);
      if (name=="MAX")    return max(a,b);
      if (name=="RANDOM") return (float)((int)a+(int)(esp_random()%max(1,(int)(b-a)+1)));
      if (name=="MOD")    return (b!=0)?(float)((long)a%(long)b):0.0f;
      return a;
    }
    float parseNumberOrIdent() {
      skipSpace();
      if (pos<s.length()&&(isDigit(s[pos])||s[pos]=='.')) {
        size_t start=pos; while(pos<s.length()&&(isDigit(s[pos])||s[pos]=='.'))pos++;
        return s.substring(start,pos).toFloat();
      }
      size_t start=pos;
      while(pos<s.length()&&(isAlphaNumeric(s[pos])||s[pos]=='_'))pos++;
      if(pos==start) return 0.0f;
      return tryFunction(s.substring(start,pos));
    }
    float parseFactor() {
      skipSpace();
      if(pos<s.length()&&s[pos]=='('){pos++;float v=parseExpr();skipSpace();if(pos<s.length()&&s[pos]==')')pos++;return v;}
      if(pos<s.length()&&s[pos]=='-'){pos++;return -parseFactor();}
      return parseNumberOrIdent();
    }
    float parseTerm() {
      float v=parseFactor();
      for(;;){skipSpace(); if(pos<s.length()&&(s[pos]=='*'||s[pos]=='/')){char op=s[pos++];float rhs=parseFactor();v=(op=='*')?v*rhs:(rhs!=0?v/rhs:0);}else break;}
      return v;
    }
    float parseExpr() {
      float v=parseTerm();
      for(;;){skipSpace(); if(pos<s.length()&&(s[pos]=='+'||s[pos]=='-')){char op=s[pos++];float rhs=parseTerm();v=(op=='+')?v+rhs:v-rhs;}else break;}
      return v;
    }
  };
  float eval(const String& expr, JsonObject vars, JsonObject strvars={}) {
    Parser p(expr,vars,strvars); return p.parseExpr();
  }
}

static String formatVarValue(const String& varName, const String& fmt, JsonObject vars, JsonObject strvars) {
  if (strvars.containsKey(varName)) return strvars[varName].as<String>();
  float val = vars.containsKey(varName) ? vars[varName].as<float>() : 0.0f;
  if (fmt.length()==0||fmt=="int") return String((long)val);
  if (fmt.startsWith(".")&&fmt.endsWith("f")) { int d=fmt.substring(1,fmt.length()-1).toInt(); return String(val,constrain(d,0,6)); }
  if (fmt=="sec_to_mss") {
    unsigned long ms=(unsigned long)fabsf(val);
    unsigned long mins=ms/60000,secs=(ms%60000)/1000,ms3=ms%1000;
    char buf[16]; sprintf(buf,"%02lu:%02lu.%03lu",mins,secs,ms3); return String(buf);
  }
  if (fmt=="time") {
    unsigned long ep=(unsigned long)fabsf(val);
    int hh=(ep/3600)%24,mm=(ep%3600)/60;
    char buf[8]; sprintf(buf,"%02d:%02d",hh,mm); return String(buf);
  }
  return String(val,0);
}

void interpolateSay(String text, JsonObject vars, JsonObject strvars) {
  String out;
  for (size_t i = 0; i < text.length(); i++) {
    if (text[i]=='{') {
      int end = text.indexOf('}', i);
      if (end > 0) {
        String spec = text.substring(i+1, end);
        int cp = spec.indexOf(':');
        String vn = cp>=0?spec.substring(0,cp):spec, fmt = cp>=0?spec.substring(cp+1):"";
        out += formatVarValue(vn, fmt, vars, strvars);
        i = end; continue;
      }
    }
    out += text[i];
  }
  Serial.println("AI: " + out);
  addAssistantMessage(out);
}

static String interpolateStr(const String& tmpl, JsonObject vars, JsonObject strvars) {
  String out;
  for (size_t i = 0; i < tmpl.length(); i++) {
    if (tmpl[i]=='{') {
      int end = tmpl.indexOf('}', i);
      if (end > 0) {
        String spec = tmpl.substring(i+1, end);
        int cp = spec.indexOf(':');
        String vn = cp>=0?spec.substring(0,cp):spec, fmt = cp>=0?spec.substring(cp+1):"";
        out += formatVarValue(vn, fmt, vars, strvars);
        i = end; continue;
      }
    }
    out += tmpl[i];
  }
  return out;
}

// ── Validation ─────────────────────────────────────────
bool validateSkillOps(JsonArrayConst ops, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err) {
  if (depth > 4) { err="nesting too deep"; return false; }
  if (ops.size() > 50) { err="too many ops"; return false; }
  for (JsonObjectConst op : ops) {
    const char* t = op["op"]|"";
    if (!strlen(t)) { err="missing op type"; return false; }
    static const char* allowed[] = {
      "set","inc","set_str","if","loop","say","remember","recall","groq","end", nullptr
    };
    bool ok = false; for (int i=0; allowed[i]; i++) if(strcmp(t,allowed[i])==0){ok=true;break;}
    if (!ok) { err=String("unknown op: ")+t; return false; }
    if (strcmp(t,"if")==0) {
      if (!op["then"].is<JsonArrayConst>()) { err="if missing then"; return false; }
      if (!validateSkillOps(op["then"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
      if (op["else"].is<JsonArrayConst>())
        if (!validateSkillOps(op["else"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
    }
    if (strcmp(t,"loop")==0) {
      if (!op["body"].is<JsonArrayConst>()) { err="loop missing body"; return false; }
      if (!validateSkillOps(op["body"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
    }
  }
  return true;
}

// ── Execute skill ops ───────────────────────────────────
void executeSkillOps(JsonArrayConst ops, JsonObject vars, JsonObject strvars) {
  for (JsonObjectConst op : ops) {
    esp_task_wdt_reset();
    const char* t = op["op"]|"";

    if (strcmp(t,"set")==0) {
      const char* var = op["var"]|""; const char* expr = op["expr"]|"";
      if (strlen(var)&&vars.containsKey(var)) vars[var]=SkillExpr::eval(String(expr),vars,strvars);

    } else if (strcmp(t,"inc")==0) {
      const char* var = op["var"]|""; float step = op["step"]|1.0f;
      if (strlen(var)&&vars.containsKey(var)) vars[var]=vars[var].as<float>()+step;

    } else if (strcmp(t,"set_str")==0) {
      const char* var = op["var"]|""; const char* val = op["val"]|"";
      if (strlen(var)) strvars[var]=interpolateStr(String(val),vars,strvars);

    } else if (strcmp(t,"say")==0) {
      const char* text = op["text"]|"";
      interpolateSay(String(text),vars,strvars);

    } else if (strcmp(t,"if")==0) {
      const char* lhsVar = op["var"]|""; const char* cmp = op["cmp"]|"==";
      float lhs = vars.containsKey(lhsVar)?vars[lhsVar].as<float>():0.0f;
      float rhs = SkillExpr::eval(String(op["val"]|"0"),vars,strvars);
      bool result = (strcmp(cmp,"==")==0)?lhs==rhs:(strcmp(cmp,"!=")==0)?lhs!=rhs:
                    (strcmp(cmp,"<")==0)?lhs<rhs:(strcmp(cmp,">")==0)?lhs>rhs:
                    (strcmp(cmp,"<=")==0)?lhs<=rhs:(strcmp(cmp,">=")==0)?lhs>=rhs:false;
      if (result) executeSkillOps(op["then"].as<JsonArrayConst>(),vars,strvars);
      else if (op["else"].is<JsonArrayConst>()) executeSkillOps(op["else"].as<JsonArrayConst>(),vars,strvars);

    } else if (strcmp(t,"loop")==0) {
      int count = constrain((int)(op["count"]|1),1,Config::MAX_LOOP_COUNT);
      JsonArrayConst body = op["body"].as<JsonArrayConst>();
      for (int li=0; li<count; li++) { esp_task_wdt_reset(); executeSkillOps(body,vars,strvars); }

    } else if (strcmp(t,"remember")==0) {
      const char* key = op["key"]|"";
      const char* srcVar = op["var"]|""; const char* srcStrvar = op["strvar"]|"";
      String val;
      if (strlen(srcVar)>0&&vars.containsKey(srcVar)) {
        val = String(vars[srcVar].as<float>(),2);
        while(val.endsWith("0")&&val.indexOf('.')>=0) val.remove(val.length()-1);
        if(val.endsWith(".")) val.remove(val.length()-1);
      } else if (strlen(srcStrvar)>0&&strvars.containsKey(srcStrvar)) {
        val = strvars[srcStrvar].as<String>();
      }
      if (strlen(key)>0&&val.length()>0) { rememberFact(String(key),val); Serial.println("💾 Skill saved: "+String(key)+" = "+val); }

    } else if (strcmp(t,"recall")==0) {
      const char* key = op["key"]|""; const char* dst = op["strvar"]|"";
      String val = recallFact(String(key));
      strvars[dst] = val.length()>0 ? val : String("(not set)");

    } else if (strcmp(t,"groq")==0) {
      const char* promptTmpl = op["prompt"]|""; const char* dst = op["strvar"]|"";
      String prompt = interpolateStr(String(promptTmpl),vars,strvars);
      if (prompt.length()>0&&heapOk()&&WiFi.status()==WL_CONNECTED) {
        String reply = groqSimpleCall(prompt, 0.3f, 100);
        strvars[dst] = reply.length()>0 ? reply : String("(no reply)");
        Serial.println("🤖 " + String(dst) + ": " + strvars[dst].as<String>());
      }

    } else if (strcmp(t,"end")==0) {
      return;
    }
  }
}

void runSkillAction(int skillIdx, const String& actionName) {
  if (skillIdx < 0 || skillIdx >= (int)skillJson.size()) return;
  JsonDocument doc;
  if (deserializeJson(doc, skillJson[skillIdx])) return;

  JsonObject vars    = doc["vars"].is<JsonObject>()    ? doc["vars"].as<JsonObject>()    : doc.createNestedObject("vars");
  JsonObject strvars = doc["strvars"].is<JsonObject>() ? doc["strvars"].as<JsonObject>() : doc.createNestedObject("strvars");

  JsonObject actions = doc["actions"].as<JsonObject>();
  if (!actions.containsKey(actionName)) return;

  aiState = AI_LEARNING; stateChangeTime = millis();
  executeSkillOps(actions[actionName].as<JsonArrayConst>(), vars, strvars);

  // Persist updated vars
  String updated; serializeJson(doc, updated);
  skillJson[skillIdx] = updated;
  if (!(testingSkill && skillIdx == pendingSkillIndex)) saveSkills();
}

void listSkills() {
  if (skillNames.empty()) { Serial.println("🧠 No self-taught skills yet — just ask for something new!"); return; }
  Serial.println("\n🧠 ═══ SELF-TAUGHT SKILLS ═══");
  for (size_t i = 0; i < skillNames.size(); i++) {
    JsonDocument doc; String desc = "";
    if (!deserializeJson(doc, skillJson[i])) desc = doc["description"]|"";
    Serial.println("  " + String(i) + ". " + skillNames[i] + " — " + desc);
  }
  Serial.println("══════════════════════════════");
}

void removeSkill(const String& name) {
  for (size_t i = 0; i < skillNames.size(); i++) {
    if (skillNames[i].equalsIgnoreCase(name)) {
      Serial.println("🗑️  Forgot skill: " + skillNames[i]);
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i);
      rebuildSkillTriggerIndex(); saveSkills(); return;
    }
  }
  Serial.println("❌ No skill named '" + name + "'. Use /skills to see the list.");
}

void saveSkills() {
  JsonDocument doc; JsonArray arr = doc["skills"].to<JsonArray>();
  for (const auto& raw : skillJson) {
    JsonDocument one;
    if (!deserializeJson(one, raw)) arr.add(one);
  }
  File file = FFat.open("/skills.json", FILE_WRITE);
  if (file) { serializeJson(doc, file); file.close(); }
}

void loadSkills() {
  skillNames.clear(); skillJson.clear();
  if (!FFat.exists("/skills.json")) return;
  File file = FFat.open("/skills.json", FILE_READ); if (!file) return;
  JsonDocument doc;
  if (deserializeJson(doc, file)) { file.close(); return; }
  for (JsonObject s : doc["skills"].as<JsonArray>()) {
    String name = s["name"]|"unnamed";
    String raw; serializeJson(s, raw);
    skillNames.push_back(name); skillJson.push_back(raw);
  }
  rebuildSkillTriggerIndex();
  file.close();
  Serial.println("✅ Loaded " + String(skillNames.size()) + " self-taught skill(s)");
}

bool looksLikeFeatureRequest(const String& input) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return false;
  String prompt =
    "Reply with EXACTLY one word: FEATURE or CHAT.\n"
    "FEATURE = the user wants this embedded device to perform/track/do a concrete new "
    "repeatable action (timers, counters, stopwatches, games, calculators, converters, trackers).\n"
    "CHAT = conversation, question, small talk, or something needing live knowledge/search.\n"
    "Message: \"" + input + "\"";
  String verdict = groqSimpleCall(prompt, 0.0f, 8);
  verdict.trim(); verdict.toUpperCase();
  return verdict.startsWith("FEATURE");
}

// ── Generic HTTPS JSON POST ─────────────────────────────
String postJson(const String& url, const String& body) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";
  esp_task_wdt_reset();
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.setTimeout(Config::SKILL_GEN_TIMEOUT_MS);
  http.begin(secClient, url); http.addHeader("Content-Type", "application/json");
  int code = http.POST(body); esp_task_wdt_reset();
  String result; if (code > 0) result = http.getString();
  http.end(); return result;
}

// ── Extract JSON object from raw text ─────────────────
static String extractJsonObject(const String& raw) {
  int s = raw.indexOf('{'), e = raw.lastIndexOf('}');
  if (s >= 0 && e > s) return raw.substring(s, e+1);
  return "";
}

// ── SKILL_SYSTEM_PROMPT ─────────────────────────────────
const char* SKILL_SYSTEM_PROMPT = R"PROMPT(You write tiny "skill" definitions for a voice/text assistant running on a memory-constrained ESP32 microcontroller. The device has a SAFE, sandboxed interpreter — you may ONLY use the operations below. Never invent new op types or fields.

Output STRICT JSON only (no markdown fences, no commentary):

{
  "name": "snake_case_identifier <=32 chars",
  "description": "one short sentence",
  "vars": { "<name>": <number or boolean initial value> },
  "strvars": { "<name>": "<initial string value>" },
  "triggers": { "<action_name>": ["phrase", "..."] },
  "actions": { "<action_name>": [ <op>, ... ] }
}

vars: max 12 numeric. strvars: max 4 string vars.
triggers/actions: max 8 actions, 1-5 phrases each, max 16 ops per action.

Allowed ops:
- {"op":"set","var":"<var>","expr":"<arithmetic>"}
  Special tokens: MILLIS NOW_HOUR NOW_MIN NOW_SEC NOW_DAY NOW_MONTH NOW_YEAR RAND100 STRLEN_varname
  Functions: ABS(x) FLOOR(x) CEIL(x) ROUND(x) MIN(a,b) MAX(a,b) RANDOM(lo,hi) MOD(a,b) SIN(x) COS(x)
- {"op":"inc","var":"<var>","step":<n>}
- {"op":"set_str","var":"<strvar>","val":"<text with {var} interpolation>"}
- {"op":"say","text":"<text with {var:.2f} or {strvar} interpolation>"}
- {"op":"if","var":"<var>","cmp":"==|!=|<|>|<=|>=","val":"<expr>","then":[ops],"else":[ops]}
- {"op":"loop","count":<n>,"body":[ops]}
- {"op":"remember","key":"<key>","var":"<var>"}  or  {"op":"remember","key":"<key>","strvar":"<strvar>"}
- {"op":"recall","key":"<key>","strvar":"<strvar>"}
- {"op":"groq","prompt":"<text with {var}/{strvar}>","strvar":"<dest strvar>"}
- {"op":"end"}

Example — word-of-the-day:
{"name":"word_of_day","description":"Fetch a word of the day","vars":{},"strvars":{"word":""},"triggers":{"get":["word of the day","give me a new word","daily word"]},"actions":{"get":[{"op":"groq","prompt":"Give me one interesting English word with its definition in one sentence.","strvar":"word"},{"op":"say","text":"Word of the day: {word}"}]}}
)PROMPT";

// ── Gemini skill writer ──────────────────────────────────
String geminiGenerateSkill(const String& request, const String& previousSkillJson, const String& feedback) {
  if (String(Config::GEMINI_API_KEY) == "PASTE_YOUR_GEMINI_API_KEY_HERE") {
    Serial.println("⚠️  Set Config::GEMINI_API_KEY first.");
    return "";
  }
  String userPrompt = "The user asked the assistant to do this:\n\"" + request + "\"\n\nWrite the skill JSON for it.";
  if (feedback.length() > 0)
    userPrompt += "\n\nPrevious attempt:\n" + previousSkillJson + "\n\nReviewer feedback:\n" + feedback + "\n\nProduce a corrected skill JSON.";

  JsonDocument gemReq;
  gemReq["system_instruction"]["parts"][0]["text"] = SKILL_SYSTEM_PROMPT;
  gemReq["contents"][0]["parts"][0]["text"] = userPrompt;
  gemReq["generationConfig"]["temperature"] = 0.2;
  gemReq["generationConfig"]["maxOutputTokens"] = 800;

  String body; serializeJson(gemReq, body);
  String url = String(Config::GEMINI_ENDPOINT) + "?key=" + Config::GEMINI_API_KEY;

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String respRaw = postJson(url, body);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  if (respRaw.length() == 0) { Serial.println("⚠️  Gemini request failed."); return ""; }

  JsonDocument resp;
  if (deserializeJson(resp, respRaw)) { Serial.println("⚠️  Gemini returned unparseable output."); return ""; }

  const char* apiErr = resp["error"]["message"]|"";
  if (strlen(apiErr) > 0) { Serial.println("⚠️  Gemini error: " + String(apiErr)); return ""; }

  const char* raw = resp["candidates"][0]["content"]["parts"][0]["text"]|"";
  return extractJsonObject(String(raw));
}

// ── learnNewSkill ────────────────────────────────────────
void learnNewSkill(const String& request) {
  Serial.println("\n🧠 Learning new skill for: \"" + request + "\"...");
  aiState = AI_LEARNING; stateChangeTime = millis();

  String candidate = geminiGenerateSkill(request, "", "");
  if (candidate.length() == 0) {
    Serial.println("❌ Couldn't generate skill (Gemini unavailable or bad response).");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  beginSkillTest(request, candidate);
}

void beginSkillTest(const String& request, const String& candidateJson) {
  JsonDocument doc;
  if (deserializeJson(doc, candidateJson)) {
    Serial.println("❌ Generated skill JSON is invalid — can't test it.");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  // Validate
  if (!doc.containsKey("name") || !doc.containsKey("actions")) {
    Serial.println("❌ Skill missing required fields (name, actions).");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  String name = doc["name"].as<String>();
  if (name.length() == 0 || name.length() > 32) {
    Serial.println("❌ Invalid skill name.");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  // Check for existing skill with same name (backup for retry)
  pendingBackupName = ""; pendingBackupJson = "";
  for (size_t i = 0; i < skillNames.size(); i++) {
    if (skillNames[i] == name) {
      pendingBackupName = skillNames[i]; pendingBackupJson = skillJson[i];
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i); break;
    }
  }

  skillNames.push_back(name); skillJson.push_back(candidateJson);
  pendingSkillIndex = (int)skillNames.size() - 1;
  pendingSkillRequest = request;
  testingSkill = true;
  rebuildSkillTriggerIndex();

  aiState = AI_EXCITED; stateChangeTime = millis();
  Serial.println("✨ Learned: " + name + " — " + String(doc["description"]|""));
  Serial.println("Try it, then: keep it? (yes/no)");

  // Auto-try it
  String lower = request; lower.toLowerCase(); String bestAction;
  for (JsonPair kv : doc["triggers"].as<JsonObject>()) {
    for (JsonVariant p : kv.value().as<JsonArray>()) {
      String phrase = p.as<String>(); phrase.toLowerCase();
      if (lower.indexOf(phrase) >= 0) { bestAction = kv.key().c_str(); break; }
    }
    if (bestAction.length() > 0) break;
  }
  if (bestAction.length() == 0) {
    JsonObject actions = doc["actions"].as<JsonObject>();
    for (JsonPair kv : actions) { bestAction = kv.key().c_str(); break; }
  }
  if (bestAction.length() > 0) runSkillAction(pendingSkillIndex, bestAction);
}

void commitPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String name = skillNames[pendingSkillIndex];
  if ((int)skillNames.size() > Config::MAX_SKILLS) {
    int removeIdx = (pendingSkillIndex == 0) ? 1 : 0;
    skillNames.erase(skillNames.begin()+removeIdx); skillJson.erase(skillJson.begin()+removeIdx);
    if (removeIdx < pendingSkillIndex) pendingSkillIndex--;
  }
  rebuildSkillTriggerIndex(); saveSkills();
  updateKnowledgeDomain("self-taught skills", 25);
  testingSkill = false; pendingSkillIndex = -1; pendingBackupName = ""; pendingBackupJson = "";
  aiState = AI_EXCITED; stateChangeTime = millis();
  Serial.println("✅ Kept — " + name + " is saved.");
}

void discardPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String name = skillNames[pendingSkillIndex];
  skillNames.erase(skillNames.begin()+pendingSkillIndex);
  skillJson.erase(skillJson.begin()+pendingSkillIndex);
  if (pendingBackupJson.length() > 0) {
    skillNames.push_back(pendingBackupName); skillJson.push_back(pendingBackupJson);
  }
  rebuildSkillTriggerIndex();
  testingSkill = false; pendingSkillIndex = -1; pendingBackupName = ""; pendingBackupJson = "";
  aiState = AI_IDLE; stateChangeTime = millis();
  Serial.println("🗑️  Discarded — " + name + " not saved.");
}

void retryPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String request = pendingSkillRequest; discardPendingSkill(); learnNewSkill(request);
}
