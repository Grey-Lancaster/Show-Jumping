// Web demo: the part of TFT_eSPI the arena draws with, as a plain RGB565 sprite in memory. The shapes follow
// TFT_eSPI's own algorithms (so small ellipses vanish and wide lines get the same edges as on the boards) and the
// text uses TFT_eSPI's fonts 1, 2 and 4 (web/fonts, copied from the library).
#pragma once
#include "Arduino.h"

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800
#define TFT_GREEN 0x07E0
#define TFT_DARKGREY 0x7BEF

class TFT_eSPI {   // the panel itself: the demo only shows the frame sprite, so drawing straight on it does nothing
 public:
  void init() {}
  void setRotation(int) {}
  void invertDisplay(bool) {}
  void fillScreen(uint32_t) {}
  void fillRect(int, int, int, int, uint32_t) {}
  void setTextColor(uint16_t, uint16_t = 0) {}
  int16_t drawString(const char *, int, int, int) { return 0; }
  int16_t drawString(const String &, int, int, int) { return 0; }
};

class TFT_eSprite {
 public:
  explicit TFT_eSprite(TFT_eSPI *) {}
  void setColorDepth(int) {}
  void *createSprite(int w, int h) { _w = w; _h = h; buf = (uint16_t *)calloc((size_t)w * h, 2); return buf; }
  void *getPointer() { return buf; }
  int16_t width() const { return _w; }
  int16_t height() const { return _h; }
  void pushSprite(int, int);                       // web_main.cpp: hands the frame to the page

  void drawPixel(int32_t x, int32_t y, uint32_t c) { if ((uint32_t)x < (uint32_t)_w && (uint32_t)y < (uint32_t)_h) buf[y * _w + x] = (uint16_t)c; }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) {
    if ((uint32_t)y >= (uint32_t)_h || w <= 0) return;
    int32_t x1 = x + w; if (x < 0) x = 0; if (x1 > _w) x1 = _w;
    for (uint16_t *p = buf + y * _w + x; x < x1; x++) *p++ = (uint16_t)c;
  }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t c) { for (int32_t i = 0; i < h; i++) drawPixel(x, y + i, c); }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { for (int32_t i = 0; i < h; i++) drawFastHLine(x, y + i, w, c); }
  void fillScreen(uint32_t c) { fillRect(0, 0, _w, _h, c); }

  void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c) {
    int32_t x = 0, dx = 1, dy = r + r, p = -(r >> 1);
    drawFastHLine(x0 - r, y0, dy + 1, c);
    while (x < r) {
      if (p >= 0) { drawFastHLine(x0 - x, y0 + r, dx, c); drawFastHLine(x0 - x, y0 - r, dx, c); dy -= 2; p -= dy; r--; }
      dx += 2; p += dx; x++;
      drawFastHLine(x0 - r, y0 + x, dy + 1, c); drawFastHLine(x0 - r, y0 - x, dy + 1, c);
    }
  }
  void drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c) {
    int32_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    drawPixel(x0, y0 + r, c); drawPixel(x0, y0 - r, c); drawPixel(x0 + r, y0, c); drawPixel(x0 - r, y0, c);
    while (x < y) {
      if (f >= 0) { y--; ddy += 2; f += ddy; }
      x++; ddx += 2; f += ddx;
      drawPixel(x0 + x, y0 + y, c); drawPixel(x0 - x, y0 + y, c); drawPixel(x0 + x, y0 - y, c); drawPixel(x0 - x, y0 - y, c);
      drawPixel(x0 + y, y0 + x, c); drawPixel(x0 - y, y0 + x, c); drawPixel(x0 + y, y0 - x, c); drawPixel(x0 - y, y0 - x, c);
    }
  }
  void fillCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t corner, int32_t delta, uint32_t c) {
    int32_t f = 1 - r, ddx = 1, ddy = -r - r, y = 0;
    delta++;
    while (y < r) {
      if (f >= 0) {
        if (corner & 1) drawFastHLine(x0 - y, y0 + r, y + y + delta, c);
        if (corner & 2) drawFastHLine(x0 - y, y0 - r, y + y + delta, c);
        r--; ddy += 2; f += ddy;
      }
      y++; ddx += 2; f += ddx;
      if (corner & 1) drawFastHLine(x0 - r, y0 + y, r + r + delta, c);
      if (corner & 2) drawFastHLine(x0 - r, y0 - y, r + r + delta, c);
    }
  }
  void drawCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t corner, uint32_t c) {
    int32_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0;
    while (x < r) {
      if (f >= 0) { r--; ddy += 2; f += ddy; }
      x++; ddx += 2; f += ddx;
      if (corner & 4) { drawPixel(x0 + x, y0 + r, c); drawPixel(x0 + r, y0 + x, c); }
      if (corner & 2) { drawPixel(x0 + x, y0 - r, c); drawPixel(x0 + r, y0 - x, c); }
      if (corner & 8) { drawPixel(x0 - r, y0 + x, c); drawPixel(x0 - x, y0 + r, c); }
      if (corner & 1) { drawPixel(x0 - r, y0 - x, c); drawPixel(x0 - x, y0 - r, c); }
    }
  }
  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
    fillRect(x, y + r, w, h - r - r, c);
    fillCircleHelper(x + r, y + h - r - 1, r, 1, w - r - r - 1, c);
    fillCircleHelper(x + r, y + r, r, 2, w - r - r - 1, c);
  }
  void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
    drawFastHLine(x + r, y, w - r - r, c); drawFastHLine(x + r, y + h - 1, w - r - r, c);
    drawFastVLine(x, y + r, h - r - r, c); drawFastVLine(x + w - 1, y + r, h - r - r, c);
    drawCircleHelper(x + r, y + r, r, 1, c); drawCircleHelper(x + w - r - 1, y + r, r, 2, c);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, c); drawCircleHelper(x + r, y + h - r - 1, r, 8, c);
  }
  void fillEllipse(int32_t x0, int32_t y0, int32_t rx, int32_t ry, uint32_t c) {
    if (rx < 2 || ry < 2) return;
    int32_t x, y, rx2 = rx * rx, ry2 = ry * ry, fx2 = 4 * rx2, fy2 = 4 * ry2, s;
    for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
      drawFastHLine(x0 - x, y0 - y, x + x + 1, c); drawFastHLine(x0 - x, y0 + y, x + x + 1, c);
      if (s >= 0) { s += fx2 * (1 - y); y--; }
      s += ry2 * ((4 * x) + 6);
    }
    for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
      drawFastHLine(x0 - x, y0 - y, x + x + 1, c); drawFastHLine(x0 - x, y0 + y, x + x + 1, c);
      if (s >= 0) { s += fy2 * (1 - x); x--; }
      s += rx2 * ((4 * y) + 6);
    }
  }
  void drawEllipse(int32_t x0, int32_t y0, int32_t rx, int32_t ry, uint32_t c) {
    if (rx < 2 || ry < 2) return;
    int32_t x, y, rx2 = rx * rx, ry2 = ry * ry, fx2 = 4 * rx2, fy2 = 4 * ry2, s;
    for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
      drawPixel(x0 + x, y0 + y, c); drawPixel(x0 - x, y0 + y, c); drawPixel(x0 - x, y0 - y, c); drawPixel(x0 + x, y0 - y, c);
      if (s >= 0) { s += fx2 * (1 - y); y--; }
      s += ry2 * ((4 * x) + 6);
    }
    for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
      drawPixel(x0 + x, y0 + y, c); drawPixel(x0 - x, y0 + y, c); drawPixel(x0 - x, y0 - y, c); drawPixel(x0 + x, y0 - y, c);
      if (s >= 0) { s += fy2 * (1 - x); x--; }
      s += rx2 * ((4 * y) + 6);
    }
  }
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c) {
    int32_t a, b, y, last;
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y2, y1); std::swap(x2, x1); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y0 == y2) {
      a = b = x0;
      if (x1 < a) a = x1; else if (x1 > b) b = x1;
      if (x2 < a) a = x2; else if (x2 > b) b = x2;
      drawFastHLine(a, y0, b - a + 1, c);
      return;
    }
    int32_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1, sa = 0, sb = 0;
    last = y1 == y2 ? y1 : y1 - 1;
    for (y = y0; y <= last; y++) {
      a = x0 + sa / dy01; b = x0 + sb / dy02; sa += dx01; sb += dx02;
      if (a > b) std::swap(a, b);
      drawFastHLine(a, y, b - a + 1, c);
    }
    sa = dx12 * (y - y1); sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
      a = x1 + sa / dy12; b = x0 + sb / dy02; sa += dx12; sb += dx02;
      if (a > b) std::swap(a, b);
      drawFastHLine(a, y, b - a + 1, c);
    }
  }
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) {
    const int32_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t e = dx + dy;
    for (;;) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      const int32_t e2 = 2 * e;
      if (e2 >= dy) { e += dy; x0 += sx; }
      if (e2 <= dx) { e += dx; y0 += sy; }
    }
  }
  // TFT_eSPI's drawWedgeLine with equal end radii. The arena always passes bg == fg, so the soft edge blends to
  // the same colour: every pixel closer than r + 0.5 - 1/32 is solid, as on the boards.
  void drawWideLine(float ax, float ay, float bx, float by, float wd, uint32_t fg, uint32_t) {
    const float r = wd / 2.0f, lim = r + 0.5f - 1.0f / 32;
    if (fabsf(ax - bx) < 0.01f && fabsf(ay - by) < 0.01f) bx += 0.01f;
    int32_t x0 = (int32_t)floorf(fminf(ax, bx) - r), x1 = (int32_t)ceilf(fmaxf(ax, bx) + r);
    int32_t y0 = (int32_t)floorf(fminf(ay, by) - r), y1 = (int32_t)ceilf(fmaxf(ay, by) + r);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= _w) x1 = _w - 1; if (y1 >= _h) y1 = _h - 1;
    const float bax = bx - ax, bay = by - ay, l2 = bax * bax + bay * bay;
    for (int32_t y = y0; y <= y1; y++)
      for (int32_t x = x0; x <= x1; x++) {
        const float px = x - ax, py = y - ay;
        float h = (px * bax + py * bay) / l2; h = h < 0 ? 0 : h > 1 ? 1 : h;
        const float dx = px - bax * h, dy = py - bay * h;
        if (dx * dx + dy * dy < lim * lim) buf[y * _w + x] = (uint16_t)fg;
      }
  }

  void setTextDatum(uint8_t d) { datum = d; }
  void setTextColor(uint16_t c) { tc = c; }
  void setTextColor(uint16_t c, uint16_t) { tc = c; }
  int16_t textWidth(const char *s, int font);
  int16_t drawString(const char *s, int32_t x, int32_t y, int font);
  int16_t drawString(const String &s, int32_t x, int32_t y, int font) { return drawString(s.c_str(), x, y, font); }

 private:
  uint16_t *buf = nullptr;
  int _w = 0, _h = 0;
  uint8_t datum = TL_DATUM;
  uint16_t tc = TFT_WHITE;
};
