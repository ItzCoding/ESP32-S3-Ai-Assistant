// ==================================================================
//  ESP32 AI Assistant - chat history and safe context compression
// ==================================================================

int estimateTokens(const char* text) {
  return text ? ((int)strlen(text) + 3) / 4 : 0;
}

static int effectiveChatMessageLimit() {
  return g_hasPsram ? Config::MAX_CHAT_MESSAGES : Config::NO_PSRAM_CHAT_MESSAGES;
}

// This function must be called from loop(), not from a nested response call.
bool summarizeChatHistory(bool force) {
  const int count = (int)chatHistory.size();
  if (count < 4) return false;
  if (!force && count < (effectiveChatMessageLimit() * 3 / 4)) return false;

  Serial.println("\n🧠 Tidying up the older conversation context...");

  int startIdx = 0;
  while (startIdx < count && chatHistory[startIdx].role == "system") startIdx++;
  int cutoff = startIdx + (count - startIdx) / 2;
  if (cutoff <= startIdx) return false;

  String historyText;
  historyText.reserve(2048);
  for (int i = startIdx; i < cutoff; i++) {
    historyText += (chatHistory[i].role == "assistant") ? "Assistant: " : "User: ";
    historyText += chatHistory[i].content;
    historyText += '\n';
    if (historyText.length() >= 3500) break;
  }
  if (historyText.isEmpty()) return false;

  String summaryPrompt;
  summaryPrompt.reserve(historyText.length() + 400);
  summaryPrompt =
    "Summarize this conversation in 3-5 plain sentences. Preserve user facts, "
    "preferences, decisions, unfinished tasks, and important context. Do not invent anything.\n\n";
  summaryPrompt += historyText;

  String summary = aiSimpleCall(summaryPrompt, 0.1f, 200);
  if (summary.isEmpty()) {
    Serial.println("⚠️  I couldn't compress the history right now; the original messages are safe.");
    return false;
  }

  chatHistory.erase(chatHistory.begin() + startIdx, chatHistory.begin() + cutoff);
  ChatMessage compressed;
  compressed.role = "system";
  compressed.content = "[Conversation summary] " + summary;
  if (compressed.content.length() > Config::CHAT_MSG_LEN)
    compressed.content.remove(Config::CHAT_MSG_LEN);
  chatHistory.insert(chatHistory.begin() + startIdx, compressed);
  g_dirtyChat = true;

  Serial.println("✅ Older context compressed safely.");
  return true;
}

void limitChatHistoryByTokens(int maxTokens) {
  const int tokenLimit = g_hasPsram
    ? maxTokens
    : min(maxTokens, Config::NO_PSRAM_CHAT_TOKENS);
  const int messageLimit = effectiveChatMessageLimit();

  int total = 0;
  for (const auto& m : chatHistory) total += (m.content.length() + 3) / 4;
  while ((total > tokenLimit || (int)chatHistory.size() > messageLimit) && chatHistory.size() > 2) {
    total -= (chatHistory.front().content.length() + 3) / 4;
    chatHistory.erase(chatHistory.begin());
  }
}

static ChatMessage makeChatMessage(const char* role, const String& text) {
  ChatMessage m;
  m.role = role;
  m.content = text;
  if (m.content.length() > Config::CHAT_MSG_LEN)
    m.content.remove(Config::CHAT_MSG_LEN);
  return m;
}

void addUserMessage(const String& msg) {
  chatHistory.push_back(makeChatMessage("user", msg));
  limitChatHistoryByTokens(Config::MAX_CHAT_TOKENS);
  g_dirtyChat = true;
}

void addAssistantMessage(const String& msg) {
  chatHistory.push_back(makeChatMessage("assistant", msg));
  limitChatHistoryByTokens(Config::MAX_CHAT_TOKENS);
  g_dirtyChat = true;
  if ((int)chatHistory.size() >= (effectiveChatMessageLimit() * 3 / 4))
    g_historyCompressionPending = true;
}

void saveChatHistory() {
  JsonDocument doc;
  JsonArray arr = doc["history"].to<JsonArray>();
  for (const auto& m : chatHistory) {
    JsonObject o = arr.add<JsonObject>();
    o["role"] = m.role;
    o["content"] = m.content;
  }
  File f = FFat.open("/chat.json", FILE_WRITE);
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void loadChatHistory() {
  if (!FFat.exists("/chat.json")) return;
  File f = FFat.open("/chat.json", FILE_READ);
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f)) {
    f.close();
    return;
  }
  f.close();

  chatHistory.clear();
  for (JsonObject o : doc["history"].as<JsonArray>()) {
    String role = o["role"] | "";
    String content = o["content"] | "";
    if ((role == "user" || role == "assistant" || role == "system") && !content.isEmpty())
      chatHistory.push_back(makeChatMessage(role.c_str(), content));
  }
  limitChatHistoryByTokens(Config::MAX_CHAT_TOKENS);
}
