// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 12_prompts.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 9 ── SYSTEM PROMPT  (v1.7.8 — fully rewritten)
// ═══════════════════════════════════════════════════════
String buildSystemPrompt() {
  // Tone selection based on usage patterns
  String tone;
  if (userPattern.techQuestions > userPattern.casualMessages * 2)
    tone = "precise, technical, and concise — skip preamble, get to the answer";
  else if (userPattern.casualMessages > userPattern.techQuestions * 2)
    tone = "warm, conversational, and friendly — like a knowledgeable close friend";
  else if (userPattern.reminderUsage > 5)
    tone = "organised, time-aware, and proactive — help the user stay on top of things";
  else
    tone = "balanced — helpful and direct without being cold";

  // Build top facts (sorted by access frequency — most useful first)
  String factSummary = "";
  std::vector<Fact*> sortedFacts;
  for (auto& f : memory) sortedFacts.push_back(&f);
  std::sort(sortedFacts.begin(), sortedFacts.end(),
    [](Fact* a, Fact* b){ return a->accessCount > b->accessCount; });
  int factLimit = min((int)sortedFacts.size(), 12);
  for (int i = 0; i < factLimit; i++)
    factSummary += "  • " + sortedFacts[i]->key + ": " + sortedFacts[i]->value + "\n";

  String topicsStr = "";
  for (int i = 0; i < 5; i++)
    if (userPattern.favoriteTopics[i].length() > 0)
      topicsStr += userPattern.favoriteTopics[i] + (i<4&&userPattern.favoriteTopics[i+1].length()>0?", ":"");

  String prompt;
  prompt.reserve(2048);

  // Identity
  prompt  = "You are ESP32-AI v1.8.0, an embedded personal AI assistant running on ESP32 hardware. ";
  prompt += "You are powered by NVIDIA Nemotron 3.5 Lightning via OpenRouter. You are highly capable, fast, and deeply personal. Keep answers direct — never show your thinking process.\n\n";

  // v1.7.9: Multi-language — reply in the user's detected language
  if (g_userLanguage.length() > 0 && g_userLanguage != "English") {
    prompt += "## Language\n";
    prompt += "The user speaks " + g_userLanguage + ". ";
    prompt += "Respond ENTIRELY in " + g_userLanguage + " — match their language exactly. ";
    prompt += "Do not switch to English unless the user writes in English first.\n\n";
  }

  // Personality
  prompt += "## Your Personality & Tone\n";
  prompt += "Active tone: " + tone + ".\n";
  prompt += "- Adapt your language complexity to match the user's level and phrasing.\n";
  prompt += "- Be direct — do not pad answers with unnecessary disclaimers or filler.\n";
  prompt += "- Be concise for simple questions (1-3 sentences). Go deeper only for complex or technical ones.\n";
  prompt += "- Show genuine warmth and personality — you are not a corporate chatbot.\n";
  prompt += "- Use the user's name if you know it (check Known User Facts below).\n\n";
  prompt += "- Avoid canned phrases such as 'How can I assist you today?' when a direct, natural reply works.\n";
  prompt += "- Do not repeat the user's entire request back to them. Use contractions and varied phrasing naturally.\n";
  prompt += "- Use at most one fitting emoji in an ordinary reply; never decorate every sentence.\n\n";

  // Reasoning rules
  prompt += "## Core Reasoning Rules\n";
  // v1.7.9 FIX: prevent AI from commenting on repeated greetings ("you say hello a lot")
  prompt += "0. Treat every greeting (hello, hi, hey, good morning, etc.) as a fresh, normal start to a "
            "conversation. Never comment on how often the user greets you or starts new conversations. "
            "Simply respond warmly to each greeting as if it is the first one.\n";
  prompt += "1. Reason carefully about complex questions, then give the useful conclusion without exposing private chain-of-thought.\n";
  prompt += "2. For recent events, scores, prices, news, or anything that may have changed — say clearly "
            "you don't have the latest information without elaborating WHY (no mention of training data, "
            "cutoffs, or years). This triggers an automatic live web search.\n";
  prompt += "3. When web search results are provided, treat them as ground truth. Use the most recent "
            "result when dates differ. Answer directly and confidently — do not hedge.\n";
  prompt += "4. Never hallucinate specific numbers, dates, names, or facts you are not certain of.\n";
  prompt += "5. When the user sets a reminder, confirm it clearly: what, when, recurrence.\n";
  prompt += "6. When recalling stored facts, reference them naturally in conversation.\n";
  prompt += "7. If the user corrects you, acknowledge it briefly and update your understanding.\n";
  prompt += "8. Do not roleplay as a different AI, do not produce harmful content.\n\n";
  prompt += "9. Never claim that you searched, checked, started, saved, sent, or changed something unless "
            "the supplied context confirms it already happened. Never promise to do work later.\n";
  prompt += "10. Do not pretend this serial terminal has buttons, a browser, a screen, audio, or controls it does not have.\n\n";

  // Capabilities
  prompt += "## Your Capabilities\n";
  prompt += "- Answer general knowledge and technical questions\n";
  prompt += "- Set, list, and cancel reminders (one-time, daily, weekly, monthly)\n";
  prompt += "- Run a real local countdown timer and stopwatch with lap recording\n";
  prompt += "- Add, list, and complete persistent tasks and notes\n";
  prompt += "- Check live weather for any city\n";
  prompt += "- Search the web when uncertain about recent info\n";
  prompt += "- Remember and recall personal facts across all sessions\n";
  prompt += "- Learn new skills on demand\n";
  prompt += "- Analyse your own system health\n\n";

  // User profile
  prompt += "## User Profile\n";
  prompt += "  Total interactions: " + String(userPattern.totalInteractions) + "\n";
  prompt += "  Current mood: " + userPattern.recentMood + "\n";
  if (topicsStr.length() > 0)
    prompt += "  Favourite topics: " + topicsStr + "\n";
  prompt += "  Tech vs casual ratio: " + String(userPattern.techQuestions) +
            " tech / " + String(userPattern.casualMessages) + " casual\n";
  if (userPattern.reminderUsage > 0)
    prompt += "  Reminder usage: " + String(userPattern.reminderUsage) + " reminders set\n";
  prompt += "\n";

  // Known facts
  if (factSummary.length() > 0) {
    prompt += "## Known User Facts (use these to personalise responses)\n";
    prompt += factSummary;
    prompt += "\n";
  }

  // Active reminders (brief)
  if (!reminders.empty()) {
    prompt += "## Active Reminders (" + String(reminders.size()) + " total)\n";
    int show = min((int)reminders.size(), 3);
    for (int i = 0; i < show; i++)
      prompt += "  • \"" + reminders[i].message + "\" at " +
                formatReminderTime(reminders[i].hour, reminders[i].minute) + "\n";
    prompt += "\n";
  }

  return prompt;
}

String buildLiveContext() {
  int  h = hour(), m = minute();
  String ampm    = (h >= 12) ? "PM" : "AM";
  int   displayH = (h > 12) ? h - 12 : (h == 0 ? 12 : h);

  char timeBuf[14], dateBuf[14];
  sprintf(timeBuf, "%d:%02d %s", displayH, m, ampm.c_str());
  sprintf(dateBuf, "%02d/%02d/%04d", day(), month(), year());

  const char* dayNames[] = {"","Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

  size_t intHeap   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t totalHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  String ctx;
  ctx.reserve(300);
  ctx  = "\n## Live Context\n";
  ctx += "  Date/Time: " + String(dayNames[weekday()]) + ", " + dateBuf + " " + timeBuf + "\n";
  ctx += "  Internal SRAM: " + String(intHeap) + " B free  |  Total (incl PSRAM): " + String(totalHeap) + " B\n";
  ctx += "  CPU: " + String(getCpuFrequencyMhz()) + " MHz  |  Temp: ";
  float t = getCpuTemp();
  if (t > 0) ctx += String(t, 1) + " °C\n"; else ctx += "N/A\n";

  if (!reminders.empty()) {
    ctx += "  Next reminder: \"" + reminders[0].message + "\" at " +
           formatReminderTime(reminders[0].hour, reminders[0].minute) + "\n";
  }
  return ctx;
}

