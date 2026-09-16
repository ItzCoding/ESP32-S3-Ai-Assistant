// Natural productivity features and persistent API telemetry.

String chooseAiModel(const String& prompt, bool simpleCall) {
  if (g_modelMode == MODEL_FAST || simpleCall) return Config::AI_MODEL;
  if (g_modelMode == MODEL_SMART) return Config::AI_COMPLEX_MODEL;
  String lower=prompt; lower.toLowerCase();
  bool complex = prompt.length() > 320 || lower.indexOf("code") >= 0 ||
    lower.indexOf("debug") >= 0 || lower.indexOf("analyse") >= 0 ||
    lower.indexOf("analyze") >= 0 || lower.indexOf("compare") >= 0 ||
    lower.indexOf("explain why") >= 0 || lower.indexOf("step by step") >= 0;
  return complex ? Config::AI_COMPLEX_MODEL : Config::AI_MODEL;
}

void applyAiModelRouting(JsonDocument& doc, const String& prompt, bool simpleCall) {
  String primary=chooseAiModel(prompt,simpleCall);
  g_lastSelectedModel=primary;
  JsonArray models=doc["models"].to<JsonArray>();
  models.add(primary);
  String second = primary == Config::AI_MODEL ? Config::AI_COMPLEX_MODEL : Config::AI_MODEL;
  if (second != primary) models.add(second);
  if (String(Config::AI_FALLBACK_MODEL) != primary && String(Config::AI_FALLBACK_MODEL) != second)
    models.add(Config::AI_FALLBACK_MODEL);
}

void consolePrompt() { Serial.print("\nYOU  > "); }

static String formatDateTime(uint32_t epoch);
static bool parseNaturalDateTime(const String& input, uint32_t& due, bool allowTimeOnly);

static String normalizedSearchQuery(String query) {
  query.trim(); query.toLowerCase();
  while(query.indexOf("  ")>=0) query.replace("  "," ");
  return query;
}

static bool cachedSearchResult(const String& query, bool recency, String& result) {
  String key=normalizedSearchQuery(query);
  unsigned long ttl=recency?Config::SEARCH_CACHE_RECENT_MS:Config::SEARCH_CACHE_GENERAL_MS;
  for(auto it=g_searchCache.begin();it!=g_searchCache.end();) {
    bool expired=false;
    if (it->savedAt) {
      // A persisted entry cannot be aged safely until NTP has restored wall time.
      expired = timeStatus()==timeNotSet || (uint32_t)now()-it->savedAt>ttl/1000UL;
    } else expired=millis()-it->savedMillis>ttl;
    if(expired){it=g_searchCache.erase(it);g_dirtySearchCache=true;continue;}
    if(it->query==key){result=it->results;++g_apiStats.searchCacheHits;g_dirtyApiStats=true;return true;}
    ++it;
  }
  return false;
}

static void cacheSearchResult(const String& query,const String& result,bool recency) {
  String key=normalizedSearchQuery(query);
  for(auto it=g_searchCache.begin();it!=g_searchCache.end();++it)
    if(it->query==key){g_searchCache.erase(it);break;}
  if((int)g_searchCache.size()>=Config::MAX_SEARCH_CACHE)g_searchCache.erase(g_searchCache.begin());
  SearchCacheItem item;item.query=key;item.results=result;item.recency=recency;
  item.savedAt=timeStatus()==timeNotSet?0:(uint32_t)now();item.savedMillis=millis();
  g_searchCache.push_back(item);g_dirtySearchCache=true;
}

void saveSearchCache(){
  JsonDocument d(&g_jsonAllocator);JsonArray a=d["items"].to<JsonArray>();
  for(const auto& c:g_searchCache){JsonObject o=a.add<JsonObject>();o["q"]=c.query;o["r"]=c.results;o["at"]=c.savedAt;o["recent"]=c.recency;}
  g_dirtySearchCache=!saveStateFile("/search_cache.json",d);
}

void loadSearchCache(){
  if(!FFat.exists("/search_cache.json"))return;File f=FFat.open("/search_cache.json",FILE_READ);if(!f)return;
  JsonDocument d(&g_jsonAllocator);if(deserializeJson(d,f)){f.close();return;}f.close();g_searchCache.clear();
  for(JsonObject o:d["items"].as<JsonArray>()){SearchCacheItem c;c.query=o["q"].as<String>();c.results=o["r"].as<String>();c.savedAt=o["at"]|0U;c.savedMillis=millis();c.recency=o["recent"]|false;if(!c.query.isEmpty()&&!c.results.isEmpty())g_searchCache.push_back(c);}
}

void purgeExpiredMemories(bool announce){
  if(timeStatus()==timeNotSet)return;uint32_t current=(uint32_t)now();int removed=0;
  for(auto it=memory.begin();it!=memory.end();)if(it->expiresAt&&current>=it->expiresAt){it=memory.erase(it);++removed;}else ++it;
  if(removed){g_dirtyMemory=true;if(announce)Serial.println("Expired "+String(removed)+" temporary memor"+String(removed==1?"y.":"ies."));}
}

static int naturalCountBeforeUnit(const String& lower,int unitPos){
  int end=unitPos-1;while(end>=0&&lower[end]==' ')--end;int start=end;while(start>=0&&isDigit(lower[start]))--start;
  if(end>=0&&start<end)return constrain(lower.substring(start+1,end+1).toInt(),1,365);
  static const char* words[]={"one","two","three","four","five","six","seven","eight","nine","ten"};
  String prefix=lower.substring(0,unitPos);prefix.trim();
  for(int i=0;i<10;++i)if(prefix.endsWith(words[i]))return i+1;return 1;
}

static bool parseMemoryExpiry(const String& input,uint32_t& expiry,int& cutAt){
  if(timeStatus()==timeNotSet)return false;String lower=input;lower.toLowerCase();
  int until=lower.lastIndexOf(" until ");if(until>=0){uint32_t due=0;if(parseNaturalDateTime(input.substring(until+7),due,true)){expiry=due;cutAt=until;return true;}}
  int fp=lower.lastIndexOf(" for ");if(fp<0)return false;String tail=lower.substring(fp+5);
  struct Unit{const char* word;uint32_t seconds;};static const Unit units[]={{"minute",60},{"hour",3600},{"day",86400},{"week",604800},{"month",2592000}};
  for(const auto& u:units){int p=tail.indexOf(u.word);if(p>=0){int count=naturalCountBeforeUnit(tail,p);expiry=(uint32_t)now()+count*u.seconds;cutAt=fp;return true;}}
  return false;
}

static bool handleExpiringMemory(const String& input,const String& lower){
  if(!(lower.startsWith("remember ")||lower.startsWith("remember that ")))return false;
  uint32_t expiry=0;int cut=0;if(!parseMemoryExpiry(input,expiry,cut))return false;
  String content=input.substring(lower.startsWith("remember that ")?14:9,cut);content.trim();
  int split=content.indexOf(" is ");if(split<1)split=content.indexOf(" = ");if(split<1)return false;
  int skip=content.substring(split,split+4)==" is "?4:3;
  String key=normalizeMemoryKey(content.substring(0,split)),value=content.substring(split+skip);value.trim();
  if(key.startsWith("my "))key=key.substring(3);rememberFact(key,value);
  for(auto& f:memory)if(f.key.equalsIgnoreCase(normalizeMemoryKey(key))){f.expiresAt=expiry;g_dirtyMemory=true;break;}
  Serial.println("I'll remember "+key+" until "+formatDateTime(expiry)+".");return true;
}

static const char* taskPriorityName(TaskPriority p) {
  switch (p) {
    case TASK_URGENT: return "urgent";
    case TASK_HIGH:   return "high";
    case TASK_LOW:    return "low";
    default:          return "normal";
  }
}

static String formatDateTime(uint32_t epoch) {
  if (!epoch) return "no deadline";
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %s",
           year(epoch), month(epoch), day(epoch), formatReminderTime(hour(epoch), minute(epoch)).c_str());
  return String(buf);
}

static int monthFromName(const String& value) {
  static const char* names[] = {"january","february","march","april","may","june",
    "july","august","september","october","november","december"};
  for (int i = 0; i < 12; ++i)
    if (value.indexOf(names[i]) >= 0 || value.indexOf(String(names[i]).substring(0, 3)) >= 0) return i + 1;
  return 0;
}

// Parses today/tomorrow, ISO dates and English month dates. Time parsing remains
// delegated to the existing natural-language clock parser.
static bool parseNaturalDateTime(const String& input, uint32_t& dueAt, bool allowTimeOnly = false) {
  if (timeStatus() == timeNotSet) return false;
  String lower = input; lower.toLowerCase();
  ParsedTime pt{};
  parseTime(lower, pt);
  int targetYear = year(), targetMonth = month(), targetDay = day();
  bool hasDate = false;

  if (lower.indexOf("tomorrow") >= 0) {
    time_t t = now() + SECS_PER_DAY;
    targetYear = year(t); targetMonth = month(t); targetDay = day(t); hasDate = true;
  } else if (lower.indexOf("today") >= 0 || lower.indexOf("tonight") >= 0) {
    hasDate = true;
  } else {
    static const char* weekdays[] = {"sunday","monday","tuesday","wednesday","thursday","friday","saturday"};
    for (int i=0;i<7 && !hasDate;++i) if (lower.indexOf(weekdays[i])>=0) {
      int targetWeekday=i+1;
      int delta=(targetWeekday-weekday()+7)%7; if(delta==0)delta=7;
      time_t t=now()+(time_t)delta*SECS_PER_DAY;
      targetYear=year(t); targetMonth=month(t); targetDay=day(t); hasDate=true;
    }
    // YYYY-MM-DD
    for (unsigned int i = 0; !hasDate && i + 9 < lower.length(); ++i) {
      if (isDigit(lower[i]) && isDigit(lower[i+1]) && isDigit(lower[i+2]) && isDigit(lower[i+3]) &&
          lower[i+4] == '-' && isDigit(lower[i+5]) && isDigit(lower[i+6]) && lower[i+7] == '-' &&
          isDigit(lower[i+8]) && isDigit(lower[i+9])) {
        targetYear = lower.substring(i, i+4).toInt();
        targetMonth = lower.substring(i+5, i+7).toInt();
        targetDay = lower.substring(i+8, i+10).toInt();
        hasDate = true; break;
      }
    }
    if (!hasDate) {
      int namedMonth = monthFromName(lower);
      if (namedMonth) {
        int monthPos = -1;
        static const char* names[] = {"january","february","march","april","may","june",
          "july","august","september","october","november","december"};
        monthPos = lower.indexOf(names[namedMonth - 1]);
        if (monthPos < 0) monthPos = lower.indexOf(String(names[namedMonth - 1]).substring(0, 3));
        int pos = monthPos;
        while (pos < (int)lower.length() && !isDigit(lower[pos])) ++pos;
        if (pos < (int)lower.length()) {
          targetMonth = namedMonth;
          targetDay = lower.substring(pos).toInt();
          targetYear = year();
          hasDate = targetDay >= 1 && targetDay <= 31;
        }
      }
    }
  }

  if (!hasDate && (!allowTimeOnly || !pt.found)) return false;
  int h = 9, m = 0;
  if (pt.found) {
    if (pt.isRelative) { dueAt = now() + (uint32_t)pt.relativeMinutes * 60U; return true; }
    h = pt.hour; m = pt.minute;
  }
  tmElements_t tm{};
  tm.Year = CalendarYrToTm(targetYear); tm.Month = targetMonth; tm.Day = targetDay;
  tm.Hour = h; tm.Minute = m; tm.Second = 0;
  time_t candidate = makeTime(tm);
  // makeTime normalizes impossible dates (for example February 31). Reject
  // those instead of silently moving the user's deadline into another month.
  if (year(candidate)!=targetYear || month(candidate)!=targetMonth || day(candidate)!=targetDay) return false;
  if (!hasDate && candidate <= now()) candidate += SECS_PER_DAY;
  if (candidate <= now() || targetMonth < 1 || targetMonth > 12 || targetDay < 1 || targetDay > 31) return false;
  dueAt = (uint32_t)candidate;
  return true;
}

static TaskPriority parseTaskPriority(const String& lower) {
  if (lower.indexOf("urgent") >= 0 || lower.indexOf("critical") >= 0) return TASK_URGENT;
  if (lower.indexOf("high priority") >= 0 || lower.indexOf("important") >= 0) return TASK_HIGH;
  if (lower.indexOf("low priority") >= 0 || lower.indexOf("whenever") >= 0) return TASK_LOW;
  return TASK_NORMAL;
}

static String extractTaskTitle(String text) {
  String lower = text; lower.toLowerCase();
  static const char* prefixes[] = {"please ","add a task to ","add a task ","create a task to ",
    "create a task ","new task ","i need to ","i have to ","i must ","put on my task list ",nullptr};
  for (int i = 0; prefixes[i]; ++i) if (lower.startsWith(prefixes[i])) {
    text.remove(0, strlen(prefixes[i])); lower.remove(0, strlen(prefixes[i])); break;
  }
  static const char* cuts[] = {" due "," by "," tomorrow"," today"," tonight"," category ",
    " high priority"," low priority"," urgent"," important",nullptr};
  int cut = text.length();
  for (int i = 0; cuts[i]; ++i) { int p = lower.indexOf(cuts[i]); if (p >= 0 && p < cut) cut = p; }
  text = text.substring(0, cut); text.trim();
  return text;
}

static void addStructuredTask(const String& original) {
  String lower = original; lower.toLowerCase();
  TaskItem task;
  task.id = g_nextTaskId++;
  task.title = extractTaskTitle(original);
  task.priority = parseTaskPriority(lower);
  task.createdAt = timeStatus() == timeNotSet ? 0 : (uint32_t)now();
  parseNaturalDateTime(original, task.dueAt, false);
  int categoryPos = lower.indexOf(" category ");
  if (categoryPos >= 0) { task.category = original.substring(categoryPos + 10); task.category.trim(); }
  if (task.title.isEmpty()) { Serial.println("Tell me what the task should be."); return; }
  tasks.push_back(task); g_dirtyTasks = true;
  Serial.print("Task added: " + task.title + " [" + taskPriorityName(task.priority) + "]");
  if (task.dueAt) Serial.print(" due " + formatDateTime(task.dueAt));
  Serial.println();
}

static void listStructuredTasks(bool includeCompleted = false) {
  int shown = 0;
  Serial.println("\nYour tasks:");
  for (const auto& task : tasks) {
    if (!includeCompleted && task.completed) continue;
    Serial.print("  " + String(++shown) + ". " + task.title);
    Serial.print(" [" + String(taskPriorityName(task.priority)) + "]");
    if (task.dueAt) Serial.print(" - due " + formatDateTime(task.dueAt));
    if (task.completed) Serial.print(" - completed");
    Serial.println();
  }
  if (!shown) Serial.println("  You're all caught up.");
}

static bool finishStructuredTask(int displayIndex, const String& words) {
  int shown = 0;
  String needle = words; needle.toLowerCase();
  for (auto& task : tasks) {
    if (task.completed) continue;
    String title = task.title; title.toLowerCase();
    if ((displayIndex > 0 && ++shown == displayIndex) ||
        (displayIndex <= 0 && needle.length() > 3 && title.indexOf(needle) >= 0)) {
      task.completed = true;
      task.completedAt = timeStatus() == timeNotSet ? 0 : (uint32_t)now();
      g_dirtyTasks = true;
      Serial.println("Completed: " + task.title);
      return true;
    }
  }
  return false;
}

static TaskItem* activeTaskByNumber(int number){
  if(number<1)return nullptr;int shown=0;for(auto& t:tasks)if(!t.completed&&++shown==number)return &t;return nullptr;
}

static int firstNaturalNumber(const String& text){
  for(unsigned int i=0;i<text.length();++i)if(isDigit(text[i]))return text.substring(i).toInt();return -1;
}

static int numberAfterWord(const String& lower,const char* word){
  int p=lower.indexOf(word);if(p<0)return -1;p+=strlen(word);while(p<(int)lower.length()&&lower[p]==' ')++p;
  return p<(int)lower.length()&&isDigit(lower[p])?lower.substring(p).toInt():-1;
}

static bool editTaskNaturally(const String& input,const String& lower){
  if(lower.indexOf("task")<0)return false;
  bool editing=lower.indexOf("rename")>=0||lower.indexOf("change")>=0||lower.indexOf("move")>=0||
    lower.indexOf("reschedule")>=0||lower.indexOf("priority")>=0||lower.indexOf("make task")>=0;
  if(!editing)return false;int number=numberAfterWord(lower,"task");if(number<1&&std::count_if(tasks.begin(),tasks.end(),[](const TaskItem& t){return !t.completed;})==1)number=1;
  TaskItem* task=activeTaskByNumber(number);
  if(!task){Serial.println("I couldn't find that active task. Say 'show my tasks' first.");return true;}
  TaskPriority priority=parseTaskPriority(lower);
  if(lower.indexOf("priority")>=0||lower.indexOf("urgent")>=0||lower.indexOf("important")>=0){
    task->priority=priority;g_dirtyTasks=true;Serial.println("Updated task "+String(number)+" priority to "+taskPriorityName(priority)+".");return true;
  }
  uint32_t due=0;
  if(parseNaturalDateTime(input,due,true)&&(lower.indexOf("move")>=0||lower.indexOf("reschedule")>=0||
      lower.indexOf(" due")>=0||lower.indexOf(" by ")>=0)){
    task->dueAt=due;g_dirtyTasks=true;Serial.println("Moved \""+task->title+"\" to "+formatDateTime(due)+".");return true;
  }
  int to=lower.lastIndexOf(" to ");
  if(to>=0){String title=input.substring(to+4);title.trim();if(!title.isEmpty()){task->title=title;g_dirtyTasks=true;Serial.println("Task renamed to \""+title+"\".");return true;}}
  Serial.println("Tell me the new task name, priority, or due date.");return true;
}

static bool editReminderNaturally(const String& input,const String& lower){
  if(lower.indexOf("reminder")<0||lower.indexOf("set a reminder")>=0||lower.indexOf("snooze")>=0)return false;
  bool editing=lower.indexOf("rename")>=0||lower.indexOf("change")>=0||lower.indexOf("move")>=0||lower.indexOf("reschedule")>=0;
  if(!editing)return false;int number=numberAfterWord(lower,"reminder");if(number<1&&reminders.size()==1)number=1;
  if(number<1||number>(int)reminders.size()){Serial.println("I couldn't find that reminder. Say 'show my reminders' first.");return true;}
  Reminder& r=reminders[number-1];uint32_t due=0;
  if(parseNaturalDateTime(input,due,true)&&(lower.indexOf("move")>=0||lower.indexOf("reschedule")>=0||
      lower.indexOf(" at ")>=0||lower.indexOf(" tomorrow")>=0||lower.indexOf(" on ")>=0)){
    r.hour=hour(due);r.minute=minute(due);if(r.recurrence==ONCE)r.dueAt=due;r.triggered=false;
    g_dirtyReminders=true;Serial.println("Reminder "+String(number)+" moved to "+formatDateTime(due)+".");return true;
  }
  int to=lower.lastIndexOf(" to ");
  if(to>=0){String message=input.substring(to+4);message.trim();if(!message.isEmpty()){r.message=message;g_dirtyReminders=true;Serial.println("Reminder renamed to \""+message+"\".");return true;}}
  Serial.println("Tell me the new reminder message or time.");return true;
}

void migrateLegacyTasks() {
  bool migrated = false;
  for (auto it = memory.begin(); it != memory.end();) {
    if (!it->key.startsWith("task_")) { ++it; continue; }
    TaskItem task; task.id = g_nextTaskId++; task.title = it->value;
    task.createdAt = timeStatus() == timeNotSet ? 0 : (uint32_t)now();
    tasks.push_back(task); it = memory.erase(it); migrated = true;
  }
  if (migrated) { g_dirtyTasks = true; g_dirtyMemory = true; }
}

void saveTasks() {
  JsonDocument doc(&g_jsonAllocator); JsonArray arr = doc["tasks"].to<JsonArray>();
  doc["nextId"] = g_nextTaskId;
  for (const auto& t : tasks) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = t.id; o["title"] = t.title; o["category"] = t.category;
    o["priority"] = (int)t.priority; o["dueAt"] = t.dueAt; o["createdAt"] = t.createdAt;
    o["completedAt"] = t.completedAt; o["completed"] = t.completed;
  }
  g_dirtyTasks = !saveStateFile("/tasks.json", doc);
}

void loadTasks() {
  if (!FFat.exists("/tasks.json")) return;
  File f = FFat.open("/tasks.json", FILE_READ); if (!f) return;
  JsonDocument doc(&g_jsonAllocator); if (deserializeJson(doc, f)) { f.close(); return; } f.close();
  tasks.clear(); g_nextTaskId = doc["nextId"] | 1;
  for (JsonObject o : doc["tasks"].as<JsonArray>()) {
    TaskItem t; t.id=o["id"]|g_nextTaskId; t.title=o["title"].as<String>();
    t.category=o["category"].as<String>(); t.priority=(TaskPriority)(o["priority"]|1);
    t.dueAt=o["dueAt"]|0U; t.createdAt=o["createdAt"]|0U;
    t.completedAt=o["completedAt"]|0U; t.completed=o["completed"]|false;
    if (!t.title.isEmpty()) { tasks.push_back(t); g_nextTaskId=max(g_nextTaskId,t.id+1); }
  }
}

void recordApiUsage(const char* service, bool success, uint32_t latencyMs, size_t inChars, size_t outChars) {
  if (!strcmp(service,"ai")) { ++g_apiStats.aiRequests; if (!success) ++g_apiStats.aiFailures; }
  else if (!strcmp(service,"search")) { ++g_apiStats.searchRequests; if (!success) ++g_apiStats.searchFailures; }
  else if (!strcmp(service,"weather")) { ++g_apiStats.weatherRequests; if (!success) ++g_apiStats.weatherFailures; }
  else if (!strcmp(service,"ota")) { ++g_apiStats.otaRequests; if (!success) ++g_apiStats.otaFailures; }
  g_apiStats.estimatedInputTokens += (inChars + 3) / 4;
  g_apiStats.estimatedOutputTokens += (outChars + 3) / 4;
  g_apiStats.totalLatencyMs += latencyMs; g_dirtyApiStats = true;
}

void printApiUsageStats() {
  uint32_t total = g_apiStats.aiRequests + g_apiStats.searchRequests + g_apiStats.weatherRequests + g_apiStats.otaRequests;
  Serial.println("\nAPI usage statistics:");
  Serial.printf("  AI: %u requests, %u failures\n", g_apiStats.aiRequests, g_apiStats.aiFailures);
  Serial.printf("  Search: %u requests, %u failures\n", g_apiStats.searchRequests, g_apiStats.searchFailures);
  Serial.printf("  Weather: %u requests, %u failures\n", g_apiStats.weatherRequests, g_apiStats.weatherFailures);
  Serial.printf("  Updates: %u checks/downloads, %u failures\n", g_apiStats.otaRequests, g_apiStats.otaFailures);
  Serial.printf("  Estimated AI tokens: %llu in / %llu out\n",
    (unsigned long long)g_apiStats.estimatedInputTokens, (unsigned long long)g_apiStats.estimatedOutputTokens);
  Serial.printf("  Average request time: %lu ms; repaired responses: %u\n",
    total ? (unsigned long)(g_apiStats.totalLatencyMs / total) : 0UL, g_apiStats.repairedResponses);
  Serial.printf("  Search cache: %u hits; %u entries stored\n",g_apiStats.searchCacheHits,(unsigned)g_searchCache.size());
}

void saveApiStats() {
  JsonDocument d(&g_jsonAllocator);
  d["ai"]=g_apiStats.aiRequests; d["aiFail"]=g_apiStats.aiFailures;
  d["search"]=g_apiStats.searchRequests; d["searchFail"]=g_apiStats.searchFailures;
  d["weather"]=g_apiStats.weatherRequests; d["weatherFail"]=g_apiStats.weatherFailures;
  d["ota"]=g_apiStats.otaRequests; d["otaFail"]=g_apiStats.otaFailures;
  d["inTok"]=g_apiStats.estimatedInputTokens; d["outTok"]=g_apiStats.estimatedOutputTokens;
  d["latency"]=g_apiStats.totalLatencyMs; d["repairs"]=g_apiStats.repairedResponses;
  d["cacheHits"]=g_apiStats.searchCacheHits;
  g_dirtyApiStats = !saveStateFile("/api_stats.json", d);
}

void loadApiStats() {
  if (!FFat.exists("/api_stats.json")) return;
  File f=FFat.open("/api_stats.json",FILE_READ); if(!f)return;
  JsonDocument d(&g_jsonAllocator); if(deserializeJson(d,f)){f.close();return;} f.close();
  g_apiStats.aiRequests=d["ai"]|0U; g_apiStats.aiFailures=d["aiFail"]|0U;
  g_apiStats.searchRequests=d["search"]|0U; g_apiStats.searchFailures=d["searchFail"]|0U;
  g_apiStats.weatherRequests=d["weather"]|0U; g_apiStats.weatherFailures=d["weatherFail"]|0U;
  g_apiStats.otaRequests=d["ota"]|0U; g_apiStats.otaFailures=d["otaFail"]|0U;
  g_apiStats.estimatedInputTokens=d["inTok"]|0ULL; g_apiStats.estimatedOutputTokens=d["outTok"]|0ULL;
  g_apiStats.totalLatencyMs=d["latency"]|0ULL; g_apiStats.repairedResponses=d["repairs"]|0U;
  g_apiStats.searchCacheHits=d["cacheHits"]|0U;
}

bool responseNeedsRepair(const String& response) {
  if (response.length() < 8 || response.indexOf("No response from OpenRouter") >= 0) return true;
  String r=response; r.trim();
  if (r.endsWith(":") || r.endsWith("...") || r.endsWith("```")) return true;
  if (r.length() > 120) {
    String a=r.substring(0,40), b=r.substring(r.length()/2,r.length()/2+40);
    if (a == b) return true;
  }
  return false;
}

static bool snoozeLastReminder(const String& input) {
  if (g_lastReminderMessage.isEmpty()) { Serial.println("There is no recent reminder to snooze."); return true; }
  String lower=input; lower.toLowerCase(); ParsedTime pt{}; parseTime(lower,pt);
  int minutes = pt.isRelative ? pt.relativeMinutes : 10;
  time_t due = now() + (time_t)minutes * 60;
  addReminder(g_lastReminderMessage, hour(due), minute(due), ONCE, 0, 0);
  reminders.back().dueAt = (uint32_t)due; g_dirtyReminders = true;
  Serial.println("Snoozed for " + String(minutes) + " minutes."); return true;
}

static bool handleDatedReminder(const String& input, const String& lower) {
  if (lower.indexOf("remind me") < 0 && lower.indexOf("set a reminder") < 0 && lower.indexOf("alert me") < 0) return false;
  uint32_t due=0; if (!parseNaturalDateTime(input,due,false)) return false;
  String content=input; String lc=lower;
  static const char* pfx[]={"remind me to ","remind me about ","remind me ","set a reminder to ","set a reminder for ","alert me to ",nullptr};
  for(int i=0;pfx[i];++i) if(lc.startsWith(pfx[i])) {content.remove(0,strlen(pfx[i]));lc.remove(0,strlen(pfx[i]));break;}
  static const char* cuts[]={" tomorrow"," today"," tonight"," at "," by ",nullptr};
  int cut=content.length(); for(int i=0;cuts[i];++i){int x=lc.indexOf(cuts[i]);if(x>=0&&x<cut)cut=x;}
  int namedMonth=monthFromName(lc);
  if(namedMonth){
    static const char* months[]={"january","february","march","april","may","june","july","august","september","october","november","december"};
    int mp=lc.indexOf(months[namedMonth-1]); if(mp<0)mp=lc.indexOf(String(months[namedMonth-1]).substring(0,3));
    int on=lc.lastIndexOf(" on ",mp); if(on>=0&&on<cut)cut=on;
  }
  content=content.substring(0,cut);content.trim(); if(content.isEmpty())content="Reminder";
  addReminder(content,hour(due),minute(due),ONCE,0,0); reminders.back().dueAt=due; g_dirtyReminders=true;
  Serial.println("Reminder set: \""+content+"\" on "+formatDateTime(due)); return true;
}

bool handleSmartNaturalInput(const String& input) {
  if (input.startsWith("/")) return false;
  String lower=input; lower.toLowerCase();
  lower.trim();
  if(lower=="help"||lower=="what can you do"||lower=="show me what you can do"){
    printHelp();return true;
  }
  if(handleExpiringMemory(input,lower))return true;
  if(lower.indexOf("use the fast model")>=0||lower=="use fast mode"){
    g_modelMode=MODEL_FAST;if(g_preferencesReady)g_preferences.putUChar("model_mode",g_modelMode);
    Serial.println("AI model mode set to fast, with automatic fallbacks.");return true;
  }
  if(lower.indexOf("use the smart model")>=0||lower.indexOf("use the best model")>=0){
    g_modelMode=MODEL_SMART;if(g_preferencesReady)g_preferences.putUChar("model_mode",g_modelMode);
    Serial.println("AI model mode set to smart, with automatic fallbacks.");return true;
  }
  if(lower.indexOf("choose the model automatically")>=0||lower=="automatic model mode"){
    g_modelMode=MODEL_AUTO;if(g_preferencesReady)g_preferences.putUChar("model_mode",g_modelMode);
    Serial.println("Automatic model selection enabled.");return true;
  }
  if(lower.indexOf("which model")>=0||lower.indexOf("model are you using")>=0){
    const char* mode=g_modelMode==MODEL_FAST?"fast":g_modelMode==MODEL_SMART?"smart":"automatic";
    Serial.println("Model mode: "+String(mode)+". Last selected primary: "+g_lastSelectedModel+".");return true;
  }
  if(lower.indexOf("changelog")>=0||lower.indexOf("what's new in the update")>=0||lower.indexOf("whats new in the update")>=0){
    if(g_updateNotes.isEmpty())checkForUpdate(false);printFirmwareChangelog();return true;
  }
  if(lower.indexOf("clear")>=0&&lower.indexOf("search cache")>=0){g_searchCache.clear();g_dirtySearchCache=true;Serial.println("Search cache cleared.");return true;}
  if ((lower.indexOf("api")>=0 || lower.indexOf("token")>=0) &&
      (lower.indexOf("usage")>=0 || lower.indexOf("statistics")>=0 || lower.indexOf("stats")>=0 ||
       lower.indexOf("requests")>=0)) {
    printApiUsageStats(); return true;
  }
  if (lower.indexOf("check for update")>=0 || lower.indexOf("check for firmware")>=0 || lower=="is there an update") {
    checkForUpdate(false); return true;
  }
  if ((lower.indexOf("install")>=0 || lower.indexOf("apply")>=0 || lower.indexOf("download")>=0) &&
      (lower.indexOf("update")>=0 || lower.indexOf("firmware")>=0)) { installUpdate(); return true; }
  if (lower.indexOf("snooze")>=0 && (lower.indexOf("reminder")>=0 || !g_lastReminderMessage.isEmpty())) return snoozeLastReminder(input);
  if(editReminderNaturally(input,lower))return true;
  if (handleDatedReminder(input,lower)) return true;
  if (lower=="show my tasks" || lower=="list my tasks" || lower=="what are my tasks" || lower=="what do i need to do") {
    listStructuredTasks(false); return true;
  }
  bool addTask = lower.startsWith("add a task") || lower.startsWith("create a task") || lower.startsWith("new task") ||
    lower.startsWith("i need to ") || lower.startsWith("i have to ") || lower.startsWith("i must ");
  if (addTask) { addStructuredTask(input); return true; }
  if(editTaskNaturally(input,lower))return true;
  if (lower.startsWith("complete task") || lower.startsWith("finish task") || lower.startsWith("mark task") || lower.startsWith("i finished ")) {
    int number=-1; for(unsigned int i=0;i<lower.length();++i)if(isDigit(lower[i])){number=lower.substring(i).toInt();break;}
    String words=input;
    if(lower.startsWith("i finished ")) words=input.substring(11);
    else { int p=words.indexOf(' '); if(p>=0)words=words.substring(p+1); }
    if (!finishStructuredTask(number,words)) Serial.println("I couldn't identify that active task. Say 'show my tasks' first.");
    return true;
  }
  return false;
}
