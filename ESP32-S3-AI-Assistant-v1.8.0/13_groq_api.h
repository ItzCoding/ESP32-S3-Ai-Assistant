// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 13_groq_api.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 10 ── AI API (OpenRouter)
// ═══════════════════════════════════════════════════════

static void configureSecureClient(WiFiClientSecure& c) { c.setInsecure(); }

// ── aiSimpleCall ──────────────────────────────────────
String aiSimpleCall(const String& prompt, float temp, int maxTok) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";

  for (int attempt = 0; attempt < Config::AI_MAX_RETRIES; attempt++) {
    esp_task_wdt_reset();

    JsonDocument reqDoc;
    reqDoc["model"]       = Config::AI_MODEL;
  reqDoc["reasoning"]["enabled"] = false;   // v1.8.0: no chain-of-thought — save tokens
    reqDoc["temperature"] = temp;
    reqDoc["max_tokens"]  = maxTok;
    JsonArray msgs  = reqDoc["messages"].to<JsonArray>();
    JsonObject uMsg = msgs.add<JsonObject>();
    uMsg["role"]    = "user";
    uMsg["content"] = prompt;

    String body; serializeJson(reqDoc, body);

    setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
    String raw = dcPost(Config::AI_ENDPOINT,
                        "Bearer " + customApiKey,
                        body,
                        Config::SENTIMENT_TIMEOUT_MS,
                        false);
    setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

    if (raw.length() > 0) {
      JsonDocument resp;
      if (!deserializeJson(resp, raw) && resp.containsKey("choices")) {
        String result = resp["choices"][0]["message"]["content"].as<String>();
        result.trim();
        return result;
      }
      return "";
    }
    if (attempt < Config::AI_MAX_RETRIES - 1) {
      int delayMs = 800 * (attempt + 1);
      delay(delayMs);
    }
  }
  return "";
}

// ── v1.7.8: aiFunctionCall ────────────────────────────
// Uses OpenRouter function/tool calling (OpenAI-compatible).
// toolsJson = JSON array string of tool definitions.
// Returns FnCallResult with the chosen function and its args.
FnCallResult aiFunctionCall(const String& userMsg, const String& toolsJson) {
  FnCallResult res = {false, "", ""};
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return res;

  JsonDocument reqDoc;
  reqDoc["model"]       = Config::AI_MODEL;
  reqDoc["reasoning"]["enabled"] = false;   // v1.8.0: no chain-of-thought — save tokens
  reqDoc["temperature"] = 0.0f;
  reqDoc["max_tokens"]  = 256;
  reqDoc["stream"]      = false;

  // Parse tools array
  JsonDocument toolsDoc;
  if (deserializeJson(toolsDoc, toolsJson)) return res;
  reqDoc["tools"] = toolsDoc.as<JsonArray>();
  reqDoc["tool_choice"] = "auto";

  JsonArray msgs = reqDoc["messages"].to<JsonArray>();
  JsonObject sys = msgs.add<JsonObject>();
  sys["role"]    = "system";
  sys["content"] = "You are a function-calling assistant. Call the appropriate function based on the user's message.";
  JsonObject u = msgs.add<JsonObject>();
  u["role"]    = "user";
  u["content"] = userMsg;

  String body; serializeJson(reqDoc, body);

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String raw = dcPost(Config::AI_ENDPOINT,
                      "Bearer " + customApiKey,
                      body, Config::SENTIMENT_TIMEOUT_MS, false);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  if (raw.length() == 0) return res;

  JsonDocument resp;
  if (deserializeJson(resp, raw)) return res;

  // Check for tool_calls in response
  JsonVariant toolCalls = resp["choices"][0]["message"]["tool_calls"];
  if (!toolCalls.is<JsonArray>() || toolCalls.as<JsonArray>().size() == 0) return res;

  JsonObject call = toolCalls[0]["function"];
  res.name     = call["name"].as<String>();
  res.argsJson = call["arguments"].as<String>();
  res.valid    = res.name.length() > 0;
  return res;
}

// ── consumeSSE ──────────────────────────────────────────
static String consumeSSE(HTTPClient& http, unsigned long timeoutMs, bool printTokens) {
  WiFiClient* stream = http.getStreamPtr();
  String fullReply, line;
  unsigned long start = millis();
  bool done = false;

  JsonDocument chunk;

  while (!done && stream->connected() && (millis() - start) < timeoutMs) {
    while (!done && stream->available() && (millis() - start) < timeoutMs) {
      char c = (char)stream->read();
      if (c == '\n') {
        line.trim();
        if (line.startsWith("data: ")) {
          String payload = line.substring(6);
          if (payload == "[DONE]") { done = true; break; }
          chunk.clear();
          if (!deserializeJson(chunk, payload)) {
            const char* delta = chunk["choices"][0]["delta"]["content"] | "";
            if (*delta) {
              if (printTokens) Serial.print(delta);
              fullReply += delta;
            }
          }
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }
    if (!done) delay(1);
  }
  fullReply.trim();
  return fullReply;
}

// ── aiStream ──────────────────────────────────────────
String aiStream(const String& userPrompt, const String& sysPrompt, float temp, int maxTok) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";

  JsonDocument reqDoc;
  reqDoc["model"]       = Config::AI_MODEL;
  reqDoc["reasoning"]["enabled"] = false;   // v1.8.0: no chain-of-thought — save tokens
  reqDoc["temperature"] = temp;
  reqDoc["max_tokens"]  = maxTok;
  reqDoc["stream"]      = true;

  JsonArray msgs = reqDoc["messages"].to<JsonArray>();
  if (sysPrompt.length() > 0) {
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"]    = "system";
    sys["content"] = sysPrompt;
  }
  JsonObject u = msgs.add<JsonObject>();
  u["role"]    = "user";
  u["content"] = userPrompt;

  String body; serializeJson(reqDoc, body);

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String result = dcPost(Config::AI_ENDPOINT,
                         "Bearer " + customApiKey,
                         body, Config::HTTP_TIMEOUT_MS, true);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);
  return result;
}

// ── sendToAIStream ────────────────────────────────────
String sendToAIStream(const String& userMessage, const String& extraContext) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) {
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis();
    return "❌ No internal SRAM or no WiFi connection.";
  }

  JsonDocument doc;
  doc["model"]       = Config::AI_MODEL;
  doc["reasoning"]["enabled"] = false;   // v1.8.0: no chain-of-thought — save tokens
  doc["temperature"] = customTemperature;
  doc["max_tokens"]  = customMaxTokens;
  doc["top_p"]       = 0.9;
  doc["stream"]      = true;

  JsonArray messages = doc["messages"].to<JsonArray>();

  JsonObject sys  = messages.add<JsonObject>();
  sys["role"]     = "system";
  sys["content"]  = buildSystemPrompt() + buildLiveContext() +
    (extraContext.length() > 0
      ? "\n## Live Web Search Results Injected\n"
        "The following search results were fetched right now and represent current, accurate data. "
        "Use them as your answer. Do NOT hedge, do NOT mention training cutoffs, do NOT say 'I don't know'. "
        "If multiple dates appear in results, use the most recent one. Answer directly.\n"
      : "");

  for (const ChatMessage& cm : chatHistory) {
    String role = cm.role;
    if (role == "model")  role = "assistant";
    JsonObject entry  = messages.add<JsonObject>();
    entry["role"]     = role;
    entry["content"]  = cm.content;
  }

  JsonObject uEntry = messages.add<JsonObject>();
  uEntry["role"]    = "user";
  uEntry["content"] = extraContext.length() > 0
    ? "[Live Search Results]\n" + extraContext + "\n\n[User Question]\n" + userMessage +
      "\n\nAnswer directly using the search results. Do not disclaim uncertainty."
    : userMessage;

  String body; serializeJson(doc, body);

  // v1.7.9 FIX: Print the "AI: " prefix BEFORE streaming starts so
  // tokens from consumeSSE appear correctly labelled in Serial Monitor.
  Serial.print("\n🤖 Assistant: ");

  String fullReply = dcPost(Config::AI_ENDPOINT,
                            "Bearer " + customApiKey,
                            body, Config::HTTP_TIMEOUT_MS, true, true);

  // Terminate streaming line regardless of outcome
  Serial.println();

  if (fullReply.length() == 0) {
    fullReply = "❌ No response from OpenRouter. Check WiFi and API key.";
    // v1.7.9 FIX: Print the error — previously it was stored but never shown
    Serial.println(fullReply);
    aiState   = AI_ERROR; blinkCount = 0; lastBlink = millis();
  } else {
    aiState         = AI_REPLIED;
    stateChangeTime = millis();
  }
  return fullReply;
}

// ── aiIsUncertain ───────────────────────────────────────
bool aiIsUncertain(const String& reply) {
  String r = reply; r.toLowerCase();
  static const char* phrases[] = {
    "i don't know","i do not know","i'm not sure","i am not sure",
    "i'm not certain","i am not certain","i don't have","i do not have",
    "i cannot find","i can't find","i cannot provide","i can't provide",
    "my knowledge","knowledge cutoff","training data","i'm unable to",
    "i am unable to","no information","no data on","not aware of",
    "couldn't find","could not find","beyond my knowledge",
    "as of my last update","as of my last training","up to date",
    "up-to-date","real-time information","real time information",
    "don't have access to real-time","do not have access to real-time",
    "i don't have the ability to browse","cannot browse the internet",
    "i recommend checking","i suggest checking","i'd recommend searching",
    "i would recommend searching","check a reliable source","check the latest",
    "not able to confirm","unable to confirm","i lack","i may not have",
    "may be outdated","might be outdated","not current","not up to date",
    nullptr
  };
  for (int i = 0; phrases[i]; i++) if (r.indexOf(phrases[i]) >= 0) return true;
  return false;
}

bool isRecencyQuery(const String& lowerMsg) {
  static const char* cues[] = {
    "latest","current","recent","recently","now","today","tonight",
    "this year","this week","this month","who won","who's winning",
    "whos winning","score","result","results","update","updates",
    "new ","just happened","breaking","live","happening now","as of",
    "right now","at the moment","currently","at present","nowadays",
    "these days","this season","this quarter","this morning","this evening",
    nullptr
  };
  for (int i = 0; cues[i]; i++)
    if (lowerMsg.indexOf(cues[i]) >= 0) return true;
  return false;
}

String buildSearchQuery(const String& message) {
  String q = message, lq = message;
  lq.toLowerCase();
  static const char* fillers[] = {
    "can you tell me","tell me about","what is","what are","who is",
    "who are","please","could you","i want to know","look up",
    "search for","find","google","?", nullptr
  };
  for (int i = 0; fillers[i]; i++) {
    int idx = lq.indexOf(fillers[i]);
    if (idx >= 0) { q.remove(idx, strlen(fillers[i])); lq.remove(idx, strlen(fillers[i])); }
  }
  q.trim();
  if (q.length() == 0) q = message;

  String lqTrim = q; lqTrim.toLowerCase();
  bool hasYear = false;
  for (int y = 2020; y <= 2030; y++) {
    if (lqTrim.indexOf(String(y)) >= 0) { hasYear = true; break; }
  }
  if (!hasYear && isRecencyQuery(lqTrim)) {
    static const char* monthNames[] = {
      "January","February","March","April","May","June",
      "July","August","September","October","November","December"
    };
    q += " " + String(monthNames[month() - 1]) + " " + String(year());
  }
  return q;
}

