// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 09_state_flush.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 6.5 ── DIRTY STATE FLUSH
// ═══════════════════════════════════════════════════════
void flushDirtyState() {
  if (!g_storageReady) return;
  if (g_dirtySkills) saveSkills();
  if (g_dirtyMemory)    { saveMemory(); }
  if (g_dirtyReminders) { saveReminders(); }
  if (g_dirtySentiment) { saveSentimentData(); }
  if (g_dirtyPattern)   { saveUserPattern(); }
  if (g_dirtyKnowledge) { saveKnowledgeDomains(); }
  if (g_dirtyChat)      { saveChatHistory(); }
  if (g_dirtyTasks)     { saveTasks(); }
  if (g_dirtyApiStats)  { saveApiStats(); }
  if (g_dirtySearchCache) { saveSearchCache(); }
}

