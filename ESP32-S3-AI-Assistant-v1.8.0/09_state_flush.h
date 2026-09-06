// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 09_state_flush.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 6.5 ── DIRTY STATE FLUSH
// ═══════════════════════════════════════════════════════
void flushDirtyState() {
  if (g_dirtyMemory)    { saveMemory();         g_dirtyMemory    = false; }
  if (g_dirtyReminders) { saveReminders();       g_dirtyReminders = false; }
  if (g_dirtySentiment) { saveSentimentData();   g_dirtySentiment = false; }
  if (g_dirtyPattern)   { saveUserPattern();     g_dirtyPattern   = false; }
  if (g_dirtyKnowledge) { saveKnowledgeDomains();g_dirtyKnowledge = false; }
  if (g_dirtyChat)      { saveChatHistory();      g_dirtyChat      = false; }
}

