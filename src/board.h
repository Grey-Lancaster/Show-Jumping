// Board selection. Show Jumping. Build env: cyd40 (default; the only one so far).
#pragma once

#if defined(BOARD_CYD40)
  // Freenove ESP32 Display 4.0" (ESP32-32E, no PSRAM): ST7796 480x320, XPT2046 resistive touch. CYD code, 4" panel.
  #define DEFAULT_SWAPRB false
  #define BOARD_ID "cyd40"
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
#else
  #error "Show Jumping only has the cyd40 build so far"
#endif

// The arena always lives in a 480x320 "virtual" space; the Canvas maps it to the panel.
#define VW 480
#define VH 320
