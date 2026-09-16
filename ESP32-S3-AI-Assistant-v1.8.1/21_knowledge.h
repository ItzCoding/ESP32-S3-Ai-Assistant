// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 21_knowledge.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 16 ── KNOWLEDGE DOMAINS
// ═══════════════════════════════════════════════════════

void initializeKnowledgeDomains() {
  knowledgeDomains = {
    {"personal",      0, 0.3f, 255, 200, 100},
    {"technical",     0, 0.3f, 100, 150, 255},
    {"weather",       0, 0.3f, 150, 220, 255},
    {"reminders",     0, 0.3f, 180, 100, 255},
    {"general",       0, 0.3f, 100, 255, 150},
    {"news",          0, 0.3f, 255, 150,  50},
    {"entertainment", 0, 0.3f, 255, 100, 200},
    {"health",        0, 0.3f,  80, 220,  80},
    {"finance",       0, 0.3f, 200, 200,  50},
    {"food",          0, 0.3f, 255, 140,   0},
    {"sports",        0, 0.3f,  50, 200, 255},
  };
  saveKnowledgeDomains();
}

void updateKnowledgeDomain(const String& domain, int xpGain) {
  for (auto& kd : knowledgeDomains) {
    if (kd.domain == domain) {
      int oldXP = kd.experiencePoints; kd.experiencePoints += xpGain;
      kd.confidenceLevel = min(0.95f, 0.3f + kd.experiencePoints / 500.0f);
      if ((oldXP/100) < (kd.experiencePoints/100)) { aiState = AI_EVOLVING; stateChangeTime = millis(); }
      g_dirtyKnowledge = true; return;
    }
  }
  for (auto& kd : knowledgeDomains)
    if (kd.domain == "general") { kd.experiencePoints += xpGain; g_dirtyKnowledge = true; return; }
}

KnowledgeArea* getDominantKnowledge() {
  if (knowledgeDomains.empty()) return nullptr;
  return &*std::max_element(knowledgeDomains.begin(), knowledgeDomains.end(),
    [](const KnowledgeArea& a, const KnowledgeArea& b){ return a.experiencePoints < b.experiencePoints; });
}

void saveKnowledgeDomains() {
  JsonDocument doc(&g_jsonAllocator); JsonArray arr = doc["domains"].to<JsonArray>();
  for (const auto& kd : knowledgeDomains) {
    JsonObject o = arr.add<JsonObject>();
    o["domain"] = kd.domain; o["xp"] = kd.experiencePoints; o["conf"] = kd.confidenceLevel;
    o["r"] = kd.colorR; o["g"] = kd.colorG; o["b"] = kd.colorB;
  }
  g_dirtyKnowledge = !saveStateFile("/knowledge.json", doc);
}

void loadKnowledgeDomains() {
  if (!FFat.exists("/knowledge.json")) return;
  File f = FFat.open("/knowledge.json", FILE_READ); if (!f) return;
  JsonDocument doc(&g_jsonAllocator); if (deserializeJson(doc, f)) { f.close(); return; }
  knowledgeDomains.clear();
  for (JsonObject o : doc["domains"].as<JsonArray>())
    knowledgeDomains.push_back({
      o["domain"].as<String>(), o["xp"]|0, o["conf"]|0.3f,
      (uint8_t)(o["r"]|100), (uint8_t)(o["g"]|100), (uint8_t)(o["b"]|100)
    });
  f.close();
}

