// Web demo: the board layer for the browser. The page (docs/demo/index.html) calls setup() once through
// demo_init, then demo_frame every animation frame (the firmware's loop()), pulls sound through demo_audio and
// passes clicks in through demo_touch. src/main.cpp and src/audio.cpp are compiled unchanged.
#include <emscripten/emscripten.h>
#include "Arduino.h"
#include "TFT_eSPI.h"
#include "web_stubs.h"
#include "../src/board.h"
#include "../src/hal.h"
#include "../src/audio.h"
#include "../src/wifi_setup.h"

#include "fonts/glcdfont.c"
#include "fonts/Font16.c"
#include "fonts/Font32rle.c"
static const unsigned char *const glcd = font;   // the 5x7 font (its array is called "font")
#define chr_hgt_f16 16
#define chr_hgt_f32 26

SerialPort Serial;
EspClass ESP;
WiFiClass WiFi;
ArduinoOTAClass ArduinoOTA;
MDNSClass MDNS;
ElegantOTAClass ElegantOTA;

// The firmware's clock is moved on by the page (demo_frame), in steps of at most 50 ms: a browser that slows a
// background tab down then runs several steps per paint instead of the round crawling (loop() caps dt at 0.1 s).
static double g_now = 1;
uint32_t millis() { return (uint32_t)g_now; }
uint32_t micros() { return (uint32_t)(g_now * 1000.0); }
static uint32_t rs = 1;
uint32_t esp_random() { return (uint32_t)(emscripten_random() * 4294967295.0); }
void randomSeed(unsigned long s) { rs = s ? s : 1; }
static uint32_t nextRand() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
long random(long n) { return n > 0 ? (long)(nextRand() % (uint32_t)n) : 0; }
long random(long lo, long hi) { return hi > lo ? lo + random(hi - lo) : lo; }
void EspClass::restart() { EM_ASM(location.reload()); }

// ---------------------------------------------------------------- text (TFT_eSPI fonts 1, 2 and 4)
static int charW(char c, int font) {
  const int i = (c > 31 && c != 127) ? c - 32 : 0;   // char is signed: 128+ is negative
  if (font == 2) return widtbl_f16[i];
  if (font == 4) return widtbl_f32[i];
  return 6;
}

int16_t TFT_eSprite::textWidth(const char *s, int font) {
  int w = 0;
  while (*s) w += charW(*s++, font);
  return w;
}

int16_t TFT_eSprite::drawString(const char *s, int32_t x, int32_t y, int font) {
  const int w = textWidth(s, font), h = font == 2 ? chr_hgt_f16 : font == 4 ? chr_hgt_f32 : 8;
  if (datum == TC_DATUM || datum == MC_DATUM || datum == BC_DATUM) x -= w / 2;
  if (datum == TR_DATUM || datum == MR_DATUM || datum == BR_DATUM) x -= w;
  if (datum == ML_DATUM || datum == MC_DATUM || datum == MR_DATUM) y -= h / 2;
  if (datum >= BL_DATUM) y -= h;
  for (; *s; s++) {
    const unsigned char c = (unsigned char)*s;
    if (font == 2 || font == 4) {
      if (c < 32 || c > 127) continue;
      const int cw = charW(c, font);
      if (font == 2) {                       // rows of (width + 6) / 8 bytes, MSB first
        const unsigned char *d = chrtbl_f16[c - 32];
        const int bpr = (cw + 6) / 8;
        for (int r = 0; r < chr_hgt_f16; r++)
          for (int k = 0; k < bpr; k++) {
            const unsigned char line = d[bpr * r + k];
            for (int b = 0; b < 8; b++) if (line & (0x80 >> b)) drawPixel(x + k * 8 + b, y + r, tc);
          }
      } else {                               // run-length: top bit set = that many + 1 ink pixels, else gap
        const unsigned char *d = chrtbl_f32[c - 32];
        int pc = 0;
        const int total = cw * chr_hgt_f32;
        while (pc < total) {
          unsigned char line = *d++;
          if (line & 0x80) {
            for (int n = (line & 0x7F) + 1; n--; pc++) drawPixel(x + pc % cw, y + pc / cw, tc);
          } else pc += line + 1;
        }
      }
      x += cw;
    } else {                                 // GLCD 5x7 in a 6x8 cell, one byte per column, LSB on top
      for (int i = 0; i < 5; i++) {
        unsigned char line = glcd[c * 5 + i];
        for (int j = 0; j < 8; j++, line >>= 1) if (line & 1) drawPixel(x + i, y + j, tc);
      }
      x += 6;
    }
  }
  return w;
}

// ---------------------------------------------------------------- the frame, touch and sound
static uint16_t *g_shown = nullptr;
void TFT_eSprite::pushSprite(int, int) { g_shown = (uint16_t *)getPointer(); }

static bool g_down = false;
static int g_tx = 0, g_ty = 0;

void halDisplayBegin() {}
void halApplyRotation() {}
void halApplyColors() {}
void halTouchBegin() {}
bool halTouch(int &x, int &y) { if (g_down) { x = g_tx; y = g_ty; } return g_down; }
bool halTouchCal(float[6]) { return false; }
void halRecalibrateTouch() {}
void halAudioStart() {}
void halApplySound() {}
void halBacklight(bool) {}

void wifiSetupBegin(const char *, std::function<void(const String &ip)> onConnected) { onConnected("browser"); }
void wifiSetupLoop() {}
void wifiForgetAll() {}
bool wifiLangEs() { return false; }
void wifiSetLangEs(bool) {}

extern WebServer server;
void setup();
void loop();

extern "C" {
EMSCRIPTEN_KEEPALIVE void demo_init() { setup(); }
EMSCRIPTEN_KEEPALIVE void demo_frame(double ms) {   // run the arena for ms of real time, then the frame is ready
  if (ms > 1000) ms = 1000;                         // back from a long sleep: don't replay it all
  do { const double step = ms > 50 ? 50 : ms; g_now += step; ms -= step; loop(); } while (ms > 0);
}
EMSCRIPTEN_KEEPALIVE uint16_t *demo_pixels() { return g_shown; }
EMSCRIPTEN_KEEPALIVE int demo_width() { return PANEL_W; }
EMSCRIPTEN_KEEPALIVE int demo_height() { return PANEL_H; }
EMSCRIPTEN_KEEPALIVE void demo_touch(int down, int x, int y) { g_down = down != 0; g_tx = x; g_ty = y; }

static int16_t g_audio[4096];
EMSCRIPTEN_KEEPALIVE int16_t *demo_audio(int n) {   // n mono samples at 16 kHz
  if (n > 4096) n = 4096;
  audioMix(g_audio, n);
  return g_audio;
}

// The firmware's own routes: demo_api("/api/rake"), demo_api("/api/force?what=fall") ... returns the reply.
EMSCRIPTEN_KEEPALIVE const char *demo_api(const char *url) {
  std::string u(url), path = u, q;
  const size_t qm = u.find('?');
  if (qm != std::string::npos) { path = u.substr(0, qm); q = u.substr(qm + 1); }
  server.args.clear();
  size_t p = 0;
  while (p < q.size()) {
    size_t e = q.find('&', p); if (e == std::string::npos) e = q.size();
    const std::string kv = q.substr(p, e - p);
    const size_t eq = kv.find('=');
    std::string k = kv.substr(0, eq), v = eq == std::string::npos ? "" : kv.substr(eq + 1), dv;
    for (size_t i = 0; i < v.size(); i++) {      // %xx and + decoding
      if (v[i] == '%' && i + 2 < v.size()) { dv += (char)strtol(v.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
      else dv += v[i] == '+' ? ' ' : v[i];
    }
    server.args[k] = String(dv);
    p = e + 1;
  }
  auto r = server.routes.find(path);
  server.reply = String("{\"error\":\"no such route\"}");
  if (r != server.routes.end()) r->second();
  return server.reply.c_str();
}
}
