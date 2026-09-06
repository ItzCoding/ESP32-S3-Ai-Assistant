# Board setup

The firmware detects PSRAM at runtime. It uses the full conversation profile when
PSRAM is available and a smaller, safer profile when it is not.

## ESP32-S3-N16R8 / N16RB

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: choose a 16 MB scheme that includes OTA and FATFS
- USB CDC On Boot: `Enabled` when using the native USB serial port

## ESP32-S3 without PSRAM

- Select the correct flash size for the board.
- PSRAM: `Disabled`
- Choose a partition scheme that includes FATFS if persistent memory is wanted.

## Original ESP32

- Select the board's real model and flash size.
- Select PSRAM only if that exact module has it.
- For a 4 MB module, the full firmware needs a 2 MB app slot. Use `No FS 4MB
  (2MB APP x2)` to keep OTA, or `No OTA (2MB APP/2MB FATFS)` to keep persistent
  chat/memory. A 4 MB chip cannot fit two 2 MB OTA slots plus a filesystem.
- On 8 MB or 16 MB modules, choose a matching scheme with both OTA and FATFS.

There is deliberately no sketch-level `partitions.csv`: Arduino applies such a
file to every target, and a forced 16 MB table makes ordinary 4 MB ESP32 boards
unsafe or unbootable. The partition scheme must match the physical flash.
