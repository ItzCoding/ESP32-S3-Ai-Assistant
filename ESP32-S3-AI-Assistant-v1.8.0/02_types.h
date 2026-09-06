// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 02_types.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 2 ── TYPES & DATA STRUCTURES
// ═══════════════════════════════════════════════════════

enum RecurrenceType : uint8_t { ONCE, DAILY, WEEKLY, MONTHLY };

struct Reminder {
  String        message;
  uint8_t       hour, minute, dayOfWeek, dayOfMonth;
  RecurrenceType recurrence;
  bool          triggered;
  uint32_t      triggerCount;
};

struct Fact {
  String key, value;
  uint32_t      accessCount;
  unsigned long lastAccess;
};

struct ChatMessage {
  // Dynamic strings keep the 2 KB content buffer off loopTask's small stack.
  // This is the direct fix for the stack-canary panic during history summary.
  String role;
  String content;
};

struct SentimentLog {
  String sentiment;
  float  score;
  unsigned long timestamp;
};

struct UserPattern {
  int           totalInteractions = 0;
  int           morningChats      = 0;
  int           eveningChats      = 0;
  String        favoriteTopics[5];
  unsigned long lastInteraction   = 0;
  String        recentMood        = "neutral";
  int           techQuestions     = 0;
  int           casualMessages    = 0;
  int           reminderUsage     = 0;
};

struct KnowledgeArea {
  String  domain;
  int     experiencePoints;
  float   confidenceLevel;
  uint8_t colorR, colorG, colorB;
};

enum AIState : uint8_t {
  AI_IDLE, AI_THINKING, AI_REPLIED, AI_ERROR,
  AI_ALERT, AI_EXCITED, AI_CONCERNED, AI_PROACTIVE,
  AI_LEARNING, AI_EVOLVING
};

enum Intent : uint8_t {
  INTENT_NONE = 0,
  INTENT_REMINDER_SET,
  INTENT_REMINDER_LIST,
  INTENT_REMINDER_CANCEL,
  INTENT_MEMORY_SAVE,
  INTENT_MEMORY_RECALL,
  INTENT_MEMORY_FORGET,
  INTENT_NOTE_ADD,
  INTENT_NOTE_RECALL,
  INTENT_TASK_ADD,
  INTENT_SEARCH,
  INTENT_SUMMARY,
  INTENT_SYSTEM_STATUS,
  INTENT_WEATHER,
  INTENT_CORRECTION,
  INTENT_FOLLOWUP_TIME,
  INTENT_CHAT
};

struct ParsedTime {
  bool          found;
  int           hour;
  int           minute;
  bool          isRelative;
  int           relativeMinutes;
  bool          isTomorrow;
  RecurrenceType recurrence;
  int           dayOfWeek;
};

struct ParsedEntities {
  String     content;
  ParsedTime time;
  String     reference;
  int        referenceIndex;
};

struct ParsedCommand {
  Intent         intent;
  ParsedEntities entities;
  float          confidence;
};

struct NLContext {
  Intent         lastIntent;
  ParsedEntities lastEntities;
  Intent         pendingIntent;
  ParsedEntities pendingEntities;
  String         lastTopic;
  unsigned long  lastUpdate;
};

// ── v1.7.8: Function call result ─────────────────────
struct FnCallResult {
  bool   valid;
  String name;      // function name
  String argsJson;  // raw args JSON string
};

