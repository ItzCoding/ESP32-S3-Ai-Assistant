// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 10_input_handler.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 7 ── INPUT HANDLER
// ═══════════════════════════════════════════════════════
static String formatElapsed(unsigned long elapsedMs) {
  unsigned long totalSeconds = elapsedMs / 1000UL;
  unsigned long hours = totalSeconds / 3600UL;
  unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
  unsigned long seconds = totalSeconds % 60UL;
  char buf[32];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu.%03lu",
           hours, minutes, seconds, elapsedMs % 1000UL);
  return String(buf);
}

static unsigned long stopwatchElapsedNow() {
  return g_stopwatchElapsed + (g_stopwatchRunning ? millis() - g_stopwatchStarted : 0UL);
}

static unsigned long timerRemainingNow() {
  if (!g_timerRunning || g_timerPaused) return g_timerRemaining;
  unsigned long elapsed = millis() - g_timerStarted;
  return elapsed >= g_timerRemaining ? 0UL : g_timerRemaining - elapsed;
}

static unsigned long durationPart(const String& text, const char* unit, unsigned long multiplier) {
  int unitPos = text.indexOf(unit);
  if (unitPos < 0) return 0;
  int end = unitPos - 1;
  while (end >= 0 && text[end] == ' ') end--;
  int start = end;
  while (start >= 0 && isDigit(text[start])) start--;
  if (end < 0 || start == end) return 0;
  return (unsigned long)text.substring(start + 1, end + 1).toInt() * multiplier;
}

static unsigned long parseDurationMs(const String& input) {
  String text = input;
  text.toLowerCase();
  unsigned long total = durationPart(text, "hour", 3600000UL) +
                        durationPart(text, "minute", 60000UL) +
                        durationPart(text, "second", 1000UL);
  if (total == 0) {
    static const char* words[] = {"zero", "one", "two", "three", "four", "five",
                                  "six", "seven", "eight", "nine", "ten"};
    for (int i = 1; i <= 10 && total == 0; i++) {
      if (text.indexOf(String(" ") + words[i] + " ") >= 0 || text.startsWith(String(words[i]) + " ")) {
        if (text.indexOf("hour") >= 0) total = (unsigned long)i * 3600000UL;
        else if (text.indexOf("minute") >= 0) total = (unsigned long)i * 60000UL;
        else if (text.indexOf("second") >= 0) total = (unsigned long)i * 1000UL;
      }
    }
  }
  return min(total, 7UL * 24UL * 3600000UL);
}

void processLocalTimer() {
  if (!g_timerRunning || g_timerPaused || timerRemainingNow() > 0) return;
  g_timerRunning = false;
  g_timerRemaining = 0;
  aiState = AI_ALERT;
  stateChangeTime = millis();
  Serial.println("\n🔔 ───────────── TIMER FINISHED ─────────────");
  Serial.println("   Time's up!");
  Serial.println("────────────────────────────────────────────\n");
}

static void listTasksNatural() {
  int shown = 0;
  Serial.println("\n✅ Your tasks");
  for (const auto& fact : memory) {
    if (fact.key.startsWith("task_"))
      Serial.println("   " + String(++shown) + ". " + fact.value);
  }
  if (shown == 0) Serial.println("   You're all caught up — no tasks saved.");
}

static bool completeTask(int displayIndex) {
  if (displayIndex < 1) return false;
  int shown = 0;
  for (auto it = memory.begin(); it != memory.end(); ++it) {
    if (!it->key.startsWith("task_")) continue;
    if (++shown == displayIndex) {
      String completed = it->value;
      memory.erase(it);
      g_dirtyMemory = true;
      Serial.println("🎉 Done — completed: " + completed);
      return true;
    }
  }
  return false;
}

static int firstNumberIn(const String& text) {
  for (unsigned int i = 0; i < text.length(); i++) {
    if (isDigit(text[i])) return text.substring(i).toInt();
  }
  return -1;
}

void handleInput(const String& input) {
  String lower = input;
  lower.toLowerCase();

  Serial.println("\n👤 You: " + input);
  updateUserPattern(input);

  if      (lower == "/help")    { printHelp(); }
  else if (lower == "/version") { printVersion(); }
  else if (lower == "/diag")    { systemDiagnostics(); }
  else if (lower == "/tasks" || lower == "show my tasks" || lower == "list my tasks" ||
           lower == "what are my tasks") { listTasksNatural(); }
  else if (lower.startsWith("/task add ")) {
    String task = input.substring(10); task.trim();
    if (task.isEmpty()) Serial.println("📝 Tell me what task you want to add.");
    else {
      rememberFact("task_" + String(++noteTaskCounter), task);
      Serial.println("✅ Added to your tasks: " + task);
    }
  }
  else if (lower.startsWith("/done") || lower.startsWith("complete task") ||
           lower.startsWith("finish task") ||
           (lower.startsWith("mark task") && lower.indexOf("done") >= 0)) {
    int taskNumber = firstNumberIn(lower);
    if (!completeTask(taskNumber))
      Serial.println("🤔 I couldn't find that task. Use /tasks to see the numbered list.");
  }
  else if (lower == "/reminders" || lower == "/list") { listReminders(); }
  else if (lower.startsWith("/remove") || lower.startsWith("/delete")) {
    int spacePos = input.indexOf(' ');
    // v1.8.0 FIX: previously "/remove abc" called toInt() → 0 and silently
    // deleted reminder #0. Validate the argument is actually a number first.
    String arg = (spacePos >= 0) ? input.substring(spacePos + 1) : "";
    arg.trim();
    bool numeric = (arg.length() > 0);
    for (unsigned int i = 0; i < arg.length() && numeric; i++)
      if (!isDigit(arg[i])) numeric = false;
    if (!numeric) {
      Serial.println("❌ Usage: /remove [number]   (use /reminders to see indices)");
    } else {
      int idx = arg.toInt();
      if (idx >= 0 && idx < (int)reminders.size()) {
        Serial.println("✅ Removed: " + reminders[idx].message);
        removeReminder(idx);
      } else {
        Serial.println("❌ Invalid index. Use /reminders to see the list.");
      }
    }
  }
  else if (lower.startsWith("/weather")) { getWeather(input.substring(8)); }
  else if (lower.startsWith("/search"))  {
    // v1.8.0: don't fire an empty Serper query when no argument given
    String q = input.substring(7); q.trim();
    if (q.length() == 0) Serial.println("❌ Usage: /search [query]");
    else searchWeb(q);
  }
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
    if (chatHistory.size() < 4)
      Serial.println("ℹ️  There isn't enough conversation to summarize yet.");
    else
      summarizeChatHistory(true);
  }
  else if (lower == "/update")  { checkForUpdate(false); }
  else if (lower == "/install") { installUpdate(); }

  // ── v1.8.0 new commands ─────────────────────────────
  else if (lower == "/time" || lower == "/date") {
    const char* dayNames[] = {"Error","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    int h12 = hour() % 12; if (h12 == 0) h12 = 12;
    char buf[40];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d  %d:%02d %s",
             day(), month(), year(), h12, minute(), hour() >= 12 ? "PM" : "AM");
    Serial.println(String("\n🕒 ") + dayNames[weekday()] + ", " + buf +
                   "  (UTC offset " + String(Config::NTP_OFFSET_SEC / 3600) + "h)");
  }
  else if (lower.startsWith("/forget")) {
    String key = normalizeMemoryKey(input.substring(7));
    if (key.length() == 0) {
      Serial.println("❌ Usage: /forget [key]   (see /memory for stored keys)");
    } else if (memory.empty()) {
      Serial.println("📭 Nothing stored yet.");
    } else {
      String lkey = key; lkey.toLowerCase();
      bool found = false;
      for (auto it = memory.begin(); it != memory.end(); ) {
        String k = it->key; k.toLowerCase();
        if (k == lkey) {
          Serial.println("🗑️  Forgot: " + it->key + " = " + it->value);
          it = memory.erase(it);
          found = true;
        } else {
          ++it;
        }
      }
      if (found) {
        g_dirtyMemory = true;
        flushDirtyState();
        Serial.println("✅ Saved. " + String(memory.size()) + " facts remain.");
      } else {
        Serial.println("❌ No fact with key \"" + key + "\". Use /memory to list keys.");
      }
    }
  }
  else if (lower == "/reboot") {
    Serial.println("♻️  Flushing state and rebooting...");
    flushDirtyState();
    delay(300);
    ESP.restart();
  }
  else if (lower.startsWith("/wifi")) {
    // v1.8.0 FIX: password is now the LAST space-separated token, so SSIDs
    // that contain spaces (e.g. "My Home WiFi") parse correctly.
    String args = input.substring(5); args.trim();
    int sp = args.lastIndexOf(' ');
    if (sp < 0) {
      Serial.println("❌ Usage: /wifi [ssid] [password]");
      Serial.println("   (SSID may contain spaces — the last word is the password)");
      Serial.println("   Current SSID: \"" + g_wifiSsid + "\"");
    } else {
      String ssid = args.substring(0, sp); ssid.trim();
      String pass = args.substring(sp + 1); pass.trim();
      g_wifiSsid = ssid; g_wifiPassword = pass;
      if (g_preferencesReady) {
        g_preferences.putString("wifi_ssid", ssid);
        g_preferences.putString("wifi_pass", pass);
      }
      Serial.println("\n📶 Switching to \"" + ssid + "\" (saved for future boots)...");
      WiFi.disconnect(false, true);
      delay(100);
      WiFi.begin(ssid.c_str(), pass.c_str());
      Serial.println("⏳ Connecting in background — run /diag in a moment to verify.");
    }
  }
  else if (lower == "/keys") {
    Serial.println("\n🔑 API keys currently loaded:");
    Serial.println("  OpenRouter (AI):     " + (g_aiKey.length() > 8 ? "****" + g_aiKey.substring(g_aiKey.length() - 4) : "(not set)"));
    Serial.println("  OpenRouter (skills): " + (g_skillKey.length() > 8 ? "****" + g_skillKey.substring(g_skillKey.length() - 4) : "(not set)"));
    Serial.println("  Meteosource:         " + (g_weatherKey.length() > 8 ? "****" + g_weatherKey.substring(g_weatherKey.length() - 4) : "(not set)"));
    Serial.println("  Serper:              " + (g_serperKey.length() > 8 ? "****" + g_serperKey.substring(g_serperKey.length() - 4) : "(not set)"));
    Serial.println("  Set one: /keys [ai|skill|weather|serper] [key]");
    Serial.println("  Models:  AI=" + String(Config::AI_MODEL) +
                   "  Skills=" + String(Config::SKILL_MODEL));
  }
  else if (lower.startsWith("/keys ")) {
    // v1.8.0: set API keys at runtime without reflashing — stored in NVS
    String args = input.substring(6); args.trim();
    int sp = args.indexOf(' ');
    if (sp < 0) {
      Serial.println("❌ Usage: /keys [ai|skill|weather|serper] [key]");
    } else {
      String svc = args.substring(0, sp); svc.trim(); svc.toLowerCase();
      String key = args.substring(sp + 1); key.trim();
      if (svc == "ai") {
        g_aiKey = key; customApiKey = key;
        if (g_preferencesReady) g_preferences.putString("groq_key", key);
        Serial.println("✅ OpenRouter AI key saved. Talking model: " + String(Config::AI_MODEL));
      } else if (svc == "skill") {
        g_skillKey = key;
        if (g_preferencesReady) g_preferences.putString("gemini_key", key);
        Serial.println("✅ OpenRouter skill key saved. Skill model: " + String(Config::SKILL_MODEL));
      } else if (svc == "weather") {
        g_weatherKey = key;
        if (g_preferencesReady) g_preferences.putString("weather_key", key);
        Serial.println("✅ Meteosource key saved.");
      } else if (svc == "serper") {
        g_serperKey = key;
        if (g_preferencesReady) g_preferences.putString("serper_key", key);
        Serial.println("✅ Serper key saved.");
      } else {
        Serial.println("❌ Unknown service \"" + svc + "\". Use: ai, skill, weather, serper");
      }
    }
  }

  // Real local countdown timer.
  else if ((lower.indexOf("timer") >= 0) &&
           (lower.indexOf("start") >= 0 || lower.indexOf("set") >= 0 ||
            lower.startsWith("timer for ") ||
            (lower.startsWith("/timer ") && firstNumberIn(lower) > 0)) &&
           lower.indexOf("stopwatch") < 0) {
    unsigned long duration = parseDurationMs(lower);
    if (duration == 0) {
      Serial.println("⏲️  How long should I set it for? Try: set a timer for 30 seconds.");
    } else {
      g_timerRemaining = duration;
      g_timerStarted = millis();
      g_timerPaused = false;
      g_timerRunning = true;
      Serial.println("⏲️  Timer started for " + formatElapsed(duration) + ". I'll alert you here.");
    }
  }
  else if (lower == "/timer cancel" || lower == "cancel timer" || lower == "cancel the timer") {
    g_timerRunning = false; g_timerPaused = false; g_timerRemaining = 0;
    Serial.println("🗑️  Timer cancelled.");
  }
  else if (lower == "/timer pause" || lower == "pause timer" || lower == "pause the timer") {
    if (g_timerRunning && !g_timerPaused) {
      g_timerRemaining = timerRemainingNow();
      g_timerPaused = true;
      Serial.println("⏸️  Timer paused with " + formatElapsed(g_timerRemaining) + " remaining.");
    } else Serial.println("⏲️  There isn't a running timer to pause.");
  }
  else if (lower == "/timer resume" || lower == "resume timer" || lower == "resume the timer") {
    if (g_timerRunning && g_timerPaused) {
      g_timerStarted = millis(); g_timerPaused = false;
      Serial.println("▶️  Timer resumed.");
    } else Serial.println("⏲️  There isn't a paused timer.");
  }
  else if (lower == "/timer" || lower == "timer" || lower.indexOf("time left") >= 0 ||
           lower.indexOf("timer status") >= 0) {
    if (!g_timerRunning) Serial.println("⏲️  No timer is active.");
    else Serial.println("⏲️  " + formatElapsed(timerRemainingNow()) + " remaining" +
                        String(g_timerPaused ? " (paused)." : "."));
  }

  // A real local stopwatch: no model call and no simulated controls.
  else if (lower == "/stopwatch start" || lower == "start stopwatch" ||
           lower == "start a stopwatch" || lower == "start the stopwatch") {
    if (!g_stopwatchRunning) {
      g_stopwatchStarted = millis();
      g_stopwatchRunning = true;
      Serial.println("⏱️  Stopwatch started — ask me for the elapsed time whenever you like.");
    } else {
      Serial.println("⏱️  It's already running: " + formatElapsed(stopwatchElapsedNow()));
    }
  }
  else if (lower == "/stopwatch stop" || lower == "stop stopwatch" ||
           lower == "stop the stopwatch") {
    if (g_stopwatchRunning) {
      g_stopwatchElapsed += millis() - g_stopwatchStarted;
      g_stopwatchRunning = false;
      Serial.println("⏹️  Stopwatch stopped at " + formatElapsed(g_stopwatchElapsed) + ".");
    } else {
      Serial.println("⏱️  The stopwatch isn't running.");
    }
  }
  else if (lower == "/stopwatch reset" || lower == "reset stopwatch" ||
           lower == "reset the stopwatch") {
    g_stopwatchRunning = false;
    g_stopwatchElapsed = 0;
    g_stopwatchLaps.clear();
    Serial.println("🔄 Stopwatch reset to 00:00:00.000.");
  }
  else if (lower == "/stopwatch lap" || lower == "lap" || lower == "record a lap") {
    if (!g_stopwatchRunning) {
      Serial.println("⏱️  Start the stopwatch before recording a lap.");
    } else {
      unsigned long lap = stopwatchElapsedNow();
      if (g_stopwatchLaps.size() >= 20) g_stopwatchLaps.erase(g_stopwatchLaps.begin());
      g_stopwatchLaps.push_back(lap);
      Serial.println("🏁 Lap " + String(g_stopwatchLaps.size()) + ": " + formatElapsed(lap));
    }
  }
  else if (lower == "/stopwatch" || lower == "stopwatch" ||
           lower.indexOf("stopwatch time") >= 0 || lower.indexOf("elapsed time") >= 0) {
    Serial.println("⏱️  " + formatElapsed(stopwatchElapsedNow()) +
                   String(g_stopwatchRunning ? " (running)" : " (stopped)"));
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

