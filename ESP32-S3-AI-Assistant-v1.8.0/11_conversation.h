// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 11_conversation.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 8 ── CONVERSATION ENGINE
// ═══════════════════════════════════════════════════════
void processConversation(const String& userMsg) {
  if (!heapOk()) {
    Serial.println("⚠️  Low internal SRAM — skipping AI call. Try /clear to free space.");
    return;
  }

  // v1.7.9: Auto-detect language and update if changed
  {
    String detected = detectLanguage(userMsg);
    if (detected.length() > 0 && detected != g_userLanguage) {
      g_userLanguage = detected;
      rememberFact("language", detected);
      if (detected != "English")
        Serial.println("🌐 Language detected: " + detected);
    }
  }

  calculateThinkingComplexity(userMsg);
  aiState = AI_THINKING;
  // v1.7.9 FIX: removed direct updateLED() call — ledTask owns all NeoPixel writes.
  // Calling strip.show() from both tasks simultaneously causes a race condition.

  // v1.7.8: Mood-adaptive temperature
  customTemperature = computeMoodTemperature();

  String sentiment      = "neutral";
  float  sentimentScore = 0.5f;
  String sentResult     = detectSentiment(userMsg);
  int    scoreStart     = sentResult.indexOf('(');
  int    scoreEnd       = (scoreStart > 0) ? sentResult.indexOf(')', scoreStart) : -1;
  // v1.8.0 FIX: the old code called substring(x, indexOf(')')) without checking
  // that a ')' actually exists — a malformed reply like "happy(0.8" made the
  // end index -1 and produced a wild substring() with a negative length.
  if (scoreStart > 0 && scoreEnd > scoreStart) {
    sentimentScore = sentResult.substring(scoreStart + 1, scoreEnd).toFloat();
    sentiment      = sentResult.substring(0, scoreStart - 1);
  } else {
    sentiment = sentResult;
  }
  sentiment.trim();
  trackSentiment(sentiment, sentimentScore);

  // v1.7.8: CPU to full speed for HTTP
  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);

  // Fetch changing information before asking the model. This prevents a stale
  // answer followed by a second, contradictory answer.
  String webCtx;
  bool liveSearchAttempted = false;
  String lowerMsg = userMsg;
  lowerMsg.toLowerCase();
  if (isRecencyQuery(lowerMsg) && WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🔎 Checking live sources...");
    liveSearchAttempted = true;
    webCtx = fetchWebSearchResults(buildSearchQuery(userMsg));
  }

  nlContext.lastTopic = userMsg;
  nlContext.lastIntent = INTENT_CHAT;
  nlContext.lastUpdate = millis();

  String aiReply = sendToAIStream(userMsg, webCtx);

  String replyLower = aiReply;
  replyLower.toLowerCase();
  const bool promisedSearch =
    replyLower.indexOf("let me search") >= 0 ||
    replyLower.indexOf("i'll search") >= 0 ||
    replyLower.indexOf("i am checking") >= 0 ||
    replyLower.indexOf("i'm checking") >= 0;
  if (!liveSearchAttempted && webCtx.isEmpty() && (aiIsUncertain(aiReply) || promisedSearch) &&
      WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🔎 Checking live sources for a better answer...");
    webCtx = fetchWebSearchResults(buildSearchQuery(userMsg));
    if (webCtx.length() > 0) {
      Serial.println("🌐 Live results received.");
      aiReply = sendToAIStream(userMsg, webCtx);
    }
  }

  // v1.7.8: Back to idle after HTTP
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  autoLearnFromMessage(userMsg);
  String topic = analyzeConversationTopic(userMsg);
  if (topic.length() > 0) updateKnowledgeDomain(topic, 10);

  smartResponseEnhancement(aiReply);
  addUserMessage(userMsg);
  addAssistantMessage(aiReply);
  respondToMood();

  aiState         = AI_REPLIED;
  stateChangeTime = millis();
}

