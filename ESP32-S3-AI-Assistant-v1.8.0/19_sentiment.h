// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 19_sentiment.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 14 ── SENTIMENT & MOOD  (v1.7.8: improved)
// ═══════════════════════════════════════════════════════

String detectSentiment(const String& message) {
  String lower = message; lower.toLowerCase();
  int pos = 0, neg = 0;

  struct KW { const char* word; int score; };
  static const KW positive[] = {
    {"great",2},{"awesome",2},{"love",2},{"happy",2},{"excellent",2},
    {"wonderful",2},{"fantastic",2},{"thank",1},{"good",1},{"nice",1},
    {"perfect",2},{"brilliant",2},{"amazing",2},{"superb",2},{"joy",2},
    {"excited",2},{"thrilled",2},{"delighted",2},{"pleased",1},{"glad",1},
    {"cheerful",1},{"content",1},{"satisfied",1},{"grateful",2},{"blessed",2},
    {"positive",1},{"optimistic",1},{"hopeful",1},{"energetic",1},{"motivated",2},
    {"productive",1},{"successful",2},{"won",1},{"winning",1},{"achieved",2},
    {"accomplished",2},{"proud",2},{"confident",1},{"strong",1},{"healthy",1},
    {"laughing",2},{"smiling",2},{"enjoying",1},{"celebrating",2},{"exciting",1},
    {"interesting",1},{"helpful",1},{"useful",1},{"easy",1},{"smooth",1},
    {"!",1},{"😊",2},{"😄",2},{"😍",2},{"🎉",2},{"❤️",2},{"👍",1},{"✅",1},
    {nullptr,0}
  };
  static const KW negative[] = {
    {"terrible",2},{"awful",2},{"hate",2},{"sad",2},{"angry",2},
    {"frustrated",2},{"horrible",2},{"bad",1},{"worst",2},{"annoying",1},
    {"depressed",3},{"anxious",2},{"stressed",2},{"overwhelmed",2},{"exhausted",2},
    {"tired",1},{"bored",1},{"lonely",2},{"miserable",3},{"hopeless",2},
    {"disappointing",2},{"disappointed",2},{"failed",2},{"failure",2},{"broke",1},
    {"broken",1},{"stuck",1},{"lost",1},{"confused",1},{"worried",2},
    {"scared",2},{"afraid",2},{"angry",2},{"furious",3},{"upset",2},
    {"crying",2},{"hurt",2},{"pain",2},{"suffering",2},{"struggling",2},
    {"difficult",1},{"hard time",2},{"tough",1},{"rough",1},{"ugh",2},
    {"argh",2},{"damn",1},{"hate this",3},{"can't stand",2},{"sick of",2},
    {"😞",2},{"😢",2},{"😡",2},{"😤",2},{"💔",2},{"😭",2},{"🤬",3},
    {nullptr,0}
  };

  for (int i = 0; positive[i].word; i++) if (lower.indexOf(positive[i].word) >= 0) pos += positive[i].score;
  for (int i = 0; negative[i].word; i++) if (lower.indexOf(negative[i].word) >= 0) neg += negative[i].score;

  // Strong signal — skip the AI API call
  if (pos >= 3 && neg == 0) return "positive (0.90)";
  if (pos >= 2 && neg == 0) return "positive (0.80)";
  if (neg >= 3 && pos == 0) return "negative (0.90)";
  if (neg >= 2 && pos == 0) return "negative (0.80)";
  if (pos == 0 && neg == 0) return "neutral (0.50)";
  if (pos > neg) return "positive (" + String(0.5f + (pos-neg)*0.1f, 2) + ")";
  if (neg > pos) return "negative (" + String(0.5f + (neg-pos)*0.1f, 2) + ")";

  // Ambiguous — use the AI API
  String prompt =
    "Classify sentiment. JSON only (no markdown): {\"s\":\"positive\",\"c\":0.8}\n"
    "s = positive/negative/neutral, c = confidence 0.0-1.0\n"
    "Message: \"" + message + "\"";
  String raw = aiSimpleCall(prompt, 0.0f, 24);
  if (raw.length() > 0) {
    if (raw.startsWith("```")) { int s = raw.indexOf('\n')+1, e = raw.lastIndexOf("```"); if(e>s){raw=raw.substring(s,e);raw.trim();} }
    JsonDocument p; if (!deserializeJson(p, raw)) return p["s"].as<String>() + " (" + String(p["c"]|0.5f, 2) + ")";
  }
  return "neutral (0.50)";
}

// v1.7.8: Mood-adaptive temperature computation
float computeMoodTemperature() {
  if (sentimentHistory.empty()) return Config::AI_TEMPERATURE;

  float scoreSum = 0;
  int count = min((int)sentimentHistory.size(), 5);
  int start = (int)sentimentHistory.size() - count;
  for (int i = start; i < (int)sentimentHistory.size(); i++) {
    const auto& s = sentimentHistory[i];
    if (s.sentiment.startsWith("positive")) scoreSum += s.score;
    else if (s.sentiment.startsWith("negative")) scoreSum -= s.score;
  }
  float avg = scoreSum / count;
  // avg: negative = lower temp (careful), positive = higher temp (playful)
  // Map [-1, 1] to [0.42, 0.88]
  float temp = 0.65f + avg * 0.23f;
  return constrain(temp, 0.42f, 0.88f);
}

void trackSentiment(const String& sentiment, float score) {
  sentimentHistory.push_back({sentiment, score, timeClient.getEpochTime()});
  if ((int)sentimentHistory.size() > Config::MAX_SENTIMENT_LOG)
    sentimentHistory.erase(sentimentHistory.begin());
  if (sentiment.startsWith("positive")) { consecutivePositive++; consecutiveNegative = 0; }
  else if (sentiment.startsWith("negative")) { consecutiveNegative++; consecutivePositive = 0; }
  else { consecutivePositive = 0; consecutiveNegative = 0; }
  userPattern.recentMood = sentiment.startsWith("positive") ? "positive" :
                           sentiment.startsWith("negative") ? "negative" : "neutral";
  g_dirtySentiment = true; g_dirtyPattern = true;
}

void respondToMood() {
  if (consecutivePositive >= 3) { celebratePositiveVibes(); consecutivePositive = 0; }
  if (consecutiveNegative >= 2) { offerComfort();           consecutiveNegative = 0; }
}

void celebratePositiveVibes() {
  Serial.println("\n✨ Loving the energy! Great things are happening. 🌟");
  aiState = AI_EXCITED; stateChangeTime = millis();
}

void offerComfort() {
  String name = recallFact("name");
  if (name.length() > 0)
    Serial.println("\n💙 " + name + ", things seem a bit tough. I'm here if you want to talk.");
  else
    Serial.println("\n💙 Things seem a bit tough lately. I'm here if you need to talk it through.");
  aiState = AI_CONCERNED; stateChangeTime = millis();
}

void smartResponseEnhancement(String& response) {
  if (userPattern.recentMood == "negative" && response.indexOf("?") < 0)
    if (random(100) < 25) response += " Let me know if there's anything else I can help with.";
}

