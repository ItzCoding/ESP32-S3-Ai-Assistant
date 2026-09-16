// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 03_globals.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 3 ── GLOBAL STATE
// ═══════════════════════════════════════════════════════

WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", Config::NTP_OFFSET_SEC, Config::NTP_UPDATE_INTERVAL_MS);

String inputString    = "";
bool   stringComplete = false;
static bool g_inputOverflow = false;

std::vector<Fact>         memory;
std::vector<Reminder>     reminders;
std::vector<ChatMessage>  chatHistory;
std::vector<SentimentLog> sentimentHistory;
std::vector<KnowledgeArea>knowledgeDomains;
std::vector<TaskItem>      tasks;
static ApiUsageStats       g_apiStats;
static uint32_t            g_nextTaskId = 1;
static bool                g_dirtyTasks = false;
static bool                g_dirtyApiStats = false;
static std::vector<SearchCacheItem> g_searchCache;
static bool                g_dirtySearchCache = false;
static ModelMode           g_modelMode = MODEL_AUTO;
static String              g_lastSelectedModel = Config::AI_MODEL;
static String              g_lastReminderMessage;

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

// v1.7.8: evening summary
bool eveningSummaryGiven = false;

static uint32_t noteTaskCounter = 0;

// ── Telemetry counters ──────────────────────────────────
int           httpTimeoutCount   = 0;
int           wifiReconnectCount = 0;
uint32_t      heapSnapshot       = 0;
unsigned long heapSnapshotTime   = 0;

// Runtime-overridable API settings
String customApiKey      = Config::AI_KEY;
float  customTemperature = Config::AI_TEMPERATURE;
int    customMaxTokens   = Config::AI_MAX_TOKENS;

// The setup wizard is shown only once after a confirmed OTA update.
// USB uploads, version changes, and placeholder values must not trigger it.
static Preferences g_preferences;
static bool        g_preferencesReady = false;
static bool        g_setupPending     = false;
static constexpr const char* OTA_SETUP_ONCE_KEY = "ota_setup_once_v2";
static String g_wifiSsid      = Config::SSID;
static String g_wifiPassword  = Config::PASSWORD;
static String g_aiKey      = Config::AI_KEY;
static String g_weatherKey   = Config::WEATHER_KEY;
static String g_serperKey    = Config::SERPER_API_KEY;
static String g_skillKey    = Config::SKILL_KEY;

// v1.7.8: dirty-write flags (batch FATFS writes)
static bool g_dirtyMemory   = false;
static bool g_dirtyReminders= false;
static bool g_dirtySentiment= false;
static bool g_dirtyPattern  = false;
static bool g_dirtyKnowledge= false;
static bool g_dirtyChat     = false;
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

#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(CONFIG_IDF_TARGET_ESP32S3)
static temperature_sensor_handle_t g_tsens = nullptr;
#else
static void* g_tsens = nullptr; // temperature driver unavailable on Arduino core 2.x
#endif

static char*  g_psramReqBuf  = nullptr;
static char*  g_psramRespBuf = nullptr;
static bool   g_hasPsram     = false;
static bool   g_historyCompressionPending = false;
static bool          g_stopwatchRunning = false;
static unsigned long g_stopwatchStarted = 0;
static unsigned long g_stopwatchElapsed = 0;
static std::vector<unsigned long> g_stopwatchLaps;
static bool          g_timerRunning = false;
static bool          g_timerPaused = false;
static unsigned long g_timerStarted = 0;
static unsigned long g_timerRemaining = 0;

// ── Dual-Core AI Worker ─────────────────────────────
struct DualCoreReq {
  String url;
  String auth;
  String body;
  int    timeoutMs;
  bool   doStream;
  bool   printTokens;
};
struct DualCoreResp {
  String reply;
  bool   ok;
  int    httpCode;
};
static DualCoreReq       g_dcReq;
static DualCoreResp      g_dcResp;
static volatile bool     g_dcBusy    = false;
static SemaphoreHandle_t g_dcReqSem  = nullptr;
static SemaphoreHandle_t g_dcRespSem = nullptr;
static TaskHandle_t      g_dcTask    = nullptr;

// v1.7.8: WiFi reconnect state
static bool          g_wifiReconnecting   = false;
static unsigned long g_wifiReconnectStart = 0;

// ── OTA update state ─────────────────────────────────
static bool   g_updateAvailable  = false;
static String g_latestVersion    = "";
static String g_latestBinUrl     = "";
static String g_updateNotes      = "";
static size_t g_latestBinSize    = 0;
static bool   g_otaCheckedOnBoot = false;

// ═══════════════════════════════════════════════════════
// v1.7.9 ── FreeRTOS INFRASTRUCTURE
// ═══════════════════════════════════════════════════════

// ── Handles ──────────────────────────────────────────
static SemaphoreHandle_t g_serialMutex  = nullptr; // thread-safe Serial output
static QueueHandle_t     g_inputQueue   = nullptr; // Serial input line queue
static TimerHandle_t     g_tmrReminder  = nullptr;
static TimerHandle_t     g_tmrFlush     = nullptr;
static TimerHandle_t     g_tmrProactive = nullptr;
static TimerHandle_t     g_tmrWifi      = nullptr;
static TimerHandle_t     g_tmrNtp       = nullptr;
static TimerHandle_t     g_tmrBriefing  = nullptr;
static TimerHandle_t     g_tmrHeapSnap  = nullptr;
static TaskHandle_t      g_ledTask      = nullptr;
static TaskHandle_t      g_otaTask      = nullptr;

// ── Event flags (set by timer callbacks, consumed by loop()) ──
static volatile bool g_flagReminder  = false;
static volatile bool g_flagFlush     = false;
static volatile bool g_flagWifiCheck = false;
static volatile bool g_flagNtpSync   = false;
static volatile bool g_flagMorning   = false;
static volatile bool g_flagEvening   = false;
static volatile bool g_flagProactive = false;
static volatile bool g_flagHeapSnap  = false;

// ── v1.7.9: Multi-language auto-detect ─────────────────
static String g_userLanguage = "English"; // persisted via facts["language"]

// ── Thread-safe serial helper ────────────────────────
inline void serialPrintln(const String& s) {
  if (g_serialMutex) xSemaphoreTake(g_serialMutex, portMAX_DELAY);
  Serial.println(s);
  if (g_serialMutex) xSemaphoreGive(g_serialMutex);
}
inline void serialPrintf(const char* fmt, ...) {
  if (g_serialMutex) xSemaphoreTake(g_serialMutex, portMAX_DELAY);
  char buf[256]; va_list a; va_start(a,fmt); vsnprintf(buf,sizeof(buf),fmt,a); va_end(a);
  Serial.print(buf);
  if (g_serialMutex) xSemaphoreGive(g_serialMutex);
}



static volatile bool g_flagBriefingCheck = false;
