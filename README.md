# Show Jumping

An animated show jumping arena for the **CYD 4.0"** (Freenove ESP32 Display 4.0", ESP32-32E, ST7796 480x320, resistive touch). Built the same way as Deer Hunt and the Fish Tank: one 480x320 virtual world, drawn procedurally in 32-row strips over a scenery cache kept in flash (the board has no PSRAM).

## Easiest way: the web installer

### [➡ Open the Web Installer](https://grey-lancaster.github.io/Show-Jumping/)

Use **Chrome or Edge on a computer**, plug the board in by USB, click **Connect**, then **Flash**. The first start asks you to calibrate the touch screen (five crosshairs) and then to pick your WiFi on the screen.

Prefer to download files? The [latest release](../../releases/latest) has a factory file (a new board) and an update file (a board that is already running; keeps WiFi and settings).

## What it does
- A horse and rider canter round a **course of six fences** in front of a packed grandstand: the near line left to right (fences 1-3: a vertical, an oxer, a wall), then the far line right to left (4-6).
- Every round is **timed**; a knocked rail falls to the sand and costs **4 faults**. At the finish the scoreboard shows *Clear round!* or the faults, the crowd claps, and the next of eight riders comes in. The leader is shown top-right.
- **Tap the screen to ride yourself.** A tap between rounds makes the next round yours; during a round every tap makes the horse take off right there, so time it before each fence (too early or too late and the rail comes down; no tap and he goes straight through it).
- Sounds, all synthesised: canter hoofbeats, the start bell, knocked poles, applause.
- **WiFi is set up on the screen** with [TouchWifiProvisioner](https://github.com/Grey-Lancaster/TouchWifiProvisioner) (BOOT skips it). Web page at `http://showjump40.local`: leaderboard, next rider, sound and volume, flip screen, swap red/blue, invert colours, screenshot, recalibrate touch, hostname, forget WiFi, restart.
- **Firmware update page** at `/update` ([ElegantOTA](https://github.com/ayushsharma82/ElegantOTA)) and ArduinoOTA. BOOT shows the connection info.

## Builds
| Env | Board |
|---|---|
| `cyd40` (default) | Freenove ESP32 Display 4.0" (ESP32-32E, "FNK0103S" docs), ST7796 480x320, resistive touch, no PSRAM |

`pio run -e cyd40`. Pushing a tag like `v0.1.0` makes CI build it, publish a release with a factory and an update file, and refresh the web installer in `docs/` (GitHub Pages serves `docs/` from `main`).

## Layout
- `src/main.cpp` - arena, course, horse and rider, rounds, scoreboard, web page.
- `src/audio.cpp` - the synthesised sounds.
- `src/hal_cyd28.cpp`, `src/wifi_setup.cpp`, `src/canvas.h` - the board layer from Deer Hunt.
- `docs/` - the web installer. `.github/` - CI, release notes and the installer refresh.
