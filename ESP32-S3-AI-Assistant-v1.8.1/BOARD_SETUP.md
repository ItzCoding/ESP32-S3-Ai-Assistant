# ESP32-S3 N16R8 setup

This sketch targets **16 MB flash and 8 MB OPI PSRAM**. OpenRouter chat and
skill generation, Serper search, Meteosource weather, and GitHub OTA are retained,
including runtime credential settings.

Arduino IDE settings:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `Custom` (use the included `partitions.csv`)
- Flash Mode: match the module flash; OPI PSRAM does not imply OPI flash.
- USB CDC On Boot: `Enabled` for native USB serial
- Serial Monitor: `115200`, newline enabled

The included `partitions.csv` overrides the menu partition table: two 6 MiB OTA
slots and a 3.9375 MiB FAT filesystem, ending exactly at 16 MiB. **Do not upload
this table to smaller flash chips.** Other boards require replacing/removing the
table and selecting matching settings. Layout changes can invalidate stored data.

Dependencies: ESP32 Arduino core, ArduinoJson 7, NTPClient, Time, Adafruit NeoPixel.
The locally installed ESP32 core is 3.3.11.

## New commands and reliability improvements

- `/health` or `/storage`: flash, PSRAM, internal heap, storage and clock status.
- `/save`: flush changes and report whether anything remains unsaved.
- `/storage format CONFIRM`: explicitly erase FAT storage and save current RAM
  state. Use on fresh unformatted boards only when erasing old files is intended.
  This does not erase NVS credentials.

Mount failure no longer automatically formats storage. Offline storage means
changes remain in RAM and are lost on power-off. Saves verify temporary JSON and
retain the previous committed file as `.bak`. Startup restores missing/invalid
main files from valid backups. This reduces interrupted-write damage, but cannot
guarantee recovery from FAT metadata corruption or hardware failure. `/clear`
also removes backups and temporary files.

Large chat, API and skill JSON documents prefer PSRAM, falling back to the
available heap if necessary, leaving more internal RAM for networking.

Failed saves remain pending for the next 30-second flush, including skills.
Health output counts failed saves and recovered files.

## Natural productivity features

Normal use no longer depends on remembering slash commands. Examples:

- `Remind me to call Sam tomorrow at 6 PM` supports dated reminders; overdue
  one-time reminders are delivered after a reboot, and `snooze that reminder
  for 15 minutes` creates a persistent snooze.
- `Add a high priority task to submit the report by Friday`, `show my tasks`,
  `rename task 2 to send the final report`, and `move task 2 to Friday` use a
  persistent editable task list with priorities and due dates. Older `task_`
  memory entries migrate automatically.
- Reminders are numbered from 1 and can be edited naturally: `rename reminder
  1 to call the dentist` or `move reminder 1 to Friday at 9 AM`.
- Automatic model mode selects a fast or stronger primary model from the prompt
  and sends an ordered fallback list to OpenRouter. Say `use the fast model`,
  `use the smart model`, or `choose the model automatically`; the choice is
  saved across restarts.
- Search results are cached for 15 minutes for current queries and six hours for
  general queries. Say `clear the search cache` to remove cached results.
- Temporary facts expire automatically after NTP time is available. Example:
  `Remember my hotel is Ocean View for 3 days`.
- `Show my API usage statistics` reports persistent request, failure, latency,
  estimated token, and response-repair counts.
- Live search answers use numbered titles, URLs, dates, and citations. Search
  snippets are delimited as untrusted data and cannot override assistant rules.
- Empty, obviously truncated, or repetitive AI output gets one controlled
  repair attempt. The single-retry limit prevents loops and runaway usage.
- `Check for firmware updates`, `show the firmware changelog`, and `Install the
  update` work naturally. OTA now
  restricts assets to the configured GitHub repository, rejects helper images,
  checks the declared size against the inactive OTA partition, saves state
  before flashing, and marks an image valid only after setup completes when
  bootloader rollback support is enabled.

Scheduled reminders and automatic briefings wait for the first successful NTP
sync. Local countdowns work offline. Timers and reminders are serviced while the
main task waits for the background AI worker. Other blocking network paths can
still delay alerts. Daily briefing reset uses the date, avoiding reliance on
observing midnight; calendar checks now run on the main task.

## Board verification

1. Upload and run `/health`; expect 16 MB flash and 8 MB PSRAM.
2. Initialize fresh storage explicitly, add a fact/task/reminder, run `/save`,
   reboot, and check persistence.
3. Boot without WiFi: expect awaiting NTP, no scheduled reminders, working local
   timers. Restore WiFi and verify clock synchronization.
4. Start a short timer before an AI request and verify it fires during the wait.
5. Exercise chat, search, weather, skills and update checks with your credentials.

Compile the `.ino` source in Arduino IDE with the settings above before uploading.
Keep the compiled application below the 6 MiB OTA slot size; the IDE Custom menu
may display a larger limit.
