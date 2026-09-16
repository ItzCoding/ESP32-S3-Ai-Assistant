# ESP32-S3 AI Assistant

A natural-language AI assistant for ESP32-S3 boards with persistent memory, editable tasks and reminders, live web search, weather, custom skills, and safe over-the-air firmware updates.

**Current release:** v1.8.1  
**Recommended board:** ESP32-S3 N16R8/N16RB with 16 MB flash and 8 MB OPI PSRAM  
**Interface:** Arduino Serial Monitor at 115200 baud  
**Source:** [`ESP32-S3-AI-Assistant-v1.8.1`](ESP32-S3-AI-Assistant-v1.8.1)

## What is new in v1.8.1

- Automatic multi-model routing with fast, smart, and automatic modes plus ordered OpenRouter fallbacks.
- Editable tasks and reminders using natural requests.
- Persistent search-result caching with shorter expiry for current information.
- Temporary memories that expire after a natural duration or date.
- Persistent API request, failure, latency, token-estimate, repair, and cache-hit statistics.
- Firmware changelog display from GitHub release notes.
- Improved live-search answers with numbered citations and stronger prompt-injection boundaries.
- One controlled repair attempt for empty, truncated, or repetitive AI responses.
- Safer OTA validation for repository origin, application image name, binary size, and target partition.
- A cleaner Serial Monitor interface with consistent one-based numbering and natural help.
- More reliable JSON persistence with verified temporary files and recoverable backups.
- Fixes for invalid reminder cancellation, invalid calendar dates, word-based memory durations, and cached data before NTP synchronization.

## Natural examples

```text
Remind me to call Sam tomorrow at 6 PM
Move reminder 1 to Friday at 9 AM
Rename reminder 1 to call the dentist

Add a high priority task to submit the report by Friday
Rename task 2 to send the final report
Move task 2 to Monday
I finished task 2

Remember my hotel is Ocean View for 3 days
What's the latest ESP32-S3 news?
Show my API usage statistics

Use the fast model
Use the smart model
Choose the model automatically

Check for firmware updates
Show the firmware changelog
Install the update
```

Say `help` in the Serial Monitor for more examples. Existing slash commands remain available for maintenance and compatibility.

## Required hardware and Arduino settings

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash size | 16 MB |
| PSRAM | OPI PSRAM |
| Partition table | Included `partitions.csv` |
| USB mode | Hardware CDC and JTAG |
| USB CDC on boot | Enabled |
| Serial baud | 115200 |

The included custom partition table provides two 6 MiB OTA application slots and an FFat data partition. Do not flash it to a board with less than 16 MB of flash.

Required libraries:

- ESP32 Arduino Core 3.3.11
- ArduinoJson 7.4.3
- NTPClient 3.2.1
- Time 1.6.1
- Adafruit NeoPixel 1.15.5

## Credentials

The public source and release firmware contain placeholders only. No private Wi-Fi password or API key is included.

Edit [`01_config.h`](ESP32-S3-AI-Assistant-v1.8.1/01_config.h) before compiling:

```cpp
constexpr const char* SSID           = "YOUR_WIFI_SSID";
constexpr const char* PASSWORD       = "YOUR_WIFI_PASSWORD";
constexpr const char* AI_KEY         = "YOUR_OPENROUTER_API_KEY";
constexpr const char* SKILL_KEY      = "YOUR_OPENROUTER_SKILL_API_KEY";
constexpr const char* WEATHER_KEY    = "YOUR_METEOSOURCE_API_KEY";
constexpr const char* SERPER_API_KEY = "YOUR_SERPER_API_KEY";
```

Credentials can also be entered at runtime with `/wifi` and `/keys`; they are stored in ESP32 Preferences. The AI and skill placeholders may be replaced with the same OpenRouter key.

| Service | Purpose |
|---|---|
| OpenRouter | Conversations, model fallback, and skill generation |
| Serper | Live web search |
| Meteosource | Live weather |
| GitHub | Release checks, changelogs, and OTA firmware downloads |

## Build and install

1. Download the v1.8.1 source archive or clone this repository.
2. Open `ESP32-S3-AI-Assistant-v1.8.1/ESP32-S3-AI-Assistant-v1.8.1.ino`.
3. Add credentials to `01_config.h`, or configure them later through Serial Monitor.
4. Select the board settings above.
5. Compile and upload over USB.
6. Open Serial Monitor at 115200 baud with New Line enabled.

For storage initialization and verification steps, see [`BOARD_SETUP.md`](ESP32-S3-AI-Assistant-v1.8.1/BOARD_SETUP.md).

## OTA releases

The firmware checks the configured GitHub repository for the latest release. It accepts only a suitable application `.bin` hosted under that repository's release-download path and verifies that the declared size fits the inactive OTA partition.

The GitHub Actions workflow builds the placeholder-only firmware when a tag such as `v1.8.1` is pushed. The application binary is attached to the matching GitHub release automatically.

## License

See [`LICENSE`](LICENSE).
