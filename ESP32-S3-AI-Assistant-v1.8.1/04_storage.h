// Recoverable JSON storage. Called only from the main task.
// Prefer external RAM for large JSON trees; preserve internal RAM for WiFi/TLS.
class AssistantJsonAllocator : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
    void* ptr = g_hasPsram ? heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : nullptr;
    return ptr ? ptr : heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
  void deallocate(void* ptr) override { heap_caps_free(ptr); }
  void* reallocate(void* ptr, size_t size) override {
    if (!ptr) return allocate(size);
    if (!size) { deallocate(ptr); return nullptr; }
    void* resized = g_hasPsram ? heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : nullptr;
    return resized ? resized : heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
  }
};
static AssistantJsonAllocator g_jsonAllocator;

static bool g_storageReady = false;
static uint32_t g_storageFailures = 0;
static uint32_t g_storageRecoveries = 0;
static bool g_dirtySkills = false;

static bool validStateFile(const String& path) {
  File file = FFat.open(path, FILE_READ);
  if (!file) return false;
  JsonDocument doc(&g_jsonAllocator);
  bool valid = !deserializeJson(doc, file) && doc.is<JsonObject>();
  file.close();
  return valid;
}

static void recoverStateFiles() {
  const char* paths[] = {"/memory.json", "/reminders.json", "/chat.json",
    "/pattern.json", "/sentiment.json", "/knowledge.json", "/skills.json",
    "/tasks.json", "/api_stats.json", "/search_cache.json"};
  for (const char* path : paths) {
    String backup = String(path) + ".bak";
    if (!validStateFile(path) && validStateFile(backup)) {
      if (FFat.exists(path) && !FFat.remove(path)) continue;
      if (FFat.rename(backup, path)) {
        ++g_storageRecoveries;
        Serial.printf("Recovered saved state: %s\n", path);
      }
    }
  }
}

static bool saveStateFile(const char* path, const JsonDocument& doc) {
  if (!g_storageReady || doc.overflowed()) {
    ++g_storageFailures;
    return false;
  }
  String temp = String(path) + ".tmp";
  String backup = String(path) + ".bak";
  File file = FFat.open(temp, FILE_WRITE);
  if (!file) { ++g_storageFailures; return false; }
  size_t expected = measureJson(doc);
  size_t written = serializeJson(doc, file);
  file.flush();
  bool complete = written == expected && file.size() == expected;
  file.close();
  if (!complete || !validStateFile(temp)) {
    FFat.remove(temp);
    ++g_storageFailures;
    return false;
  }
  // Keep the previous committed file until the replacement is verified.
  if (FFat.exists(path)) {
    if ((FFat.exists(backup) && !FFat.remove(backup)) ||
        !FFat.rename(path, backup)) {
      ++g_storageFailures;
      return false;
    }
  }
  if (!FFat.rename(temp, path)) {
    if (FFat.exists(backup)) FFat.rename(backup, path);
    ++g_storageFailures;
    return false;
  }
  return true;
}

static void printHardwareHealth() {
  Serial.printf("Flash: %u MB; PSRAM: %u MB (%u bytes free)\n",
    ESP.getFlashChipSize() / 1048576U, ESP.getPsramSize() / 1048576U,
    ESP.getFreePsram());
  Serial.printf("Internal heap: %u free, %u largest block, %u minimum\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  Serial.printf("Storage: %s; failed saves: %u; recovered files: %u\n",
    g_storageReady ? "mounted" : "offline", g_storageFailures, g_storageRecoveries);
  if (g_storageReady)
    Serial.printf("Storage bytes: %u used / %u total\n",
      (unsigned)FFat.usedBytes(), (unsigned)FFat.totalBytes());
  Serial.printf("Clock: %s; WiFi: %s\n",
    timeStatus() == timeNotSet ? "awaiting NTP" : "synchronized",
    WiFi.status() == WL_CONNECTED ? "connected" : "offline");
  if (ESP.getFlashChipSize() != 16U * 1048576U || ESP.getPsramSize() != 8U * 1048576U)
    Serial.println("Expected N16R8 profile: 16 MB flash and 8 MB OPI PSRAM. Check BOARD_SETUP.md.");
}
