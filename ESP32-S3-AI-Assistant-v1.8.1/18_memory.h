// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 18_memory.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 13 ── MEMORY
// ═══════════════════════════════════════════════════════

String normalizeMemoryKey(String key) {
  key.trim();
  while (key.length() > 0) {
    char c = key[key.length() - 1];
    if (c == '?' || c == '!' || c == '.' || c == ',' || c == ']' || c == ':')
      key.remove(key.length() - 1);
    else
      break;
  }
  String lower = key;
  lower.toLowerCase();
  if (lower.startsWith("my ")) key = key.substring(3);
  else if (lower.startsWith("the ")) key = key.substring(4);
  key.trim();
  return key;
}

void rememberFact(const String& rawKey, const String& rawValue) {
  purgeExpiredMemories(false);
  String key = normalizeMemoryKey(rawKey);
  String value = rawValue;
  value.trim();
  if (key.length() == 0 || value.length() == 0) return;
  if (key.length() > 64) key.remove(64);
  if (value.length() > 512) value.remove(512);
  unsigned long epochNow = timeClient.getEpochTime();
  for (auto& f : memory) {
    if (f.key.equalsIgnoreCase(key)) {
      f.value = value; f.lastAccess = epochNow; f.accessCount++; f.expiresAt = 0;
      g_dirtyMemory = true; return;
    }
  }
  if ((int)memory.size() >= Config::MAX_MEMORY_FACTS) {
    auto it = std::min_element(memory.begin(), memory.end(),
      [](const Fact& a, const Fact& b){ return a.accessCount < b.accessCount; });
    memory.erase(it);
  }
  memory.push_back({key, value, 1, epochNow});
  g_dirtyMemory = true;
}

String recallFact(const String& key) {
  purgeExpiredMemories(false);
  String normalized = normalizeMemoryKey(key);
  unsigned long epochNow = timeClient.getEpochTime();
  for (auto& f : memory) {
    if (f.key.equalsIgnoreCase(normalized)) {
      f.accessCount++; f.lastAccess = epochNow; g_dirtyMemory = true; return f.value;
    }
  }
  return "";
}

bool removeFact(const String& rawKey) {
  String key = normalizeMemoryKey(rawKey);
  if (key.length() == 0) {
    Serial.println("⚠️  removeFact: no key — use /clear to wipe everything.");
    return false;
  }
  const size_t oldSize = memory.size();
  memory.erase(std::remove_if(memory.begin(), memory.end(),
    [&](const Fact& f){ return f.key.equalsIgnoreCase(key); }), memory.end());
  const bool removed = memory.size() != oldSize;
  if (removed) g_dirtyMemory = true;
  return removed;
}

void saveMemory() {
  JsonDocument doc(&g_jsonAllocator); JsonArray arr = doc["memory"].to<JsonArray>();
  for (const auto& f : memory) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = f.key; o["value"] = f.value;
    o["accessCount"] = f.accessCount; o["lastAccess"] = f.lastAccess;
    o["expiresAt"] = f.expiresAt;
  }
  g_dirtyMemory = !saveStateFile("/memory.json", doc);
}

void loadMemory() {
  if (!FFat.exists("/memory.json")) return;
  File file = FFat.open("/memory.json", FILE_READ); if (!file) return;
  JsonDocument doc(&g_jsonAllocator);
  if (deserializeJson(doc, file)) { file.close(); return; }
  memory.clear();
  noteTaskCounter = 0;
  for (JsonObject f : doc["memory"].as<JsonArray>()) {
    if ((int)memory.size() >= Config::MAX_MEMORY_FACTS) break;
    String key = normalizeMemoryKey(f["key"].as<String>());
    String value = f["value"].as<String>();
    if (key.isEmpty() || value.isEmpty()) continue;
    if (key.length() > 64) key.remove(64);
    if (value.length() > 512) value.remove(512);
    memory.push_back({key, value, f["accessCount"]|1, f["lastAccess"]|0UL, f["expiresAt"]|0U});
    if (key.startsWith("note_") || key.startsWith("task_")) {
      int underscore = key.indexOf('_');
      noteTaskCounter = max(noteTaskCounter, (uint32_t)key.substring(underscore + 1).toInt());
    }
  }
  file.close();
}

