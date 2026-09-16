// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 04_declarations.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 4 ── FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════

String aiSimpleCall(const String& prompt, float temp = 0.1f, int maxTok = 128);
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens);
String aiStream(const String& userPrompt, const String& sysPrompt = "",
                  float temp = 0.7f, int maxTok = 1024);
String sendToAIStream(const String& userMessage, const String& extraContext = "");
// v1.7.8: Function calling
FnCallResult aiFunctionCall(const String& userMsg, const String& toolsJson);
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
bool   getWeather(String city);
void   searchWeb(const String& query);
String urlEncode(const String& str);

void   rememberFact(const String& key, const String& value);
String recallFact(const String& key);
bool   removeFact(const String& key);
String normalizeMemoryKey(String key);
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
bool   handleSmartNaturalInput(const String& input);
void   loadTasks(); void saveTasks();
void   loadApiStats(); void saveApiStats();
void   loadSearchCache(); void saveSearchCache();
void   purgeExpiredMemories(bool announce = false);
String chooseAiModel(const String& prompt, bool simpleCall = false);
void   applyAiModelRouting(JsonDocument& doc, const String& prompt, bool simpleCall = false);
void   printFirmwareChangelog();
void   consolePrompt();
void   printApiUsageStats();
void   migrateLegacyTasks();
void   recordApiUsage(const char* service, bool success, uint32_t latencyMs,
                      size_t inputChars = 0, size_t outputChars = 0);
bool   responseNeedsRepair(const String& response);

void   addUserMessage(const String& msg);
void   addAssistantMessage(const String& msg);
void   limitChatHistoryByTokens(int maxTokens = Config::MAX_CHAT_TOKENS);
bool   summarizeChatHistory(bool force = false);
void   loadChatHistory();  void saveChatHistory();

String detectSentiment(const String& message);
void   trackSentiment(const String& sentiment, float score);
void   respondToMood();
void   celebratePositiveVibes();
void   offerComfort();
void   smartResponseEnhancement(String& response);
float  computeMoodTemperature();  // v1.7.8

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
void   generateEveningSummary();  // v1.7.8

void   handleInput(const String& input);
void   processConversation(const String& userMsg);
void   processLocalTimer();
void   printHelp();
void   printVersion();
void   clearAll();
void   flushDirtyState(); // v1.7.8

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
void   loadRuntimeSettings();
void   runCredentialWizard();
static String  dcPost(const String& url, const String& authBearer, const String& body,
                      int timeoutMs, bool doStream, bool printTokens = false);
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
String aiVerifySkill(const String& candidateJson, const String& request); // v1.7.9
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
String postJson(const String& url, const String& body, const String& authBearer = "");
String skillModelGenerate(const String& request, const String& previousSkillJson, const String& feedback);
static String extractJsonObject(const String& raw);

// ═══════════════════════════════════════════════════════
void   checkForUpdate(bool silent = false);
void   installUpdate();
bool   isNewerVersion(const String& remote, const String& current);
static String otaFetchLatestRelease();

// ── v1.7.9 new declarations ──────────────────────────
String detectLanguage(const String& msg);
void   doWifiReconnect();
static void otaTaskFn(void* pvParam);
static void ledTaskFn(void* pvParam);
static void tmrReminderCB(TimerHandle_t t);
static void tmrFlushCB(TimerHandle_t t);
static void tmrWifiCB(TimerHandle_t t);
static void tmrNtpCB(TimerHandle_t t);
static void tmrBriefingCB(TimerHandle_t t);
static void tmrProactiveCB(TimerHandle_t t);
static void tmrHeapSnapCB(TimerHandle_t t);

