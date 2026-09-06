// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 08_loop.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 6 ── MAIN LOOP  (v1.7.9: FreeRTOS flag-driven)
// ═══════════════════════════════════════════════════════
void loop() {
  esp_task_wdt_reset();
  processLocalTimer();

  // ── Consume FreeRTOS timer flags ──────────────────────
  // Each flag is set by a lightweight timer callback; heavy
  // work runs here on the main task to keep data access safe.

  if (g_flagWifiCheck) {
    g_flagWifiCheck = false;
    doWifiReconnect();
  }
  if (g_flagNtpSync) {
    g_flagNtpSync = false;
    if (timeClient.update()) setTime(timeClient.getEpochTime());
  }
  if (g_flagHeapSnap) {
    g_flagHeapSnap   = false;
    heapSnapshot     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heapSnapshotTime = millis();
  }
  if (g_flagReminder) {
    g_flagReminder = false;
    processReminders();
  }
  if (g_flagFlush) {
    g_flagFlush = false;
    flushDirtyState();
  }
  if (g_flagMorning) {
    g_flagMorning        = false;
    morningBriefingGiven = true;
    generateMorningBriefing();
  }
  if (g_flagEvening) {
    g_flagEvening      = false;
    eveningSummaryGiven = true;
    generateEveningSummary();
  }
  if (g_flagProactive) {
    g_flagProactive = false;
    checkProactiveOpportunity();
  }

  // Run history compression from the shallow loop call stack. Previously it
  // was invoked from addAssistantMessage() deep inside processConversation(),
  // which overflowed loopTask and triggered the stack-canary panic.
  if (g_historyCompressionPending) {
    g_historyCompressionPending = false;
    summarizeChatHistory(false);
  }

  // ── Serial input → input queue ────────────────────────
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (g_inputOverflow) {
        Serial.println("⚠️  Message was too long (maximum 512 characters).");
        inputString = "";
        g_inputOverflow = false;
      } else if (inputString.length() > 0) {
        String* p = new String(inputString);
        inputString = "";
        if (!p) {
          Serial.println("⚠️  Not enough memory to accept that message.");
        } else if (!g_inputQueue || !xQueueSend(g_inputQueue, &p, 0)) {
          delete p;
          Serial.println("⚠️  Input queue is busy — please send that again.");
        }
      }
    } else if (c != '\r') {
      if (inputString.length() < Config::MAX_INPUT_LEN) inputString += c;
      else g_inputOverflow = true;
    }
  }

  // ── Process one queued input per loop iteration ───────
  {
    String* pending = nullptr;
    if (g_inputQueue && xQueueReceive(g_inputQueue, &pending, 0) == pdTRUE && pending) {
      pending->trim();
      if (pending->length() > 0) handleInput(*pending);
      delete pending;
    }
  }

  // LED is handled by the dedicated ledTask; no direct call needed.
  delay(5);
}

