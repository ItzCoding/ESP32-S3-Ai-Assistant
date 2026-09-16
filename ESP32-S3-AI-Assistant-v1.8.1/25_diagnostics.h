// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 25_diagnostics.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

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
#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(CONFIG_IDF_TARGET_ESP32S3)
  temperature_sensor_get_celsius(g_tsens, &celsius);
#endif
  return celsius;
}

void systemDiagnostics() {
  unsigned long uptimeSec = (millis() - bootTime) / 1000;
  uint32_t intHeap   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t totalHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  float cpuTemp      = getCpuTemp();
  int   cpuFreq      = getCpuFrequencyMhz();  // v1.7.8

  Serial.println("\n📊 ═══ SYSTEM DIAGNOSTICS ═══");
  Serial.printf("  Version:     %s\n", Config::VERSION);
  Serial.printf("  AI model:    %s mode; last primary %s\n",
                g_modelMode==MODEL_FAST?"fast":g_modelMode==MODEL_SMART?"smart":"automatic",
                g_lastSelectedModel.c_str());
  Serial.printf("  Uptime:      %lu s  (%lu h %lu m)\n", uptimeSec, uptimeSec/3600, (uptimeSec%3600)/60);
  if (cpuTemp < 0)
    Serial.println("  CPU Temp:    unavailable on this board/core");
  else
    Serial.printf("  CPU Temp:    %.1f °C%s\n", cpuTemp,
                  cpuTemp > 75 ? " ⚠️  CRITICAL" : cpuTemp > 65 ? " ⚠️  HIGH" :
                  cpuTemp > 55 ? " ⚠️  WARM" : " ✅");
  Serial.printf("  CPU Freq:    %d MHz  %s\n", cpuFreq,  // v1.7.8
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
  Serial.printf("  AI worker:   background network task %s\n", g_dcTask ? "running ✅" : "not started ⚠️");
  Serial.printf("  HTTP errors: %d this session\n", httpTimeoutCount);
  Serial.printf("  Interactions:%d\n", userPattern.totalInteractions);
  Serial.printf("  Mood:        %s  (temp=%.2f)\n", userPattern.recentMood.c_str(), computeMoodTemperature());
  Serial.printf("  Reminders:   %d / %d\n", (int)reminders.size(), Config::MAX_REMINDERS);
  Serial.printf("  Memory facts:%d / %d\n", (int)memory.size(), Config::MAX_MEMORY_FACTS);
  Serial.printf("  Chat msgs:   %d / %d\n", (int)chatHistory.size(), effectiveChatMessageLimit());
  Serial.printf("  Skills:      %d / %d\n", (int)skillNames.size(), Config::MAX_SKILLS);
  Serial.printf("  Open tasks:  %d\n", (int)std::count_if(tasks.begin(), tasks.end(),
    [](const TaskItem& t){ return !t.completed; }));
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

  String report = aiStream(tele, sys, 0.1f, 350);
  if (!report.isEmpty()) Serial.println(report);
  Serial.println("\n══════════════════════════════");
}

void clearAll() {
  memory.clear(); chatHistory.clear(); reminders.clear(); tasks.clear(); g_searchCache.clear();
  g_apiStats = ApiUsageStats(); g_nextTaskId = 1;
  sentimentHistory.clear(); userPattern = UserPattern(); knowledgeDomains.clear();
  const char* files[] = {
    "/memory.json","/chat.json","/reminders.json",
    "/pattern.json","/sentiment.json","/knowledge.json","/skills.json",
    "/tasks.json","/api_stats.json","/search_cache.json", nullptr
  };
  for (int i = 0; files[i]; i++) {
    FFat.remove(files[i]);
    FFat.remove(String(files[i]) + ".bak");
    FFat.remove(String(files[i]) + ".tmp");
  }
  aiState = AI_IDLE;
  Serial.println("✅ All data cleared. Restarting...");
  delay(1000); ESP.restart();
}

void printHelp() {
  Serial.println("\n📖 Talk naturally — slash commands are optional");
  Serial.println("Try saying:");
  Serial.println("  Remind me to call Sam tomorrow at 6 PM");
  Serial.println("  Snooze that reminder for 15 minutes");
  Serial.println("  Add a high priority task to submit the report by Friday");
  Serial.println("  Rename task 2 to send the final report");
  Serial.println("  Move reminder 1 to Friday at 9 AM");
  Serial.println("  Remember my hotel is Ocean View for 3 days");
  Serial.println("  Use the fast model / Choose the model automatically");
  Serial.println("  What's the latest ESP32 news?");
  Serial.println("  Show my API usage statistics");
  Serial.println("  Check for firmware updates / Install the update");
  Serial.println("  Remember my city is Colombo / What's the weather?");
  Serial.println("Maintenance shortcuts: /health, /save, /keys, /clear, /reboot");
  Serial.println("Language detected: " + g_userLanguage);
}

void printVersion() {
  Serial.println("\n" + String(Config::VERSION));
  const char* modelMode=g_modelMode==MODEL_FAST?"fast":g_modelMode==MODEL_SMART?"smart":"automatic";
  Serial.println("Models:    " + String(modelMode) + "; last primary " + g_lastSelectedModel);
  Serial.println("Cache:     " + String(g_searchCache.size()) + " search result(s)");
  Serial.println("Language:  " + g_userLanguage);                          // v1.7.9
  Serial.println("CPU freq:  " + String(getCpuFrequencyMhz()) + " MHz");
  Serial.println("Mood temp: " + String(computeMoodTemperature(), 2));
  Serial.println("Reminders: " + String(reminders.size()) + "/" + String(Config::MAX_REMINDERS));
  Serial.println("Facts:     " + String(memory.size()) + "/" + String(Config::MAX_MEMORY_FACTS));
  Serial.println("Chat msgs: " + String(chatHistory.size()) + "/" + String(effectiveChatMessageLimit()));
  Serial.println("Skills:    " + String(skillNames.size()) + "/" + String(Config::MAX_SKILLS));
  Serial.println("Int SRAM:  " + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) + " bytes");
  Serial.println("PSRAM:     " + String(g_hasPsram ? "enabled" : "not present/disabled"));
  Serial.println("AI worker: " + (g_dcTask ? String("running") : String("direct mode")));
  Serial.println("LED task:  " + (g_ledTask ? String("running ✅") : String("not started")));  // v1.7.9
  Serial.println("Timers:    " + (g_tmrReminder ? String("FreeRTOS SW timers ✅") : String("not started")));// v1.7.9
  Serial.println("Uptime:    " + String((millis()-bootTime)/1000) + " s");
  Serial.println("CPU temp:  " + String(getCpuTemp(), 1) + " °C");
  if (g_updateAvailable)
    Serial.println("Update:    v" + g_latestVersion + " available — type /install");
  else if (g_otaCheckedOnBoot)
    Serial.println("Update:    up to date (checked on boot)");
  else
    Serial.println("Update:    not checked yet — type /update");
}

