# ESP32-S3 AI Assistant v1.7.7

A self-learning, voice-and-text AI assistant running entirely on an ESP32-S3 microcontroller — powered by Groq's Llama 3.3 70B, Gemini, real-time web search, mood-adaptive responses, and a sandboxed skills engine.

---

## Table of Contents

- [Screenshots](#screenshots)
- [Overview](#overview)
- [What's New in v1.7.7](#whats-new-in-v177)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Software Requirements](#software-requirements)
- [Arduino IDE Board Settings](#arduino-ide-board-settings)
- [API Keys](#api-keys)
- [Configuration](#configuration)
- [Installation](#installation)
- [Usage](#usage)
- [Skills System](#skills-system)
- [Architecture](#architecture)
- [Serial Monitor Reference](#serial-monitor-reference)
- [Troubleshooting](#troubleshooting)
- [Limits & Constraints](#limits--constraints)
- [Security Notice](#security-notice)
- [License](#license)
- [Credits](#credits)

---

## Screenshots

**Boot & Hello**

![Boot & Hello](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.7/Extra/Hello.png)

**Skill Learning**

![Learning skills](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.7/Extra/Learning%20skills.png)

**System Diagnosis**

![System Diagnosis](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.7/Extra/Diagnosis.png)

**Web Search**

![Web Search](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.7/Extra/Web%20Search.png)

---

## Overview

ESP32-S3 AI Assistant is a full-featured embedded AI chatbot that runs on a single ESP32-S3 module. It connects to Groq's ultra-fast inference API to deliver responses via **llama-3.3-70b-versatile**, uses **Gemini 2.5 Flash** to write and save new skills on the fly, calls native **Groq function/tool calling** to dispatch structured actions, searches the web via **Serper.dev**, fetches live weather from **Meteosource**, adapts its tone to your mood in real time, and persists everything — memory facts, reminders, learned skills, sentiment logs — to on-board FFat flash storage.

The sketch works on **any ESP32-S3 board**, with or without OPI PSRAM. If PSRAM is present it is used automatically for larger HTTP buffers; if not, the sketch falls back to internal SRAM seamlessly with no code changes required.

All interaction happens over **USB Serial** (115200 baud). Type a message, press Enter, read the reply.

---

## What's New in v1.7.7

| Change | Details |
|---|---|
| **Model upgrade** | Now runs `llama-3.3-70b-versatile` — the most capable text model on Groq's API |
| **Groq function calling** | `groqFunctionCall()` uses native OpenAI-compatible tool calling to dispatch structured actions |
| **Mood-adaptive temperature** | `computeMoodTemperature()` maps recent sentiment history to a Groq `temperature` between 0.42 and 0.88 — careful when you're stressed, playful when you're upbeat |
| **CPU frequency scaling** | Idles at 80 MHz while waiting for input, jumps to 240 MHz during HTTP calls, then drops back down |
| **AI-powered chat summarization** | Chat history is compressed with a real Groq call instead of crude string concatenation |
| **AI-generated evening summary** | A 20:00 daily recap generated from your interactions, mood, memory facts, and tomorrow's reminders |
| **Massively expanded NL parser** | Far larger filler-word and intent-cue vocabulary for natural-language reminders, memory, and search |
| **Bigger buffers & limits** | PSRAM request/response buffers tripled (24 KB / 64 KB); chat, memory, and sentiment limits raised for 8 MB PSRAM boards |
| **Longer watchdog window** | WDT timeout raised to 40 s and HTTP timeout to 35 s to give the 70B model room to respond |

---

## Features

| Category | Details |
|---|---|
| **AI Backbone** | Groq · llama-3.3-70b-versatile · SSE streaming response |
| **Function Calling** | Native Groq tool/function calling for structured action dispatch |
| **Mood Engine** | Sentiment-driven temperature scaling — replies get warmer or more careful depending on your recent mood |
| **Skill Generation** | Gemini 2.5 Flash writes new skills in a sandboxed DSL |
| **Web Search** | Serper.dev live Google search results injected as context, with recency-aware query building |
| **Weather** | Meteosource real-time weather by city name |
| **Reminders** | Natural-language reminder parsing, persistent, fires on time, supports once/daily/weekly/monthly recurrence |
| **Memory** | Up to 80 persistent user facts stored in FFat |
| **Sentiment** | Per-conversation mood tracking, log of last 40 interactions |
| **User Patterns** | Learns topic preferences, chat cadence, and reminder usage over time |
| **AI Chat Summarization** | Groq-powered compression of chat history instead of naive truncation |
| **Evening Summary** | AI-generated daily recap at 20:00 covering mood, activity, and tomorrow's reminders |
| **Proactive Check-ins** | Time-of-day aware nudges (morning, lunch, afternoon, evening) when you've been quiet |
| **NTP Clock** | Synced real-time clock, configurable UTC offset |
| **Diagnostics** | CPU temp · CPU frequency · free heap · uptime · PSRAM status · AI health scan |
| **Dual-core AI** | HTTP call runs on Core 0; LED + reminders + WiFi reconnect stay alive on Core 1 |
| **PSRAM Buffers** | 24 KB req + 64 KB resp buffers used if PSRAM is present; falls back to internal SRAM automatically |
| **CPU Scaling** | 80 MHz idle / 240 MHz active — throttles down automatically between requests |
| **Batched Flash** | Dirty-flag system — writes sentiment/pattern/knowledge in 30 s batches |
| **Non-blocking WiFi Reconnect** | Recovers from dropped WiFi without blocking the main loop |
| **WDT Safety** | 40-second watchdog with explicit resets throughout the loop |
| **NeoPixel Status** | Single RGB LED shows idle / thinking / error / excited / concerned / proactive / learning states |

---

## Hardware Requirements

| Component | Specification |
|---|---|
| **Microcontroller** | ESP32-S3 Dev Module |
| **Flash** | 16 MB |
| **PSRAM** | Optional — 8 MB OPI PSRAM gives larger HTTP buffers; works fine without it |
| **LED** | WS2812B NeoPixel × 1 (GPIO 48 — onboard on most S3 boards) |
| **USB** | USB-to-Serial adapter or native USB CDC |

> **Note:** Many ESP32-S3 DevKit boards (e.g. Unexpected Maker FeatherS3, Adafruit QT Py S3, generic S3 DevKitC-1) have the NeoPixel and OPI PSRAM already onboard. If your board has no PSRAM, everything still works — the sketch detects this at boot and uses internal SRAM instead.

---

## Wiring

### NeoPixel LED

If your board has an onboard WS2812B, no wiring is needed — just confirm the data pin matches `Config::NEOPIXEL_PIN` (default **GPIO 48**).

For an external NeoPixel:

```
ESP32-S3 GPIO 48  ──►  NeoPixel DIN
ESP32-S3 3.3V/5V  ──►  NeoPixel VCC
ESP32-S3 GND      ──►  NeoPixel GND
```

> Change `NEOPIXEL_PIN` in the `Config` namespace if your board uses a different GPIO.

---

## Software Requirements

### Arduino IDE

- Arduino IDE **2.x** (recommended) or 1.8.19+
- ESP32 Arduino core **3.x** (`espressif/arduino-esp32`)

Install the ESP32 core via **File → Preferences → Additional Board URLs**:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then install via **Tools → Board → Boards Manager → esp32 by Espressif**.

### Libraries

Install all of the following via **Tools → Manage Libraries**:

| Library | Author | Version |
|---|---|---|
| `ArduinoJson` | Benoit Blanchon | 7.x |
| `NTPClient` | Fabrice Weinberg | any |
| `Time` (TimeLib) | Michael Margolis / Paul Stoffregen | any |
| `Adafruit NeoPixel` | Adafruit | any |

The following are **built into the ESP32 Arduino core** — no separate install needed:

- `WiFi`, `WiFiClientSecure`, `HTTPClient`
- `FFat`, `FS`
- `esp_task_wdt`, `esp_heap_caps`
- `driver/temperature_sensor`
- `esp_psram` *(automatically included only when PSRAM is enabled in board settings)*

---

## Arduino IDE Board Settings

Open **Tools** and configure as shown below. The only field that differs depending on your hardware is **PSRAM**.

| Setting | Value |
|---|---|
| **Board** | `ESP32S3 Dev Module` |
| **PSRAM** | `OPI PSRAM` if your board has PSRAM · `Disabled` if it does not |
| **Flash Size** | `16MB (128Mb)` |
| **Partition Scheme** | `16M Flash (3MB APP/9.9MB FATFS)` |
| **Upload Speed** | `921600` |
| **USB CDC On Boot** | `Enabled` *(if using native USB)* |

> **Not sure if your board has PSRAM?** Set it to `Disabled` — the sketch will compile and run either way. If you later set it to `OPI PSRAM` on a board that has it, the sketch will automatically start using the larger HTTP buffers.

---

## API Keys

You need free-tier keys from four services:

| Service | Used For | Get Key At |
|---|---|---|
| **Groq** | Main AI inference (llama-3.3-70b-versatile) + function calling | [console.groq.com](https://console.groq.com/keys) |
| **Google Gemini** | Writing new skills on demand | [aistudio.google.com/apikey](https://aistudio.google.com/apikey) |
| **Serper** | Live web search results | [serper.dev](https://serper.dev) |
| **Meteosource** | Real-time weather by city | [meteosource.com](https://www.meteosource.com) |

---

## Configuration

Open the `.ino` file and fill in the `Config` namespace near the top of the file. The six values you **must** change before compiling:

```cpp
namespace Config {

  // ── Wi-Fi ─────────────────────────────────────────────
  constexpr const char* SSID           = "YOUR_WIFI_SSID";
  constexpr const char* PASSWORD       = "YOUR_WIFI_PASSWORD";

  // ── Groq ──────────────────────────────────────────────
  constexpr const char* GROQ_KEY       = "YOUR_GROQ_API_KEY";        // console.groq.com
  constexpr const char* GROQ_MODEL     = "llama-3.3-70b-versatile";

  // ── Weather ───────────────────────────────────────────
  constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY"; // meteosource.com

  // ── Web Search ────────────────────────────────────────
  constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";      // serper.dev

  // ── Gemini (skill writer) ─────────────────────────────
  constexpr const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";      // aistudio.google.com/apikey

}
```

> ⚠️ **Never commit real keys or Wi-Fi credentials to a public repository.** Replace every placeholder above with your own values in a local, untracked copy before uploading to your board — see [Security Notice](#security-notice).

### Optional Tweaks

| Constant | Default | Description |
|---|---|---|
| `NTP_OFFSET_SEC` | `19800` | UTC offset in seconds (19800 = UTC+5:30 IST) |
| `NEOPIXEL_PIN` | `48` | GPIO pin for WS2812B data |
| `WDT_TIMEOUT_S` | `40` | Watchdog timeout in seconds |
| `HTTP_TIMEOUT_MS` | `35000` | Per-request HTTP timeout |
| `SENTIMENT_TIMEOUT_MS` | `12000` | Timeout for sentiment/function-call requests |
| `MAX_CHAT_MESSAGES` | `30` | Rolling chat history window |
| `MAX_CHAT_TOKENS` | `3000` | Rolling chat history token budget |
| `MAX_MEMORY_FACTS` | `80` | Max persistent user facts |
| `MAX_REMINDERS` | `30` | Max stored reminders |
| `MAX_SENTIMENT_LOG` | `40` | Max stored sentiment log entries |
| `MAX_SKILLS` | `20` | Max saved skills |
| `AI_MAX_TOKENS` | `1024` | Max output tokens per AI response |
| `CPU_FREQ_IDLE` / `CPU_FREQ_ACTIVE` | `80` / `240` | CPU frequency (MHz) while idle vs. during HTTP calls |
| `PSRAM_REQ_SIZE` / `PSRAM_RESP_SIZE` | `24576` / `65536` | PSRAM buffer sizes in bytes (request / response) |

---

## Installation

1. **Clone or download** this repository.

2. **Rename the sketch folder** so it matches the `.ino` filename exactly:
   ```
   ESP32_S3_Ai_Assistant/
   └── ESP32_S3_Ai_Assistant.ino
   ```
   Arduino IDE requires the folder and file name to match.

3. **Fill in your credentials** in the `Config` namespace (see [Configuration](#configuration)).

4. **Set board settings** in Arduino IDE (see [Arduino IDE Board Settings](#arduino-ide-board-settings)).

5. **Connect your ESP32-S3** via USB and select the correct port under **Tools → Port**.

6. Click **Upload**. Compilation takes ~30–60 seconds.

7. Open **Tools → Serial Monitor**, set baud rate to **115200**, and watch the boot sequence.

---

## Usage

All interaction is through the **Serial Monitor** at 115200 baud.

### Boot Output

**With PSRAM enabled and detected:**
```
🚀 ESP32-AI v1.7.7-GROQ (Llama 3.3 70B) STARTING...
✅ PSRAM: req=24576B resp=65536B  total free=8338 KB
✅ Temperature sensor ready
✅ WDT configured
✅ FFat mounted
Connecting to WiFi...
✅ WiFi connected — IP: 192.168.1.42
✅ Dual-core AI worker on Core 0

💡 ESP32-AI v1.7.7-GROQ (Llama 3.3 70B) READY
```

**Without PSRAM (also perfectly normal):**
```
🚀 ESP32-AI v1.7.7-GROQ (Llama 3.3 70B) STARTING...
⚠️  PSRAM not detected — using internal SRAM for HTTP buffers
✅ Temperature sensor ready
✅ WDT configured
✅ FFat mounted
Connecting to WiFi...
✅ WiFi connected — IP: 192.168.1.42
✅ Dual-core AI worker on Core 0
```

> The `⚠️ PSRAM not detected` line is **not an error** — the sketch uses internal SRAM instead and all features work the same.

### Chatting

Type any message and press **Enter**:

```
You: What's the weather in London?
🤖AI: It's currently 18°C in London (feels like 15°C), partly cloudy.

You: Remind me to take my medicine in 10 minutes
🤖AI: ✅ Reminder set for 10 minutes from now.

You: Search for the latest news on Mars missions
🤖AI: [injects live Serper results and summarises]
```

### Slash Commands

| Command | What it does |
|---|---|
| `/help` | Full help screen |
| `/version` | Version, model, and live stats |
| `/diag` | Full diagnostics + AI health scan |
| `/reminders` | List all reminders |
| `/remove N` | Delete reminder N |
| `/memory` | Show all stored facts |
| `/summary` | AI-compress conversation history |
| `/weather [city]` | Live weather (uses stored city if omitted) |
| `/search [query]` | Web search + AI summary |
| `/clear` | Wipe all data & restart |
| `/skills` | List self-taught skills |
| `/skills remove [name]` | Forget a skill |
| `/skills keep` | Save pending skill |
| `/skills discard` | Discard pending skill |
| `/skills retry` | Regenerate pending skill |

### Natural Language

You don't need slash commands for most things — just talk to it:

```
"remember my name is Cash"
"can you please remember that my name is Cash"
"call me Cash"
"remind me to take medicine at 8pm"
"what's the weather in London"
"what do you know about me"
"search for best laptops 2025"
"don't let me forget my meeting at 3pm"
```

---

## Skills System

The skills engine lets the assistant **write and save new capabilities on demand** using Google Gemini as the code writer and a sandboxed DSL interpreter for safe execution.

### Creating a Skill

Just ask naturally:

```
You: Create a skill that counts from 1 to 5 and says each number
You: Create a skill that remembers my daily step goal and reports it
You: Create a skill that asks AI for a motivational quote and says it
```

Gemini writes the skill JSON, the assistant validates and saves it to FFat, and it is immediately available to trigger. You can approve (`/skills keep`), discard (`/skills discard`), or regenerate (`/skills retry`) a freshly generated skill before it's committed.

### DSL Operations

| Op | Description |
|---|---|
| `say` | Output text — supports `{varName}` and numeric/string interpolation |
| `set` | Set a numeric variable from an arithmetic expression |
| `set_str` | Set a string variable, with `{var}` interpolation |
| `inc` | Increment a numeric variable by a step |
| `if` / `else` | Conditional branching (`==`, `!=`, `<`, `>`, `<=`, `>=`) |
| `loop` | Repeat a block of ops a fixed number of times |
| `remember` | Persist a variable (numeric or string) to the memory system — survives reboot |
| `recall` | Load a persisted value back into a string variable |
| `groq` | Make an inline Groq AI call and store the result in a string variable |

### DSL Expression Functions

`ABS`, `FLOOR`, `CEIL`, `ROUND`, `MIN`, `MAX`, `MOD`, `SIN`, `COS`, `RANDOM`, `RAND100`, `STRLEN_varname`, `MILLIS`, `NOW_SEC`, `NOW_MIN`, `NOW_HOUR`, `NOW_DAY`, `NOW_MONTH`, `NOW_YEAR`

### Skills Limits

| Limit | Value |
|---|---|
| Max skills stored | 20 |
| Max numeric vars per skill | 12 |
| Max string vars per skill | 4 |
| Max loop iterations | 50 |
| Max skill JSON size | 4096 bytes |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      ESP32-S3                           │
│                                                         │
│  Core 1 (Arduino loop)          Core 0                  │
│  ┌──────────────────────┐       ┌─────────────────────┐ │
│  │ Serial input         │       │ aiHttpTask          │ │
│  │ NL intent parser      │  ──►  │  Groq HTTPS POST    │ │
│  │ NTP clock sync        │       │  SSE stream parse   │ │
│  │ Reminder checks       │  ◄──  │  Function calling   │ │
│  │ Non-blocking WiFi      │       │  Response delivery  │ │
│  │   reconnect            │       └─────────────────────┘ │
│  │ NeoPixel updates       │                               │
│  │ WDT resets             │       CPU frequency scaling   │
│  │ Batched flash flush    │       80 MHz idle / 240 MHz   │
│  │ Mood-adaptive temp      │       active                 │
│  └──────────────────────┘                               │
│                                                         │
│  HTTP Buffers (auto-selected at boot)                   │
│  ├── OPI PSRAM  [ 24 KB req / 64 KB resp ]  ← if present│
│  └── Internal SRAM  [ String-based ]        ← fallback  │
│                                                         │
│  FFat Flash Filesystem                                  │
│  ├── /memory.json      — user facts                     │
│  ├── /reminders.json   — pending reminders               │
│  ├── /skills.json      — learned skill definitions       │
│  ├── /sentiment.json   — mood log                        │
│  ├── /patterns.json    — topic preferences                │
│  └── /knowledge.json   — knowledge domains                │
└─────────────────────────────────────────────────────────┘
         │                                   │
    HTTPS / TLS                        HTTPS / TLS
         │                                   │
  ┌──────▼──────┐                   ┌────────▼────────┐
  │  Groq API   │                   │  Gemini API     │
  │  llama-3.3  │                   │  2.5 Flash      │
  │  70b-       │                   │  (skill writer) │
  │  versatile  │                   │                 │
  └─────────────┘                   └─────────────────┘
         │
  ┌──────▼──────┐    ┌──────────────┐
  │ Serper.dev  │    │ Meteosource  │
  │ Web search  │    │ Weather API  │
  └─────────────┘    └──────────────┘
```

### Data Flow for an AI Request

1. User types a message → Core 1's NL parser detects intent (reminder, memory, search, weather, chat, etc.)
2. Request body is handed to the **Core 0 AI task** via binary semaphore
3. CPU frequency scales up to 240 MHz; Core 0 serializes into the **PSRAM buffer** if available, or a standard `String` if not
4. Core 1 spins: feeds WDT, updates NeoPixel every 20 ms, handles non-blocking WiFi reconnect — fully responsive either way
5. Core 0 POSTs the request (with mood-adjusted temperature) and streams the SSE response into the **PSRAM buffer** if available, or `http.getString()` if not
6. Core 1 receives the completed response, prints it to Serial, and CPU frequency drops back to 80 MHz

---

## Serial Monitor Reference

### LED Status Colours

| Colour / Pattern | Meaning |
|---|---|
| 🟢 Slow green pulse | Idle, connected |
| 🔵 Fast blue pulse | Thinking / waiting for AI |
| 🟡 Yellow | WiFi reconnecting |
| 🔴 Red flash | Error |
| ✨ Excited pulse | Celebrating positive mood streak |
| 💙 Concerned glow | Offering comfort after a negative streak |
| 💡 Proactive flash | Assistant-initiated check-in |

### Log Prefixes

| Prefix | Meaning |
|---|---|
| `✅` | Success |
| `⚠️` | Warning / non-fatal issue |
| `❌` | Error |
| `[Core0]` | AI HTTP task log |
| `[Core1]` | Main loop log |
| `[Flash]` | FFat read/write |
| `🤖AI:` | Assistant response |

---

## Troubleshooting

### Compilation Errors

| Error | Fix |
|---|---|
| Library not found | Install via **Tools → Manage Libraries** |
| `esp_psram.h: No such file` | Set **Tools → PSRAM → Disabled** — the include is automatically skipped |
| Any other compile error | Make sure you are on ESP32 Arduino core **3.x**, not 2.x |

### Runtime Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| `⚠️ PSRAM not detected` at boot | Board has no PSRAM, or PSRAM set to Disabled | **This is normal** — sketch uses internal SRAM instead, no action needed |
| `Dual-core task create failed` | Stack too small | Increase the AI task's stack size in `xTaskCreatePinnedToCore` (currently 12 KB) |
| CPU temp always `0.0°C` | Temp sensor init failed | Not critical — everything else still works |
| No WiFi connection | Wrong credentials | Double-check `SSID` and `PASSWORD` in Config |
| WiFi drops and doesn't recover | Non-blocking reconnect stuck | Check `/diag` for reconnect count; power-cycle if it never clears |
| AI responses slow or timeout | Groq key invalid, rate limited, or 70B model under load | Verify key at [console.groq.com](https://console.groq.com/keys); increase `HTTP_TIMEOUT_MS` |
| Skill generation fails | Gemini key invalid | Verify key at [aistudio.google.com](https://aistudio.google.com/apikey) |
| Weather always fails | Meteosource key invalid or city not found | Try a major city name; verify key |
| LED freezes during AI call | Single-core fallback | Boot log should say `✅ Dual-core AI worker on Core 0` |
| Crash / reboot loop | Stack overflow or low SRAM | Run `/clear` to free chat history; reduce `MAX_CHAT_MESSAGES` or `MAX_CHAT_TOKENS` |
| FFat mount failed | Wrong partition scheme | Set **Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)** |

---

## Limits & Constraints

| Resource | Limit |
|---|---|
| Chat history window | 30 messages / 3000 tokens (auto-summarized by AI) |
| Per-message length | 2000 characters |
| Memory facts | 80 entries |
| Reminders | 30 entries |
| Sentiment log | 40 entries |
| Learned skills | 20 |
| AI max output tokens | 1024 per response |
| HTTP timeout | 35 seconds (12s for sentiment/function calls) |
| WDT timeout | 40 seconds |
| Flash batch interval | 30 seconds (sentiment, patterns, knowledge) |
| Reminder / memory saves | Immediate — not batched, never lost on crash |

---

## Security Notice

> ⚠️ API keys and Wi-Fi credentials in this sketch are stored as plain text and transmitted over TLS but **not** validated against a certificate authority (TLS verification is disabled for ESP32 compatibility). Never commit real keys, passwords, or tokens to a public repository — keep a local, untracked copy of your filled-in `Config` namespace, or move secrets to a `secrets.h` file excluded via `.gitignore`. Do not use production or billing-sensitive keys. This project is intended for personal and hobby use.

---

## License

Add your preferred license here (e.g. MIT) before publishing.

---

### Author

**ItzCoding**

Creator, architect, and developer of the ESP32-S3 AI Assistant project.
Designed the hardware integration, skills engine DSL, dual-core architecture, mood engine, and all firmware logic.

### Powered By

**Hardware Platform**

| Technology | Role |
|---|---|
| **Arduino** | Development framework and IDE |
| **ESP32-S3** | Microcontroller — dual-core Xtensa LX7, 16 MB Flash, optional OPI PSRAM |
| **Espressif ESP-IDF** | Underlying RTOS, peripheral drivers, and heap management |

**AI & APIs**

| Service | Role | Link |
|---|---|---|
| **Groq** | Ultra-fast AI inference + function calling — llama-3.3-70b-versatile | [groq.com](https://groq.com) |
| **Meta Llama 3.3 70B** | Large language model running on Groq | [llama.meta.com](https://llama.meta.com) |
| **Google Gemini 2.5 Flash** | On-demand skill generation | [deepmind.google](https://deepmind.google/technologies/gemini/) |
| **Serper.dev** | Real-time Google web search results | [serper.dev](https://serper.dev) |
| **Meteosource** | Live weather data by city | [meteosource.com](https://www.meteosource.com) |
| **NTP Pool Project** | Network time synchronisation | [pool.ntp.org](https://www.ntppool.org) |

**Open-Source Libraries**

| Library | Author / Maintainer | License |
|---|---|---|
| **ArduinoJson** | Benoit Blanchon | MIT |
| **Adafruit NeoPixel** | Adafruit Industries | LGPL-3.0 |
| **NTPClient** | Fabrice Weinberg | MIT |
| **Time (TimeLib)** | Michael Margolis, Paul Stoffregen | LGPL |
| **arduino-esp32** | Espressif Systems | Apache-2.0 |
| **FreeRTOS** | Real Time Engineers Ltd | MIT |

---

*ESP32-S3 AI Assistant v1.7.7 · Made by ItzCoding · Powered by Arduino, ESP32, Groq and Gemini*
