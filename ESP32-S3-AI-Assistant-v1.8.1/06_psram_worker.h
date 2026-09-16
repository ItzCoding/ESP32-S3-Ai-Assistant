// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 06_psram_worker.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 4.5 ── PSRAM INIT & DUAL-CORE WORKER
// ═══════════════════════════════════════════════════════

void initPsram() {
  g_hasPsram = ESP.getPsramSize() > 0;
  if (!g_hasPsram) {
    Serial.println("ℹ️  PSRAM not detected — using the low-memory profile");
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
    g_hasPsram = false;
    Serial.println("⚠️  PSRAM allocation failed — using the low-memory profile");
  }
}

static String promptForSetting(const char* label, const String& current) {
  Serial.print(label);
  Serial.print(": ");
  while (!Serial.available()) {
    esp_task_wdt_reset();
    delay(20);
  }
  String value = Serial.readStringUntil('\n');
  value.trim();
  return value.length() ? value : current;
}

void loadRuntimeSettings() {
  g_preferencesReady = g_preferences.begin("ai-assistant", false);
  if (!g_preferencesReady) {
    Serial.println("⚠️  Settings storage unavailable — using compiled settings");
    customApiKey = g_aiKey;
    return;
  }

  g_wifiSsid     = g_preferences.getString("wifi_ssid", Config::SSID);
  g_wifiPassword = g_preferences.getString("wifi_pass", Config::PASSWORD);
  g_aiKey      = g_preferences.getString("groq_key", Config::AI_KEY);
  g_weatherKey   = g_preferences.getString("weather_key", Config::WEATHER_KEY);
  g_serperKey    = g_preferences.getString("serper_key", Config::SERPER_API_KEY);
  g_skillKey    = g_preferences.getString("gemini_key", Config::SKILL_KEY);
  g_modelMode   = (ModelMode)constrain(g_preferences.getUChar("model_mode", MODEL_AUTO),
                                      (uint8_t)MODEL_AUTO, (uint8_t)MODEL_SMART);
  customApiKey   = g_aiKey;

  // Accept both the current marker and the legacy marker written by the
  // previous firmware. Read them BEFORE cleanup so a successful update from
  // the previous release can still open setup after this firmware boots.
  const bool currentOtaSuccess = g_preferences.getBool(OTA_SETUP_ONCE_KEY, false);
  const bool legacyOtaSuccess  = g_preferences.getBool("setup_pending", false);
  g_setupPending = currentOtaSuccess || legacyOtaSuccess;
  // Remove legacy state only after it has been checked.
  g_preferences.remove("setup_pending");
  g_preferences.remove("ota_setup_version");
}

void runCredentialWizard() {
  Serial.println("\n╔══════════════════════════════════════════════╗");
  Serial.println("║        ESP32-AI CONNECTION SETUP             ║");
  Serial.println("╠══════════════════════════════════════════════╣");
  Serial.println("║ Enter each value, then press Enter.          ║");
  Serial.println("║ Press Enter to keep a saved/default value.   ║");
  Serial.println("╚══════════════════════════════════════════════╝\n");

  g_wifiSsid     = promptForSetting("Wi-Fi SSID", g_wifiSsid);
  g_wifiPassword = promptForSetting("Wi-Fi password", g_wifiPassword);
  g_aiKey      = promptForSetting("OpenRouter API key (AI)", g_aiKey);
  g_weatherKey   = promptForSetting("Meteosource API key", g_weatherKey);
  g_serperKey    = promptForSetting("Serper API key", g_serperKey);
  g_skillKey    = promptForSetting("OpenRouter API key (skills)", g_skillKey);
  customApiKey   = g_aiKey;

  if (g_preferencesReady) {
    g_preferences.putString("wifi_ssid", g_wifiSsid);
    g_preferences.putString("wifi_pass", g_wifiPassword);
    g_preferences.putString("groq_key", g_aiKey);
    g_preferences.putString("weather_key", g_weatherKey);
    g_preferences.putString("serper_key", g_serperKey);
    g_preferences.putString("gemini_key", g_skillKey);
    g_preferences.putBool(OTA_SETUP_ONCE_KEY, false);
  }
  g_setupPending = false;
  Serial.println("\n✅ Settings saved. Connecting with the new values...\n");
}

static void aiHttpTask(void* param) {
  for (;;) {
    if (xSemaphoreTake(g_dcReqSem, portMAX_DELAY) == pdTRUE) {
      g_dcResp.ok    = false;
      g_dcResp.reply = "";
      g_dcResp.httpCode = -1;

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

      g_dcResp.httpCode = code;
      if (code >= 200 && code < 300) {
        if (g_dcReq.doStream) {
          g_dcResp.reply = consumeSSE(http, g_dcReq.timeoutMs, g_dcReq.printTokens);
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
                     const String& body, int timeoutMs, bool doStream, bool printTokens) {
  const unsigned long apiStarted = millis();
  if (!g_dcTask) {
    // Fallback: single-core
    WiFiClientSecure secClient; secClient.setInsecure();
    HTTPClient http; http.setTimeout(timeoutMs);
    http.begin(secClient, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authBearer);
    int code = http.POST(body);
    String result;
    if (code >= 200 && code < 300) {
      result = doStream ? consumeSSE(http, timeoutMs, printTokens) : http.getString();
    } else {
      httpTimeoutCount++;
      Serial.printf("⚠️  API request failed (HTTP %d)\n", code);
    }
    http.end();
    recordApiUsage("ai", code >= 200 && code < 300 && result.length() > 0,
                   millis() - apiStarted, body.length(), result.length());
    return result;
  }

  if (g_dcBusy) {
    Serial.println("⚠️  AI service is still finishing the previous request.");
    return "";
  }

  // Remove a stale completion token left by a request that timed out locally.
  while (xSemaphoreTake(g_dcRespSem, 0) == pdTRUE) {}

  g_dcReq.url       = url;
  g_dcReq.auth      = authBearer;
  g_dcReq.body      = body;
  g_dcReq.timeoutMs = timeoutMs;
  g_dcReq.doStream  = doStream;
  g_dcReq.printTokens = printTokens;
  g_dcBusy          = true;
  xSemaphoreGive(g_dcReqSem);

  const unsigned long started = millis();
  const unsigned long waitMs = (unsigned long)timeoutMs + 10000UL;
  while (xSemaphoreTake(g_dcRespSem, pdMS_TO_TICKS(250)) != pdTRUE) {
    esp_task_wdt_reset();
    processLocalTimer();
    processReminders();
    // v1.7.9 FIX: removed updateLED() call here — the dedicated ledTask (Core 1)
    // owns all strip.show() calls. Calling it from the main loop too causes a
    // race condition on the NeoPixel bus that can crash the ESP32.
    vTaskDelay(pdMS_TO_TICKS(20));
    if (millis() - started > waitMs) {
      // The worker still owns the global request/response objects. Keep it
      // marked busy so a later call cannot overwrite live data.
      Serial.println("❌ AI request timed out");
      recordApiUsage("ai", false, millis() - apiStarted, body.length(), 0);
      return "";
    }
  }
  if (!g_dcResp.ok) {
    Serial.printf("⚠️  API request failed (HTTP %d)\n", g_dcResp.httpCode);
    recordApiUsage("ai", false, millis() - apiStarted, body.length(), 0);
    return "";
  }
  recordApiUsage("ai", true, millis() - apiStarted, body.length(), g_dcResp.reply.length());
  return g_dcResp.reply;
}

