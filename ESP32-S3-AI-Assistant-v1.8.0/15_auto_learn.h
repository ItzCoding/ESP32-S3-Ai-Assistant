// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 15_auto_learn.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 11 ── AUTO-LEARN  (v1.7.8: improved)
// ═══════════════════════════════════════════════════════
void autoLearnFromMessage(const String& userMsg) {
  if (!heapOk()) return;

  String prompt =
    "Analyze this message for important personal facts worth remembering long-term.\n"
    "Message: \"" + userMsg + "\"\n\n"
    "Rules:\n"
    "- Extract: name, age, birthday, city, country, job, hobby, preference, relationship, "
    "  pet, allergy, anniversary, email, phone, vehicle, language, religion, diet, school, "
    "  sport, favourite food/drink/movie/book/show, or any other personal identifying fact.\n"
    "- Ignore questions, reminders, weather queries, generic statements, and greetings.\n"
    "- Key must be a short lowercase label (e.g. name, city, job, wife_name, pet_name).\n"
    "- Value must be specific (e.g. 'Colombo' not 'a city', 'Cash' not 'a name').\n"
    "- If multiple facts exist, pick only the most important one.\n\n"
    "If a clear fact exists: {\"learned\":true,\"key\":\"name\",\"value\":\"Cash\"}\n"
    "Otherwise: {\"learned\":false}\n"
    "JSON only, no explanation, no markdown.";

  String raw = aiSimpleCall(prompt, 0.0f, 80);
  if (raw.length() == 0) return;

  if (raw.startsWith("```")) {
    int s = raw.indexOf('\n') + 1, e = raw.lastIndexOf("```");
    if (e > s) raw = raw.substring(s, e);
    raw.trim();
  }

  JsonDocument parsed;
  if (deserializeJson(parsed, raw) || !parsed["learned"].as<bool>()) return;

  String key = parsed["key"].as<String>();
  String val = parsed["value"].as<String>();
  if (key.length() > 1 && key.length() < 40 && val.length() > 0 && val.length() < 120) {
    // Don't re-save if already known with same value
    String existing = recallFact(key);
    if (existing != val) {
      rememberFact(key, val);
      Serial.println("💾 Auto-learned: " + key + " = " + val);
    }
  }
}

