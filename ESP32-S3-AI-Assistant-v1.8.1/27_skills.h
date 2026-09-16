// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 27_skills.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 22 ── SELF-TAUGHT SKILLS ENGINE  (v1.7.6 DSL, preserved)
// ═══════════════════════════════════════════════════════

bool isAlnumCh(char c) { return isalnum((unsigned char)c) || c=='_'; }
char toLowerCh(char c) { return (c>='A'&&c<='Z')?(c+32):c; }

String stripToAlnum(const String& s) {
  String r=""; for(char c:s) if(isAlnumCh(c)) r+=toLowerCh(c); return r;
}

String extractKeywords(String phrase) {
  phrase.toLowerCase();
  static const char* stop[]={"a","an","the","to","of","for","in","on","at","is","are","am",
    "i","my","me","you","your","it","this","that","please","can","could","would","will",
    "do","does","did","have","has","had","be","been","being","and","or","but","with",nullptr};
  String result=""; int start=0;
  for(int i=0;i<=(int)phrase.length();i++){
    if(i==(int)phrase.length()||!isAlnumCh(phrase[i])){
      if(i>start){
        String word=phrase.substring(start,i); bool isStop=false;
        for(int j=0;stop[j];j++) if(word==stop[j]){isStop=true;break;}
        if(!isStop&&word.length()>2){if(result.length())result+=" "; result+=word;}
      }
      start=i+1;
    }
  }
  return result;
}

void rebuildSkillTriggerIndex() {
  skillTriggerIndex.clear();
  for (size_t i = 0; i < skillJson.size(); i++) {
    JsonDocument doc(&g_jsonAllocator); if (deserializeJson(doc, skillJson[i])) continue;
    for (JsonPair kv : doc["triggers"].as<JsonObject>()) {
      for (JsonVariant p : kv.value().as<JsonArray>()) {
        String phrase = p.as<String>(); phrase.toLowerCase();
        skillTriggerIndex.push_back({(int)i, String(kv.key().c_str()), phrase, extractKeywords(phrase)});
      }
    }
  }
}

bool matchLearnedSkill(const String& input, int& outSkillIdx, String& outAction) {
  String lower = input; lower.toLowerCase();

  // v1.7.9 FIX: bidirectional substring check.
  // Old: lower.indexOf(t.phrase) — only fired when the full phrase was INSIDE the input.
  //      So input="stop" never matched phrase="stop the stopwatch".
  // Fix: also match when the INPUT is a substring of the PHRASE (short bare commands).
  for (const auto& t : skillTriggerIndex) {
    if (lower.indexOf(t.phrase) >= 0 || t.phrase.indexOf(lower) >= 0) {
      outSkillIdx = t.skillIdx; outAction = t.action; return true;
    }
  }

  // Fuzzy keyword match
  String inputKw = extractKeywords(lower);

  // v1.7.9 FIX: adaptive threshold.
  // When the whole input is a single keyword (e.g. "stop"), require only score >= 1
  // so it can still match a multi-keyword trigger phrase.
  int inputKwCount = 0;
  {
    String _tmp = inputKw; int _from = 0, _p;
    while ((_p = _tmp.indexOf(' ', _from)) >= 0) { inputKwCount++; _from = _p + 1; }
    if (_tmp.length() > 0) inputKwCount++;
  }
  int fuzzyThreshold = (inputKwCount <= 1) ? 1 : 2;

  const SkillTrigger* best = nullptr; int bestScore = 0;
  for (const auto& t : skillTriggerIndex) {
    if (t.keywords.length() == 0) continue;
    int score = 0;
    int from = 0, p;
    while ((p = t.keywords.indexOf(' ', from)) >= 0) {
      String kw = t.keywords.substring(from, p);
      if (kw.length() > 2 && inputKw.indexOf(kw) >= 0) score++;
      from = p + 1;
    }
    String last = t.keywords.substring(from);
    if (last.length() > 2 && inputKw.indexOf(last) >= 0) score++;
    if (score > bestScore) { bestScore = score; best = &t; }
  }
  if (best && bestScore >= fuzzyThreshold) { outSkillIdx = best->skillIdx; outAction = best->action; return true; }
  return false;
}

// ── Expression evaluator ─────────────────────────────
namespace SkillExpr {
  struct Parser {
    const String& s; size_t pos = 0;
    JsonObject vars; JsonObject strvars;
    Parser(const String& s_, JsonObject v, JsonObject sv) : s(s_), vars(v), strvars(sv) {}
    void skipSpace() { while (pos < s.length() && s[pos]==' ') pos++; }
    float resolveIdent(const String& id) {
      if (id=="MILLIS")    return (float)millis();
      if (id=="NOW_HOUR")  return (float)hour();
      if (id=="NOW_MIN")   return (float)minute();
      if (id=="NOW_SEC")   return (float)second();
      if (id=="NOW_DAY")   return (float)day();
      if (id=="NOW_MONTH") return (float)month();
      if (id=="NOW_YEAR")  return (float)year();
      if (id=="RAND100")   return (float)(esp_random()%101);
      if (vars.containsKey(id)) return vars[id].as<float>();
      if (id.startsWith("STRLEN_")) {
        String sv = id.substring(7);
        if (strvars.containsKey(sv)) return (float)strlen(strvars[sv]|"");
      }
      return 0.0f;
    }
    float tryFunction(const String& name) {
      skipSpace(); if (pos >= s.length() || s[pos]!='(') return resolveIdent(name);
      pos++;
      float a = parseExpr(); skipSpace();
      if (name=="ABS")   { if(pos<s.length()&&s[pos]==')')pos++; return fabsf(a); }
      if (name=="FLOOR") { if(pos<s.length()&&s[pos]==')')pos++; return floorf(a); }
      if (name=="CEIL")  { if(pos<s.length()&&s[pos]==')')pos++; return ceilf(a); }
      if (name=="ROUND") { if(pos<s.length()&&s[pos]==')')pos++; return roundf(a); }
      if (name=="SIN")   { if(pos<s.length()&&s[pos]==')')pos++; return sinf(a); }
      if (name=="COS")   { if(pos<s.length()&&s[pos]==')')pos++; return cosf(a); }
      if (pos<s.length()&&s[pos]==',') pos++;
      float b = parseExpr(); skipSpace();
      if (pos<s.length()&&s[pos]==')') pos++;
      if (name=="MIN")    return min(a,b);
      if (name=="MAX")    return max(a,b);
      if (name=="RANDOM") return (float)((int)a+(int)(esp_random()%max(1,(int)(b-a)+1)));
      if (name=="MOD")    return (b!=0)?(float)((long)a%(long)b):0.0f;
      return a;
    }
    float parseNumberOrIdent() {
      skipSpace();
      if (pos<s.length()&&(isDigit(s[pos])||s[pos]=='.')) {
        size_t start=pos; while(pos<s.length()&&(isDigit(s[pos])||s[pos]=='.'))pos++;
        return s.substring(start,pos).toFloat();
      }
      size_t start=pos;
      while(pos<s.length()&&(isAlphaNumeric(s[pos])||s[pos]=='_'))pos++;
      if(pos==start) return 0.0f;
      return tryFunction(s.substring(start,pos));
    }
    float parseFactor() {
      skipSpace();
      if(pos<s.length()&&s[pos]=='('){pos++;float v=parseExpr();skipSpace();if(pos<s.length()&&s[pos]==')')pos++;return v;}
      if(pos<s.length()&&s[pos]=='-'){pos++;return -parseFactor();}
      return parseNumberOrIdent();
    }
    float parseTerm() {
      float v=parseFactor();
      for(;;){skipSpace(); if(pos<s.length()&&(s[pos]=='*'||s[pos]=='/')){char op=s[pos++];float rhs=parseFactor();v=(op=='*')?v*rhs:(rhs!=0?v/rhs:0);}else break;}
      return v;
    }
    float parseExpr() {
      float v=parseTerm();
      for(;;){skipSpace(); if(pos<s.length()&&(s[pos]=='+'||s[pos]=='-')){char op=s[pos++];float rhs=parseTerm();v=(op=='+')?v+rhs:v-rhs;}else break;}
      return v;
    }
  };
  float eval(const String& expr, JsonObject vars, JsonObject strvars={}) {
    Parser p(expr,vars,strvars); return p.parseExpr();
  }
}

static String formatVarValue(const String& varName, const String& fmt, JsonObject vars, JsonObject strvars) {
  if (strvars.containsKey(varName)) return strvars[varName].as<String>();
  float val = vars.containsKey(varName) ? vars[varName].as<float>() : 0.0f;
  if (fmt.length()==0||fmt=="int") return String((long)val);
  if (fmt.startsWith(".")&&fmt.endsWith("f")) { int d=fmt.substring(1,fmt.length()-1).toInt(); return String(val,constrain(d,0,6)); }
  if (fmt=="sec_to_mss") {
    unsigned long ms=(unsigned long)fabsf(val);
    unsigned long mins=ms/60000,secs=(ms%60000)/1000,ms3=ms%1000;
    char buf[16]; sprintf(buf,"%02lu:%02lu.%03lu",mins,secs,ms3); return String(buf);
  }
  if (fmt=="time") {
    unsigned long ep=(unsigned long)fabsf(val);
    int hh=(ep/3600)%24,mm=(ep%3600)/60;
    char buf[8]; sprintf(buf,"%02d:%02d",hh,mm); return String(buf);
  }
  return String(val,0);
}

void interpolateSay(String text, JsonObject vars, JsonObject strvars) {
  String out;
  for (size_t i = 0; i < text.length(); i++) {
    if (text[i]=='{') {
      int end = text.indexOf('}', i);
      if (end > 0) {
        String spec = text.substring(i+1, end);
        int cp = spec.indexOf(':');
        String vn = cp>=0?spec.substring(0,cp):spec, fmt = cp>=0?spec.substring(cp+1):"";
        out += formatVarValue(vn, fmt, vars, strvars);
        i = end; continue;
      }
    }
    out += text[i];
  }
  Serial.println("AI: " + out);
  addAssistantMessage(out);
}

static String interpolateStr(const String& tmpl, JsonObject vars, JsonObject strvars) {
  String out;
  for (size_t i = 0; i < tmpl.length(); i++) {
    if (tmpl[i]=='{') {
      int end = tmpl.indexOf('}', i);
      if (end > 0) {
        String spec = tmpl.substring(i+1, end);
        int cp = spec.indexOf(':');
        String vn = cp>=0?spec.substring(0,cp):spec, fmt = cp>=0?spec.substring(cp+1):"";
        out += formatVarValue(vn, fmt, vars, strvars);
        i = end; continue;
      }
    }
    out += tmpl[i];
  }
  return out;
}

// ── Validation ─────────────────────────────────────────
bool validateSkillOps(JsonArrayConst ops, JsonObject varsSchema, JsonObject strVarsSchema, int depth, String& err) {
  if (depth > 4) { err="nesting too deep"; return false; }
  if (ops.size() > 50) { err="too many ops"; return false; }
  for (JsonObjectConst op : ops) {
    const char* t = op["op"]|"";
    if (!strlen(t)) { err="missing op type"; return false; }
    static const char* allowed[] = {
      "set","inc","set_str","if","loop","say","remember","recall","ai","end", nullptr
    };
    bool ok = false; for (int i=0; allowed[i]; i++) if(strcmp(t,allowed[i])==0){ok=true;break;}
    if (!ok) { err=String("unknown op: ")+t; return false; }
    if (strcmp(t,"if")==0) {
      if (!op["then"].is<JsonArrayConst>()) { err="if missing then"; return false; }
      if (!validateSkillOps(op["then"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
      if (op["else"].is<JsonArrayConst>())
        if (!validateSkillOps(op["else"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
    }
    if (strcmp(t,"loop")==0) {
      if (!op["body"].is<JsonArrayConst>()) { err="loop missing body"; return false; }
      if (!validateSkillOps(op["body"].as<JsonArrayConst>(),varsSchema,strVarsSchema,depth+1,err)) return false;
    }
  }
  return true;
}

// ── Execute skill ops ───────────────────────────────────
void executeSkillOps(JsonArrayConst ops, JsonObject vars, JsonObject strvars) {
  for (JsonObjectConst op : ops) {
    esp_task_wdt_reset();
    const char* t = op["op"]|"";

    if (strcmp(t,"set")==0) {
      const char* var = op["var"]|""; const char* expr = op["expr"]|"";
      if (strlen(var)&&vars.containsKey(var)) vars[var]=SkillExpr::eval(String(expr),vars,strvars);

    } else if (strcmp(t,"inc")==0) {
      const char* var = op["var"]|""; float step = op["step"]|1.0f;
      if (strlen(var)&&vars.containsKey(var)) vars[var]=vars[var].as<float>()+step;

    } else if (strcmp(t,"set_str")==0) {
      const char* var = op["var"]|""; const char* val = op["val"]|"";
      if (strlen(var)) strvars[var]=interpolateStr(String(val),vars,strvars);

    } else if (strcmp(t,"say")==0) {
      const char* text = op["text"]|"";
      interpolateSay(String(text),vars,strvars);

    } else if (strcmp(t,"if")==0) {
      const char* lhsVar = op["var"]|""; const char* cmp = op["cmp"]|"==";
      float lhs = vars.containsKey(lhsVar)?vars[lhsVar].as<float>():0.0f;
      float rhs = SkillExpr::eval(String(op["val"]|"0"),vars,strvars);
      bool result = (strcmp(cmp,"==")==0)?lhs==rhs:(strcmp(cmp,"!=")==0)?lhs!=rhs:
                    (strcmp(cmp,"<")==0)?lhs<rhs:(strcmp(cmp,">")==0)?lhs>rhs:
                    (strcmp(cmp,"<=")==0)?lhs<=rhs:(strcmp(cmp,">=")==0)?lhs>=rhs:false;
      if (result) executeSkillOps(op["then"].as<JsonArrayConst>(),vars,strvars);
      else if (op["else"].is<JsonArrayConst>()) executeSkillOps(op["else"].as<JsonArrayConst>(),vars,strvars);

    } else if (strcmp(t,"loop")==0) {
      int count = constrain((int)(op["count"]|1),1,Config::MAX_LOOP_COUNT);
      JsonArrayConst body = op["body"].as<JsonArrayConst>();
      for (int li=0; li<count; li++) { esp_task_wdt_reset(); executeSkillOps(body,vars,strvars); }

    } else if (strcmp(t,"remember")==0) {
      const char* key = op["key"]|"";
      const char* srcVar = op["var"]|""; const char* srcStrvar = op["strvar"]|"";
      String val;
      if (strlen(srcVar)>0&&vars.containsKey(srcVar)) {
        val = String(vars[srcVar].as<float>(),2);
        while(val.endsWith("0")&&val.indexOf('.')>=0) val.remove(val.length()-1);
        if(val.endsWith(".")) val.remove(val.length()-1);
      } else if (strlen(srcStrvar)>0&&strvars.containsKey(srcStrvar)) {
        val = strvars[srcStrvar].as<String>();
      }
      if (strlen(key)>0&&val.length()>0) { rememberFact(String(key),val); Serial.println("💾 Skill saved: "+String(key)+" = "+val); }

    } else if (strcmp(t,"recall")==0) {
      const char* key = op["key"]|""; const char* dst = op["strvar"]|"";
      String val = recallFact(String(key));
      strvars[dst] = val.length()>0 ? val : String("(not set)");

    } else if (strcmp(t,"ai")==0) {
      const char* promptTmpl = op["prompt"]|""; const char* dst = op["strvar"]|"";
      String prompt = interpolateStr(String(promptTmpl),vars,strvars);
      if (prompt.length()>0&&heapOk()&&WiFi.status()==WL_CONNECTED) {
        String reply = aiSimpleCall(prompt, 0.3f, 100);
        strvars[dst] = reply.length()>0 ? reply : String("(no reply)");
        Serial.println("🤖 " + String(dst) + ": " + strvars[dst].as<String>());
      }

    } else if (strcmp(t,"end")==0) {
      return;
    }
  }
}

void runSkillAction(int skillIdx, const String& actionName) {
  if (skillIdx < 0 || skillIdx >= (int)skillJson.size()) return;
  JsonDocument doc(&g_jsonAllocator);
  if (deserializeJson(doc, skillJson[skillIdx])) return;

  JsonObject vars    = doc["vars"].is<JsonObject>()    ? doc["vars"].as<JsonObject>()    : doc.createNestedObject("vars");
  JsonObject strvars = doc["strvars"].is<JsonObject>() ? doc["strvars"].as<JsonObject>() : doc.createNestedObject("strvars");

  JsonObject actions = doc["actions"].as<JsonObject>();
  if (!actions.containsKey(actionName)) return;

  aiState = AI_LEARNING; stateChangeTime = millis();
  executeSkillOps(actions[actionName].as<JsonArrayConst>(), vars, strvars);

  // Persist updated vars
  String updated; serializeJson(doc, updated);
  skillJson[skillIdx] = updated;
  if (!(testingSkill && skillIdx == pendingSkillIndex)) saveSkills();
}

void listSkills() {
  if (skillNames.empty()) { Serial.println("🧠 No self-taught skills yet — just ask for something new!"); return; }
  Serial.println("\n🧠 ═══ SELF-TAUGHT SKILLS ═══");
  for (size_t i = 0; i < skillNames.size(); i++) {
    JsonDocument doc(&g_jsonAllocator); String desc = "";
    if (!deserializeJson(doc, skillJson[i])) desc = doc["description"]|"";
    Serial.println("  " + String(i) + ". " + skillNames[i] + " — " + desc);
  }
  Serial.println("══════════════════════════════");
}

void removeSkill(const String& name) {
  for (size_t i = 0; i < skillNames.size(); i++) {
    if (skillNames[i].equalsIgnoreCase(name)) {
      Serial.println("🗑️  Forgot skill: " + skillNames[i]);
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i);
      rebuildSkillTriggerIndex(); saveSkills(); return;
    }
  }
  Serial.println("❌ No skill named '" + name + "'. Use /skills to see the list.");
}

void saveSkills() {
  JsonDocument doc(&g_jsonAllocator); JsonArray arr = doc["skills"].to<JsonArray>();
  for (const auto& raw : skillJson) {
    JsonDocument one(&g_jsonAllocator);
    if (!deserializeJson(one, raw)) arr.add(one);
  }
  g_dirtySkills = !saveStateFile("/skills.json", doc);
}

void loadSkills() {
  skillNames.clear(); skillJson.clear();
  if (!FFat.exists("/skills.json")) return;
  File file = FFat.open("/skills.json", FILE_READ); if (!file) return;
  JsonDocument doc(&g_jsonAllocator);
  if (deserializeJson(doc, file)) { file.close(); return; }
  for (JsonObject s : doc["skills"].as<JsonArray>()) {
    String name = s["name"]|"unnamed";
    String raw; serializeJson(s, raw);
    skillNames.push_back(name); skillJson.push_back(raw);
  }
  rebuildSkillTriggerIndex();
  file.close();
  Serial.println("✅ Loaded " + String(skillNames.size()) + " self-taught skill(s)");
}

bool looksLikeFeatureRequest(const String& input) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return false;

  // v1.7.9 FIX: before spending an API call, check whether the input
  // overlaps with an existing skill name.  Short commands like "stop",
  // "reset", "check" share keyword(s) with "stopwatch" / "counter" etc. and
  // should route to matchLearnedSkill, NOT spawn a brand-new skill.
  if (!skillNames.empty()) {
    String _lowerInput = input; _lowerInput.toLowerCase();
    for (const auto& _sName : skillNames) {
      String _n = _sName; _n.replace("_", " "); _n.toLowerCase();
      // Full skill name inside input, or full input inside skill name
      if (_lowerInput.indexOf(_n) >= 0 || _n.indexOf(_lowerInput) >= 0) return false;
      // Any individual word of the skill name appears in input (len > 2 to skip noise)
      int _from = 0;
      while (_from < (int)_n.length()) {
        int _sp = _n.indexOf(' ', _from);
        String _word = (_sp < 0) ? _n.substring(_from) : _n.substring(_from, _sp);
        if (_word.length() > 2 && _lowerInput.indexOf(_word) >= 0) return false;
        if (_sp < 0) break;
        _from = _sp + 1;
      }
    }
  }

  String prompt =
    "Reply with EXACTLY one word: FEATURE or CHAT.\n"
    "FEATURE = the user wants this embedded device to perform/track/do a concrete new "
    "repeatable action (timers, counters, stopwatches, games, calculators, converters, trackers).\n"
    "CHAT = conversation, question, small talk, or something needing live knowledge/search.\n"
    "Message: \"" + input + "\"";
  String verdict = aiSimpleCall(prompt, 0.0f, 8);
  verdict.trim(); verdict.toUpperCase();
  return verdict.startsWith("FEATURE");
}

// ── Generic HTTPS JSON POST ─────────────────────────────
String postJson(const String& url, const String& body, const String& authBearer) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return "";
  esp_task_wdt_reset();
  WiFiClientSecure secClient; secClient.setInsecure();
  HTTPClient http; http.setTimeout(Config::SKILL_GEN_TIMEOUT_MS);
  http.begin(secClient, url); http.addHeader("Content-Type", "application/json");
  // v1.8.0: optional Bearer auth — required for OpenRouter (was Gemini key-in-URL)
  if (authBearer.length() > 0) {
    http.addHeader("Authorization", authBearer);
    http.addHeader("X-Title", "ESP32-S3-AI-Assistant");   // OpenRouter app attribution
  }
  int code = http.POST(body); esp_task_wdt_reset();
  String result; if (code >= 200 && code < 300) result = http.getString();
  else if (code != 0) Serial.println("⚠️  Skill API request failed (HTTP " + String(code) + ")");
  else Serial.printf("⚠️  postJson: HTTP error %d\n", code);
  http.end(); return result;
}

// ── Extract JSON object from raw text ─────────────────
static String extractJsonObject(const String& raw) {
  int s = raw.indexOf('{'), e = raw.lastIndexOf('}');
  if (s >= 0 && e > s) return raw.substring(s, e+1);
  return "";
}

// ── SKILL_SYSTEM_PROMPT ─────────────────────────────────
// v1.7.9: Expanded with stopwatch reference example, trigger rules, timing rules
const char* SKILL_SYSTEM_PROMPT = R"PROMPT(You write tiny "skill" definitions for a voice/text assistant running on a memory-constrained ESP32 microcontroller. The device has a SAFE, sandboxed interpreter — you may ONLY use the operations below. Never invent new op types or fields.

Output STRICT JSON only (no markdown fences, no commentary):

{
  "name": "snake_case_identifier <=32 chars",
  "description": "one short sentence",
  "vars": { "<name>": <number or boolean initial value> },
  "strvars": { "<name>": "<initial string value>" },
  "triggers": { "<action_name>": ["phrase", "..."] },
  "actions": { "<action_name>": [ <op>, ... ] }
}

STRICT LIMITS: vars max 12 numeric. strvars max 4.
triggers/actions: max 8 actions, 1-5 trigger phrases each, max 16 ops per action.

━━ TRIGGER RULES (critical — read carefully) ━━
Every action that can be invoked by a short word MUST include that bare word as one of its trigger phrases, PLUS one or more descriptive phrases.
  Good: ["stop", "stop the stopwatch", "pause timer"]
  BAD:  ["stop the stopwatch"]  ← "stop" alone will NEVER match this
Mandatory short triggers per action name:
  start  → must include: "start", "begin", "go"
  stop   → must include: "stop", "pause", "halt"
  reset  → must include: "reset", "clear", "restart"
  check  → must include: "check", "show", "status", "how long"
  add    → must include: "add"
  remove → must include: "remove", "delete"
Always include BOTH the short bare word AND longer descriptive phrases.

━━ TIMING RULES (for stopwatches / countdown / interval) ━━
To record a start timestamp:  {"op":"set","var":"start_ms","expr":"MILLIS"}
Elapsed milliseconds:         expr = "MILLIS - start_ms"
Elapsed seconds  (display):   divide by 1000  → {elapsed:.1f} seconds
Elapsed minutes  (display):   divide by 60000
NEVER use NOW_SEC alone for a stopwatch — it wraps at 59 and gives wrong results.

━━ Allowed ops ━━
- {"op":"set","var":"<var>","expr":"<arithmetic>"}
  Special tokens: MILLIS NOW_HOUR NOW_MIN NOW_SEC NOW_DAY NOW_MONTH NOW_YEAR RAND100 STRLEN_varname
  Functions: ABS(x) FLOOR(x) CEIL(x) ROUND(x) MIN(a,b) MAX(a,b) RANDOM(lo,hi) MOD(a,b) SIN(x) COS(x)
- {"op":"inc","var":"<var>","step":<n>}
- {"op":"set_str","var":"<strvar>","val":"<text with {var} interpolation>"}
- {"op":"say","text":"<text with {var:.2f} or {strvar} interpolation>"}
- {"op":"if","var":"<var>","cmp":"==|!=|<|>|<=|>=","val":"<expr>","then":[ops],"else":[ops]}
- {"op":"loop","count":<n>,"body":[ops]}
- {"op":"remember","key":"<key>","var":"<var>"}  or  {"op":"remember","key":"<key>","strvar":"<strvar>"}
- {"op":"recall","key":"<key>","strvar":"<strvar>"}
- {"op":"ai","prompt":"<text with {var}/{strvar}>","strvar":"<dest strvar>"}
- {"op":"end"}

━━ Example 1 — stopwatch (correct timing, full short+long triggers, running-guard) ━━
{"name":"stopwatch","description":"Tracks elapsed time in milliseconds since start","vars":{"start_ms":0,"stop_ms":0,"elapsed":0,"running":0},"strvars":{},"triggers":{"start":["start","begin","go","start the stopwatch","start timer"],"stop":["stop","pause","halt","stop the stopwatch","stop timer"],"reset":["reset","clear","restart","reset the stopwatch"],"check":["check","show","status","how long","elapsed","time so far"]},"actions":{"start":[{"op":"if","var":"running","cmp":"==","val":"1","then":[{"op":"say","text":"Stopwatch is already running."}],"else":[{"op":"set","var":"start_ms","expr":"MILLIS"},{"op":"set","var":"running","expr":"1"},{"op":"say","text":"Stopwatch started."}]}],"stop":[{"op":"if","var":"running","cmp":"==","val":"0","then":[{"op":"say","text":"Stopwatch is not running."}],"else":[{"op":"set","var":"stop_ms","expr":"MILLIS"},{"op":"set","var":"elapsed","expr":"(stop_ms - start_ms) / 1000"},{"op":"set","var":"running","expr":"0"},{"op":"say","text":"Stopped. Elapsed time: {elapsed:.1f} seconds."}]}],"reset":[{"op":"set","var":"start_ms","expr":"0"},{"op":"set","var":"stop_ms","expr":"0"},{"op":"set","var":"elapsed","expr":"0"},{"op":"set","var":"running","expr":"0"},{"op":"say","text":"Stopwatch reset to zero."}],"check":[{"op":"if","var":"running","cmp":"==","val":"1","then":[{"op":"set","var":"elapsed","expr":"(MILLIS - start_ms) / 1000"},{"op":"say","text":"Still running: {elapsed:.1f} seconds so far."}],"else":[{"op":"say","text":"Last recorded time: {elapsed:.1f} seconds."}]}]}}

━━ Example 2 — word-of-the-day ━━
{"name":"word_of_day","description":"Fetch a word of the day","vars":{},"strvars":{"word":""},"triggers":{"get":["word of the day","give me a new word","daily word","word"]},"actions":{"get":[{"op":"ai","prompt":"Give me one interesting English word with its definition in one sentence.","strvar":"word"},{"op":"say","text":"Word of the day: {word}"}]}}
)PROMPT";

// ── Skill writer — powered by OpenRouter ─────────────────────
// v1.7.9: Injects existing skill context so the skill model resolves ambiguous commands.
//   e.g. "stop" with a stopwatch loaded → the skill model generates a stop action for
//   the stopwatch, not a generic "stop all processes" skill.
String skillModelGenerate(const String& request, const String& previousSkillJson, const String& feedback) {
  String userPrompt = "The user asked the assistant to do this:\n\"" + request + "\"\n\n";

  // ── Existing skill context ──────────────────────────────────────────────────
  if (!skillNames.empty()) {
    userPrompt += "IMPORTANT CONTEXT — Skills already loaded on this device:\n";
    for (size_t _si = 0; _si < skillNames.size() && _si < 8; _si++) {
      userPrompt += "  • '" + skillNames[_si] + "'";
      JsonDocument _existDoc(&g_jsonAllocator);
      if (!deserializeJson(_existDoc, skillJson[_si])) {
        const char* _desc = _existDoc["description"] | "";
        if (strlen(_desc) > 0) userPrompt += " — " + String(_desc);
        JsonObject _acts = _existDoc["actions"].as<JsonObject>();
        if (!_acts.isNull()) {
          userPrompt += " (actions: ";
          bool _first = true;
          for (JsonPair _kv : _acts) {
            if (!_first) userPrompt += ", ";
            userPrompt += _kv.key().c_str(); _first = false;
          }
          userPrompt += ")";
        }
      }
      userPrompt += "\n";
    }
    userPrompt +=
      "If the user's request is most naturally a COMMAND for one of the above skills "
      "(e.g. \"stop\" → stop action of 'stopwatch', \"reset\" → reset action of 'stopwatch'), "
      "generate a COMPLETE skill with the SAME NAME as that existing skill, "
      "including ALL expected actions (start, stop, reset, check, etc.) with correct MILLIS-based timing. "
      "Do NOT generate a generic \"stop all\" or \"stop playback\" skill when a stopwatch already exists.\n\n";
  }
  // ──────────────────────────────────────────────────────────────────────────

  userPrompt += "Write the skill JSON for it.";
  if (feedback.length() > 0)
    userPrompt += "\n\nPrevious attempt:\n" + previousSkillJson + "\n\nReviewer feedback:\n" + feedback + "\n\nProduce a corrected skill JSON.";

  // OpenRouter uses the same OpenAI-compatible messages/choices schema as the
  // main chat engine (v1.8.0: replaces the old Gemini generateContent schema).
  JsonDocument reqDoc(&g_jsonAllocator);
  reqDoc["model"]                  = Config::SKILL_MODEL;
  reqDoc["reasoning"]["enabled"] = false;   // v1.8.0: no chain-of-thought — save tokens
  reqDoc["messages"][0]["role"]    = "system";
  reqDoc["messages"][0]["content"] = SKILL_SYSTEM_PROMPT;
  reqDoc["messages"][1]["role"]    = "user";
  reqDoc["messages"][1]["content"] = userPrompt;
  reqDoc["temperature"]            = 0.2;
  reqDoc["max_tokens"]             = 900;

  String body; serializeJson(reqDoc, body);

  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  String respRaw = postJson(Config::SKILL_ENDPOINT, body, String("Bearer ") + g_skillKey);
  setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);

  if (respRaw.length() == 0) {
    Serial.println("⚠️  Skill generation: no response from the skill model.");
    return "";
  }

  JsonDocument resp(&g_jsonAllocator);
  DeserializationError parseErr = deserializeJson(resp, respRaw);
  if (parseErr) {
    Serial.println("⚠️  Skill generation: unparseable response (" +
                   String(parseErr.c_str()) + ").");
    return "";
  }

  const char* apiErr = resp["error"]["message"]|"";
  if (strlen(apiErr) > 0) {
    Serial.println("⚠️  Skill-model error: " + String(apiErr));
    return "";
  }

  const char* content = resp["choices"][0]["message"]["content"]|"";
  String raw(content);
  raw.trim();
  if (raw.length() == 0) {
    Serial.println("⚠️  Skill model returned an empty response.");
    return "";
  }
  return extractJsonObject(raw);
}

// ── aiVerifySkill ─────────────────────────────────────
// v1.7.9: Context-aware review — checks trigger coverage (short commands
//         required), MILLIS-based timing, variable init, purpose correctness,
//         and that every action produces audible feedback.
String aiVerifySkill(const String& candidateJson, const String& request) {
  if (!heapOk() || WiFi.status() != WL_CONNECTED) return ""; // skip if unavailable

  String prompt =
    "You are a strict code reviewer for an ESP32 microcontroller skill DSL.\n"
    "The user's original request: \"" + request + "\"\n\n"
    "Skill JSON to review:\n" + candidateJson + "\n\n"
    "Check ONLY these potential issues:\n"
    "1. PURPOSE: Does the skill correctly implement what the user actually asked for? "
       "A stopwatch must track real elapsed time with start/stop/reset/check. "
       "A 'stop' request for a stopwatch must stop the stopwatch — NOT stop audio or processes.\n"
    "2. TRIGGER COVERAGE: Every action that can be invoked by a short word (stop/start/reset/check) "
       "MUST include that short bare word as one of its trigger phrases. "
       "Triggers like [\"stop the stopwatch\"] are WRONG — they never fire when user says just \"stop\". "
       "Correct example: [\"stop\", \"stop the stopwatch\", \"pause\"].\n"
    "3. VARIABLE INIT: All numeric vars used in 'set'/'if' ops must appear in 'vars'. "
       "All string vars used in 'set_str'/'say' must appear in 'strvars'.\n"
    "4. TIMING CORRECTNESS: Skills that track elapsed time must store MILLIS as a timestamp. "
       "Elapsed seconds = (MILLIS - start_ms) / 1000. "
       "Never use NOW_SEC alone for a stopwatch — it wraps at 59 giving wrong totals.\n"
    "5. INTERPOLATION: 'say' ops must use {varname} or {varname:.Nf} — no bare variable names outside braces.\n"
    "6. FEEDBACK: Every action must end with at least one 'say' op confirming what happened.\n\n"
    "If EVERYTHING is correct: reply with exactly the two letters: OK\n"
    "If there are problems: give ONE short paragraph describing exactly what to fix. "
    "No corrected JSON, no bullet lists, no preamble — just the plain fix description.";

  String verdict = aiSimpleCall(prompt, 0.0f, 280);
  verdict.trim();
  if (verdict.length() == 0) return "";             // treat API failure as OK
  if (verdict == "OK" || verdict.startsWith("OK"))  return "";
  return verdict;  // non-empty = found problems
}

// ── learnNewSkill ────────────────────────────────────────
// v1.7.9: Gemini generates → Groq verifies → Gemini corrects (up to 2 rounds)
void learnNewSkill(const String& request) {
  Serial.println("\n🧠 Learning new skill for: \"" + request + "\"...");
  aiState = AI_LEARNING; stateChangeTime = millis();

  String candidate = skillModelGenerate(request, "", "");
  if (candidate.length() == 0) {
    Serial.println("❌ Couldn't generate skill (model error or bad response).");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  // ── Gemini→Groq→Gemini verification loop (up to 2 correction rounds) ──
  for (int round = 0; round < 2; round++) {
    Serial.println("🔍 AI verifying skill (round " + String(round + 1) + ")...");
    String feedback = aiVerifySkill(candidate, request);
    if (feedback.length() == 0) {
      Serial.println("✅ AI verification passed.");
      break; // skill is good — proceed
    }
    // AI found issues — log them and send back to Gemini for correction
    Serial.println("⚠️  AI found issues: " +
                   feedback.substring(0, min((int)feedback.length(), 100)) +
                   (feedback.length() > 100 ? "..." : ""));
    Serial.println("🔄 Sending feedback to the skill model for correction...");
    String revised = skillModelGenerate(request, candidate, feedback);
    if (revised.length() == 0) {
      Serial.println("⚠️  Skill-model correction failed — using previous candidate.");
      break; // use whatever we had
    }
    candidate = revised;
    // After the last round we use whatever the skill model produced (no third Groq check)
  }

  beginSkillTest(request, candidate);
}

void beginSkillTest(const String& request, const String& candidateJson) {
  JsonDocument doc(&g_jsonAllocator);
  if (deserializeJson(doc, candidateJson)) {
    Serial.println("❌ Generated skill JSON is invalid — can't test it.");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  // Validate
  if (!doc.containsKey("name") || !doc.containsKey("actions")) {
    Serial.println("❌ Skill missing required fields (name, actions).");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  String name = doc["name"].as<String>();
  if (name.length() == 0 || name.length() > 32) {
    Serial.println("❌ Invalid skill name.");
    aiState = AI_ERROR; blinkCount = 0; lastBlink = millis(); return;
  }

  // Check for existing skill with same name (backup for retry)
  pendingBackupName = ""; pendingBackupJson = "";
  for (size_t i = 0; i < skillNames.size(); i++) {
    if (skillNames[i] == name) {
      pendingBackupName = skillNames[i]; pendingBackupJson = skillJson[i];
      skillNames.erase(skillNames.begin()+i); skillJson.erase(skillJson.begin()+i); break;
    }
  }

  skillNames.push_back(name); skillJson.push_back(candidateJson);
  pendingSkillIndex = (int)skillNames.size() - 1;
  pendingSkillRequest = request;
  testingSkill = true;
  rebuildSkillTriggerIndex();

  aiState = AI_EXCITED; stateChangeTime = millis();
  Serial.println("✨ Learned: " + name + " — " + String(doc["description"]|""));
  Serial.println("Try it, then: keep it? (yes/no)");

  // Auto-try it
  String lower = request; lower.toLowerCase(); String bestAction;
  for (JsonPair kv : doc["triggers"].as<JsonObject>()) {
    for (JsonVariant p : kv.value().as<JsonArray>()) {
      String phrase = p.as<String>(); phrase.toLowerCase();
      if (lower.indexOf(phrase) >= 0) { bestAction = kv.key().c_str(); break; }
    }
    if (bestAction.length() > 0) break;
  }
  if (bestAction.length() == 0) {
    JsonObject actions = doc["actions"].as<JsonObject>();
    for (JsonPair kv : actions) { bestAction = kv.key().c_str(); break; }
  }
  if (bestAction.length() > 0) runSkillAction(pendingSkillIndex, bestAction);
}

void commitPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String name = skillNames[pendingSkillIndex];
  if ((int)skillNames.size() > Config::MAX_SKILLS) {
    int removeIdx = (pendingSkillIndex == 0) ? 1 : 0;
    skillNames.erase(skillNames.begin()+removeIdx); skillJson.erase(skillJson.begin()+removeIdx);
    if (removeIdx < pendingSkillIndex) pendingSkillIndex--;
  }
  rebuildSkillTriggerIndex(); saveSkills();
  updateKnowledgeDomain("self-taught skills", 25);
  testingSkill = false; pendingSkillIndex = -1; pendingBackupName = ""; pendingBackupJson = "";
  aiState = AI_EXCITED; stateChangeTime = millis();
  Serial.println("✅ Kept — " + name + " is saved.");
}

void discardPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String name = skillNames[pendingSkillIndex];
  skillNames.erase(skillNames.begin()+pendingSkillIndex);
  skillJson.erase(skillJson.begin()+pendingSkillIndex);
  if (pendingBackupJson.length() > 0) {
    skillNames.push_back(pendingBackupName); skillJson.push_back(pendingBackupJson);
  }
  rebuildSkillTriggerIndex();
  testingSkill = false; pendingSkillIndex = -1; pendingBackupName = ""; pendingBackupJson = "";
  aiState = AI_IDLE; stateChangeTime = millis();
  Serial.println("🗑️  Discarded — " + name + " not saved.");
}

void retryPendingSkill() {
  if (!testingSkill) { Serial.println("No pending skill."); return; }
  String request = pendingSkillRequest; discardPendingSkill(); learnNewSkill(request);
}

