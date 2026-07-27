# ESP32-S3 AI Assistant v1.7.8

An advanced, self-learning, voice-and-text embedded AI companion running natively on an ESP32-S3 microcontroller. Powered by Groq's Llama 3.3 70B, Google Gemini 2.5 Flash, real-time Google search via Serper.dev, Meteosource weather engine, GitHub OTA firmware updates, mood-adaptive response system, persistent note and task management, and a sandboxed DSL skills engine.

---

## Table of Contents

- [Screenshots](#screenshots)
- [System Architecture](#system-architecture)
- [What's New in v1.7.8](#whats-new-in-v178)
- [Key Features](#key-features)
- [Hardware Requirements](#hardware-requirements)
- [Pinout & Wiring Guide](#pinout--wiring-guide)
- [Software Requirements & Dependencies](#software-requirements--dependencies)
- [Arduino IDE Configuration](#arduino-ide-configuration)
- [Partition Table Setup](#partition-table-setup)
- [API Key Setup & Provider Overview](#api-key-setup--provider-overview)
- [Configuration & Credentials](#configuration--credentials)
- [Installation & Flashing Guide](#installation--flashing-guide)
- [User Guide & Operation](#user-guide--operation)
- [Serial Interface Protocol](#serial-interface-protocol)
- [Complete Slash Command Reference](#complete-slash-command-reference)
- [Natural Language Intent Parser](#natural-language-intent-parser)
- [Sandboxed Skills Engine & DSL](#sandboxed-skills-engine--dsl)
- [DSL Syntax Specification](#dsl-syntax-specification)
- [Skill Lifecycle Management](#skill-lifecycle-management)
- [Example Custom Skills](#example-custom-skills)
- [Mood Engine & Adaptive Personality](#mood-engine--adaptive-personality)
- [Memory, Notes & Task Persistence](#memory-notes--task-persistence)
- [System Health, Telemetry & Diagnostics](#system-health-telemetry--diagnostics)
- [GitHub Over-The-Air (OTA) Updates](#github-over-the-air-ota-updates)
- [Dual-Core Task & Memory Management](#dual-core-task--memory-management)
- [Status LED Visual Guide](#status-led-visual-guide)
- [Troubleshooting & FAQ](#troubleshooting--faq)
- [Technical Specifications & Limits](#technical-specifications--limits)
- [Security Considerations](#security-considerations)
- [Development & Contributions](#development--contributions)
- [License](#license)
- [Credits & Acknowledgments](#credits--acknowledgments)

---

## Screenshots

### Boot Sequence & System Initialization
![Boot & Hello](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-AI-Assistant-v1.7.8/Extra/Hello.png)

### Real-Time Skill Generation & DSL Execution
![Learning skills](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-AI-Assistant-v1.7.8/Extra/Learning%20skills.png)

### Deep System Telemetry & AI Health Scan
![System Diagnosis](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-AI-Assistant-v1.7.8/Extra/Diagnosis.png)

### Web Search & Information Synthesis
![Web Search](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant/blob/main/ESP32-S3-AI-Assistant-v1.7.8/Extra/Web%20Search.png)

---

## System Architecture

The ESP32-S3 AI Assistant is engineered specifically for high-efficiency embedded AI deployment. By distributing responsibilities across both Xtensa LX7 cores, the system provides instantaneous responses while remaining resilient against network latency or watchdog resets.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ESP32-S3 Microcontroller                           │
│                                                                             │
│  Core 1 (Main Event Loop)                Core 0 (Async Network & AI)        │
│  ┌─────────────────────────────────┐     ┌────────────────────────────────┐ │
│  │ USB Serial Protocol Parser      │     │ Groq HTTPS Client (Core AI)    │ │
│  │ Natural Language Intent Engine  │ ──► │ Gemini HTTPS Client (Skills)   │ │
│  │ NTP Time Sync & Daily Alarms    │     │ Serper Google Search Engine    │ │
│  │ Task & Reminder Engine          │ ◄── │ Meteosource Weather Service    │ │
│  │ Non-blocking WiFi Supervisor    │     │ SSE Streaming JSON Parser      │ │
│  │ GitHub OTA Binary Flasher       │     │ Function & Tool Call Dispatch  │ │
│  │ NeoPixel Status Animation Engine│     └────────────────────────────────┘ │
│  │ Batched FFat Flash Sync Engine  │                                        │
│  │ Watchdog Reset & Temp Telemetry │     Dynamic Frequency Scaling (DFS)    │
│  └─────────────────────────────────┘     80 MHz (Idle) ◄──► 240 MHz (Active)│
│                                                                             │
│  Memory Allocator (Dynamic PSRAM / Internal SRAM Fallback)                  │
│  FFat Flash Filesystem (/memory, /tasks, /notes, /skills, /logs)            │
└─────────────────────────────────────────────────────────────────────────────┘
                                  │                         │
                             TLS 1.2 / HTTPS           TLS 1.2 / HTTPS
                                  │                         │
              ┌───────────────────┴─────┐                 ┌───┴───────────────────┐
              │     Groq Cloud API      │                 │    Google Gemini API  │
              │ llama-3.3-70b-versatile │                 │     Gemini 2.5 Flash  │
              └────────────┌────────────┘                 └───────────────────────┘
                          │                                         │
               ┌───────────┴───────────┐                  ┌───────────┴───────────┐
               │      Serper.dev       │                  │   Meteosource API     │
               │    Google Search      │                  │    Live Weather       │
               └───────────────────────┘                  └───────────────────────┘
```

---

## Overview

The **ESP32-S3 AI Assistant v1.7.8** represents a major leap forward in edge computing and micro-AI integration. Operating on a single ESP32-S3 module, this system combines state-of-the-art cloud inference with local intelligence and persistent hardware storage.

By querying Groq's high-throughput infrastructure, the assistant delivers near-instantaneous streaming dialogue generated by **Llama 3.3 70B**. When given complex requests outside its native capabilities, it interfaces with **Gemini 2.5 Flash** to write executable DSL skills on the fly. Structured actions such as querying live search engines, getting weather forecasts, saving user memories, or setting context-aware alarms are executed cleanly via Groq native tool/function calling.

The system requires no external MCU or SBC, operating on any standard ESP32-S3 dev board. It automatically detects Octal/Quad PSRAM to expand HTTP buffers up to 64 KB, while offering seamless fallback to internal SRAM on lower-spec modules.

---

## What's New in v1.7.8

| Component / Feature | Upgrade Summary | Impact |
|---|---|---|
| **GitHub Native OTA** | Over-the-air firmware upgrades directly from GitHub Release binaries using `/update` and `/install`. | Eliminates the need to connect USB cables for code updates. |
| **Notes & To-Do Engine** | Persistent context-aware note taking and checklist system with full natural language recall. | Seamless organizational management stored in flash filesystem. |
| **AI System Health Scan** | Enhanced telemetry command (`/diag`) reporting SRAM drift, CPU temperature, HTTP error flags, and flash endurance. | Real-time monitoring of device health and heap stability. |
| **Expanded Intent Parser** | Broadened speech pattern handling, correction logic ("scratch that", "no wait, change to"), and filler-word dropping. | Highly forgiving natural language interpretation. |
| **Adaptive Tone Engine** | Continuous sentiment tracking scales prompt dynamics between technical, warm, and structured formats. | Dynamic conversational flow tailored to user mood. |
| **Optimized SSE Parser** | Rewritten streaming JSON reader optimized for Groq's fast token throughput. | Prevents buffer overflows and eliminates response stutter. |

---

## Key Features

- **Blazing Fast AI Inference**: Powered by Groq's `llama-3.3-70b-versatile` running via Server-Sent Events (SSE) streaming.
- **On-Demand Skill Generation**: Uses Google Gemini 2.5 Flash to automatically draft, compile, and store custom micro-skills.
- **GitHub Over-The-Air (OTA) Flashing**: Fetch released binaries over HTTPS directly from GitHub releases without physical wiring.
- **Native Groq Tool Calling**: Direct tool-call parsing for dynamic weather retrieval, web search, memory persistence, and alarm dispatch.
- **Live Google Search Integration**: Real-time context retrieval using Serper.dev for current events and real-time facts.
- **Meteosource Weather API**: Accurate localized weather updates and multi-day meteorological forecasts.
- **Long-Term Memory Persistence**: Automatically logs up to 80 user facts, preferences, notes, and task lists into FFat flash memory.
- **Mood Tracking & Sentiment Engine**: Analyzes conversation history over a rolling 40-message window to adapt assistant warmth and temperance.
- **Proactive Time-of-Day Nudges**: Automated morning summaries, afternoon check-ins, and evening recaps synced via NTP.
- **Sandboxed DSL Runtime**: Safe execution environment for learned user skills without risking firmware crashes or stack overflows.
- **Dual-Core Hardware Pipeline**: Core 0 handles network requests and SSL handshakes; Core 1 keeps serial, clock, and status NeoPixel running at 60 FPS.
- **Hardware Power Optimization**: Automatic CPU frequency scaling between 80 MHz idle and 240 MHz active inference states.

---

## Hardware Requirements

| Hardware Component | Requirement / Specification | Recommended Model |
|---|---|---|
| **Microcontroller** | ESP32-S3 Dual-Core Xtensa LX7 @ 240MHz | ESP32-S3-DevKitC-1 / ESP32-S3-WROOM-1 |
| **Flash Memory** | 16 MB SPI Flash (Quad/Octal SPI) | N16R8 / N16R2 modules |
| **PSRAM** | 8 MB OPI PSRAM (Optional, but recommended) | OPI PSRAM supported natively |
| **Status LED** | WS2812B / SK6812 Addressable RGB LED | Onboard GPIO 48 NeoPixel |
| **Serial Interface** | USB CDC or CP2102 / CH340 USB-to-UART | USB Type-C Port |
| **Power Supply** | 5V DC via USB-C (Minimum 500mA output) | Quality USB Cable |

---

## Pinout & Wiring Guide

### Default Hardware Configuration

Most standard ESP32-S3 development boards feature an onboard RGB NeoPixel LED attached to GPIO 48. If using an external NeoPixel strip or custom breakout board, wire according to the diagram below.

```
+-------------------+                      +-----------------------+
|  ESP32-S3 Board   |                      |  WS2812B NeoPixel     |
|                   |                      |                       |
|   GPIO 48 (Data)  |--------------------->|  DIN (Data Input)     |
|   5V / VBUS       |--------------------->|  VDD / +5V            |
|   GND             |--------------------->|  GND                  |
+-------------------+                      +-----------------------+
```

*Note: Pin numbers can be customized inside the `Config` namespace in `ESP32_S3_Ai_Assistant.ino`.*

---

## Software Requirements & Dependencies

To compile and upload the firmware, ensure your development environment is properly configured.

### Required Software IDE
- **Arduino IDE**: Version 2.2.1 or higher.
- **ESP32 Arduino Core**: Version 3.0.0 or higher.

### Third-Party Library Dependencies
Install the following libraries via the **Arduino IDE Library Manager** (`Ctrl+Shift+I`):

1. **ArduinoJson** by *Benoit Blanchon* (Version `7.x.x` required)
2. **NTPClient** by *Fabrice Weinberg*
3. **Time** (TimeLib) by *Michael Margolis / Paul Stoffregen*
4. **Adafruit NeoPixel** by *Adafruit*

### Core ESP32 Built-in Libraries (No Installation Needed)
The following libraries are included with the Espressif ESP32 Arduino Core:
- `WiFi.h` & `WiFiClientSecure.h`
- `HTTPClient.h`
- `FFat.h` & `FS.h`
- `Update.h`
- `esp_task_wdt.h`
- `driver/temperature_sensor.h`
- `esp_heap_caps.h`

---

## Arduino IDE Configuration

Select **ESP32S3 Dev Module** from the **Tools → Board** menu and match the precise settings listed in the table below:

| Configuration Menu Item | Correct Target Value |
|---|---|
| **Board** | `ESP32S3 Dev Module` |
| **Port** | Select active COM / `/dev/ttyACM` port |
| **USB CDC On Boot** | `Enabled` (Crucial for Serial Monitor over native USB) |
| **CPU Frequency** | `240MHz (WiFi)` |
| **Core Debug Level** | `None` (or `Error` for debugging) |
| **Erase All Flash Before Sketch Upload** | `Disabled` (Set to `Enabled` on first flash) |
| **Flash Frequency** | `80MHz` |
| **Flash Mode** | `QIO 80MHz` or `OPI 80MHz` |
| **Flash Size** | `16MB (128Mb)` |
| **JTAG Adapter** | `Disabled` |
| **Memory Model** | `Default 4MB with internal RAM` |
| **Partition Scheme** | `16M Flash (3MB APP/9.9MB FATFS)` |
| **PSRAM** | `OPI PSRAM` (Select `Disabled` if module lacks PSRAM) |
| **Upload Speed** | `921600` |

---

## Partition Table Setup

This firmware requires custom flash partitioning to provide ample application room alongside a large **FFat (FATFS)** file system for storing long-term memory, notes, tasks, logs, and skills.

If using custom partition setups, verify that your `partitions.csv` is configured as follows:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
app1,     app,  ota_1,   0x310000,0x300000,
ffat,     data, fat,     0x610000,0x9E0000,
```

---

## API Key Setup & Provider Overview

To enable the full spectrum of AI and web features, register for free keys across the four supported external services:

```
┌─────────────────┬───────────────────────────────┬──────────────────────────────────────────┐
│ Provider        │ System Function               │ Registration URL                         │
├─────────────────┼───────────────────────────────┼──────────────────────────────────────────┤
│ Groq Cloud      │ Main AI Inference & Tools     │ [https://console.groq.com/keys](https://console.groq.com/keys)            │
│ Google Gemini   │ Skill Synthesis & DSL Writer  │ [https://aistudio.google.com/apikey](https://aistudio.google.com/apikey)       │
│ Serper.dev      │ Live Google Web Search        │ [https://serper.dev](https://serper.dev)                       │
│ Meteosource     │ Live Weather & Forecasts      │ [https://www.meteosource.com](https://www.meteosource.com)              │
└─────────────────┴───────────────────────────────┴──────────────────────────────────────────┘
```

---

## Configuration & Credentials

Open `ESP32_S3_Ai_Assistant.ino` in your editor and update the constants within the `Config` namespace at the head of the file:

```cpp
namespace Config {

  // ── Wi-Fi Connection Credentials ─────────────────────────────────────────
  constexpr const char* SSID           = "YOUR_WIFI_NETWORK_SSID";
  constexpr const char* PASSWORD       = "YOUR_WIFI_PASSWORD";

  // ── Groq AI Engine Settings ──────────────────────────────────────────────
  constexpr const char* GROQ_KEY       = "gsk_YourGroqApiKeyHere";
  constexpr const char* GROQ_MODEL     = "llama-3.3-70b-versatile";

  // ── Google Gemini Skill Writer ───────────────────────────────────────────
  constexpr const char* GEMINI_API_KEY = "AIzaSyYourGeminiApiKeyHere";

  // ── Web Search Credentials (Serper.dev) ──────────────────────────────────
  constexpr const char* SERPER_API_KEY = "YourSerperApiKeyHere";

  // ── Weather Engine Credentials (Meteosource) ──────────────────────────────
  constexpr const char* WEATHER_KEY    = "YourMeteosourceApiKeyHere";
  constexpr const char* DEFAULT_CITY   = "London";

  // ── Hardware Pin Configuration ───────────────────────────────────────────
  constexpr uint8_t NEOPIXEL_PIN       = 48;
  constexpr uint8_t NUM_LEDS           = 1;

  // ── Timezone Configuration ──────────────────────────────────────────────
  constexpr long UTC_OFFSET_SECONDS    = 0; // Set local offset e.g. 19800 for UTC+5:30
}
```

---

## Installation & Flashing Guide

1. **Clone or Download the Repository**:
   ```bash
   git clone [https://github.com/ItzCoding/ESP32-S3-Ai-Assistant.git](https://github.com/ItzCoding/ESP32-S3-Ai-Assistant.git)
   ```
2. **Setup Folder Structure**: Ensure the parent folder matches the `.ino` sketch name:
   ```
   ESP32_S3_Ai_Assistant/
   ├── ESP32_S3_Ai_Assistant.ino
   ├── partitions.csv
   └── README.md
   ```
3. **Configure Settings**: Update Wi-Fi and API keys inside `Config`.
4. **Connect Board**: Plug the ESP32-S3 board into your computer via USB.
5. **Set IDE Parameters**: Choose `ESP32S3 Dev Module` and select your serial COM port.
6. **Flash Sketch**: Click **Upload** (`Ctrl+U`) in Arduino IDE.
7. **Launch Monitor**: Open **Tools → Serial Monitor** set to `115200 baud` with `Both NL & CR` enabled.

---

## User Guide & Operation

### Serial Interface Protocol

All user communication with the ESP32-S3 AI Assistant takes place over standard Serial communication at 115200 baud.

When booted, the assistant displays system initialization flags, mounts the FFat storage partition, connects to Wi-Fi, syncs with NTP time servers, and posts the ready prompt:

```
🚀 ESP32-AI v1.7.8-GROQ (Llama 3.3 70B) STARTING...
✅ PSRAM: req=24576B resp=65536B  total free=8338 KB
✅ Temperature sensor ready
✅ WDT configured (40s)
✅ FFat mounted successfully
Connecting to WiFi SSID: Home_Network...
✅ WiFi connected — IP: 192.168.1.105
✅ Core 0 AI HTTP Worker initialized

💡 ESP32-AI v1.7.8-GROQ (Llama 3.3 70B) READY
You: 
```

---

### Complete Slash Command Reference

Slash commands provide instant control over device state without invoking full LLM processing cycles:

| Command Syntax | Parameters | Detailed Description |
|---|---|---|
| `/help` | None | Displays full interactive command cheat sheet and active system settings. |
| `/version` | None | Prints build version, model parameters, free heap, uptime, and IP address. |
| `/diag` | None | Performs deep health scan: SRAM drift, CPU temp, HTTP error counters, flash status. |
| `/update` | None | Checks GitHub repository for newer release tags and firmware binaries. |
| `/install` | None | Downloads latest released `.bin` binary over HTTPS and flashes via OTA. |
| `/reminders` | None | Lists all upcoming tasks, single alarms, and recurring reminders with indices. |
| `/remove` | `<index>` | Deletes specific reminder or task by numerical ID (e.g., `/remove 2`). |
| `/memory` | None | Displays all long-term facts stored in persistent user knowledge base. |
| `/notes` | None | Prints all stored persistent user text notes and checklists. |
| `/summary` | None | Invokes Groq to compress active chat session history into concise memory state. |
| `/weather` | `[city]` | Queries Meteosource for immediate weather conditions in target city. |
| `/search` | `<query>` | Executes Serper.dev web search and returns AI-synthesized real-time answer. |
| `/skills` | None | Displays list of self-generated micro-skills stored in flash storage. |
| `/skills remove`| `<name>` | Permanently deletes a saved micro-skill from file storage. |
| `/skills keep` | None | Confirms and saves recently compiled draft skill into active memory. |
| `/skills discard`| None | Discards pending draft skill without saving. |
| `/skills retry`  | None | Asks Gemini 2.5 Flash to regenerate DSL code for pending skill draft. |
| `/clear` | None | Performs factory wipe: erases all memory, notes, tasks, skills, and reboots. |

---

### Natural Language Intent Parser

The natural language parser on Core 1 handles unstructured conversational input seamlessly. It automatically extracts intent without requiring rigid syntax.

#### Supported Natural Language Patterns

- **Memory Registration**:
  - *"Remember that my coffee preference is espresso with oat milk."*
  - *"Keep in mind that my Wi-Fi router password is stored in the cabinet."*
- **Task & Note Creation**:
  - *"Add a note: order extra ESP32 development boards next week."*
  - *"Put buy groceries on my to-do list."*
  - *"Show my saved notes."*
- **Reminder & Alarm Scheduling**:
  - *"Remind me to call John at 5pm."*
  - *"Set an alarm for 7:30 AM every weekday."*
- **Live Search & Weather Inquiry**:
  - *"What is the current stock price of Apple?"*
  - *"Is it going to rain in Tokio tomorrow?"*
- **Phrase Correction Handling**:
  - *"Remind me to water plants at 4pm... actually, make that 6pm."*
  - *"Scratch that, remove my last note."*

---

## Sandboxed Skills Engine & DSL

When asked to perform custom routines beyond standard built-in functions, the system invokes **Google Gemini 2.5 Flash** to draft custom macro scripts using a specialized, sandboxed Domain-Specific Language (DSL).

### DSL Syntax Specification

The internal DSL runtime evaluates linear command blocks safely without allowing arbitrary micro-controller code execution:

```
COMMAND: SAY <text>           -> Output speech text to Serial
COMMAND: DELAY <milliseconds> -> Non-blocking delay thread
COMMAND: LED <color_hex>      -> Set NeoPixel RGB status color
COMMAND: FETCH_AI <prompt>    -> Execute sub-prompt query to Groq
COMMAND: STORE_VAR <key=val>  -> Persist temporary runtime variable
COMMAND: REPEAT <N> ... END   -> Bounded loop execution
```

### Skill Lifecycle Management

```
User Request ──► Gemini 2.5 Flash ──► DSL Syntax Validator ──► Pending Draft
                                                                     │
       ┌───────────────────────────────┬─────────────────────────────┘
       ▼                               ▼                             ▼
`/skills keep`                  `/skills retry`              `/skills discard`
Saved to /skills/              Regenerates DSL              Flushed from memory
```

### Example Custom Skills

#### Skill 1: Daily Focus Timer
```dsl
SKILL: FocusTimer
  LED #FF5500
  SAY Starting 25 minute productivity timer.
  DELAY 1500000
  LED #00FF00
  SAY Focus session complete! Take a break.
END
```

#### Skill 2: Morning Inspiration Macro
```dsl
SKILL: MorningInspiration
  LED #00FFFF
  FETCH_AI Give me a inspiring 1-sentence quote for starting the workday.
  SAY Have a productive day!
END
```

---

## Mood Engine & Adaptive Personality

The assistant features an embedded sentiment analysis matrix that monitors user input over a rolling 40-message context buffer. The user's emotional state directly modifies the LLM system prompt dynamics and temperature settings:

```
┌─────────────────┬─────────────────────┬───────────────────────────┬──────────────────┐
│ Emotional State │ Temperature Scale   │ Output Tone Adaptation    │ LED Status Color │
├─────────────────┼─────────────────────┼───────────────────────────┼──────────────────┤
│ Joyful / Upbeat │ 0.85 (High)         │ Enthusiastic, creative    │ Fast Pink Pulse  │
│ Neutral / Tech  │ 0.60 (Balanced)     │ Concise, precise, clear   │ Cyan Breathing   │
│ Frustrated      │ 0.35 (Low/Focused)  │ Direct, empathetic, calm  │ Warm Gold Glow   │
│ Tired / Evening │ 0.50 (Gentle)       │ Soft, brief, practical    │ Soft Amber Glow  │
└─────────────────┴─────────────────────┴───────────────────────────┴──────────────────┘
```

---

## Memory, Notes & Task Persistence

All persistent data structures are managed through the on-board **FFat (Fat File System)** partition. The firmware employs a **dirty-flag batching system** that queues memory writes in RAM and flushes them to flash storage every 30 seconds, preserving flash memory lifecycle endurance.

### Storage Allocation Breakdown

```
/ffat partition (9.9 MB Total)
├── /memory.json     -> Stores up to 80 user preference facts
├── /tasks.json      -> Stores active reminders, alarms, to-do lists
├── /notes.txt       -> Stores persistent user notes and snippets
├── /skills/         -> Directory containing user-taught DSL micro-skills
└── /sentiment.log   -> Rolling log of interaction sentiment history
```

---

## System Health, Telemetry & Diagnostics

Executing `/diag` triggers a deep hardware diagnostics scan that reads physical silicon sensors and memory allocation pools:

```
==================================================
📊 ESP32-S3 AI ASSISTANT DIAGNOSTIC TELEMETRY
==================================================
• System Version    : v1.7.8-GROQ
• Chip Revision     : ESP32-S3 (v0.2)
• CPU Frequency     : 240 MHz (Active) / 80 MHz (Idle)
• Internal CPU Temp : 41.2 °C
• Free Heap Memory  : 284,120 Bytes
• SRAM Drift Rate   : -0.02 KB/hr (Stable)
• PSRAM Status      : 8,388,608 Bytes Total / 7,921,040 Free
• Flash FFat Free   : 9,214,400 Bytes / 9,900,000 Total
• WiFi RSSI Signal  : -58 dBm (Excellent)
• Uptime            : 4 Days, 12 Hours, 34 Mins
• HTTP Error Rate   : 0.00% (0 errors / 421 calls)
==================================================
```

---

## GitHub Over-The-Air (OTA) Updates

Firmware updates are checked and applied directly from GitHub Releases using HTTPS TLS verification:

1. **Check for Updates**: Issue `/update` in the Serial Monitor. The ESP32-S3 connects to GitHub's REST API and compares local version `v1.7.8` with the latest release tag.
2. **Install Update**: Issue `/install`. The device streams the pre-compiled `.bin` binary (`ESP32-S3-Ai-Assistant-v1.7.8.bin`), writes to the secondary OTA app partition (`app1`), verifies MD5 checksum, and reboots automatically into the new release.

---

## Dual-Core Task & Memory Management

To maintain continuous UI animations and avoid watchdog timer (`WDT`) resets during long HTTP socket operations, workloads are strictly partitioned between the two physical cores:

- **Core 0 (Network & Inference Worker)**: Dedicated exclusively to high-latency blocking network tasks, TLS handshakes, SSE streaming parsers, and external API requests.
- **Core 1 (Application & System Core)**: Runs the main Arduino `loop()`, user Serial interface, intent parsing, NeoPixel animation ticks, real-time clock checks, and watchdog timer feeding.

---

## Status LED Visual Guide

The single WS2812B RGB NeoPixel communicates system state instantly through visual patterns:

```
🟢 Slow Green Pulse     ──► System Ready / Idle State
🔵 Fast Blue Breathing  ──► Communicating with Groq / Waiting for AI
🟡 Solid Yellow         ──► Wi-Fi Connection Loss / Reconnecting
🩵 Teal Flash           ──► GitHub OTA Update Downloading
✨ Rainbow Pulse        ──► Positive Mood Streak Detected
💙 Soft Sapphire Glow   ──► Empathetic / Supportive Response Mode
💡 White Flash          ──► Proactive Assistant Nudge / Hourly Alarm
🔴 Red Double Flash     ──► System Error / Invalid API Key
```

---

## Troubleshooting & FAQ

### 1. The device fails to mount FFat storage on first boot.
- **Cause**: The flash filesystem is unformatted or partition sizes match standard 4MB layouts.
- **Solution**: In Arduino IDE, ensure **Partition Scheme** is set to `16M Flash (3MB APP/9.9MB FATFS)`. The firmware will automatically format FFat on first boot.

### 2. Compilation Error: `FFat.h: No such file or directory` or PSRAM errors.
- **Cause**: Incorrect board core selected.
- **Solution**: Go to **Tools → Board → ESP32 Arduino** and ensure you have selected **ESP32S3 Dev Module**.

### 3. Serial Monitor displays garbage characters.
- **Cause**: Baud rate mismatch or USB CDC flag disabled.
- **Solution**: Set Serial Monitor speed to `115200 baud`. Ensure **USB CDC On Boot** is set to `Enabled` in the Tools menu.

### 4. HTTP Error 429 during AI queries.
- **Cause**: Rate limit exceeded on Groq or Serper free tier API keys.
- **Solution**: Wait 60 seconds before issuing new requests or verify your key status on the Groq Console dashboard.

---

## Technical Specifications & Limits

| Operational Boundary | Maximum System Threshold |
|---|---|
| Conversation Context Window | 30 Messages / ~3,000 Tokens (Auto-summarized) |
| Long-term Memory Facts | 80 Dedicated Key-Value Entries |
| Reminders & Task Items | 30 Active Scheduled Items |
| Custom Learned Skills | 20 DSL Micro-Scripts |
| HTTP Socket Timeout | 35 Seconds (12s max for function dispatch) |
| Hardware Watchdog Timeout | 40 Seconds Hard Reset Boundary |

---

## Security Considerations

- **API Key Protection**: API credentials inside `Config` are compiled into device flash memory. Never publish unencrypted source code containing live keys to public repositories.
- **TLS Channel Security**: All outbound connections to Groq, Gemini, Serper, Meteosource, and GitHub utilize HTTPS / TLS 1.2 transport layer encryption.
- **Sandboxed Execution**: Custom micro-skills run exclusively within the internal DSL interpreter, preventing arbitrary remote code execution (RCE).

---

## Development & Contributions

Contributions, bug reports, and feature requests are welcome!

1. **Fork the Repository**: Create your own feature branch.
2. **Commit Changes**: Follow clear commit messaging guidelines.
3. **Submit Pull Request**: Open a PR targeting the `main` branch.

---

## License

This project is licensed under the **MIT License**. You are free to use, modify, and distribute this software for personal or commercial projects.

---

## Credits & Acknowledgments

- **Creator & Lead Architect**: **ItzCoding**
- **AI Models**: Groq Cloud (`llama-3.3-70b-versatile`) & Google AI Studio (`gemini-2.5-flash`)
- **Web APIs**: Serper.dev Search Engine & Meteosource Weather
- **Frameworks**: Espressif Systems Arduino ESP32 Core

---
*ESP32-S3 AI Assistant v1.7.8 · Designed & Developed by ItzCoding · Powered by Arduino & Edge AI*
