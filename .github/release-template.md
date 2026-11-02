## Easiest: the web installer

### [➡ Open the Web Installer](https://grey-lancaster.github.io/Show-Jumping/)

Use **Chrome or Edge on a computer**, plug the board in by USB, pick your board in the list and click Connect, then Flash. No software to install. The files below are for updating a board that is already running, or for flashing by hand.

## Board

| Build | Board | Display |
| --- | --- | --- |
| `cyd40` | **Freenove ESP32 Display 4.0"** (ESP32-32E, "FNK0103S" docs) | ST7796 480×320, resistive touch, speaker, no PSRAM |

Colours wrong? The board's web page has **Swap red/blue** and **Invert colours** switches. Nothing is damaged.

## Which file do I want?
| File | Use it to | WiFi and settings |
| --- | --- | --- |
| `Show-Jumping-<build>-factory.bin` | install on a new board, or recover one | **wiped** (you set up WiFi again on the screen) |
| `Show-Jumping-<build>-update.bin` | update a board that is already running Show Jumping | **kept** |

**Factory file:** flash it at offset 0, for example
`esptool.py --chip esp32 write_flash 0x0 Show-Jumping-cyd40-factory.bin`, then press RESET.

**Update file:** open the board's web page (`http://showjump40.local`), press **Firmware update**, choose the file and let it upload; it restarts by itself. From a computer: `espota.py -i <ip> -p 3232 -f Show-Jumping-cyd40-update.bin -r`.
