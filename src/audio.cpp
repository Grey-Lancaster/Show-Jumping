// Show jumping sounds, synthesised at 16 kHz in audioMix: hoofbeats on sand, the hollow clunk of a knocked pole,
// the judge's bell and the crowd's applause. The CYD HAL also pulls a 44.1 kHz ambience stream (audioBubbles44);
// there is no ambience recording here, so that stream is silent. Volume / mute are applied to everything.
#include "audio.h"
#include "board.h"
#include "hal.h"
#include <string.h>

// Sine by table lookup (x in cycles): sinf per sample eats the CYD's core and the DAC output crackles.
static int16_t g_sinT[512];
static float sinC(float x) {
  if (!g_sinT[1]) for (int i = 0; i < 512; i++) g_sinT[i] = (int16_t)(sinf(6.2831853f * i / 512.0f) * 32000.0f);
  x -= (int)x; if (x < 0) x += 1.0f;
  return g_sinT[(int)(x * 512.0f) & 511] * (1.0f / 32000.0f);
}

static uint32_t rndState = 12345;
static inline float noise() {
  rndState = rndState * 1664525u + 1013904223u;
  return ((int32_t)(rndState >> 16) - 32768) / 32768.0f;
}

#define SR 16000
#define IDLE 0xFFFFFFFF

// ---- hoofbeats: a soft low thump with a little grit (the sand). Four voices so a canter's beats can overlap.
struct Hoof { volatile uint32_t pos; float loud; };
static Hoof hooves[4] = {{IDLE, 0}, {IDLE, 0}, {IDLE, 0}, {IDLE, 0}};
void audioHoof(float loud) {
  for (auto &h : hooves) if (h.pos == IDLE) { h.loud = loud; h.pos = 0; return; }
}
static float hoofSample(uint32_t n) {
  const float t = n / (float)SR;
  return expf(-t * 70.0f) * (0.8f * sinC(95.0f * t) + 0.35f * noise() * expf(-t * 160.0f));
}

// ---- a knocked pole: a wooden clunk on the cups, then the pole bouncing twice on the sand
static volatile uint32_t knockPos = IDLE;
void audioKnock() { knockPos = 0; }
static float knockSample(uint32_t n) {
  const float t = n / (float)SR;
  float v = expf(-t * 35.0f) * (0.6f * sinC(410.0f * t) + 0.35f * sinC(655.0f * t) + 0.25f * noise() * expf(-t * 200.0f));
  static const float B[2] = {0.28f, 0.46f};
  for (int k = 0; k < 2; k++) {
    const float u = t - B[k];
    if (u > 0) v += expf(-u * 45.0f) * (0.45f / (k + 1)) * (sinC(300.0f * u) + 0.4f * noise());
  }
  return v;
}

// ---- the start bell: two strokes of a small hand bell
static volatile uint32_t bellPos = IDLE;
void audioBell() { bellPos = 0; }
static float bellSample(uint32_t n) {
  const float t = n / (float)SR;
  float v = 0;
  for (int k = 0; k < 2; k++) {
    const float u = t - k * 0.45f;
    if (u <= 0) continue;
    v += expf(-u * 4.5f) * min(1.0f, u / 0.002f) * (0.55f * sinC(1175.0f * u) + 0.25f * sinC(2960.0f * u) + 0.12f * sinC(4110.0f * u));
  }
  return v;
}

// ---- applause: many short claps at random times, swelling in and dying away
static volatile uint32_t clapPos = IDLE, clapLen = 0;
void audioApplause(float secs) { clapLen = (uint32_t)(secs * SR); clapPos = 0; }
static float clapEnv[6];      // the claps currently sounding: remaining loudness
static float applauseSample(uint32_t n, uint32_t len) {
  const float t = n / (float)len;
  const float env = min(1.0f, t * 6.0f) * min(1.0f, (1.0f - t) * 2.5f);
  rndState = rndState * 1664525u + 1013904223u;
  if ((rndState >> 20) < 4096u * 90u / SR) {                        // about 90 claps a second
    for (auto &c : clapEnv) if (c < 0.02f) { c = 0.5f + (rndState & 255) / 512.0f; break; }
  }
  float v = 0;
  for (auto &c : clapEnv) if (c > 0.02f) { v += c * noise(); c *= 0.988f; }   // each clap rings ~5 ms
  return v * env * 0.45f;
}

// ---- the tractor: a little diesel putt-putt, a low thump about 13 times a second
static volatile bool engineOn = false;
void audioEngine(bool on) { engineOn = on; }
static float engineSample(uint32_t n) {
  const uint32_t per = SR / 13, k = n % per;
  const float t = k / (float)SR;
  return expf(-t * 55.0f) * (0.55f * sinC(62.0f * t) + 0.25f * noise() * expf(-t * 120.0f)) * 0.5f;
}
static uint32_t enginePos = 0;

static inline void add(int32_t &acc, float v) { acc += (int32_t)(v * 20000.0f); }

void audioMix(int16_t *out, int n) {
  const bool on = g_snd && !g_asleep && !g_isNight;
  for (int i = 0; i < n; i++) {
    int32_t acc = 0;
    for (auto &h : hooves) {
      if (h.pos == IDLE) continue;
      add(acc, hoofSample(h.pos) * h.loud);
      if (++h.pos > SR / 10) h.pos = IDLE;
    }
    if (knockPos != IDLE) { add(acc, knockSample(knockPos)); if (++knockPos > SR * 7 / 10) knockPos = IDLE; }
    if (bellPos != IDLE) { add(acc, bellSample(bellPos)); if (++bellPos > SR * 3 / 2) bellPos = IDLE; }
    if (clapPos != IDLE) { add(acc, applauseSample(clapPos, clapLen)); if (++clapPos >= clapLen) clapPos = IDLE; }
    if (engineOn) add(acc, engineSample(enginePos++));
    acc = on ? (acc * (int32_t)g_vol) / 60 : 0;
    out[i] = (int16_t)constrain(acc, -32768, 32767);
  }
}

#ifdef BOARD_CYD28
void audioBubbles44(int16_t *out, int n) { memset(out, 0, n * sizeof(int16_t)); }
#endif
