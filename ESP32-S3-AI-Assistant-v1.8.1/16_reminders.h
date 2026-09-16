// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 16_reminders.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

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
  r.dueAt        = 0;
  r.lastTriggeredAt = 0;
  if (recur == ONCE && timeStatus() != timeNotSet) {
    tmElements_t tm{};
    tm.Year=CalendarYrToTm(year()); tm.Month=month(); tm.Day=day(); tm.Hour=h; tm.Minute=m;
    time_t candidate=makeTime(tm);
    if (candidate <= now()) candidate += SECS_PER_DAY;
    r.dueAt=(uint32_t)candidate;
  }
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
    Serial.println("  " + String(i + 1) + ". " + r.message +
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
  if (r.recurrence == ONCE && r.dueAt)
    return !r.triggered && now() >= (time_t)r.dueAt;
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
  if (timeStatus() == timeNotSet) return;
  static time_t lastMinuteChecked = 0;
  time_t currentMinute = now() / 60;
  if (currentMinute == lastMinuteChecked) return;
  lastMinuteChecked = currentMinute;

  for (int i = (int)reminders.size() - 1; i >= 0; i--) {
    Reminder& r = reminders[i];
    if (shouldReminderTrigger(r)) {
      g_lastReminderMessage = r.message;
      Serial.println("\n🔔 ═══════════ REMINDER ═══════════");
      Serial.println("   " + r.message);
      Serial.println("   " + formatReminderTime(r.hour, r.minute) + " " +
                     getRecurrenceText(r.recurrence, r.dayOfWeek, r.dayOfMonth));
      Serial.println("══════════════════════════════════\n");
      aiState = AI_ALERT; stateChangeTime = millis();
      r.triggered = true;
      r.triggerCount++;
      r.lastTriggeredAt = (uint32_t)now();
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
  JsonDocument doc(&g_jsonAllocator);
  JsonArray arr = doc["reminders"].to<JsonArray>();
  for (const auto& r : reminders) {
    JsonObject o = arr.add<JsonObject>();
    o["msg"] = r.message; o["h"] = r.hour; o["m"] = r.minute;
    o["dow"] = r.dayOfWeek; o["dom"] = r.dayOfMonth;
    o["rec"] = (int)r.recurrence; o["triggered"] = r.triggered;
    o["count"] = r.triggerCount;
    o["dueAt"] = r.dueAt; o["lastAt"] = r.lastTriggeredAt;
  }
  g_dirtyReminders = !saveStateFile("/reminders.json", doc);
}

void loadReminders() {
  if (!FFat.exists("/reminders.json")) return;
  File f = FFat.open("/reminders.json", FILE_READ); if (!f) return;
  JsonDocument doc(&g_jsonAllocator);
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
    r.dueAt        = o["dueAt"] | 0U;
    r.lastTriggeredAt = o["lastAt"] | 0U;
    reminders.push_back(r);
  }
  f.close();
}

