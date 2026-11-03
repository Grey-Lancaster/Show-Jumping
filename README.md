# Show Jumping

An animated show jumping arena for the **CYD 4.0"** (Freenove ESP32 Display 4.0", ESP32-32E, ST7796 480x320, resistive touch). Built the same way as Deer Hunt and the Fish Tank: one 480x320 virtual world, drawn procedurally in 32-row strips over a scenery cache kept in flash (the board has no PSRAM).

## Easiest way: the web installer

### [➡ Open the Web Installer](https://grey-lancaster.github.io/Show-Jumping/)

Use **Chrome or Edge on a computer**, plug the board in by USB, click **Connect**, then **Flash**. The first start asks you to calibrate the touch screen (five crosshairs) and then to pick your WiFi on the screen.

Prefer to download files? The [latest release](../../releases/latest) has a factory file (a new board) and an update file (a board that is already running; keeps WiFi and settings).

## What it does
- **The course:** a horse and rider canter round **five fences** in front of a packed grandstand: the near line left to right (a vertical and an oxer), a U-turn at the right end of the ring, then the far line right to left (a wall, a vertical with a **water tray** under it, and an oxer).
- **Ten riders and horses** take turns (bay, chestnut, grey and black coats), each horse with its own pace, so round times differ.
- **Scoring:** every round is timed. A knocked rail falls to the sand and costs **4 faults**. At the finish a banner over the stands shows *Clear round!* or the faults, and the leader is shown top-right (the web page keeps a top-10 leaderboard).
- **Refusals:** now and then a horse props to a stop in front of a fence, the rider turns him away, loops back round and tries again (4 faults; the *Refusal +4* box comes up 1.5 s after he stops). About 1 auto round in 10 has one refusal, and 1 in 20 refuses twice at the same fence and is **eliminated** (`REFUSE_ONE`, `REFUSE_TWO` in `src/main.cpp`).
- **Falls:** every 9 rounds (`FALL_EVERY`, every round for now while you watch it) the horse digs in at a random fence and the rider goes over his head, bringing the top rail down. He gets up to applause, gathers the reins and leads the horse out of the ring on foot by the clear track at the edge (no fences in the way). Eliminated.
- **The crowd** claps at the end of every round, and after a clear round the whole stand **jumps up and down**.
- **Tap the screen to ride yourself.** A tap between rounds makes the next round yours; during a round every tap makes the horse take off right there, so time it before each fence (too early or too late and the rail comes down). Miss a take-off and he refuses; a second refusal is elimination.
- **Raking the ring:** every 5 rounds (`TRACTOR_EVERY`) a little red tractor drags a rake round the ring, leaving combed stripes in the sand. The web page has a *Rake the ring* button to send it out after the current round.
- **Sponsors:** boards round the ring for Legends Grill, Rogers Diner, Silver Spoon and Campagna Italian Cuisine. While the tractor is out a sponsor board goes up over the stands, taking turns: *Visit Legends Grill*, *Visit Rogers Diner*, *Visit the Silver Spoon*, *Visit Campagna Italian Cuisine*.
- **Sounds,** all synthesised: canter hoofbeats, the start bell, knocked poles, applause, the tractor putt-putting.
- **Names:** the web page has a *Riders and horses* list where all ten rider and horse names can be changed (kept after a restart).
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
