// Board selection. Show Jumping. Build envs: cyd40 (default), freenove4, freenove28, cyd28 and cyd28usb2 (as Deer Hunt).
#pragma once

// 2-USB CYDs (ST7789) show red and blue swapped with the stock colour order, so that build starts with the swap on.
#if defined(ST7789_DRIVER)
  #define DEFAULT_SWAPRB true
  #define BOARD_ID "cyd28usb2"
#elif defined(BOARD_CYD40)
  #define DEFAULT_SWAPRB false
  #define BOARD_ID "cyd40"
#elif defined(BOARD_CYD28)
  #define DEFAULT_SWAPRB false
  #define BOARD_ID "cyd28"
#elif defined(BOARD_FREENOVE28)
  #define DEFAULT_SWAPRB false
  #define BOARD_ID "freenove28"
#else
  #define DEFAULT_SWAPRB false
  #define BOARD_ID "freenove4"
#endif

// FONT_BIG / FONT_SMALL: the connection info. FONT_HEAD / FONT_TEXT / FONT_TINY: banners, scoreboard lines, ad boards and
// fence numbers. The arena is scaled to the panel but the built-in fonts are not, so the 320x240 boards use smaller ones.
#if defined(BOARD_CYD40)
  // Freenove ESP32 Display 4.0" (ESP32-32E, no PSRAM): ST7796 480x320, XPT2046 resistive touch. CYD code, 4" panel.
  #define BOARD_NAME "CYD 4.0"
  #define PANEL_W 480
  #define PANEL_H 320
  #define STRIP_H 32            // no PSRAM: the arena is drawn and pushed in 10 horizontal strips
  #define ROT_LEFT 3
  #define ROT_RIGHT 1
  #define DEFAULT_ROT ROT_LEFT
  #define DEFAULT_HOST "showjump40"
  #define FONT_BIG 4
  #define FONT_SMALL 2
#elif defined(BOARD_CYD28)
  #define BOARD_NAME "CYD 2.8"
  #define PANEL_W 320
  #define PANEL_H 240
  #define STRIP_H 30            // no PSRAM: drawn and pushed in 8 strips
  #define ROT_LEFT 3            // USB port on the left (assumes the USB ports are on the bottom edge in portrait)
  #define ROT_RIGHT 1
  #define DEFAULT_ROT ROT_LEFT
  #define DEFAULT_HOST "cydshowjump"
  #define FONT_BIG 2
  #define FONT_SMALL 1
#elif defined(BOARD_FREENOVE28)
  // Freenove FNK0104B/AB 2.8": ESP32-S3 N16R8 (PSRAM), ILI9341 320x240, FT6336U touch, ES8311 codec + speaker
  #define BOARD_FREENOVE_S3 1
  #define BOARD_NAME "Freenove 2.8"
  #define PANEL_W 320
  #define PANEL_H 240
  #define ROT_LEFT 3
  #define ROT_RIGHT 1
  #define DEFAULT_ROT ROT_LEFT
  #define DEFAULT_HOST "showjump28"
  #define FONT_BIG 2
  #define FONT_SMALL 1
#else
  // Freenove FNK0104S 4.0": ESP32-S3 N16R8 (PSRAM), ST7796 480x320, FT6336U touch, ES8311 codec + speaker
  #define BOARD_FREENOVE4 1
  #define BOARD_FREENOVE_S3 1
  #define BOARD_NAME "Freenove 4.0"
  #define PANEL_W 480
  #define PANEL_H 320
  #define ROT_LEFT 3            // USB port on the left
  #define ROT_RIGHT 1
  #define DEFAULT_ROT ROT_LEFT
  #define DEFAULT_HOST "showjump"
  #define FONT_BIG 4
  #define FONT_SMALL 2
#endif

#if PANEL_W >= 480
  #define FONT_HEAD 4
  #define FONT_TEXT 2
#else
  #define FONT_HEAD 2
  #define FONT_TEXT 1
#endif
#define FONT_TINY 1

// The arena always lives in a 480x320 "virtual" space; the Canvas maps it to the panel.
#define VW 480
#define VH 320
