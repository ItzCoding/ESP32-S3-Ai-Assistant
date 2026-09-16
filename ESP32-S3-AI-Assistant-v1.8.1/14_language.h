// ==================================================================
//  ESP32-S3 AI Assistant v1.8.0 - module: 14_language.h
//  Modularised firmware - single translation unit.
//  Included by the main .ino in FIXED ORDER (do not reorder includes).
// ==================================================================

// ═══════════════════════════════════════════════════════
// SECTION 10.5 ── v1.7.9 MULTI-LANGUAGE AUTO-DETECT
// ═══════════════════════════════════════════════════════

// detectLanguage() — identifies the language of a message.
// Strategy:
//   1. Unicode range scan for non-Latin scripts (instant, no API call)
//   2. Common Latin-script word patterns (Spanish/French/German/etc.)
//   3. AI micro-call for ambiguous cases (cached — won't repeat if unchanged)
String detectLanguage(const String& msg) {
  if (msg.length() == 0) return g_userLanguage;

  // ── 1. Unicode range scan ────────────────────────────
  bool hasNonAscii = false;
  bool hasArabic    = false, hasCJK      = false;
  bool hasCyrillic  = false, hasDevanagari = false;
  bool hasHebrew    = false, hasThai      = false;

  for (size_t i = 0; i + 1 < msg.length(); ) {
    unsigned char c0 = (unsigned char)msg[i];
    if (c0 < 0x80) { i++; continue; }
    hasNonAscii = true;

    // Decode first two bytes of UTF-8 sequence
    unsigned char c1 = (unsigned char)msg[i + 1];
    uint16_t cp = 0;
    if ((c0 & 0xE0) == 0xC0 && (c1 & 0xC0) == 0x80) {
      cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
      i += 2;
    } else if ((c0 & 0xF0) == 0xE0 && i + 2 < msg.length()) {
      unsigned char c2 = (unsigned char)msg[i + 2];
      cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
      i += 3;
    } else { i++; continue; }

    if      (cp >= 0x0600 && cp <= 0x06FF) hasArabic     = true;
    else if (cp >= 0x0400 && cp <= 0x04FF) hasCyrillic   = true;
    else if (cp >= 0x0900 && cp <= 0x097F) hasDevanagari = true;
    else if (cp >= 0x0590 && cp <= 0x05FF) hasHebrew     = true;
    else if (cp >= 0x0E00 && cp <= 0x0E7F) hasThai       = true;
    else if ((cp >= 0x3000 && cp <= 0x9FFF) ||
             (cp >= 0xAC00 && cp <= 0xD7FF)) hasCJK       = true;
  }

  if (hasArabic)     return "Arabic";
  if (hasCyrillic)   return "Russian";
  if (hasDevanagari) return "Hindi";
  if (hasHebrew)     return "Hebrew";
  if (hasThai)       return "Thai";
  if (hasCJK) {
    // Rough Japanese vs Chinese vs Korean discrimination
    bool hasHiragana = false, hasHangul = false;
    for (size_t i = 0; i + 2 < msg.length(); ) {
      unsigned char c0=(unsigned char)msg[i],c1=(unsigned char)msg[i+1],c2=(unsigned char)msg[i+2];
      if ((c0&0xF0)==0xE0&&(c1&0xC0)==0x80&&(c2&0xC0)==0x80) {
        uint16_t cp2=((c0&0x0F)<<12)|((c1&0x3F)<<6)|(c2&0x3F);
        if (cp2>=0x3040&&cp2<=0x30FF) hasHiragana=true;
        if (cp2>=0xAC00&&cp2<=0xD7FF) hasHangul=true;
        i+=3;
      } else i++;
    }
    if (hasHangul)   return "Korean";
    if (hasHiragana) return "Japanese";
    return "Chinese";
  }

  // ── 2. Latin-script pattern matching ─────────────────
  String lower = msg; lower.toLowerCase();

  // Spanish
  if (lower.indexOf("hola") >= 0 || lower.indexOf("gracias") >= 0 ||
      lower.indexOf("cómo") >= 0 || lower.indexOf("como estas") >= 0 ||
      lower.indexOf("por favor") >= 0 || lower.indexOf("buenos") >= 0 ||
      lower.indexOf(" que ") >= 0 || lower.indexOf(" estoy ") >= 0)
    return "Spanish";

  // French
  if (lower.indexOf("bonjour") >= 0 || lower.indexOf("merci") >= 0 ||
      lower.indexOf("s'il vous") >= 0 || lower.indexOf("je suis") >= 0 ||
      lower.indexOf("qu'est") >= 0 || lower.indexOf("comment") >= 0 ||
      lower.indexOf(" est ") >= 0 || lower.indexOf("bonsoir") >= 0)
    return "French";

  // German
  if (lower.indexOf("hallo") >= 0 || lower.indexOf("danke") >= 0 ||
      lower.indexOf("bitte") >= 0 || lower.indexOf("wie geht") >= 0 ||
      lower.indexOf("ich bin") >= 0 || lower.indexOf("guten") >= 0 ||
      lower.indexOf("schön") >= 0 || lower.indexOf("nein") >= 0)
    return "German";

  // Italian
  if (lower.indexOf("ciao") >= 0 || lower.indexOf("grazie") >= 0 ||
      lower.indexOf("per favore") >= 0 || lower.indexOf("buongiorno") >= 0 ||
      lower.indexOf("mi chiamo") >= 0 || lower.indexOf("come stai") >= 0)
    return "Italian";

  // Portuguese
  if (lower.indexOf("olá") >= 0 || lower.indexOf("obrigado") >= 0 ||
      lower.indexOf("por favor") >= 0 || lower.indexOf("bom dia") >= 0 ||
      lower.indexOf("tudo bem") >= 0 || lower.indexOf("obrigada") >= 0)
    return "Portuguese";

  // Sinhala transliteration hints (common in Sri Lanka)
  // v1.7.9 FIX: removed "mama", "oya", "api" — these are extremely common in
  // English and tech contexts and caused false Sinhala detection for normal messages.
  // Only keep unambiguous Sinhala-specific phrases.
  if (lower.indexOf("kohomada") >= 0 || lower.indexOf("oyage") >= 0 ||
      lower.indexOf("mokakda") >= 0   || lower.indexOf("machan") >= 0 ||
      lower.indexOf("aniwa") >= 0     || lower.indexOf("koheda") >= 0)
    return "Sinhala";

  // ── 3. All-ASCII or unrecognised → keep current / use the AI ──
  // Only call Groq if the message has non-ASCII we couldn't classify,
  // or if the message is long enough to be meaningful.
  if (hasNonAscii && heapOk() && WiFi.status() == WL_CONNECTED) {
    String prompt =
      "Identify the language of this text. Reply with ONE English word — just the language name "
      "(e.g. Arabic, Chinese, Russian, Sinhala, Tamil). No extra words.\nText: \""
      + msg.substring(0, 80) + "\"";
    String lang = aiSimpleCall(prompt, 0.0f, 10);
    lang.trim();
    lang.replace("\"", ""); lang.replace(".", "");
    if (lang.length() > 0 && lang.length() < 30) return lang;
  }

  // Default: English (or keep previous if short ambiguous fragment)
  return (msg.length() < 8) ? g_userLanguage : "English";
}

