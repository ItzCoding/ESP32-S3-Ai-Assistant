// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 26_led.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 21 ── LED
// ═══════════════════════════════════════════════════════

void setLEDColor(uint8_t r, uint8_t g, uint8_t b, float brightness) {
  brightness = constrain(brightness, 0.0f, 1.0f);
  strip.setPixelColor(0, strip.Color(
    (uint8_t)(r * brightness), (uint8_t)(g * brightness), (uint8_t)(b * brightness)));
  strip.show();
}

void updateLED() {
  unsigned long now = millis();
  auto pulse = [&](uint8_t r, uint8_t g, uint8_t b, float speed, float maxB) {
    if (now - lastBlink > (unsigned long)(1000.0f / (speed * 60))) {
      pulseBrightness += pulseIncreasing ? 0.02f : -0.02f;
      if (pulseBrightness >= maxB) pulseIncreasing = false;
      if (pulseBrightness <= Config::LED_MIN_BRIGHTNESS) pulseIncreasing = true;
      setLEDColor(r, g, b, pulseBrightness); lastBlink = now;
    }
  };
  switch (aiState) {
    case AI_IDLE:    setLEDColor(0, 0, 0, 0); break;
    case AI_THINKING:pulse(0, 0, 255, 1.0f + thinkingComplexity*0.1f, Config::LED_MAX_BRIGHTNESS); break;
    case AI_REPLIED: setLEDColor(0, 255, 0, Config::LED_MAX_BRIGHTNESS);
                     if (now - stateChangeTime > Config::REPLIED_FLASH_MS) aiState = AI_IDLE; break;
    case AI_ERROR:
      if (now - lastBlink > 300) {
        setLEDColor(255, 0, 0, (blinkCount%2) ? 0 : Config::LED_MAX_BRIGHTNESS);
        if (++blinkCount >= 6) { blinkCount = 0; aiState = AI_IDLE; } lastBlink = now;
      } break;
    case AI_ALERT:
      pulse(255, 50, 0, 1.2f, Config::LED_MAX_BRIGHTNESS);
      if (now - stateChangeTime > Config::REMINDER_ALERT_MS) aiState = AI_IDLE; break;
    case AI_EXCITED:
      if (now - lastBlink > 80) {
        setLEDColor(255, 215, 0,
          Config::LED_MIN_BRIGHTNESS + random(100)/100.0f*(Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS));
        lastBlink = now;
      }
      if (now - stateChangeTime > 3000) aiState = AI_IDLE; break;
    case AI_CONCERNED:
      setLEDColor(100, 150, 255, Config::LED_MAX_BRIGHTNESS * 0.7f);
      if (now - stateChangeTime > 5000) aiState = AI_IDLE; break;
    case AI_PROACTIVE:
      pulse(128, 0, 255, 0.8f, Config::LED_MAX_BRIGHTNESS * 0.8f);
      if (now - stateChangeTime > 5000) aiState = AI_IDLE; break;
    case AI_LEARNING:
      pulse(0, 255, 255, 1.5f, Config::LED_MAX_BRIGHTNESS * 0.9f);
      if (now - stateChangeTime > 2000) aiState = AI_IDLE; break;
    case AI_EVOLVING: {
      KnowledgeArea* dom = getDominantKnowledge();
      if (dom && now - lastBlink > 100) {
        float b = Config::LED_MIN_BRIGHTNESS + (Config::LED_MAX_BRIGHTNESS-Config::LED_MIN_BRIGHTNESS)*0.6f;
        setLEDColor(dom->colorR, dom->colorG, dom->colorB, b); lastBlink = now;
      }
      if (now - stateChangeTime > 1500) aiState = AI_IDLE; break;
    }
  }
}

void rainbowWave(int durationMs) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)durationMs) {
    for (int i = 0; i < 256 && millis()-start < (unsigned long)durationMs; i += 4) {
      uint8_t r = (i<85)?255-i*3:(i<170)?0:(i-170)*3;
      uint8_t g = (i<85)?i*3:(i<170)?255-(i-85)*3:0;
      uint8_t b = (i<85)?0:(i<170)?(i-85)*3:255-(i-170)*3;
      setLEDColor(r, g, b, 0.25f); delay(8);
    }
  }
  setLEDColor(0, 0, 0, 0);
}

