// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 20_user_patterns.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 15 ── USER PATTERNS & LEARNING
// ═══════════════════════════════════════════════════════

void updateUserPattern(const String& message) {
  userPattern.totalInteractions++;
  userPattern.lastInteraction = timeClient.getEpochTime();
  int h = hour();
  if (h >= 5 && h < 12)  userPattern.morningChats++;
  if (h >= 18 && h < 24) userPattern.eveningChats++;
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0 ||
      lower.indexOf("error") >= 0 || lower.indexOf("function") >= 0 ||
      lower.indexOf("debug") >= 0 || lower.indexOf("compile") >= 0 ||
      lower.indexOf("syntax") >= 0 || lower.indexOf("algorithm") >= 0)
    userPattern.techQuestions++;
  else if (message.length() < 60 && message.indexOf("?") < 0)
    userPattern.casualMessages++;
  String topic = analyzeConversationTopic(message);
  if (topic.length() > 0) {
    bool exists = false;
    for (int i = 0; i < 5; i++) if (userPattern.favoriteTopics[i] == topic) { exists = true; break; }
    if (!exists) for (int i = 0; i < 5; i++) {
      if (userPattern.favoriteTopics[i].length() == 0) { userPattern.favoriteTopics[i] = topic; break; }
    }
  }
  g_dirtyPattern = true;
}

String analyzeConversationTopic(const String& message) {
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("weather") >= 0) return "weather";
  if (lower.indexOf("remind") >= 0 || lower.indexOf("alarm") >= 0) return "reminders";
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0 || lower.indexOf("debug") >= 0) return "technical";
  if (lower.indexOf("news") >= 0 || lower.indexOf("headline") >= 0) return "news";
  if (lower.indexOf("joke") >= 0 || lower.indexOf("funny") >= 0 || lower.indexOf("laugh") >= 0) return "entertainment";
  if (lower.indexOf("food") >= 0 || lower.indexOf("eat") >= 0 || lower.indexOf("recipe") >= 0) return "food";
  if (lower.indexOf("health") >= 0 || lower.indexOf("exercise") >= 0 || lower.indexOf("gym") >= 0) return "health";
  if (lower.indexOf("money") >= 0 || lower.indexOf("finance") >= 0 || lower.indexOf("invest") >= 0) return "finance";
  if (lower.indexOf("travel") >= 0 || lower.indexOf("trip") >= 0 || lower.indexOf("holiday") >= 0) return "travel";
  if (lower.indexOf("music") >= 0 || lower.indexOf("song") >= 0 || lower.indexOf("film") >= 0 || lower.indexOf("movie") >= 0) return "media";
  if (lower.indexOf("sport") >= 0 || lower.indexOf("football") >= 0 || lower.indexOf("cricket") >= 0) return "sports";
  return "";
}

void calculateThinkingComplexity(const String& message) {
  int c = 1;
  if (message.length() > 150) c += 4;
  else if (message.length() > 80) c += 2;
  else if (message.length() > 40) c += 1;
  String lower = message; lower.toLowerCase();
  if (lower.indexOf("why") >= 0)     c += 2;
  if (lower.indexOf("how") >= 0)     c += 1;
  if (lower.indexOf("explain") >= 0) c += 2;
  if (lower.indexOf("compare") >= 0) c += 3;
  if (lower.indexOf("analyze") >= 0 || lower.indexOf("analyse") >= 0) c += 3;
  if (lower.indexOf("code") >= 0 || lower.indexOf("program") >= 0)    c += 2;
  if (lower.indexOf("difference") >= 0) c += 2;
  if (lower.indexOf("pros and cons") >= 0 || lower.indexOf("advantages") >= 0) c += 2;
  thinkingComplexity = min(10, c);
}

void saveUserPattern() {
  JsonDocument doc;
  doc["total"] = userPattern.totalInteractions; doc["morning"] = userPattern.morningChats;
  doc["evening"] = userPattern.eveningChats; doc["lastTime"] = userPattern.lastInteraction;
  doc["mood"] = userPattern.recentMood; doc["tech"] = userPattern.techQuestions;
  doc["casual"] = userPattern.casualMessages; doc["remUsage"] = userPattern.reminderUsage;
  JsonArray topics = doc["topics"].to<JsonArray>();
  for (int i = 0; i < 5; i++) if (userPattern.favoriteTopics[i].length()) topics.add(userPattern.favoriteTopics[i]);
  File f = FFat.open("/pattern.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadUserPattern() {
  if (!FFat.exists("/pattern.json")) return;
  File f = FFat.open("/pattern.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  userPattern.totalInteractions = doc["total"]|0;    userPattern.morningChats = doc["morning"]|0;
  userPattern.eveningChats      = doc["evening"]|0;  userPattern.lastInteraction = doc["lastTime"]|0UL;
  userPattern.recentMood        = doc["mood"]|"neutral"; userPattern.techQuestions = doc["tech"]|0;
  userPattern.casualMessages    = doc["casual"]|0;   userPattern.reminderUsage = doc["remUsage"]|0;
  if (doc.containsKey("topics")) {
    int i = 0;
    for (JsonVariant t : doc["topics"].as<JsonArray>())
      if (i < 5) userPattern.favoriteTopics[i++] = t.as<String>();
  }
  f.close();
}

void saveSentimentData() {
  JsonDocument doc; JsonArray arr = doc["history"].to<JsonArray>();
  for (const auto& s : sentimentHistory) {
    JsonObject o = arr.add<JsonObject>();
    o["s"] = s.sentiment; o["c"] = s.score; o["t"] = s.timestamp;
  }
  File f = FFat.open("/sentiment.json", FILE_WRITE);
  if (f) { serializeJson(doc, f); f.close(); }
}

void loadSentimentData() {
  if (!FFat.exists("/sentiment.json")) return;
  File f = FFat.open("/sentiment.json", FILE_READ); if (!f) return;
  JsonDocument doc; if (deserializeJson(doc, f)) { f.close(); return; }
  sentimentHistory.clear();
  for (JsonObject o : doc["history"].as<JsonArray>())
    sentimentHistory.push_back({o["s"].as<String>(), o["c"]|0.5f, o["t"]|0UL});
  f.close();
}

