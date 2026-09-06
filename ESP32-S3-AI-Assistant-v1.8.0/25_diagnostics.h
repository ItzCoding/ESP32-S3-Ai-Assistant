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
  Serial.printf("  Model:       %s\n", Config::AI_MODEL);
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
  Serial.println("  /update           — Check GitHub for newer firmware");
  Serial.println("  /install          — Download & flash update, then restart");
  Serial.println("  /time             — Show current date & time");
  Serial.println("  /timer [duration] — Start/status a local countdown");
  Serial.println("  /timer [pause|resume|cancel]");
  Serial.println("  /stopwatch [start|stop|reset] — Local stopwatch");
  Serial.println("  /stopwatch lap    — Record a lap");
  Serial.println("  /tasks            — List saved tasks");
  Serial.println("  /task add [text]  — Add a task");
  Serial.println("  /done N           — Complete task N");
  Serial.println("  /forget [key]     — Delete a stored fact (see /memory)");
  Serial.println("  /wifi [ssid] [pw] — Change WiFi at runtime (SSID may contain spaces)");
  Serial.println("  /keys             — Show/set API keys without reflashing");
  Serial.println("  /reboot           — Flush all settings & restart");
  Serial.println("\nv1.8.0 NEW: /time · /forget · /wifi · /reboot · reset-reason boot log");
  Serial.println("\nv1.7.9 NEW:");
  Serial.println("  Multi-language    — Just write in your language; AI auto-detects");
  Serial.println("                      and replies in Arabic, Chinese, Spanish, etc.");
  Serial.println("  Language stored   — Tell me 'my language is French' to save it.");
  Serial.println("  Language detected : " + g_userLanguage);
  Serial.println("\nNatural language — say anything, any way, e.g.:");
  Serial.println("  \"remember my name is Cash\"");
  Serial.println("  \"remind me to take medicine at 8pm\"");
  Serial.println("  \"what's the weather in London\"");
  Serial.println("  \"search for best laptops 2025\"");
  Serial.println("  \"مرحبا\" / \"你好\" / \"Hola\" — replies in your language");
  Serial.println("═══════════════════════════════════════════");
}

void printVersion() {
  Serial.println("\n" + String(Config::VERSION));
  Serial.println("Model:     " + String(Config::AI_MODEL));
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

