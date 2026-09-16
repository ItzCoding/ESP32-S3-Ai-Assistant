# ESP32-S3 AI Assistant v1.8.1

Version 1.8.1 is the largest usability and reliability update to the ESP32-S3 AI Assistant so far. It targets ESP32-S3 N16R8/N16RB boards with 16 MB flash and 8 MB OPI PSRAM while retaining adaptive support for lower-memory ESP32 configurations.

## Highlights

- Automatic multi-model routing with fast, smart, and automatic modes.
- Ordered OpenRouter model fallbacks for temporary model or provider failures.
- Natural editing for task names, priorities, and due dates.
- Natural editing and rescheduling for reminders.
- Persistent web-search caching with separate current and general-information expiry periods.
- Temporary memories that expire after a duration or at a specified date and time.
- Persistent API usage statistics, latency, token estimates, response repairs, and cache hits.
- Firmware changelog display using GitHub release notes.
- Numbered, citation-ready search evidence with stronger prompt-injection boundaries.
- One controlled automatic repair attempt for malformed AI responses.
- Safer OTA origin, filename, image-size, and inactive-partition validation.
- Cleaner Serial Monitor prompts, startup status, help, and one-based numbering.
- Verified JSON writes, backup recovery, and pending-save retries.

## Important fixes

- Invalid reminder numbers no longer cancel the final reminder.
- Impossible calendar dates are rejected instead of silently rolling into another month.
- Word-based durations such as “two days” are parsed correctly.
- Persisted cache entries are not treated as fresh before NTP synchronization.
- User search queries cannot break the untrusted-search delimiter.
- OTA selects the application image instead of bootloader or partition helper binaries.

## Download

Download `ESP32-S3-AI-Assistant-v1.8.1.zip`. It contains:

- The complete modular Arduino source
- `ESP32-S3-AI-Assistant-v1.8.1.ino`
- All numbered `.h` modules
- `partitions.csv`
- `BOARD_SETUP.md`
- Repository `README.md` and `LICENSE`
- GitHub release workflow source
- The flashable `ESP32-S3-AI-Assistant-v1.8.1.bin`

The public source and compiled firmware contain Wi-Fi and API credential placeholders only. Configure credentials in `01_config.h` before compiling, or save them later through the Serial Monitor `/wifi` and `/keys` commands.

This release intentionally provides the firmware inside the complete ZIP and does not expose a separate `.bin` asset. Extract the `.bin` before manual flashing. The current on-device OTA installer expects a standalone `.bin` release asset, so it cannot install this ZIP directly.

## Verified build

- Board: ESP32S3 Dev Module
- Flash: 16 MB
- PSRAM: OPI PSRAM
- USB CDC on boot: enabled
- ESP32 Arduino Core: 3.3.11
- Application size: approximately 1.34 MB

Created by **Sethun Vithanawasam (ItzFlameG)**.
