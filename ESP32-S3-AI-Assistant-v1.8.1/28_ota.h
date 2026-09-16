// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 28_ota.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 23 ── OTA AUTO-UPDATE  (github.com/ItzCoding/ESP32-S3-Ai-Assistant)
// ═══════════════════════════════════════════════════════
// RELEASE WORKFLOW — fully automated via GitHub Actions:
//   1. Bump FIRMWARE_VERSION in 01_config.h (e.g. "1.8.1")
//   2. Commit, then:  git tag v1.8.1 && git push origin v1.8.1
//   3. .github/workflows/release.yml builds the .bin and attaches it to a
//      GitHub Release tagged v1.8.1
//   4. Devices auto-check on boot (or via /update), then /install updates
//      over WiFi. Flash this source once via USB — OTA handles the rest.
// NOTE: OTA_GITHUB_USER / OTA_GITHUB_REPO below must point at YOUR repo.
// ────────────────────────────────────────────────────────

bool isNewerVersion(const String& remote, const String& current) {
  int rMaj=0,rMin=0,rPatch=0,cMaj=0,cMin=0,cPatch=0;
  sscanf(remote.c_str(),  "%d.%d.%d", &rMaj, &rMin, &rPatch);
  sscanf(current.c_str(), "%d.%d.%d", &cMaj, &cMin, &cPatch);
  if (rMaj != cMaj) return rMaj > cMaj;
  if (rMin != cMin) return rMin > cMin;
  return rPatch > cPatch;
}

static String otaFetchLatestRelease() {
  const unsigned long started=millis();
  WiFiClientSecure sec; sec.setInsecure();
  HTTPClient http;
  http.setTimeout(Config::OTA_CHECK_TIMEOUT_MS);
  http.begin(sec, Config::OTA_API_URL);
  http.addHeader("User-Agent", "ESP32-AI-OTA");
  http.addHeader("Accept",     "application/vnd.github+json");
  esp_task_wdt_reset();
  int code = http.GET();
  esp_task_wdt_reset();
  if (code != 200) { http.end(); recordApiUsage("ota",false,millis()-started,0,0); return ""; }
  WiFiClient* stream = http.getStreamPtr();
  String json = ""; json.reserve(4096);
  unsigned long deadline = millis() + Config::OTA_CHECK_TIMEOUT_MS;
  while (http.connected() && millis() < deadline) {
    if (stream->available()) json += (char)stream->read(); else delay(1);
    if (json.length() > 32000) break;
  }
  http.end(); recordApiUsage("ota",!json.isEmpty(),millis()-started,0,json.length()); return json;
}

static bool otaParseRelease(const String& json, String& outVer, String& outUrl, String& outNotes, size_t& outSize) {
  // ── version tag ──────────────────────────────────────
  JsonDocument doc(&g_jsonAllocator);
  if (deserializeJson(doc,json) || !doc["tag_name"].is<const char*>() || !doc["assets"].is<JsonArray>()) return false;
  outVer=doc["tag_name"].as<String>();
  if(outVer.startsWith("v")||outVer.startsWith("V"))outVer.remove(0,1);

  // ── release notes (optional) ─────────────────────────
  outNotes=doc["body"]|""; outNotes.trim();
  if(outNotes.length()>1600)outNotes.remove(1600);

  // ── find the right .bin asset ─────────────────────────
  // v1.8.0 FIX: previously it grabbed the FIRST .bin in the release, which
  // would wrongly flash partitions.bin or bootloader.bin if the release
  // attached multiple ESP32 artifacts. Now it skips those and prefers an
  // asset named like "firmware" / the project name.
  String fallback;
  size_t fallbackSize = 0;
  for(JsonObject asset:doc["assets"].as<JsonArray>()) {
    String url=asset["browser_download_url"]|"";
    String name=asset["name"]|"";
    size_t assetSize=asset["size"]|0U;
    String allowedPrefix = "https://github.com/" + String(Config::OTA_GITHUB_USER) + "/" +
                           String(Config::OTA_GITHUB_REPO) + "/releases/download/";
    if (!url.startsWith(allowedPrefix)) continue;
    String lname = name; lname.toLowerCase();
    if(!lname.endsWith(".bin"))continue;
    // Skip ESP32 helper artifacts — only a full app image may be flashed.
    if (lname.indexOf("bootloader") >= 0 || lname.indexOf("partition") >= 0 ||
        lname.indexOf("boot_app")  >= 0) continue;
    // Preferred: an asset explicitly named firmware / matching the project.
    if (lname.indexOf("firmware") >= 0 || lname.indexOf("ai-assistant") >= 0 ||
        lname.indexOf("assistant") >= 0) {
      outUrl = url; outSize = assetSize; return true;
    }
    if (fallback.isEmpty()) { fallback = url; fallbackSize = assetSize; }
  }
  if (!fallback.isEmpty()) { outUrl = fallback; outSize = fallbackSize; return true; }
  return false;
}

void printFirmwareChangelog() {
  if(g_updateNotes.isEmpty()) {
    Serial.println("No release notes are available. Ask me to check for updates first.");
    return;
  }
  Serial.println("\n──────── Firmware changelog: v"+g_latestVersion+" ────────");
  Serial.println(g_updateNotes);
  Serial.println("────────────────────────────────────────────");
}

void checkForUpdate(bool silent) {
  if (WiFi.status() != WL_CONNECTED) {
    if (!silent) Serial.println("⚠️  No WiFi — cannot check for updates."); return;
  }
  if (!silent) { Serial.println("\n🔍 Checking GitHub for firmware updates..."); aiState=AI_THINKING; stateChangeTime=millis(); }
  String json = otaFetchLatestRelease();
  if (json.length()==0) {
    if (!silent) Serial.println("❌ Could not reach api.github.com. Is the repo public?");
    aiState=AI_IDLE; return;
  }
  String remoteVer, binUrl, notes;
  size_t binSize=0;
  if (!otaParseRelease(json, remoteVer, binUrl, notes, binSize)) {
    if (!silent) {
      Serial.println("❌ No .bin asset found in the latest release.");
      Serial.println("   Attach a .bin file to the GitHub Release and try again.");
    }
    aiState=AI_IDLE; return;
  }
  g_otaCheckedOnBoot = true;
  // Keep release metadata even when this device is already current.
  g_latestVersion=remoteVer; g_updateNotes=notes;
  if (isNewerVersion(remoteVer, Config::FIRMWARE_VERSION)) {
    const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);
    if (!next || binSize < 100000 || binSize > next->size) {
      Serial.printf("Rejected update asset: size %u does not fit OTA partition.\n",(unsigned)binSize);
      g_updateAvailable=false; ++g_apiStats.otaFailures; g_dirtyApiStats=true; aiState=AI_ERROR; return;
    }
    g_updateAvailable=true; g_latestBinUrl=binUrl; g_latestBinSize=binSize;
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║       🚀 FIRMWARE UPDATE AVAILABLE            ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf( "║  Current  :  v%-29s║\n", Config::FIRMWARE_VERSION);
    Serial.printf( "║  Latest   :  v%-29s║\n", g_latestVersion.c_str());
    if (g_updateNotes.length()>0) Serial.println("║  Say 'show the firmware changelog' for notes ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf( "║  Size     :  %-30s║\n", (String(g_latestBinSize/1024)+" KB").c_str());
    Serial.println("║  Say “install the update” to continue          ║");
    Serial.println("╚══════════════════════════════════════════════╝\n");
    for (int i=0;i<2;i++){setLEDColor(0,255,255,Config::LED_MAX_BRIGHTNESS);delay(250);setLEDColor(0,0,0,0);delay(150);}
  } else {
    g_updateAvailable=false;
    if (!silent) Serial.printf("✅ Already on latest firmware (v%s).\n", Config::FIRMWARE_VERSION);
    aiState=AI_REPLIED; stateChangeTime=millis();
  }
}

void installUpdate() {
  if (!g_updateAvailable) { Serial.println("ℹ️  No update found. Type /update to check."); return; }
  if (WiFi.status()!=WL_CONNECTED) { Serial.println("❌ No WiFi — cannot download update."); return; }
  if (!heapOk()) { Serial.println("⚠️  Low SRAM. Try /clear, restart, then /install."); return; }
  const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);
  if (!next || g_latestBinSize < 100000 || g_latestBinSize > next->size ||
      !g_latestBinUrl.startsWith("https://github.com/" + String(Config::OTA_GITHUB_USER) + "/" +
                                 String(Config::OTA_GITHUB_REPO) + "/releases/download/")) {
    Serial.println("Update blocked: release asset failed origin or partition-size validation.");
    ++g_apiStats.otaFailures; g_dirtyApiStats=true; return;
  }
  flushDirtyState();
  Serial.println("\n⬇️  Downloading v"+g_latestVersion+" from GitHub...");
  Serial.println("   Do NOT power off. LED blinks teal during download.\n"); Serial.flush();
  if (g_dcTask) vTaskSuspend(g_dcTask);
  setCpuFrequencyMhz(Config::CPU_FREQ_ACTIVE);
  WiFiClientSecure sec; sec.setInsecure();
  httpUpdate.onProgress([](int current, int total){
    esp_task_wdt_reset();
    static unsigned long lastBlink=0; static bool ledOn=false;
    if (millis()-lastBlink>350){
      ledOn=!ledOn;
      if(ledOn) setLEDColor(0,200,255,Config::LED_MAX_BRIGHTNESS);
      else      setLEDColor(0,50,80,Config::LED_MAX_BRIGHTNESS);
      lastBlink=millis();
    }
    if (total>0){
      static int lastPct=-1; int pct=(current*100)/total;
      if(pct/10!=lastPct/10){Serial.printf("   %3d%%  (%d / %d bytes)\n",pct,current,total);Serial.flush();lastPct=pct;}
    }
  });
  esp_task_wdt_delete(NULL);
  // GitHub release assets redirect to a CDN. Follow that redirect so the
  // firmware binary can be downloaded successfully.
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  // v1.8.0 FIX: rebootOnUpdate() defaults to true, which makes HTTPUpdate call
  // ESP.restart() INSIDE update() — the lines below (OTA marker write, green
  // LED, message) were dead code and the "run setup once after OTA" handoff
  // never happened. We handle the restart ourselves so the marker is saved.
  httpUpdate.rebootOnUpdate(false);
  const unsigned long otaStarted=millis();
  t_httpUpdate_return ret = httpUpdate.update(sec, g_latestBinUrl);
  switch(ret){
    case HTTP_UPDATE_OK:
      recordApiUsage("ota",true,millis()-otaStarted,0,g_latestBinSize);
      if (g_preferencesReady) {
        // This one-shot marker is written only after HTTPUpdate confirms that
        // the new firmware was flashed successfully. USB uploads never write it.
        g_preferences.putBool(OTA_SETUP_ONCE_KEY, true);
        // Keep the old marker so an older release binary can still receive the
        // success handoff. Current firmware ignores and removes this marker.
        g_preferences.putBool("setup_pending", true);
      }
      Serial.println("\n✅ Flash complete — restarting..."); Serial.flush(); delay(500);
      setLEDColor(0,255,0,Config::LED_MAX_BRIGHTNESS); delay(500); esp_restart(); break;
    case HTTP_UPDATE_FAILED:
      recordApiUsage("ota",false,millis()-otaStarted,0,0);
      Serial.printf("\n❌ Flash failed: %s\n", httpUpdate.getLastErrorString().c_str());
      if(g_dcTask) vTaskResume(g_dcTask); setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);
      esp_task_wdt_add(NULL); setLEDColor(255,0,0,Config::LED_MAX_BRIGHTNESS);
      delay(1000); setLEDColor(0,0,0,0); aiState=AI_ERROR; stateChangeTime=millis(); break;
    case HTTP_UPDATE_NO_UPDATES:
      recordApiUsage("ota",false,millis()-otaStarted,0,0);
      Serial.println("\nℹ️  Server says no update needed.");
      if(g_dcTask) vTaskResume(g_dcTask); setCpuFrequencyMhz(Config::CPU_FREQ_IDLE);
      esp_task_wdt_add(NULL); aiState=AI_IDLE; break;
  }
}
