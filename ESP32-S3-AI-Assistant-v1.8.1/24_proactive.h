// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 24_proactive.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 19 ── PROACTIVE & BRIEFING  (v1.7.8: improved)
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

// v1.7.8: Evening summary — AI-generated day recap
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
  String recap = aiStream(dayData, sys, 0.7f, 200);
  if (!recap.isEmpty()) Serial.println(recap);
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

