// Show Jumping - an animated show jumping arena for the CYD 4.0" (480x320, no PSRAM).
// A horse and rider canter round a course of six fences in front of a grandstand: the near line left to right (1-3),
// then the far line right to left (4-6). Each round is timed and a knocked rail costs 4 faults; the crowd claps at the
// end and the next rider comes in. Touch the screen to take over: every tap makes the horse take off right there, so
// time it before each fence (a tap between rounds starts your own round). Same structure as Deer Hunt: the world lives
// in a 480x320 virtual space, the CYD renders and pushes it in horizontal strips over a scenery cache kept in flash.
// WiFi is set up on the screen (TouchWifiProvisioner, BOOT skips it); online there is a web page, ArduinoOTA and
// ElegantOTA at /update. BOOT then shows the connection info (press again to close).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ElegantOTA.h>
#include "esp_partition.h"
#include "board.h"
#include "canvas.h"
#include "hal.h"
#include "wifi_setup.h"
#include "audio.h"

#define FW_VERSION "0.1.0"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite strip = TFT_eSprite(&tft);    // one horizontal strip of the panel
Canvas cv;
Preferences prefs;
WebServer server(80);

// Settings / state the HAL expects
volatile int g_vol = 60;
volatile bool g_snd = true;
volatile bool g_sBub = true, g_sDiv = false, g_sFeed = false;
int g_rot = DEFAULT_ROT;
volatile bool g_asleep = false;
volatile bool g_isNight = false;
bool g_invert = false;
bool g_swaprb = DEFAULT_SWAPRB;

String g_host = DEFAULT_HOST;      // mDNS / OTA / DHCP name
static bool g_online = false;
static uint32_t restartAt = 0, infoUntil = 0;
static float g_fps = 0;
static uint32_t g_frameNo = 0;

static void saveSettings() {
  prefs.putBool("invert", g_invert); prefs.putBool("swaprb", g_swaprb);
}

static inline uint16_t RGB(int r, int g, int b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline int lerpi(int a, int b, float t) { return a + (int)((b - a) * t); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline uint16_t mix(int r0, int g0, int b0, int r1, int g1, int b1, float t) {
  return RGB(lerpi(r0, r1, t), lerpi(g0, g1, t), lerpi(b0, b1, t));
}
static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// ---------------------------------------------------------------- the arena layout
#define SAND_Y 140                 // where the arena sand starts (below the perimeter boards)
static const float LANE_Y[2] = {282, 204};   // ground rows of the near line (left to right) and the far line (right to left)
static const int LANE_DIR[2] = {1, -1};
static float scaleAt(float y) { return 0.82f + 0.40f * (y - 204) / 78.0f; }    // far line 0.82, near line 1.22
#define START_X 30                 // timer starts here on the near line
#define FINISH_X 46                // and stops here on the far line

// ---------------------------------------------------------------- riders and horses (made up)
struct Coat { uint16_t body, shade, mane, lower; };
struct Rider { const char *name, *horse; uint8_t coat; uint16_t jacket; };
static Coat COATS[4];
static Rider RIDERS[8];
#define NRIDERS 8

static void initRiders() {
  COATS[0] = {RGB(142, 82, 42), RGB(100, 56, 28), RGB(28, 22, 18), RGB(34, 26, 20)};       // bay
  COATS[1] = {RGB(184, 102, 50), RGB(138, 70, 32), RGB(156, 76, 32), RGB(170, 92, 44)};    // chestnut
  COATS[2] = {RGB(212, 212, 206), RGB(162, 162, 158), RGB(120, 120, 120), RGB(150, 150, 146)};   // grey
  COATS[3] = {RGB(44, 38, 36), RGB(26, 24, 22), RGB(14, 14, 14), RGB(232, 230, 224)};      // black with white socks
  RIDERS[0] = {"Emma", "Thunder", 0, RGB(24, 40, 90)};
  RIDERS[1] = {"Lucas", "Biscuit", 1, RGB(20, 20, 24)};
  RIDERS[2] = {"Sofia", "Starlight", 2, RGB(150, 24, 32)};
  RIDERS[3] = {"Jack", "Maverick", 3, RGB(24, 80, 50)};
  RIDERS[4] = {"Mia", "Clover", 1, RGB(24, 40, 90)};
  RIDERS[5] = {"Oliver", "Duke", 0, RGB(110, 20, 60)};
  RIDERS[6] = {"Ava", "Silver", 2, RGB(20, 20, 24)};
  RIDERS[7] = {"Noah", "Midnight", 3, RGB(150, 24, 32)};
}

// ---------------------------------------------------------------- the course
enum FType { F_VERT, F_OXER, F_WALL };
struct Part { float off, h, fall; bool checked; };   // off: along x (local units), h: top rail height (local units)
struct Fence { int lane; float x; FType type; uint16_t c1, c2; int np; Part p[2]; bool knocked; };
static Fence fences[6];
#define NFENCE 6

static void initCourse() {
  const uint16_t W = RGB(245, 245, 240);
  fences[0] = {0, 120, F_VERT, RGB(210, 40, 40), W, 1, {{0, 36}}};
  fences[1] = {0, 252, F_OXER, RGB(40, 90, 190), W, 2, {{-8, 32}, {8, 35}}};
  fences[2] = {0, 384, F_WALL, RGB(30, 140, 70), W, 1, {{0, 38}}};
  fences[3] = {1, 370, F_OXER, RGB(240, 180, 30), RGB(40, 90, 190), 2, {{-8, 35}, {8, 32}}};
  fences[4] = {1, 244, F_VERT, RGB(130, 50, 160), W, 1, {{0, 38}}};
  fences[5] = {1, 110, F_OXER, RGB(210, 40, 40), RGB(30, 140, 70), 2, {{-8, 36}, {8, 34}}};
}

static void resetCourse() {
  for (auto &f : fences) { f.knocked = false; for (int k = 0; k < f.np; k++) { f.p[k].fall = 0; f.p[k].checked = false; } }
}

static float fenceTop(const Fence &f) { float h = 0; for (int k = 0; k < f.np; k++) h = max(h, f.p[k].h); return h; }
static float jumpLen(const Fence &f) { return f.type == F_OXER ? 130 : 112; }   // take-off to landing, local units

// ---------------------------------------------------------------- the round
enum RoundSt { R_WAIT, R_RIDE, R_DONE };
struct Horse {
  float x;            // screen x of the barrel's middle
  int lane, dir;
  float ph;           // canter phase in strides
  bool air;           // in a jump
  float jx0, jlen, jH, u;   // jump: take-off x, length (screen px), peak lift (local units), progress 0..1
  float err;          // the auto rider's take-off error for the next fence (local units)
  bool turning;       // off screen at the end of the ring, coming round to the far line
  float turnT;
};
static Horse horse;
struct Round { RoundSt st; float t; int rider; bool you; float time; bool timing, started; int faults; int nf; };
static Round rnd = {R_WAIT, 0, 0, false, 0, false, false, 0, 0};

struct Result { char name[24]; int faults; float time; };
static Result board[5];
static int nBoard = 0;
static bool lastWasLeader = false;

static const char *riderName() { return rnd.you ? "You" : RIDERS[rnd.rider].name; }

static void record() {
  Result r;
  snprintf(r.name, sizeof(r.name), "%s/%s", riderName(), RIDERS[rnd.rider].horse);
  r.faults = rnd.faults; r.time = rnd.time;
  int at = nBoard;
  for (int i = 0; i < nBoard; i++)
    if (r.faults < board[i].faults || (r.faults == board[i].faults && r.time < board[i].time)) { at = i; break; }
  lastWasLeader = at == 0;
  if (at >= 5) return;
  for (int i = min(nBoard, 4); i > at; i--) board[i] = board[i - 1];
  board[at] = r;
  if (nBoard < 5) nBoard++;
}

static float autoErr() { return (float)(random(-20, 21) + random(-20, 21) + random(-20, 21)); }   // roughly normal, sd ~20

static void startRide() {
  resetCourse();
  rnd.st = R_RIDE; rnd.t = 0; rnd.time = 0; rnd.timing = false; rnd.started = false; rnd.faults = 0; rnd.nf = 0;
  horse.x = -70; horse.lane = 0; horse.dir = 1; horse.air = false; horse.turning = false; horse.err = autoErr();
  audioBell();
}

static void nextRider() {
  rnd.rider = (rnd.rider + 1) % NRIDERS;
  rnd.you = false; rnd.st = R_WAIT; rnd.t = 0;
  resetCourse();
  horse.x = -200; horse.turning = false; horse.air = false;
}

static void startJump(float len, float peak) {
  const float s = scaleAt(LANE_Y[horse.lane]);
  horse.air = true; horse.u = 0; horse.jx0 = horse.x; horse.jlen = len * s; horse.jH = peak;
  audioHoof(0.9f);
}

static void jumpFence(const Fence &f) { startJump(jumpLen(f), fenceTop(f) + 4); }

// A tap: take off now. The jump is sized for the next fence if it is close ahead, else a little hop.
static void tapJump() {
  if (horse.air || horse.turning || horse.x < -10 || horse.x > VW + 10) return;
  rnd.you = true;
  const float s = scaleAt(LANE_Y[horse.lane]);
  if (rnd.nf < NFENCE && fences[rnd.nf].lane == horse.lane) {
    const float ahead = (fences[rnd.nf].x - horse.x) * horse.dir;
    if (ahead > -10 * s && ahead < 230 * s) { jumpFence(fences[rnd.nf]); return; }
  }
  startJump(84, 18);
}

static float liftNow() { return horse.air ? 4 * horse.jH * horse.u * (1 - horse.u) : 0; }

static void updateRound(float dt) {
  rnd.t += dt;
  for (auto &f : fences) for (int k = 0; k < f.np; k++) if (f.p[k].fall > 0) f.p[k].fall += dt;
  if (rnd.st == R_WAIT) {
    if (rnd.t > 3.5f) startRide();
    return;
  }
  if (rnd.timing) rnd.time += dt;
  if (horse.turning) {
    horse.turnT -= dt;
    if (horse.turnT <= 0) { horse.turning = false; horse.lane = 1; horse.dir = -1; horse.x = VW + 70; horse.err = autoErr(); }
  } else if (horse.x > -90 && horse.x < VW + 90) {
    const float s = scaleAt(LANE_Y[horse.lane]);
    const float v = 118 * s;
    horse.x += horse.dir * v * dt;
    const bool onScreen = horse.x > -40 && horse.x < VW + 40;
    if (horse.air) {
      horse.u += v * dt / horse.jlen;
      if (horse.u >= 1) { horse.air = false; if (onScreen) { audioHoof(1.0f); audioHoof(0.8f); } }
    } else {
      const float prev = horse.ph - floorf(horse.ph);
      horse.ph += v * dt / (92 * s);
      const float cur = horse.ph - floorf(horse.ph);
      static const float BEATS[3] = {0.02f, 0.18f, 0.31f};       // the canter's three beats
      if (onScreen && rnd.st == R_RIDE)
        for (float b : BEATS) if ((prev < b && cur >= b) || (cur < prev && (b > prev || b <= cur))) audioHoof(0.35f + 0.3f * s);
    }
    // the auto rider takes off by itself (a little early or late now and then)
    if (rnd.st == R_RIDE && !rnd.you && !horse.air && rnd.nf < NFENCE && fences[rnd.nf].lane == horse.lane) {
      const Fence &f = fences[rnd.nf];
      const float tx = f.x - horse.dir * (jumpLen(f) * 0.5f + horse.err) * s;
      if ((horse.x - tx) * horse.dir >= 0) jumpFence(f);
    }
    // passing each element: high enough, or the top rail comes down
    if (rnd.st == R_RIDE && rnd.nf < NFENCE && fences[rnd.nf].lane == horse.lane) {
      Fence &f = fences[rnd.nf];
      bool all = true;
      for (int k = 0; k < f.np; k++) {
        Part &p = f.p[k];
        if (!p.checked && (horse.x - (f.x + p.off * s)) * horse.dir >= 0) {
          p.checked = true;
          if (liftNow() < p.h - 12) {
            p.fall = 0.001f;
            audioKnock();
            if (!f.knocked) { f.knocked = true; rnd.faults += 4; }
          }
        }
        all &= p.checked;
      }
      if (all) { rnd.nf++; horse.err = autoErr(); }
    }
    if (rnd.st == R_RIDE) {
      if (!rnd.started && horse.lane == 0 && horse.x >= START_X) { rnd.started = rnd.timing = true; }
      if (horse.lane == 1 && rnd.nf >= NFENCE && horse.x <= FINISH_X && rnd.timing) {
        rnd.timing = false;
        record();
        rnd.st = R_DONE; rnd.t = 0;
        audioApplause(rnd.faults == 0 ? 4.5f : 2.0f);
      }
    }
    if (horse.lane == 0 && horse.x > VW + 70 && rnd.st == R_RIDE) { horse.turning = true; horse.turnT = 2.2f; horse.air = false; }
  }
  if (rnd.st == R_DONE && rnd.t > 8) nextRider();
}

// ---------------------------------------------------------------- drawing: fences
static uint16_t g_sandDark;

static void pole(float x0, float y0, float x1, float y1, float w, uint16_t c1, uint16_t c2) {
  for (int i = 0; i < 4; i++) {
    const float a = i / 4.0f, b = (i + 1) / 4.0f;
    cv.drawWideLine(lerpf(x0, x1, a), lerpf(y0, y1, a), lerpf(x0, x1, b), lerpf(y0, y1, b), w, (i & 1) ? c2 : c1, (i & 1) ? c2 : c1);
  }
}

// Fences stand across the line of travel; an oblique view puts the far standard up and to the right of the near one.
#define DEPTH_X 0.45f
#define DEPTH_Y 0.55f
#define HALF_W 24.0f

static void standard(float x, float y, float hs, float s, uint16_t c1, int wingSide, bool wing) {
  const uint16_t W = RGB(245, 245, 240);
  if (wing) {                          // a slanted wing panel in the fence's plane, outward from the standard
    const float ox = x + wingSide * DEPTH_X * 16 * s, oy = y - wingSide * DEPTH_Y * 16 * s;
    cv.fillTriangle(x, y, ox, oy, x, y - hs, c1);
    cv.fillTriangle(ox, oy, ox, oy - hs * 0.55f, x, y - hs, c1);
    cv.drawLine(x, y - hs, ox, oy - hs * 0.55f, W);
    cv.drawLine(ox, oy, ox, oy - hs * 0.55f, W);
  }
  cv.drawWideLine(x, y, x, y - hs, 2.6f * s, W, W);
  cv.fillRect(x - 1.3f * s, y - hs, 2.6f * s, 3 * s, c1);
}

static void drawFence(const Fence &f, bool front) {
  const float ly = LANE_Y[f.lane], s = scaleAt(ly);
  const float dx = DEPTH_X * HALF_W * s, dy = DEPTH_Y * HALF_W * s;
  const float top = fenceTop(f);
  if (!cv.visible(ly - dy - (top + 14) * s, ly + dy + 12 * s)) return;
  const uint16_t W = RGB(245, 245, 240);
  for (int k = 0; k < f.np; k++) {
    const Part &p = f.p[k];
    const float px = f.x + p.off * s;
    const float hs = (top + 8) * s;
    const bool wing = k == 0 || f.np == 1;
    if (front) {                                     // near standard (+ wing), and the number on the first element
      standard(px - dx, ly + dy, hs, s, f.c1, -1, wing);
      if (k == 0) {
        cv.fillRect(px - dx - 5 * s, ly + dy - hs - 11 * s, 10 * s, 9 * s, W);
        char n[3]; snprintf(n, sizeof(n), "%d", (int)(&f - fences) + 1);
        cv.setTextDatum(MC_DATUM); cv.setTextColor(RGB(20, 20, 20));
        cv.drawString(n, px - dx, ly + dy - hs - 6.5f * s, 1);
      }
      continue;
    }
    cv.drawWideLine(px - dx, ly + dy, px + dx, ly - dy, 1.6f * s, g_sandDark, g_sandDark);    // its shadow on the sand
    standard(px + dx, ly - dy, hs, s, f.c1, 1, wing);
    // lower rails, or the wall
    if (f.type == F_WALL) {
      const float wh = (p.h - 10) * s;
      const uint16_t brick = RGB(178, 70, 50), mortar = RGB(220, 200, 180);
      cv.fillTriangle(px - dx, ly + dy, px + dx, ly - dy, px - dx, ly + dy - wh, brick);
      cv.fillTriangle(px + dx, ly - dy, px + dx, ly - dy - wh, px - dx, ly + dy - wh, brick);
      for (int r = 1; r < 4; r++) cv.drawLine(px - dx, ly + dy - wh * r / 4, px + dx, ly - dy - wh * r / 4, mortar);
      cv.drawLine(px - dx, ly + dy - wh, px + dx, ly - dy - wh, W);
    } else {
      const int rails = f.type == F_OXER ? 1 : 2;
      for (int r = 1; r <= rails; r++) {
        const float h = p.h * r / (rails + 1) * s;
        pole(px - dx, ly + dy - h, px + dx, ly - dy - h, 3.0f * s, f.c1, f.c2);
      }
    }
    // the top rail: falls when knocked, near end first, and ends up lying on the sand
    float hn = p.h, hf = p.h, shove = 0;
    if (p.fall > 0) {
      hn = max(1.5f, p.h * (1 - p.fall / 0.30f));
      hf = max(1.5f, p.h * (1 - p.fall / 0.45f));
      shove = LANE_DIR[f.lane] * 7 * clamp01(p.fall / 0.45f);
    }
    pole(px - dx + shove * s, ly + dy - hn * s, px + dx + shove * s, ly - dy - hf * s, 3.2f * s, f.c1, f.c2);
  }
}

// ---------------------------------------------------------------- drawing: horse and rider
// Local horse units: x forward, y up from the ground under the barrel, about 70 long and 46 to the withers at scale 1.
struct HP { float x, y, s, lift, cs, sn; int d; };
static inline void hpt(const HP &p, float lx, float ly, float &X, float &Y) {
  const float dy = ly - 30;
  const float rx = lx * p.cs - dy * p.sn, ry = lx * p.sn + dy * p.cs + 30;
  X = p.x + p.d * rx * p.s; Y = p.y - (ry + p.lift) * p.s;
}
static void hCircle(const HP &p, float lx, float ly, float r, uint16_t c) { float X, Y; hpt(p, lx, ly, X, Y); cv.fillCircle(X, Y, r * p.s, c); }
static void hLine(const HP &p, float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float a, b, e, f; hpt(p, x0, y0, a, b); hpt(p, x1, y1, e, f); cv.drawWideLine(a, b, e, f, w * p.s, c, c);
}
static void hTri(const HP &p, float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
  float a, b, e, f, g, h; hpt(p, x0, y0, a, b); hpt(p, x1, y1, e, f); hpt(p, x2, y2, g, h); cv.fillTriangle(a, b, e, f, g, h, c);
}

// A leg from its joint: upper and lower segment angles from straight down (positive = forward).
static void hLeg(const HP &p, float jx, float jy, float a1, float l1, float a2, float l2, float w, uint16_t up, uint16_t low, uint16_t hoof) {
  const float kx = jx + l1 * sinf(a1), ky = jy - l1 * cosf(a1);
  const float fx = kx + l2 * sinf(a2), fy = ky - l2 * cosf(a2);
  hLine(p, jx, jy, kx, ky, w, up);
  hLine(p, kx, ky, fx, fy, w * 0.6f, low);
  hCircle(p, fx, fy + 0.5f, w * 0.4f, hoof);
}

// Canter: each leg sweeps back on the ground, then folds and swings forward.
static void gaitLeg(float q, bool front, float &a1, float &a2) {
  q -= floorf(q);
  float sweep, bend = 0;
  if (q < 0.42f) sweep = 0.5f - q / 0.42f;
  else { const float w = (q - 0.42f) / 0.58f; sweep = -0.5f + w; bend = sinf(PI * w); }
  if (front) { a1 = sweep * 0.9f + bend * 0.25f; a2 = a1 - bend * 1.5f; }
  else { a1 = sweep * 0.8f - 0.32f + bend * 0.3f; a2 = sweep * 0.8f + 0.12f - bend * 0.9f; }
}

// Over a fence: front legs tucked then reaching for the landing, hind legs trailing then folding up.
static void jumpLeg(float u, bool front, float &a1, float &a2) {
  if (front) {
    const float k = clamp01((u - 0.6f) / 0.3f);
    a1 = lerpf(1.25f, 0.55f, k); a2 = lerpf(-1.2f, 0.45f, k);
  } else {
    const float k = clamp01((u - 0.35f) / 0.35f);
    a1 = lerpf(-0.9f, 0.35f, k); a2 = lerpf(-1.5f, -0.7f, k);
  }
}

static void legPose(int i, float &a1, float &a2) {
  static const float OFF[4] = {0.0f, 0.13f, 0.18f, 0.31f};   // far hind, near hind, far fore, near fore (near fore leads)
  const bool front = i >= 2;
  gaitLeg(horse.ph + OFF[i], front, a1, a2);
  if (horse.air) {
    const float k = clamp01(min(horse.u, 1 - horse.u) * 6);
    float b1, b2; jumpLeg(horse.u, front, b1, b2);
    a1 = lerpf(a1, b1, k); a2 = lerpf(a2, b2, k);
  }
}

static void drawHorse() {
  if (horse.turning || horse.x < -80 || horse.x > VW + 80) return;
  const Rider &rd = RIDERS[rnd.rider];
  const Coat &co = COATS[rd.coat];
  const float ly = LANE_Y[horse.lane], s = scaleAt(ly);
  const float k = horse.air ? clamp01(min(horse.u, 1 - horse.u) * 6) : 0;   // how far into the jump pose
  const float stride = horse.ph * 2 * PI;
  float lift = horse.air ? 4 * horse.jH * horse.u * (1 - horse.u) : 1.2f * fabsf(sinf(stride * 0.5f + 0.6f));
  float pitch = horse.air ? 0.38f * (1 - 2 * horse.u) * clamp01(sinf(PI * horse.u) * 3) : 0.05f * sinf(stride);
  if (!cv.visible(ly - (80 + lift) * s, ly + 8 * s)) return;
  HP p = {horse.x, ly, s, lift, cosf(pitch), sinf(pitch), horse.dir};

  // shadow on the sand, smaller the higher it is
  const float sh = 1.0f - clamp01(lift / 60);
  cv.fillEllipse(horse.x, ly + 1, 30 * s * (0.6f + 0.4f * sh), 4 * s * (0.6f + 0.4f * sh), g_sandDark);

  float a1, a2;
  legPose(0, a1, a2); hLeg(p, -18, 27, a1, 13, a2, 14, 5, co.shade, co.mane, RGB(30, 28, 26));   // far legs, in shade
  legPose(2, a1, a2); hLeg(p, 14, 25, a1, 12, a2, 13, 5, co.shade, co.mane, RGB(30, 28, 26));

  // tail
  const float sw = 2 * sinf(stride);
  hLine(p, -28, 40, lerpf(-34, -39, k), lerpf(30 + sw, 37, k), 4.2f, co.mane);
  hLine(p, lerpf(-34, -39, k), lerpf(30 + sw, 37, k), lerpf(-35 + sw, -48, k), lerpf(17, 33, k), 3.2f, co.mane);

  // body: hindquarters, barrel, chest
  hLine(p, -18, 28, 15, 28, 14, co.body);
  hCircle(p, -18, 34, 12, co.body);
  hCircle(p, -6, 31, 11, co.body);
  hCircle(p, 5, 31, 11, co.body);
  hCircle(p, 15, 32, 10.5f, co.body);
  // neck, mane, head
  hLine(p, 17, 38, 25, 52, 12, co.body);
  hLine(p, 24, 50, 29, 58, 8, co.body);
  hLine(p, 18, 45, 28, 60, 2.6f, co.mane);
  hLine(p, 29, 59, 38, 48, 7.5f, co.body);
  hCircle(p, 31, 54, 4.5f, co.body);
  hCircle(p, 38.5f, 47.5f, 3.8f, co.body);
  hTri(p, 27, 60, 30, 60, 27.5f, 66, co.body);
  float ex, ey; hpt(p, 32.5f, 57, ex, ey); cv.drawPixel(ex, ey, RGB(10, 10, 10));
  hpt(p, 40.5f, 48.5f, ex, ey); cv.drawPixel(ex, ey, RGB(20, 16, 14));

  // saddle pad and saddle
  hTri(p, -7, 44, 9, 44, 9, 35, RGB(245, 245, 240));
  hTri(p, -7, 44, -7, 35, 9, 35, RGB(245, 245, 240));
  hLine(p, -4, 44.5f, 8, 44.5f, 3.5f, RGB(92, 52, 26));
  hCircle(p, 8.5f, 45.5f, 1.6f, RGB(92, 52, 26));
  hCircle(p, -4.5f, 46, 1.8f, RGB(92, 52, 26));

  // rider: two-point over the fence, sitting up between them
  const float lean = lerpf(0.24f + 0.04f * sinf(stride * 2), 0.95f, k);
  const float hipX = -1, hipY = 47 + 2.5f * k;
  const float shX = hipX + 17 * sinf(lean), shY = hipY + 17 * cosf(lean);
  const float hdX = hipX + 23 * sinf(lean), hdY = hipY + 23 * cosf(lean);
  const float handX = lerpf(16, 24, k), handY = lerpf(47, 50, k);
  const uint16_t skin = RGB(236, 192, 160), white = RGB(245, 245, 240), boot = RGB(18, 16, 16), helmet = RGB(20, 22, 32);
  hLine(p, handX, handY, 38, 48, 1.0f, RGB(60, 30, 16));                // reins
  hLine(p, hipX, hipY, shX, shY, 7.5f, rd.jacket);                      // torso
  hCircle(p, hdX, hdY - 0.6f, 3.6f, skin);                              // face
  hCircle(p, hdX - 0.6f, hdY + 1.2f, 3.8f, helmet);                     // helmet
  hLine(p, hdX + 1.5f, hdY + 2.4f, hdX + 4.8f, hdY + 2.0f, 1.4f, helmet);   // peak
  const float elX = (shX + handX) * 0.5f + 1, elY = (shY + handY) * 0.5f - 3;
  hLine(p, shX, shY, elX, elY, 3.6f, rd.jacket);                        // arm
  hLine(p, elX, elY, handX, handY, 3.0f, rd.jacket);
  hCircle(p, handX, handY, 1.6f, white);                                // gloves
  hLine(p, hipX, hipY, 9, 39 + k, 5.2f, white);                         // thigh (breeches)
  hLine(p, 9, 39 + k, 6, 29, 4.2f, boot);                               // boot
  hLine(p, 6, 29, 10, 28.5f, 2.6f, boot);

  legPose(1, a1, a2); hLeg(p, -18, 27, a1, 13, a2, 14, 5.5f, co.body, co.lower, RGB(30, 28, 26));   // near legs
  legPose(3, a1, a2); hLeg(p, 14, 25, a1, 12, a2, 13, 5.5f, co.body, co.lower, RGB(30, 28, 26));
}

// ---------------------------------------------------------------- drawing: the crowd, flags, scoreboard
struct Fan { int16_t x; uint8_t y, shirt, skin; };
static Fan fans[340];
static int nFans = 0;
static uint16_t SHIRTS[8], SKINS[4];
#define STAND_ROW0 64
#define STAND_ROWH 10.5f

static void initFans() {
  SHIRTS[0] = RGB(200, 50, 50); SHIRTS[1] = RGB(50, 90, 190); SHIRTS[2] = RGB(240, 200, 60); SHIRTS[3] = RGB(240, 240, 235);
  SHIRTS[4] = RGB(60, 150, 80); SHIRTS[5] = RGB(230, 120, 40); SHIRTS[6] = RGB(150, 70, 160); SHIRTS[7] = RGB(40, 40, 46);
  SKINS[0] = RGB(240, 200, 170); SKINS[1] = RGB(214, 160, 120); SKINS[2] = RGB(160, 110, 80); SKINS[3] = RGB(100, 66, 46);
  uint32_t seed = 7;
  auto r = [&](int n) { seed = seed * 1664525u + 1013904223u; return (int)((seed >> 8) % n); };
  for (int row = 0; row < 5; row++)
    for (int x = 4 + r(5); x < VW - 3 && nFans < 340; x += 7 + r(4)) {
      if (r(100) < 14) continue;            // an empty seat
      fans[nFans++] = {(int16_t)x, (uint8_t)(STAND_ROW0 + row * STAND_ROWH), (uint8_t)r(8), (uint8_t)r(4)};
    }
}

static void drawClapping() {
  if (!(rnd.st == R_DONE && rnd.t < (rnd.faults == 0 ? 4.5f : 2.0f))) return;
  if (!cv.visible(STAND_ROW0 - 10, STAND_ROW0 + 5 * STAND_ROWH)) return;
  for (int i = 0; i < nFans; i++) {
    const uint32_t h = (i * 2654435761u) ^ (g_frameNo * 40503u);
    if ((h >> 13) % 3) continue;
    const Fan &f = fans[i];
    const float up = ((h >> 7) & 1) ? 7 : 5;
    cv.fillRect(f.x - 3, f.y - up, 2, 2, SKINS[f.skin]);
    cv.fillRect(f.x + 1, f.y - up, 2, 2, SKINS[f.skin]);
  }
}

static const int FLAG_X[5] = {40, 140, 240, 340, 440};
static void drawFlags(float t) {
  if (!cv.visible(6, 24)) return;
  static const uint16_t FC[5] = {RGB(210, 40, 40), RGB(40, 90, 190), RGB(240, 200, 40), RGB(30, 140, 70), RGB(240, 240, 235)};
  for (int i = 0; i < 5; i++) {
    const float x = FLAG_X[i];
    for (int sgm = 0; sgm < 4; sgm++) {
      const float x0 = x + 1 + sgm * 4, x1 = x0 + 4;
      const float w0 = 1.5f * sinf(t * 5 + i + sgm * 0.9f), w1 = 1.5f * sinf(t * 5 + i + (sgm + 1) * 0.9f);
      const float hh0 = 8 - sgm * 1.6f, hh1 = 8 - (sgm + 1) * 1.6f;
      cv.fillTriangle(x0, 8 + w0, x1, 8 + w1 + (8 - hh1) * 0.5f, x0, 8 + w0 + hh0, FC[i]);
      cv.fillTriangle(x1, 8 + w1 + (8 - hh1) * 0.5f, x1, 8 + w1 + (8 - hh1) * 0.5f + hh1, x0, 8 + w0 + hh0, FC[i]);
    }
  }
}

static void panel(float x, float y, float w, float h) {
  cv.fillRoundRect(x, y, w, h, 6, RGB(18, 54, 36));
  cv.drawRoundRect(x, y, w, h, 6, RGB(220, 190, 80));
}

static void drawScoreboard() {
  char b[64];
  if (cv.visible(0, 52)) {
    panel(4, 3, 236, 46);
    cv.setTextDatum(TL_DATUM);
    cv.setTextColor(RGB(255, 255, 255));
    snprintf(b, sizeof(b), "%s on %s", riderName(), RIDERS[rnd.rider].horse);
    cv.drawString(b, 12, 8, 2);
    cv.setTextColor(RGB(250, 214, 80));
    snprintf(b, sizeof(b), "Time %.1f   Faults %d", rnd.time, rnd.faults);
    cv.drawString(b, 12, 27, 2);
    panel(VW - 200, 3, 196, 26);
    cv.setTextColor(RGB(234, 234, 220));
    if (nBoard) snprintf(b, sizeof(b), "Leader %s  %d/%.1f", board[0].name, board[0].faults, board[0].time);
    else snprintf(b, sizeof(b), "Leader: no rounds yet");
    cv.drawString(b, VW - 192, 8, 2);
  }
  if (rnd.st == R_RIDE && rnd.you && cv.visible(30, 52)) {
    cv.setTextDatum(TL_DATUM); cv.setTextColor(RGB(255, 255, 255));
    cv.drawString("Tap = jump!", VW - 110, 34, 2);
  }
  if (rnd.st == R_WAIT && cv.visible(150, 200)) {
    panel(80, 152, 320, 44);
    cv.setTextDatum(MC_DATUM); cv.setTextColor(RGB(255, 255, 255));
    snprintf(b, sizeof(b), "Next: %s on %s", riderName(), RIDERS[rnd.rider].horse);
    cv.drawString(b, 240, 166, 2);
    cv.setTextColor(RGB(250, 214, 80));
    cv.drawString(rnd.you ? "Your round! Tap to jump each fence" : "Tap the screen to ride this round yourself", 240, 184, 2);
  }
  if (rnd.st == R_DONE && cv.visible(146, 214)) {
    panel(120, 148, 240, 64);
    cv.setTextDatum(MC_DATUM);
    if (rnd.faults == 0) { cv.setTextColor(RGB(120, 240, 120)); cv.drawString("CLEAR ROUND!", 240, 168, 4); }
    else { cv.setTextColor(RGB(255, 170, 70)); snprintf(b, sizeof(b), "%d FAULTS", rnd.faults); cv.drawString(b, 240, 168, 4); }
    cv.setTextColor(RGB(255, 255, 255));
    snprintf(b, sizeof(b), lastWasLeader ? "%.1f s   New leader!" : "Time %.1f s", rnd.time);
    cv.drawString(b, 240, 196, 2);
  }
}

static void drawInfo(uint32_t now);

static void drawScene(float t) {
  drawFlags(t);
  drawClapping();
  for (int lane = 1; lane >= 0; lane--) {       // far line first
    for (auto &f : fences) if (f.lane == lane) drawFence(f, false);
    if (horse.lane == lane) drawHorse();
    for (auto &f : fences) if (f.lane == lane) drawFence(f, true);
  }
  drawScoreboard();
  if (infoUntil > millis()) drawInfo(millis());
}

// ---------------------------------------------------------------- static scenery (cached in flash)
static uint32_t bgSeed = 3;
static int rnd2(int n) { bgSeed = bgSeed * 1664525u + 1013904223u; return (int)((bgSeed >> 8) % n); }

static void renderBackground(Canvas &c) {
  TFT_eSprite *s = c.s;
  bgSeed = 3;
  for (int r = 0; r < s->height(); r++) {
    const float vy = (r + c.oy) / c.sy;
    uint16_t col;
    if (vy < 26) col = mix(110, 162, 222, 186, 216, 244, vy / 26);                  // sky
    else if (vy < 34) col = RGB(58, 64, 76);                                         // roof
    else if (vy < 58) col = RGB(76, 84, 98);                                         // back wall of the stand
    else if (vy < 112) col = ((int)((vy - 58) / STAND_ROWH) & 1) ? RGB(52, 92, 140) : RGB(60, 104, 156);   // tiers
    else if (vy < 122) col = RGB(36, 96, 62);                                        // front wall
    else if (vy < SAND_Y) col = RGB(236, 236, 230);                                  // perimeter boards
    else col = mix(226, 202, 156, 200, 166, 114, (vy - SAND_Y) / (VH - SAND_Y));     // sand
    s->drawFastHLine(0, r, s->width(), col);
  }
  // clouds, roof edge, flag poles, roof posts
  for (int i = 0; i < 4; i++) { const float x = 60 + i * 120 + rnd2(40); c.fillEllipse(x, 12, 22, 5, RGB(236, 242, 250)); c.fillEllipse(x + 12, 9, 12, 5, RGB(244, 248, 252)); }
  c.fillRect(0, 33, VW, 2, RGB(120, 128, 140));
  for (int x : FLAG_X) c.drawLine(x, 6, x, 27, RGB(200, 200, 200));
  for (int x = 48; x < VW; x += 96) c.fillRect(x - 1, 34, 3, 78, RGB(44, 48, 58));
  // the crowd
  for (int i = 0; i < nFans; i++) {
    const Fan &f = fans[i];
    c.fillRoundRect(f.x - 3, f.y - 1, 6, 7, 2, SHIRTS[f.shirt]);
    c.fillCircle(f.x, f.y - 3.5f, 2.3f, SKINS[f.skin]);
  }
  c.fillRect(0, 112, VW, 1, RGB(240, 240, 230));
  // advertising boards round the arena
  static const char *ADS[6] = {"GRAND PRIX", "SHOW JUMPING", "CLEAR ROUND", "CYD 4.0", "HORSE OF THE YEAR", "SADDLE CLUB"};
  uint16_t ADC[4] = {RGB(190, 36, 36), RGB(30, 74, 156), RGB(28, 120, 70), RGB(220, 160, 30)};
  for (int i = 0; i < 6; i++) {
    const float x = i * 80 + 4;
    c.fillRect(x, 124, 72, 13, ADC[i & 3]);
    c.setTextDatum(MC_DATUM); c.setTextColor(RGB(255, 255, 255));
    c.drawString(ADS[i], x + 36, 131, 1);
  }
  c.fillRect(0, 121, VW, 2, RGB(250, 250, 250));
  // flower boxes along the boards
  static const uint16_t FL[3] = {RGB(230, 60, 80), RGB(250, 210, 60), RGB(250, 140, 200)};
  for (int x = 20; x < VW; x += 80) {
    c.fillRoundRect(x, 140, 40, 7, 2, RGB(120, 80, 50));
    c.fillEllipse(x + 20, 139, 22, 6, RGB(50, 120, 50));
    for (int k = 0; k < 9; k++) c.fillCircle(x + 2 + rnd2(36), 135 + rnd2(6), 1.4f, FL[rnd2(3)]);
  }
  // sand texture and the hoof-worn track along both lines
  const uint16_t lightSand = RGB(236, 214, 170), darkSand = RGB(186, 152, 104);
  for (int i = 0; i < 1400; i++) { const int x = rnd2(VW), y = SAND_Y + 8 + rnd2(VH - SAND_Y - 8); c.drawPixel(x, y, (i & 1) ? lightSand : darkSand); }
  for (int lane = 0; lane < 2; lane++)
    for (int x = -10; x < VW + 10; x += 5) {
      const float s = scaleAt(LANE_Y[lane]);
      c.fillEllipse(x + rnd2(4), LANE_Y[lane] + rnd2(9) - 4, 5 * s, 1.6f * s, mix(200, 166, 114, 180, 146, 98, rnd2(100) / 100.0f));
    }
  // start (near line) and finish (far line) flags: red on the right, white on the left of the line of travel
  for (int k = 0; k < 2; k++) {
    const float ly = LANE_Y[k], s = scaleAt(ly), x = k == 0 ? START_X : FINISH_X;
    const float dx = DEPTH_X * 30 * s, dy = DEPTH_Y * 30 * s;
    const float nx = x - dx, ny = ly + dy, fx = x + dx, fy = ly - dy;
    const uint16_t nearC = k == 0 ? RGB(210, 30, 30) : RGB(245, 245, 240), farC = k == 0 ? RGB(245, 245, 240) : RGB(210, 30, 30);
    c.drawWideLine(nx, ny, nx, ny - 26 * s, 1.6f * s, RGB(90, 70, 50), RGB(90, 70, 50));
    c.drawWideLine(fx, fy, fx, fy - 26 * s, 1.6f * s, RGB(90, 70, 50), RGB(90, 70, 50));
    c.fillTriangle(nx, ny - 26 * s, nx + 10 * s, ny - 22 * s, nx, ny - 18 * s, nearC);
    c.fillTriangle(fx, fy - 26 * s, fx + 10 * s, fy - 22 * s, fx, fy - 18 * s, farC);
    c.setTextDatum(MC_DATUM); c.setTextColor(RGB(120, 90, 60));
    c.drawString(k == 0 ? "START" : "FINISH", x, ly + dy + 9 * s, 1);
  }
}

// The CYD has no PSRAM and too little heap for a 300 KB background, so the scenery is drawn once into a flash
// partition ("bgcache", memory-mapped) and each strip starts as a copy of it. Same scheme as Deer Hunt.
static const uint8_t *bgCache = nullptr;
static void bgCacheInit() {
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "bgcache");
  const size_t hdr = 4096, stripBytes = (size_t)PANEL_W * STRIP_H * 2, bytes = stripBytes * (PANEL_H / STRIP_H);
  if (!p || p->size < hdr + bytes) { Serial.println("bgcache: no partition, drawing the scenery every frame"); return; }
  char want[64];
  snprintf(want, sizeof(want), "showjump-bg %s %s %s", FW_VERSION, __DATE__, __TIME__);   // redraw after every new build
  spi_flash_mmap_handle_t h;
  const void *ptr = nullptr;
  if (esp_partition_mmap(p, 0, hdr + bytes, SPI_FLASH_MMAP_DATA, &ptr, &h) != ESP_OK) { Serial.println("bgcache: mmap failed"); return; }
  if (memcmp(ptr, want, strlen(want) + 1) != 0) {
    spi_flash_munmap(h);
    Serial.println("bgcache: drawing the scenery into flash");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Raking the arena...", 20, 20, 4);
    if (esp_partition_erase_range(p, 0, (hdr + bytes + 4095) & ~(size_t)4095) != ESP_OK) { Serial.println("bgcache: erase failed"); return; }
    cv.s = &strip;
    for (int i = 0; i < PANEL_H / STRIP_H; i++) {
      cv.oy = i * STRIP_H;
      renderBackground(cv);
      if (esp_partition_write(p, hdr + i * stripBytes, strip.getPointer(), stripBytes) != ESP_OK) return;
    }
    char hdrBuf[64] = {};
    memcpy(hdrBuf, want, strlen(want) + 1);
    if (esp_partition_write(p, 0, hdrBuf, sizeof(hdrBuf)) != ESP_OK) return;   // header last: a half-written cache is never used
    if (esp_partition_mmap(p, 0, hdr + bytes, SPI_FLASH_MMAP_DATA, &ptr, &h) != ESP_OK) return;
    if (memcmp(ptr, want, strlen(want) + 1) != 0) return;
  }
  bgCache = (const uint8_t *)ptr + hdr;
  Serial.println("bgcache: ready");
}

static float curT = 0;
static void renderStrip(int i) {
  cv.s = &strip; cv.oy = i * STRIP_H;
  if (bgCache) memcpy(strip.getPointer(), bgCache + (size_t)i * PANEL_W * STRIP_H * 2, (size_t)PANEL_W * STRIP_H * 2);
  else renderBackground(cv);
  drawScene(curT);
}

// Between the strips, remember a touch that would be over before the main loop looks again.
static bool g_latch = false;
static int g_latchX, g_latchY;
static void latchTouch() {
  int x, y;
  if (!g_latch && halTouch(x, y)) { g_latch = true; g_latchX = x; g_latchY = y; }
}

// ---------------------------------------------------------------- WiFi, web page, OTA
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Show Jumping</title><style>body{font:16px system-ui;background:#10241a;color:#eaf2e6;max-width:560px;margin:0 auto;padding:16px}
h1{margin:0 0 4px}small{color:#9fb59a}.c{background:#183626;border-radius:10px;padding:14px;margin:12px 0}
label{display:block;margin:8px 0}input[type=text]{background:#10241a;color:#eaf2e6;border:1px solid #3c6a4c;border-radius:6px;padding:4px}
button{background:#d8b44a;color:#10241a;border:0;border-radius:6px;padding:8px 12px;margin:4px 4px 4px 0;font-weight:600;cursor:pointer}
button.g{background:#3c6a4c;color:#eaf2e6}table{width:100%;border-collapse:collapse}td{padding:3px 4px;border-bottom:1px solid #2a4a36}a{color:#d8b44a}</style></head><body>
<h1>Show Jumping</h1><small id="ver"></small>
<div class="c"><b id="now">-</b><br><small id="rd"></small><br><button onclick="go('/api/next')">Next rider now</button></div>
<div class="c"><b>Leaderboard</b> <small>(since the last restart)</small><table id="lb"></table></div>
<div class="c"><b>Sound</b><label><input type="checkbox" id="snd" onchange="go('/api/snd?on='+(this.checked?1:0))"> Sound on</label>
<label>Volume <input type="range" id="vol" min="0" max="100" onchange="go('/api/snd?vol='+this.value)"></label></div>
<div class="c"><b>Screen</b><br><button class="g" onclick="go('/api/rot')">Flip screen</button>
<label><input type="checkbox" id="inv" onchange="go('/api/set?inv='+(this.checked?1:0))"> Invert colours</label>
<label><input type="checkbox" id="swaprb" onchange="go('/api/set?swaprb='+(this.checked?1:0))"> Swap red / blue</label>
<button class="g" onclick="shot()">Screenshot</button> <button class="g" onclick="if(confirm('Recalibrate touch? The board restarts into five crosshairs.'))go('/api/recal')">Recalibrate touch</button></div>
<div class="c"><b>Device</b><label>Hostname <input type="text" id="host" size="16"> <button class="g" onclick="go('/api/host?host='+encodeURIComponent(host.value))">Save &amp; restart</button></label>
<a href="/update">Firmware update</a><br><button class="g" onclick="if(confirm('Forget WiFi?'))go('/api/wifi/forget')">Forget WiFi</button><button class="g" onclick="go('/api/reboot')">Restart</button></div>
<script>
const $=id=>document.getElementById(id);let first=true;
function go(u){fetch(u).then(r=>r.json()).then(load).catch(()=>{})}
function load(s){if(!s||!s.fw)return;$('ver').textContent='v'+s.fw+' - '+s.board+' - '+s.ip;
$('now').textContent=s.rider+' on '+s.horse;$('rd').textContent=s.state+'  time '+s.time.toFixed(1)+'  faults '+s.faults;
$('lb').innerHTML=s.lb.map((r,i)=>'<tr><td>'+(i+1)+'</td><td>'+r.n+'</td><td>'+r.f+' faults</td><td>'+r.t.toFixed(1)+' s</td></tr>').join('')||'<tr><td>No rounds yet</td></tr>';
if(first){$('snd').checked=s.snd;$('vol').value=s.vol;$('inv').checked=s.inv;$('swaprb').checked=s.swaprb;$('host').value=s.host;first=false}}
function shot(){fetch('/api/frame').then(r=>Promise.all([r.arrayBuffer(),+r.headers.get('X-W'),+r.headers.get('X-H')])).then(([b,w,h])=>{
const d=new DataView(b),c=document.createElement('canvas');c.width=w;c.height=h;const x=c.getContext('2d'),im=x.createImageData(w,h);
for(let i=0;i<w*h;i++){const v=d.getUint16(i*2,false);im.data[i*4]=(v>>11)<<3;im.data[i*4+1]=((v>>5)&63)<<2;im.data[i*4+2]=(v&31)<<3;im.data[i*4+3]=255}
x.putImageData(im,0,0);const a=document.createElement('a');a.download='showjumping.png';a.href=c.toDataURL();a.click()})}
go('/api/status');setInterval(()=>go('/api/status'),2000);
</script></body></html>)HTML";

static String statusJson() {
  static const char *ST[3] = {"waiting", "riding", "finished"};
  String j = "{\"fw\":\"" FW_VERSION "\",\"board\":\"" BOARD_NAME "\",\"host\":\"" + g_host + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"snd\":" + String(g_snd ? "true" : "false") + ",\"vol\":" + String(g_vol);
  j += ",\"inv\":" + String(g_invert ? "true" : "false") + ",\"swaprb\":" + String(g_swaprb ? "true" : "false");
  j += ",\"rider\":\"" + String(riderName()) + "\",\"horse\":\"" + RIDERS[rnd.rider].horse + "\",\"state\":\"" + ST[rnd.st] + "\"";
  j += ",\"time\":" + String(rnd.time, 1) + ",\"faults\":" + String(rnd.faults) + ",\"fps\":" + String(g_fps, 1) + ",\"lb\":[";
  for (int i = 0; i < nBoard; i++) {
    if (i) j += ",";
    j += "{\"n\":\"" + String(board[i].name) + "\",\"f\":" + String(board[i].faults) + ",\"t\":" + String(board[i].time, 1) + "}";
  }
  return j + "]}";
}

static bool numArg(const char *name, int lo, int hi, int &out) {
  if (!server.hasArg(name)) return false;
  String s = server.arg(name); s.trim();
  if (!s.length()) return false;
  out = constrain((int)s.toInt(), lo, hi);
  return true;
}

static void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
  // Raw copy of the screen (RGB565, big-endian) for screenshots.
  server.on("/api/frame", HTTP_GET, []() {
    server.setContentLength(PANEL_W * PANEL_H * 2);
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-W", String(PANEL_W));
    server.sendHeader("X-H", String(PANEL_H));
    server.send(200, "application/octet-stream", "");
    for (int i = 0; i < PANEL_H / STRIP_H; i++) {
      renderStrip(i);
      server.sendContent((const char *)strip.getPointer(), PANEL_W * STRIP_H * 2);
    }
  });
  server.on("/api/status", HTTP_GET, []() { server.sendHeader("Access-Control-Allow-Origin", "*"); server.send(200, "application/json", statusJson()); });
  server.on("/api/set", HTTP_ANY, []() {
    int v;
    bool colorsChanged = false;
    if (numArg("inv", 0, 1, v)) { bool b = v != 0; colorsChanged |= b != g_invert; g_invert = b; }
    if (numArg("swaprb", 0, 1, v)) { bool b = v != 0; colorsChanged |= b != g_swaprb; g_swaprb = b; }
    if (colorsChanged) halApplyColors();
    saveSettings();
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/snd", HTTP_ANY, []() {
    if (server.hasArg("on")) { g_snd = server.arg("on").toInt() != 0; prefs.putBool("snd", g_snd); }
    if (server.hasArg("vol")) { g_vol = constrain(server.arg("vol").toInt(), 0, 100); prefs.putInt("vol", g_vol); }
    halApplySound();
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/next", HTTP_ANY, []() { nextRider(); rnd.t = 3; server.send(200, "application/json", statusJson()); });
  server.on("/api/recal", HTTP_ANY, []() {      // forget the touch calibration and restart into the five crosshairs
    server.send(200, "text/plain", "OK");
    delay(200);
    halRecalibrateTouch();
  });
  server.on("/api/host", HTTP_ANY, []() {         // rename the board (mDNS / OTA name); restarts to apply
    String h = server.arg("host"); h.trim(); h.toLowerCase();
    bool ok = h.length() >= 1 && h.length() <= 31 && h[0] != '-' && h[h.length() - 1] != '-';
    for (size_t i = 0; ok && i < h.length(); i++) ok = isalnum((unsigned char)h[i]) || h[i] == '-';
    if (!ok) { server.send(400, "application/json", "{\"error\":\"letters, digits and - only (1-31 chars)\"}"); return; }
    prefs.putString("host", h);
    server.send(200, "application/json", "{\"ok\":true,\"host\":\"" + h + "\"}");
    restartAt = millis() + 800;
  });
  server.on("/api/rot", HTTP_ANY, []() {
    g_rot = g_rot == 1 ? 3 : 1;
    prefs.putInt("rot", g_rot);
    halApplyRotation();
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/wifi/forget", HTTP_ANY, []() {
    wifiForgetAll();
    server.send(200, "application/json", "{\"ok\":true}");
    restartAt = millis() + 800;
  });
  server.on("/api/reboot", HTTP_ANY, []() { server.send(200, "application/json", "{\"ok\":true}"); restartAt = millis() + 500; });
  // Firmware update page at /update (ElegantOTA). The upload runs inside server.handleClient(), so the arena pauses.
  ElegantOTA.begin(&server);
  ElegantOTA.onStart([]() { tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_WHITE); tft.drawString("Firmware update...", 10, 10, 4); });
  ElegantOTA.onProgress([](size_t cur, size_t total) {
    static uint32_t lastDraw = 0;
    if (!total || (millis() - lastDraw < 150 && cur < total)) return;   // each repaint costs SPI time, so throttle
    lastDraw = millis();
    tft.fillRect(10, 60, PANEL_W - 20, 20, TFT_DARKGREY);
    tft.fillRect(10, 60, (int)((PANEL_W - 20) * (float)cur / total), 20, TFT_GREEN);
  });
  server.begin();
}

static void startServices() {
  g_online = true;
  WiFi.setAutoReconnect(true);
  ArduinoOTA.setHostname(g_host.c_str());
  ArduinoOTA.onStart([]() { tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_WHITE); tft.drawString("OTA update...", 10, 10, 4); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int total) {
    tft.fillRect(10, 60, PANEL_W - 20, 20, TFT_DARKGREY);
    tft.fillRect(10, 60, (int)((PANEL_W - 20) * (float)p / total), 20, TFT_GREEN);
  });
  ArduinoOTA.begin();
  MDNS.begin(g_host.c_str());
  MDNS.addService("http", "tcp", 80);
  setupWeb();
  Serial.printf("WiFi connected, IP %s, host %s\n", WiFi.localIP().toString().c_str(), g_host.c_str());
}

// WiFi comes from the on-screen picker (TouchWifiProvisioner). BOOT skips it: the arena then runs offline.
static void connectWifi() {
  static volatile bool up = false;
  wifiSetupBegin(g_host.c_str(), [](const String &) { up = true; });
  uint32_t armed = millis();
  while (!up) {
    wifiSetupLoop();
    if (millis() - armed > 1500 && digitalRead(0) == LOW) break;     // BOOT pressed: skip
    delay(5);
  }
  tft.fillScreen(TFT_BLACK);
  if (up) startServices();
}

// BOOT button: connection info for 20 s
static void drawInfo(uint32_t now) {
  if (!cv.visible(50, 240)) return;
  cv.fillRoundRect(30, 50, 420, 190, 12, RGB(16, 40, 28));
  cv.drawRoundRect(30, 50, 420, 190, 12, RGB(220, 190, 80));
  cv.setTextDatum(TL_DATUM);
  cv.setTextColor(RGB(255, 255, 255));
  cv.drawString("Show Jumping  v" FW_VERSION, 46, 60, FONT_BIG);
  char b[96];
  cv.setTextColor(RGB(250, 214, 80));
  snprintf(b, sizeof(b), "http://%s.local", g_host.c_str());
  cv.drawString(b, 46, 100, FONT_BIG);
  snprintf(b, sizeof(b), "http://%s", WiFi.localIP().toString().c_str());
  cv.drawString(b, 46, 132, FONT_BIG);
  cv.setTextColor(RGB(224, 236, 220));
  snprintf(b, sizeof(b), "WiFi: %s  (%d dBm)", WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "not connected", WiFi.RSSI());
  cv.drawString(b, 46, 172, FONT_SMALL);
  snprintf(b, sizeof(b), "Up %lu min  %.1f fps  %s", millis() / 60000UL, g_fps, BOARD_NAME);
  cv.drawString(b, 46, 194, FONT_SMALL);
  snprintf(b, sizeof(b), "Press BOOT to close (%lu s)", (unsigned long)((infoUntil - now) / 1000));
  cv.drawString(b, 46, 216, FONT_SMALL);
}

void setup() {
  Serial.begin(115200);
  prefs.begin("showjump", false);
  g_rot = prefs.getInt("rot", DEFAULT_ROT);
  if (g_rot != 1 && g_rot != 3) g_rot = DEFAULT_ROT;
  g_host = prefs.getString("host", DEFAULT_HOST);
  g_snd = prefs.getBool("snd", true);
  g_vol = constrain(prefs.getInt("vol", 60), 0, 100);
  g_invert = prefs.getBool("invert", false);
  g_swaprb = prefs.getBool("swaprb", DEFAULT_SWAPRB);
  pinMode(0, INPUT_PULLUP);        // BOOT button
  halAudioStart();
  halDisplayBegin();
  halTouchBegin();

  cv.sx = PANEL_W / (float)VW;
  cv.sy = PANEL_H / (float)VH;
  strip.setColorDepth(16);
  if (!strip.createSprite(PANEL_W, STRIP_H)) {
    tft.setTextColor(TFT_RED);
    tft.drawString("Out of memory", 10, 10, 4);
    while (true) delay(1000);
  }
  g_sandDark = RGB(170, 138, 94);
  initRiders();
  initCourse();
  initFans();
  bgCacheInit();
  randomSeed(esp_random());
  rnd.rider = random(NRIDERS);
  resetCourse();
  horse.x = -200;
  connectWifi();
}

void loop() {
  static uint32_t last = millis(), fpsT = millis();
  static int frames = 0;
  uint32_t now = millis();
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  if (g_online) {
    ArduinoOTA.handle();
    server.handleClient();
    ElegantOTA.loop();
    static uint32_t lastWifi = 0;
    if (WiFi.status() != WL_CONNECTED && now - lastWifi > 30000) { lastWifi = now; WiFi.reconnect(); }
  }
  if (restartAt && millis() > restartAt) ESP.restart();

  static bool lastBtn = true;
  bool btn = digitalRead(0);
  if (!btn && lastBtn) infoUntil = (infoUntil > now) ? 0 : now + 20000;   // BOOT: connection info
  lastBtn = btn;

  int px, py;
  bool touched = halTouch(px, py);
  if (!touched && g_latch) { touched = true; px = g_latchX; py = g_latchY; }
  g_latch = false;
  static bool wasTouched = false;
  const bool press = touched && !wasTouched;
  wasTouched = touched;
  if (press) {
    if (rnd.st == R_RIDE) tapJump();
    else { if (rnd.st == R_DONE) nextRider(); rnd.you = true; rnd.t = max(rnd.t, 2.0f); }   // your round starts in a moment
  }

  updateRound(dt);
  curT = now / 1000.0f;
  g_frameNo++;

  for (int i = 0; i < PANEL_H / STRIP_H; i++) {
    renderStrip(i);
    strip.pushSprite(0, i * STRIP_H);
    latchTouch();                                // a whole frame takes a while: a short tap would fall between two polls
  }

  frames++;
  if (now - fpsT >= 5000) {
    g_fps = frames * 1000.0f / (now - fpsT);
    Serial.printf("fps: %.1f  heap %u\n", g_fps, (unsigned)ESP.getFreeHeap());
    frames = 0; fpsT = now;
  }
}
