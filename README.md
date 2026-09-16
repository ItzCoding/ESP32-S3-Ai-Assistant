# 🤖 ESP32 AI Assistant

A modular, persistent, internet-connected AI assistant for ESP32 and ESP32-S3 boards.

Chat naturally through the Arduino Serial Monitor, remember personal information, check live weather and web results, create reminders and tasks, run timers and stopwatches, generate custom AI skills, and install firmware updates directly from GitHub.

> **Current firmware:** ESP32-AI v1.8.1  
> **Primary AI:** NVIDIA Nemotron 3.5 Lightning through OpenRouter  
> **Skill generation:** MiniMax M3 through OpenRouter  
> **Recommended board:** ESP32-S3-N16R8/N16RB with 16 MB flash and 8 MB OPI PSRAM  
> **Also supported:** ESP32-S3 and original ESP32 boards without PSRAM

---

## ✨ Highlights

- Natural conversational AI through the Serial Monitor
- Persistent long-term memory
- Reminders and task management
- Real countdown timers
- Stopwatch with lap support
- Live weather information
- Live web search
- AI-generated custom skills
- GitHub firmware updates over Wi-Fi
- Adaptive PSRAM and non-PSRAM memory profiles
- Background AI networking
- Automatic Wi-Fi reconnection
- Improved crash protection and diagnostics
- Natural follow-up questions
- Mood-aware responses
- RGB status LED feedback
- Modular Arduino codebase
- API keys use safe placeholders by default

---

## 🆕 What’s New in v1.8.1

Version 1.8.1 focuses on natural interaction, safer networking, better persistence, and a cleaner Serial Monitor experience.

### Multiple AI models

The assistant now supports three routing modes:

- **Automatic** chooses the fast or stronger primary model from the request.
- **Fast** prefers the low-latency model.
- **Smart** prefers the stronger model for code, debugging, analysis, and detailed explanations.
- Every mode supplies OpenRouter with an ordered fallback list.
- The selected mode is stored in ESP32 Preferences and survives restarts.

Natural examples:

```text
Use the fast model
Use the smart model
Choose the model automatically
Which model are you using?
```

### Editable tasks and reminders

Tasks and reminders use consistent one-based numbering and can be edited naturally.

```text
Add a high priority task to submit the report by Friday
Rename task 2 to send the final report
Move task 2 to Monday at 9 AM
Make task 2 urgent
I finished task 2

Remind me to call Sam tomorrow at 6 PM
Rename reminder 1 to call the dentist
Move reminder 1 to Friday at 9 AM
Snooze that reminder for 15 minutes
```

### Search cache and safer answers

- Current-information searches are cached for 15 minutes.
- General searches are cached for six hours.
- Cache hits avoid unnecessary Serper requests.
- Cached entries persist in FFat and expire safely after NTP synchronization.
- Search results carry numbered titles, links, dates, and excerpts.
- Search text is delimited as untrusted evidence and cannot override assistant rules.
- User queries are sanitized before being placed inside search delimiters.

### Expiring memory

Temporary facts can expire automatically:

```text
Remember my hotel is Ocean View for 3 days
Remember my parking level is 4 for two hours
Remember my appointment code is 8132 until tomorrow at 6 PM
```

Expired memories are removed after the clock synchronizes and are excluded from prompts and recall.

### API usage statistics

The firmware persistently tracks:

- AI, search, weather, and OTA request counts
- Failure counts by service
- Estimated input and output tokens
- Combined request latency
- Automatic response repairs
- Search-cache hits

Ask naturally:

```text
Show my API usage statistics
```

### Firmware changelog and safer OTA

- Release notes can be displayed from the latest GitHub release.
- OTA accepts downloads only from the configured repository release path.
- Bootloader, partition, and helper images are rejected.
- The application size is checked against the inactive OTA partition.
- Dirty state is flushed before installation.
- A successful image is marked valid after setup when rollback support is enabled.

```text
Check for firmware updates
Show the firmware changelog
Install the update
```

### Cleaner Serial Monitor

- Consistent `YOU  >` and `AI   >` labels
- Clear startup summary for firmware, model mode, PSRAM, and storage
- Natural help examples instead of a wall of commands
- Consistent one-based task and reminder numbers
- Improved error messages for invalid numbers and missing information
- Slash commands remain available for maintenance and compatibility

### Reliability fixes

- Verified temporary JSON writes with backup recovery
- Search-cache persistence and expiry handling
- Invalid dates such as February 31 are rejected
- Word durations such as “two days” parse correctly
- Invalid reminder numbers can no longer cancel the last reminder
- Empty, truncated, or repetitive AI replies receive one controlled repair attempt
- Public source and release packages contain credential placeholders only

---

## What’s New in v1.8.0

### More natural conversations

The assistant now understands requests such as:

```text
Remember my name is Sethun
What is my name?
Remember that my favourite colour is blue
What is my favourite colour?
```

Example response:

```text
🧠 Nice to meet you, Sethun! I'll remember your name. 😊
```

It can also understand follow-up questions:

```text
What's the weather in Colombo?
What about Kandy?
And Galle?
```

### Real tools instead of simulations

The firmware includes working implementations for:

- Persistent memories
- Reminders
- Tasks
- Timers
- Stopwatch and laps
- Weather requests
- Web search
- GitHub OTA updates
- AI-generated skills
- System diagnostics

### Better stability

Version 1.8.0 introduces:

- Dynamic chat-message storage
- Deferred conversation compression
- Background AI requests
- Reduced internal SRAM pressure
- Runtime PSRAM detection
- Smaller buffers on boards without PSRAM
- Safer HTTP response handling
- Automatic Wi-Fi recovery
- Watchdog-friendly processing
- Portable ESP32 task creation

---

## 🧠 AI Models

The assistant uses OpenRouter for both normal conversations and skill creation.

| Purpose | Default model |
|---|---|
| Main assistant | `nvidia/nemotron-3.5-lightning:free` |
| Skill generation | `minimax/minimax-m3:free` |

Models can be changed in `01_config.h`.

Model availability and free-tier limits are controlled by OpenRouter and may change over time.

---

## 🔌 Supported Hardware

### Recommended configuration

- ESP32-S3-N16R8 or ESP32-S3-N16RB
- 16 MB flash
- 8 MB OPI PSRAM
- USB connection
- Wi-Fi access
- Optional onboard RGB NeoPixel

### Also supported

- ESP32-S3 boards without PSRAM
- Original ESP32 boards without PSRAM
- 4 MB flash boards, with partition limitations

The firmware automatically detects PSRAM during startup.

### Memory profiles

| Profile | Chat messages | Context budget | Maximum AI output |
|---|---:|---:|---:|
| PSRAM available | 30 | 3000 characters | 1024 tokens |
| No PSRAM | 12 | 1400 characters | 512 tokens |

This allows the same firmware to run on smaller ESP32 boards while using the additional memory available on ESP32-S3-N16R8/N16RB boards.

---

## 💡 Status LED

The default status LED pin is selected automatically:

- ESP32-S3: GPIO 48
- Original ESP32: GPIO 2

Change `LED_PIN` in `01_config.h` if your board uses a different pin.

| State | LED effect |
|---|---|
| Idle | Off |
| Thinking | Blue pulse |
| Replied | Green |
| Error | Red blink |
| Alert | Orange pulse |
| Excited | Gold sparkle |
| Concerned | Pale blue |
| Proactive message | Purple pulse |
| Learning a skill | Cyan pulse |
| Evolving | Domain-based colour |

The assistant still works if your board does not have a compatible RGB LED.

---

## 📦 Required Arduino Libraries

Install these libraries using the Arduino IDE Library Manager:

- ArduinoJson
- NTPClient
- Time
- Adafruit NeoPixel

The ESP32 board package provides:

- WiFi
- WiFiClientSecure
- HTTPClient
- Preferences
- FFat
- Update
- FreeRTOS support

The GitHub build workflow currently uses:

| Dependency | Version |
|---|---|
| ESP32 Arduino Core | 3.3.11 |
| ArduinoJson | 7.4.3 |
| NTPClient | 3.2.1 |
| Time | 1.6.1 |
| Adafruit NeoPixel | 1.15.5 |

---

## ⚙️ Recommended Arduino IDE Settings

### ESP32-S3-N16R8/N16RB

Use the following settings:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | Custom, using the included `partitions.csv` |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240 MHz |
| Upload Speed | 921600 or 460800 |

The firmware may automatically reduce the CPU frequency while idle to save power.

### Original ESP32 with 4 MB flash

A 4 MB ESP32 cannot normally hold two large OTA application slots and a useful FAT filesystem at the same time.

Choose one of these options:

| Partition scheme | Result |
|---|---|
| No FS 4MB (2MB APP × 2) | GitHub OTA works, but persistent FATFS data is unavailable |
| No OTA (2MB APP/2MB FATFS) | Persistent data works, but GitHub OTA is unavailable |

For the full feature set, a board with 8 MB or 16 MB flash is recommended.

See `BOARD_SETUP.md` for detailed board-specific instructions.

---

## 🔑 Configuration

Open `01_config.h` and replace the placeholders:

```cpp
constexpr const char* SSID           = "YOUR_WIFI_SSID";
constexpr const char* PASSWORD       = "YOUR_WIFI_PASSWORD";
constexpr const char* AI_KEY         = "YOUR_OPENROUTER_API_KEY";
constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY";
constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";
constexpr const char* SKILL_KEY      = "YOUR_OPENROUTER_SKILL_API_KEY";
```

The main and skill OpenRouter keys may use the same API key.

### API services

| Service | Purpose |
|---|---|
| OpenRouter | AI conversations and skill generation |
| Meteosource | Live weather data |
| Serper | Live web search |
| GitHub | Firmware release checking and OTA downloads |

The APIs remain fully implemented. Only the private credentials have been replaced with safe placeholders.

### Previously saved credentials

Wi-Fi and API credentials can also be stored in ESP32 Preferences.

If credentials were saved on the device previously, those stored values may override the placeholders compiled into the firmware.

Use `/keys` to inspect the loaded key status without displaying complete keys.

---

## 🚀 Installation

1. Install Arduino IDE.
2. Install the Espressif ESP32 board package.
3. Install the required libraries.
4. Open `ESP32-S3-AI-Assistant-v1.8.1/ESP32-S3-AI-Assistant-v1.8.1.ino`.
5. Enter your Wi-Fi and API credentials in `01_config.h`.
6. Select the correct board, flash size, PSRAM and partition scheme.
7. Connect the ESP32 over USB.
8. Compile and upload the firmware.
9. Open Arduino Serial Monitor.
10. Set the baud rate to `115200`.
11. Set the line ending to **New Line**.
12. Wait for the ready message.

Expected startup output:

```text
🚀 ESP32-AI v1.8.1 STARTING...
✅ PSRAM buffers ready
✅ Background AI network worker ready
✅ FATFS ready
📶 Connecting...
✅ WiFi connected
✅ Time synced
✅ FreeRTOS software timers started
✅ LED status task ready
✅ OTA background task started

╭────────────────────────────────────────────╮
│  🤖 ESP32 AI Assistant is ready            │
╰────────────────────────────────────────────╯
```

---

## 💬 Natural Conversation Examples

### Personal memory

```text
Remember my name is Sethun
What is my name?

Remember that my favourite colour is blue
What is my favourite colour?
```

### Weather

```text
What's the weather in Colombo?
Weather in Kandy
What about Galle?
```

### Live search

```text
/search latest ESP32-S3 news
Who won the latest FIFA World Cup?
Did you check?
```

When a question requires recent information, the assistant can search live sources before answering.

### Reminders

```text
Remind me to drink water in 30 minutes
Remind me tomorrow to call John
What are my reminders?
```

### Tasks

```text
Add buy groceries to my tasks
Add finish the project to my task list
Show my tasks
Mark task 1 as done
```

### Timers

```text
Set a timer for 10 minutes
Start a timer for 1 hour 20 minutes
Pause the timer
Resume the timer
How much time is left?
Cancel the timer
```

Timer durations can contain seconds, minutes, hours, or multiple units. The maximum timer duration is seven days.

### Stopwatch

```text
Start the stopwatch
Lap
Stop the stopwatch
Show stopwatch status
Reset the stopwatch
```

Up to 20 stopwatch laps can be recorded.

---

## ⌨️ Serial Commands

Type `/help` to display the command list.

### System commands

| Command | Description |
|---|---|
| `/help` | Show available commands |
| `/version` | Show firmware, model and system information |
| `/diag` | Run detailed system diagnostics |
| `/time` | Show the current local date and time |
| `/summary` | Summarize the current conversation |
| `/clear` | Clear assistant data stored in FATFS and restart |
| `/reboot` | Restart the ESP32 |

### Memory commands

| Command | Description |
|---|---|
| `/memory` | List stored long-term facts |
| `/forget <key>` | Forget one stored fact |
| `/forget all` | Remove all stored facts |

Example:

```text
/forget name
```

Commands should normally be entered one at a time. For example, do not combine `/forget name` and `What is my name?` on the same line.

### Weather and search

| Command | Description |
|---|---|
| `/weather <location>` | Get live weather |
| `/search <query>` | Search live web results |

### Timers and stopwatch

| Command | Description |
|---|---|
| `/timer <duration>` | Start a countdown timer |
| `/timer status` | Show timer status |
| `/timer pause` | Pause the timer |
| `/timer resume` | Resume the timer |
| `/timer cancel` | Cancel the timer |
| `/stopwatch start` | Start the stopwatch |
| `/stopwatch stop` | Stop the stopwatch |
| `/stopwatch lap` | Record a lap |
| `/stopwatch status` | Show stopwatch information |
| `/stopwatch reset` | Reset the stopwatch |

### Tasks

| Command | Description |
|---|---|
| `/tasks` | Show saved tasks |
| `/task add <text>` | Add a task |
| `/done <number>` | Mark a task as complete |

### Skills

| Command | Description |
|---|---|
| `/skills` | List saved skills |
| `/skills teach <description>` | Generate a new skill |
| `/skills run <name>` | Run a saved skill |
| `/skills delete <name>` | Delete a skill |
| `/skills keep` | Save a tested pending skill |
| `/skills discard` | Discard a pending skill |
| `/skills retry` | Retry skill generation |

Natural `yes`, `no`, and `retry` responses are also supported during skill testing.

### Connectivity and credentials

| Command | Description |
|---|---|
| `/wifi` | Show Wi-Fi information |
| `/wifi <ssid> <password>` | Save new Wi-Fi credentials |
| `/keys` | Show safely masked API-key status |
| `/keys ai <key>` | Save the main OpenRouter key |
| `/keys skill <key>` | Save the skill-generation key |
| `/keys weather <key>` | Save the Meteosource key |
| `/keys serper <key>` | Save the Serper key |

### Firmware updates

| Command | Description |
|---|---|
| `/update` | Check GitHub for a newer firmware release |
| `/install` | Download and install the available update |

---

## 🧠 Persistent Memory

The assistant stores important information in FFat so it can survive a reboot.

Supported persistent data includes:

- Personal facts
- Chat history
- Reminders
- Tasks
- Generated skills
- Mood and personality state
- Domain experience
- Assistant settings

Current limits:

| Data type | Maximum |
|---|---:|
| Memory facts | 80 |
| Reminders | 30 |
| Chat messages with PSRAM | 30 |
| Chat messages without PSRAM | 12 |
| Saved skills | 20 |

Chat changes are flushed periodically to reduce unnecessary flash writes.

Persistent storage requires a partition scheme containing FATFS.

Timers and stopwatch sessions are runtime features and reset when the ESP32 restarts.

---

## 🧩 AI Skill Creation

The assistant can generate small custom skills from natural-language instructions.

Example:

```text
/skills teach a hydration calculator that asks how many glasses of water I drank
```

The skill system:

1. Sends the requested behaviour to MiniMax M3.
2. Receives a structured JSON skill.
3. Validates the generated operations.
4. Uses the primary AI model to review the result.
5. Attempts corrections when validation fails.
6. Runs the skill in test mode.
7. Asks whether the skill should be kept.
8. Saves approved skills to persistent storage.

### Supported skill operations

Generated skills use a safe interpreted operation system rather than compiling arbitrary C++ code.

Supported operations include:

- `set`
- `inc`
- `set_str`
- `if`
- `loop`
- `say`
- `remember`
- `recall`
- `ai`
- `end`

### Skill safety limits

| Limit | Value |
|---|---:|
| Saved skills | 20 |
| Skill-name length | 32 characters |
| Numeric variables | 12 |
| String variables | 4 |
| Maximum nesting depth | 4 |
| Operations per block | 50 |
| Maximum loop count | 50 |
| Automatic correction attempts | 2 |

Skills can calculate values, store information, recall memories, create conditional flows, repeat operations, and ask the AI for generated text.

Skills cannot install native code, create new device drivers, bypass firmware security, or directly control unsupported hardware.

---

## 🌐 Live Weather and Web Search

### Weather flow

The assistant first attempts to retrieve structured weather data from Meteosource.

If the weather API cannot find the location or returns an error, the assistant can fall back to live web-search results.

### Search flow

Serper provides current search results for questions that require recent information.

Examples include:

- News
- Current events
- Sports winners
- Recently released technology
- Live factual verification

Search results are passed to the AI as supporting context before it creates the final answer.

---

## 🔄 GitHub OTA Updates

The assistant can check GitHub Releases and display release information. Its direct installer consumes a standalone application `.bin` asset.

Configured repository:

```text
ItzCoding/ESP32-S3-Ai-Assistant
```

### Update process

1. Enter `/update`.
2. The ESP32 checks the latest GitHub release.
3. The release version is compared with `FIRMWARE_VERSION`.
4. If a newer version exists, the assistant reports it.
5. Enter `/install`.
6. The firmware binary is downloaded.
7. The update is written to the inactive OTA partition.
8. The ESP32 restarts into the new firmware.

### Important OTA requirements

- The board must already have an OTA-capable partition table.
- The initial firmware must be uploaded through USB.
- OTA cannot replace or resize the partition table.
- The GitHub repository and firmware asset must be accessible.
- The v1.8.1 release intentionally publishes one complete ZIP instead of a standalone `.bin`.
- Extract the `.bin` from that ZIP for manual flashing.
- Direct device OTA installation requires a future release to expose a standalone `.bin` asset.
- A stable Wi-Fi connection and sufficient free flash space are required.

The included GitHub Actions workflow builds firmware when a version tag beginning with `v` is published.

Example:

```text
v1.8.1
```

The release tag must match the firmware version declared in the source.

---

## 🏗️ Architecture

The firmware is separated into numbered modules to make navigation and maintenance easier.

```text
ESP32-S3-AI-Assistant-v1.8.1.ino
│
├── 01_config.h
├── 02_types.h
├── 03_globals.h
├── Network and API modules
├── Memory and persistence modules
├── Natural-language parser
├── Reminder, timer and task modules
├── Diagnostics and LED modules
├── Skill engine
└── 28_ota.h
```

### Main runtime components

- Serial input and command routing
- Natural-language intent detection
- Background AI network worker
- Long-term memory manager
- Chat-history manager
- Reminder scheduler
- FreeRTOS software timers
- Stopwatch engine
- Task manager
- Weather and search clients
- Skill interpreter
- LED status task
- OTA update task
- Wi-Fi health monitoring
- Diagnostics and temperature monitoring

Network-heavy AI requests run through a background worker so the main loop can continue handling system activity.

---

## 📊 Diagnostics

Run:

```text
/diag
```

The diagnostics screen displays:

- Firmware version
- Current AI model
- Uptime
- CPU temperature when supported
- CPU frequency
- Internal SRAM
- Total heap
- PSRAM status
- Wi-Fi connection state
- Wi-Fi reconnect count
- AI worker status
- HTTP error count
- Interaction count
- Current mood
- Reminder usage
- Memory usage
- Chat-history usage
- Skill usage
- Top experience domain

CPU-temperature reporting is available on supported ESP32-S3 configurations. Original ESP32 boards may report the sensor as unavailable.

---

## 🎭 Mood and Personality

The assistant maintains a lightweight mood state that can influence response style and AI temperature.

Possible states include:

- Neutral
- Happy
- Excited
- Concerned
- Focused
- Curious

The goal is to make the assistant feel more conversational without pretending to have real human emotions or consciousness.

---

## 🛡️ Reliability Features

The firmware includes:

- Watchdog configuration
- Dynamic request and response buffers
- Automatic PSRAM detection
- Low-memory fallback settings
- Deferred history compression
- Background AI calls
- HTTP timeout handling
- HTTP status validation
- JSON validation
- Wi-Fi reconnection
- Controlled retry behaviour
- Skill-operation limits
- Flash-write reduction
- FreeRTOS software timers
- Diagnostic health reporting

---

## 🔐 Security Notes

- Never upload real API keys to a public GitHub repository.
- Keep `01_config.h` private if it contains credentials.
- Use restricted or disposable API keys where possible.
- Rotate any key that was accidentally committed.
- The `/keys` command masks credentials in Serial Monitor output.
- Saved Wi-Fi and API credentials are stored in ESP32 Preferences.
- Anyone with physical access to the device may potentially extract stored data.
- The current HTTPS clients use certificate verification bypassing through `setInsecure()`. This simplifies embedded TLS connections but does not provide full certificate validation.

For a production deployment, certificate verification and a secure credential-provisioning process are recommended.

---

## 🧪 Testing Checklist

After uploading the firmware, test each area separately.

### Boot and hardware

- Confirm there are no restart loops.
- Confirm Wi-Fi connects.
- Confirm time synchronizes.
- Confirm PSRAM is detected correctly.
- Run `/diag`.
- Check the status LED.

### AI conversation

```text
Hello
Tell me a joke
Say that again as a pirate
```

### Memory

```text
Remember my name is Sethun
What is my name?
/memory
/forget name
```

### Live information

```text
/weather Colombo
/search latest ESP32-S3 news
```

### Timer and stopwatch

```text
/timer 10 seconds
/timer status
/stopwatch start
/stopwatch lap
/stopwatch stop
```

### Tasks and reminders

```text
/task add test the assistant
/tasks
/done 1
Remind me in 2 minutes to check the ESP32
```

### Skills

```text
/skills teach a simple number guessing game
```

Test the generated skill, then enter `yes` to save it or `no` to discard it.

### OTA

```text
/update
```

Only use `/install` when a valid newer release is available.

---

## 🧯 Troubleshooting

### The board continuously restarts

- Confirm the correct board is selected.
- Confirm the flash size.
- Confirm the PSRAM mode.
- Use OPI PSRAM for N16R8/N16RB boards.
- Use a suitable partition scheme.
- Check the Serial Monitor boot log.
- Try a lower upload speed.
- Use a stable USB cable and power source.

### The firmware is too large

Choose a partition scheme with an application partition of at least 2 MB.

### FATFS does not work

Make sure the selected partition scheme includes a FATFS partition.

Changing the partition scheme usually requires uploading the firmware through USB and may erase stored data.

### OTA fails

- Confirm the partition scheme supports OTA.
- Check Wi-Fi connectivity.
- Confirm the GitHub release exists.
- Confirm the release contains the complete ZIP and that the ZIP contains the firmware `.bin`.
- Check that the release version is newer.
- Run `/diag` and inspect the HTTP-error counter.

### The assistant reports missing API keys

Open `01_config.h` and replace the placeholder, or save the key using `/keys`.

### Weather cannot find a city

Try a more specific location:

```text
/weather Colombo, Sri Lanka
```

### Responses stop or become incomplete

- Run `/diag` and check free internal SRAM.
- Confirm PSRAM is enabled when supported.
- Restart the board.
- Check the OpenRouter account limits.
- Try a smaller or currently available model.
- Check Wi-Fi signal strength.

### Serial commands behave unexpectedly

Send one command or question per line.

For example:

```text
/forget name
```

Then send:

```text
What is my name?
```

Do not combine both instructions into one command line.

---

## 📁 Repository Files

| File | Purpose |
|---|---|
| `ESP32-S3-AI-Assistant-v1.8.1.ino` | Main Arduino sketch |
| `01_config.h` | User configuration and API placeholders |
| `03_globals.h` | Shared runtime state |
| `17_nl_parser.h` | Natural-language intent parsing |
| `27_skills.h` | AI-generated skill system |
| `28_ota.h` | GitHub OTA updater |
| `BOARD_SETUP.md` | Board and partition instructions |
| `.github/workflows/release.yml` | Automated firmware release build |

The sketch and containing directory share the v1.8.1 name required by Arduino, and the firmware reports version `1.8.1`.

---

## 🤝 Contributing

Contributions are welcome.

When submitting changes:

- Do not commit real API credentials.
- Test both PSRAM and non-PSRAM builds where possible.
- Keep original ESP32 compatibility in mind.
- Avoid long blocking operations in the main loop.
- Preserve the modular numbered-file structure.
- Update the firmware version when preparing a release.
- Confirm that the GitHub release tag matches `FIRMWARE_VERSION`.

---

## ⚠️ Project Status

This is an experimental embedded AI project.

AI responses may be incorrect, APIs may become unavailable, and free models may have rate limits or change without notice. Do not rely on the assistant for emergency, medical, legal, financial, or safety-critical decisions.

---

## 🙌 Credits

**Creator: Sethun Vithanawasam (ItzFlameG)**

Project design, firmware development, testing direction, and feature ideas by Sethun Vithanawasam.

Built with:

- Espressif ESP32
- Arduino
- OpenRouter
- NVIDIA Nemotron
- MiniMax
- Meteosource
- Serper
- ArduinoJson
- FreeRTOS

---

## ⭐ Support the Project

If you find this project useful:

- Star the repository
- Report reproducible bugs
- Suggest useful embedded-assistant features
- Share your tested board configuration
- Contribute improvements
