# ESP32-S3 AI Assistant v1.7.6

A self-learning, voice-and-text AI assistant running entirely on an ESP32-S3 microcontroller — powered by Groq, Gemini, real-time web search, and a sandboxed skills engine.

---

## Table of Contents

- [Screenshots](#screenshots)
- [Overview](#overview)
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

![Boot & Hello](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.6/Extra/Hello.png)
**Skill Learning**

![Learning skills](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.6/Extra/Learning%20skills.png)

**System Diagnosis**

![System Diagnosis](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.6/Extra/Diagnosis.png)

**Web Search**

![Web Server](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-Ai-Assistant-v1.7.6/Extra/Web%20Search.png)

---

## Overview

ESP32-S3 AI Assistant is a full-featured embedded AI chatbot that runs on a single ESP32-S3 module. It connects to Groq's ultra-fast inference API to deliver sub-second responses via **llama-3.1-8b-instant**, uses **Gemini 2.5 Flash** to write and save new skills on the fly, searches the web via **Serper.dev**, fetches live weather from **Meteosource**, and persists everything — memory facts, reminders, learned skills, sentiment logs — to on-board FFat flash storage.

The sketch works on **any ESP32-S3 board**, with or without OPI PSRAM. If PSRAM is present it is used automatically for larger HTTP buffers; if not, the sketch falls back to internal SRAM seamlessly with no code changes required.

All interaction happens over **USB Serial** (115200 baud). Type a message, press Enter, read the reply.

---

## Features

| Category | Details |
|---|---|
| **AI Backbone** | Groq · llama-3.1-8b-instant · SSE streaming response |
| **Skill Generation** | Gemini 2.5 Flash writes new skills in a sandboxed DSL |
| **Web Search** | Serper.dev live Google search results injected as context |
| **Weather** | Meteosource real-time weather by city name |
| **Reminders** | Natural-language reminder parsing, persistent, fires on time |
| **Memory** | Up to 60 persistent user facts stored in FFat |
| **Sentiment** | Per-conversation mood tracking, log of last 30 interactions |
| **User Patterns** | Learns topic preferences over time |
| **NTP Clock** | Synced real-time clock, configurable UTC offset |
| **Diagnostics** | CPU temp · free heap · uptime · PSRAM status |
| **Dual-core AI** | HTTP call runs on Core 0; LED + reminders stay alive on Core 1 |
| **PSRAM Buffers** | 8 KB req + 16 KB resp buffers used if PSRAM is present; falls back to internal SRAM automatically |
| **Batched Flash** | Dirty-flag system — writes sentiment/pattern/knowledge in 30 s batches |
| **WDT Safety** | 30-second watchdog with explicit resets throughout the loop |
| **NeoPixel Status** | Single RGB LED shows idle / thinking / error states |

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
| **Groq** | Main AI inference (llama-3.1-8b) | [console.groq.com](https://console.groq.com/keys) |
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

  // ── Weather ───────────────────────────────────────────
  constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY"; // meteosource.com

  // ── Web Search ────────────────────────────────────────
  constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";      // serper.dev

  // ── Gemini (skill writer) ─────────────────────────────
  constexpr const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";      // aistudio.google.com/apikey

}
```

### Optional Tweaks

| Constant | Default | Description |
|---|---|---|
| `NTP_OFFSET_SEC` | `19800` | UTC offset in seconds (19800 = UTC+5:30 IST) |
| `NEOPIXEL_PIN` | `48` | GPIO pin for WS2812B data |
| `WDT_TIMEOUT_S` | `30` | Watchdog timeout in seconds |
| `MAX_CHAT_MESSAGES` | `20` | Rolling chat history window |
| `MAX_MEMORY_FACTS` | `60` | Max persistent user facts |
| `MAX_REMINDERS` | `30` | Max stored reminders |
| `MAX_SKILLS` | `20` | Max saved skills |
| `HTTP_TIMEOUT_MS` | `30000` | Per-request HTTP timeout |

---

## Installation

1. **Clone or download** this repository.

2. **Rename the sketch folder** so it matches the `.ino` filename exactly:
   ```
   ESP32_AI_v1_0/
   └── ESP32_AI_v1_0.ino
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
🚀 ESP32-AI v1.0-GROQ (Llama 3.1 8B) STARTING...
✅ PSRAM: req=8192B resp=16384B  total free=8338 KB
✅ Temperature sensor ready
✅ WDT configured
✅ FFat mounted
Connecting to WiFi...
✅ WiFi connected — IP: 192.168.1.42
✅ Dual-core AI worker on Core 0
```

**Without PSRAM (also perfectly normal):**
```
🚀 ESP32-AI v1.0-GROQ (Llama 3.1 8B) STARTING...
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

You: Search for latest news on Mars missions
🤖AI: [injects live Serper results and summarises]
```

### Built-in Commands

| What you say | What happens |
|---|---|
| `what's your CPU temperature?` | Reports on-chip temperature |
| `free heap` / `memory usage` | Reports available SRAM |
| `uptime` | Time since last boot |
| `health check` | Full diagnostics dump |
| `/clear` | Clears chat history, frees SRAM |
| `list reminders` | Shows all pending reminders |
| `list skills` | Shows all learned skills |
| `forget [fact]` | Removes a specific memory fact |

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

Gemini writes the skill JSON, the assistant validates and saves it to FFat, and it is immediately available to trigger.

### DSL Operations

| Op | Description |
|---|---|
| `say` | Output text — supports `{varName}`, `:.Nf`, `:int`, `:sec_to_mss`, `:time` format specifiers |
| `set` | Set a numeric variable |
| `set_str` | Set a string variable |
| `inc` | Increment a numeric variable |
| `add` / `sub` / `mul` / `div` | Arithmetic on numeric vars |
| `if` / `else` | Conditional branching (up to 3 levels deep) |
| `loop` | Repeat a block 1–10 times |
| `wait` | Pause execution (ms) |
| `remember` | Persist a variable to the memory system — survives reboot |
| `recall` | Load a persisted value back into a string variable |
| `groq` | Make an inline Groq AI call and store the result in a string variable |

### DSL Expression Functions

`ABS`, `FLOOR`, `CEIL`, `ROUND`, `MIN`, `MAX`, `MOD`, `SIN`, `COS`, `RANDOM`, `RAND100`, `STRLEN_varname`, `NOW_SEC`, `NOW_DAY`, `NOW_MONTH`, `NOW_YEAR`

### Skills Limits

| Limit | Value |
|---|---|
| Max skills stored | 20 |
| Max numeric vars per skill | 12 |
| Max string vars per skill | 4 |
| Max actions per skill | 8 |
| Max ops per action | 16 |
| Max loop iterations | 10 |
| Max if-nesting depth | 3 |
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
│  │ NTP clock sync       │  ──►  │  Groq HTTPS POST    │ │
│  │ Reminder checks      │       │  SSE stream parse   │ │
│  │ NeoPixel updates     │  ◄──  │  Response delivery  │ │
│  │ WDT resets           │       └─────────────────────┘ │
│  │ Batched flash flush  │                               │
│  └──────────────────────┘                               │
│                                                         │
│  HTTP Buffers (auto-selected at boot)                   │
│  ├── OPI PSRAM  [ 8 KB req / 16 KB resp ]  ← if present │
│  └── Internal SRAM  [ String-based ]       ← fallback   │
│                                                         │
│  FFat Flash Filesystem                                  │
│  ├── /memory.json      — user facts                     │
│  ├── /reminders.json   — pending reminders              │
│  ├── /skills.json      — learned skill definitions      │
│  ├── /sentiment.json   — mood log                       │
│  ├── /patterns.json    — topic preferences              │
│  └── /knowledge.json   — knowledge domains              │
└─────────────────────────────────────────────────────────┘
         │                                   │
    HTTPS / TLS                        HTTPS / TLS
         │                                   │
  ┌──────▼──────┐                   ┌────────▼────────┐
  │  Groq API   │                   │  Gemini API     │
  │  llama-3.1  │                   │  2.5 Flash      │
  │  8b-instant │                   │  (skill writer) │
  └─────────────┘                   └─────────────────┘
         │
  ┌──────▼──────┐    ┌──────────────┐
  │ Serper.dev  │    │ Meteosource  │
  │ Web search  │    │ Weather API  │
  └─────────────┘    └──────────────┘
```

### Data Flow for an AI Request

1. User types a message → Core 1 detects intent via NL parser
2. Request body is handed to **Core 0 AI task** via binary semaphore
3. Core 0 serializes into the **PSRAM buffer** if available, or a standard `String` if not
4. Core 1 spins: feeds WDT, updates NeoPixel every 20 ms — fully responsive either way
5. Core 0 POSTs the request and streams the SSE response into **PSRAM buffer** if available, or `http.getString()` if not
6. Core 1 receives the completed response and prints to Serial

---

## Serial Monitor Reference

### LED Status Colours

| Colour | Meaning |
|---|---|
| 🟢 Slow green pulse | Idle, connected |
| 🔵 Fast blue pulse | Thinking / waiting for AI |
| 🟡 Yellow | WiFi reconnecting |
| 🔴 Red flash | Error |

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
| `Dual-core task create failed` | Stack too small | Increase `AI_TASK_STACK` in `xTaskCreatePinnedToCore` |
| CPU temp always `0.0°C` | Temp sensor init failed | Not critical — everything else still works |
| No WiFi connection | Wrong credentials | Double-check `SSID` and `PASSWORD` in Config |
| AI responses slow or timeout | Groq key invalid or rate limited | Verify key at [console.groq.com](https://console.groq.com/keys) |
| Skill generation fails | Gemini key invalid | Verify key at [aistudio.google.com](https://aistudio.google.com/apikey) |
| Weather always fails | Meteosource key invalid or city not found | Try a major city name; verify key |
| LED freezes during AI call | Single-core fallback | Boot log should say `✅ Dual-core AI worker on Core 0` |
| Crash / reboot loop | Stack overflow or low SRAM | Run `/clear` to free chat history; reduce `MAX_CHAT_MESSAGES` |
| FFat mount failed | Wrong partition scheme | Set **Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)** |

---

## Limits & Constraints

| Resource | Limit |
|---|---|
| Chat history window | 20 messages / 1400 tokens (auto-trimmed) |
| Per-message length | 1200 characters |
| Memory facts | 60 entries |
| Reminders | 30 entries |
| Sentiment log | 30 entries |
| Learned skills | 20 |
| AI max output tokens | 600 per response |
| HTTP timeout | 30 seconds |
| WDT timeout | 30 seconds |
| Flash batch interval | 30 seconds (sentiment, patterns, knowledge) |
| Reminder / memory saves | Immediate — not batched, never lost on crash |

---

## Security Notice

> ⚠️ API keys are stored as plain text in the sketch and transmitted over TLS but **not** validated against a certificate authority (TLS verification is disabled for ESP32 compatibility). Do not use production or billing-sensitive keys. This project is intended for personal and hobby use.

---

### Author

**ItzCoding**

Creator, architect, and developer of the ESP32-S3 AI Assistant project.
Designed the hardware integration, skills engine DSL, dual-core architecture, and all firmware logic.

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
| **Groq** | Ultra-fast AI inference — llama-3.1-8b-instant | [groq.com](https://groq.com) |
| **Meta Llama 3.1 8B** | Large language model running on Groq | [llama.meta.com](https://llama.meta.com) |
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

*ESP32-S3 AI Assistant v1.0 · Made by ItzCoding · Powered by Arduino, ESP32, Groq and Gemini
