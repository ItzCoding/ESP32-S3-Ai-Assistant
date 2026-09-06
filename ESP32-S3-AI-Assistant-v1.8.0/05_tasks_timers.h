// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 05_tasks_timers.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 4.6 ── v1.7.9 FREERTOS TASKS & TIMER CALLBACKS
// ═══════════════════════════════════════════════════════

// ── Timer callbacks — lightweight, only set flags ──────
static void tmrReminderCB(TimerHandle_t)  { g_flagReminder  = true; }
static void tmrFlushCB(TimerHandle_t)     { g_flagFlush     = true; }
static void tmrWifiCB(TimerHandle_t)      { g_flagWifiCheck = true; }
static void tmrNtpCB(TimerHandle_t)       { g_flagNtpSync   = true; }
static void tmrProactiveCB(TimerHandle_t) { g_flagProactive = true; }
static void tmrHeapSnapCB(TimerHandle_t)  { g_flagHeapSnap  = true; }

static void tmrBriefingCB(TimerHandle_t) {
  int h = hour(), m = minute();
  // morning briefing gate
  if (autoMorningBriefing() && h == 7 && m == 30 && !morningBriefingGiven)
    g_flagMorning = true;
  // evening summary gate
  if (h == 20 && m == 0 && !eveningSummaryGiven)
    g_flagEvening = true;
  // midnight reset
  if (h == 0) { morningBriefingGiven = false; eveningSummaryGiven = false; }
}

// ── Background OTA check task (deletes itself when done) ──
// v1.7.9 FIX: subscribe this task to the WDT so calls to esp_task_wdt_reset()
// inside checkForUpdate() / otaFetchLatestRelease() do not cause
// "task not found (This)" WDT errors.
static void otaTaskFn(void* pvParam) {
  esp_task_wdt_add(NULL); // subscribe OTA task to WDT
  vTaskDelay(pdMS_TO_TICKS(8000)); // let WiFi/NTP settle
  if (WiFi.status() == WL_CONNECTED) {
    checkForUpdate(true);
  }
  esp_task_wdt_delete(NULL); // unsubscribe before deleting task
  g_otaTask = nullptr;
  vTaskDelete(nullptr);
}

// ── Dedicated LED update task (Core 1, 50 Hz) ──────────
static void ledTaskFn(void* pvParam) {
  for (;;) {
    updateLED();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ── WiFi reconnect (called from loop on g_flagWifiCheck) ──
void doWifiReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!g_wifiReconnecting) {
      wifiReconnectCount++;
      WiFi.disconnect();
      WiFi.begin(g_wifiSsid.c_str(), g_wifiPassword.c_str());
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
      timeClient.forceUpdate();
      setTime(timeClient.getEpochTime());
    }
  }
}

