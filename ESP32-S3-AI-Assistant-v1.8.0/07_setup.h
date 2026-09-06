// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 07_setup.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 5 ── SETUP
// ═══════════════════════════════════════════════════════
void setup() {
  Serial.setRxBufferSize(1024);
  Serial.begin(115200);
  delay(800);

  // v1.8.0: report WHY the chip last reset — makes crashes debuggable.
  {
    esp_reset_reason_t rr = esp_reset_reason();
    const char* reason = "unknown";
    switch (rr) {
      case ESP_RST_POWERON:  reason = nullptr; break;  // normal boot — stay quiet
      case ESP_RST_SW:       reason = "software restart (ESP.restart)"; break;
      case ESP_RST_PANIC:    reason = "panic / exception CRASH"; break;
      case ESP_RST_INT_WDT:  reason = "interrupt watchdog"; break;
      case ESP_RST_TASK_WDT: reason = "task watchdog (WDT timeout)"; break;
      case ESP_RST_WDT:      reason = "other watchdog"; break;
      case ESP_RST_BROWNOUT: reason = "brownout (power dip)"; break;
      default:               reason = "unknown"; break;
    }
    if (reason)
      Serial.printf("⚠️  Last reset reason: %s (%d)\n", reason, (int)rr);
  }

  // v1.7.8: Start at full speed for init, drop to idle after
  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = Config::WDT_TIMEOUT_S * 1000,
    // v1.7.9 FIX: was (1<<0) which watches Core 0 IDLE task.
    // aiHttpTask runs on Core 0 for up to 35 s, starving Core 0 IDLE → WDT fires.
    // 0 = don't watch any IDLE tasks; only watch explicitly subscribed tasks.
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
#else
  // Arduino-esp32 core 2.x: calling init() again just reconfigures the timeout
  esp_task_wdt_init(Config::WDT_TIMEOUT_S, true);
#endif
  {
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if      (wdt_err == ESP_OK)               Serial.println("✅ WDT configured");
    else if (wdt_err == ESP_ERR_INVALID_STATE) Serial.println("✅ WDT configured (already subscribed)");
    else    Serial.printf("⚠️  WDT subscribe failed: 0x%x\n", wdt_err);
  }

  Serial.println("\n🚀 " + String(Config::VERSION) + " STARTING...");

  initPsram();
  if (!g_hasPsram) customMaxTokens = Config::NO_PSRAM_MAX_TOKENS;

  {
#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(CONFIG_IDF_TARGET_ESP32S3)
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&cfg, &g_tsens) == ESP_OK) {
      temperature_sensor_enable(g_tsens);
      Serial.println("✅ Temperature sensor ready");
    } else {
      g_tsens = nullptr;
      Serial.println("⚠️  Temperature sensor init failed");
    }
#else
    g_tsens = nullptr;
    Serial.println("ℹ️  Internal temperature sensor unavailable on this board/core");
#endif
  }

  g_dcReqSem  = xSemaphoreCreateBinary();
  g_dcRespSem = xSemaphoreCreateBinary();
  if (g_dcReqSem && g_dcRespSem) {
    BaseType_t ok = xTaskCreate(
      aiHttpTask, "ai_http",
      12288,      // v1.7.8: 12KB stack (was 8KB) for larger 70B responses
      nullptr, 5, &g_dcTask
    );
    if (ok == pdPASS) Serial.println("✅ Background AI network worker ready");
    else              Serial.println("⚠️  AI worker task unavailable — direct network mode");
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

  loadRuntimeSettings();
  if (g_setupPending) runCredentialWizard();

  WiFi.begin(g_wifiSsid.c_str(), g_wifiPassword.c_str());
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

  // ── v1.7.9: Boot OTA check runs as background task ─────
  // (no longer blocks setup — spawned after LED init)

  strip.begin();
  strip.clear();
  strip.show();
  rainbowWave(1800);
  strip.clear();
  strip.show();

  // v1.7.8: Drop to idle CPU after init
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  // ── v1.7.9: FreeRTOS infrastructure ─────────────────
  g_serialMutex = xSemaphoreCreateMutex();
  g_inputQueue  = xQueueCreate(8, sizeof(String*));

  // Software timers (all repeating)
  g_tmrReminder  = xTimerCreate("reminder",  pdMS_TO_TICKS(30000),                     pdTRUE, nullptr, tmrReminderCB);
  g_tmrFlush     = xTimerCreate("flush",     pdMS_TO_TICKS(30000),                     pdTRUE, nullptr, tmrFlushCB);
  g_tmrWifi      = xTimerCreate("wifi",      pdMS_TO_TICKS(15000),                     pdTRUE, nullptr, tmrWifiCB);
  g_tmrNtp       = xTimerCreate("ntp",       pdMS_TO_TICKS(60000),                     pdTRUE, nullptr, tmrNtpCB);
  g_tmrBriefing  = xTimerCreate("briefing",  pdMS_TO_TICKS(60000),                     pdTRUE, nullptr, tmrBriefingCB);
  g_tmrProactive = xTimerCreate("proactive", pdMS_TO_TICKS(Config::PROACTIVE_INTERVAL_MS), pdTRUE, nullptr, tmrProactiveCB);
  g_tmrHeapSnap  = xTimerCreate("heapsnap",  pdMS_TO_TICKS(7200000),                   pdTRUE, nullptr, tmrHeapSnapCB);

  if (g_tmrReminder)  xTimerStart(g_tmrReminder,  0);
  if (g_tmrFlush)     xTimerStart(g_tmrFlush,     0);
  if (g_tmrWifi)      xTimerStart(g_tmrWifi,      0);
  if (g_tmrNtp)       xTimerStart(g_tmrNtp,       0);
  if (g_tmrBriefing)  xTimerStart(g_tmrBriefing,  0);
  if (g_tmrProactive) xTimerStart(g_tmrProactive, 0);
  if (g_tmrHeapSnap)  xTimerStart(g_tmrHeapSnap,  0);
  Serial.println("✅ FreeRTOS software timers started");

  // Unpinned tasks also work with single-core ESP32 builds.
  xTaskCreate(ledTaskFn, "led", 3072, nullptr, 2, &g_ledTask);
  if (g_ledTask) Serial.println("✅ LED status task ready");

  // Background OTA check task (any core, deletes itself)
  if (WiFi.status() == WL_CONNECTED) {
    xTaskCreate(otaTaskFn, "ota", 8192, nullptr, 3, &g_otaTask);
    if (g_otaTask) Serial.println("✅ OTA background task started");
  }

  // Load persisted language preference
  {
    String savedLang = recallFact("language");
    if (savedLang.length() > 0) g_userLanguage = savedLang;
  }

  Serial.println("\n╭────────────────────────────────────────────╮");
  Serial.println("│  🤖 ESP32 AI Assistant is ready            │");
  Serial.println("╰────────────────────────────────────────────╯");
  Serial.println("Model: " + String(Config::AI_MODEL));
  Serial.println("Memory: " + String(g_hasPsram ? "PSRAM profile" : "low-memory profile"));
  Serial.println("Type naturally, or enter /help for commands.\n");
}

