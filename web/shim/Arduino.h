// Web demo: the bits of the Arduino core the firmware uses, on top of the C++ library (Emscripten).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <functional>

using std::min;
using std::max;

#define PROGMEM
#define PGM_P const char *
#define PI 3.14159265358979f
#define TWO_PI 6.28318530717959f
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
template <class T> static inline T sq(T x) { return x * x; }
template <class T, class L, class H> static inline T constrain(T x, L lo, H hi) { return x < lo ? (T)lo : x > hi ? (T)hi : x; }

uint32_t millis();
uint32_t micros();
static inline void delay(uint32_t) {}         // nothing may block in the browser; the firmware only waits in restart paths
static inline void pinMode(int, int) {}
static inline void digitalWrite(int, int) {}
static inline int digitalRead(int) { return HIGH; }   // BOOT is never held
long random(long n);
long random(long lo, long hi);
void randomSeed(unsigned long s);
uint32_t esp_random();


class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(double v, int dec = 2) { char b[40]; snprintf(b, sizeof(b), "%.*f", dec, v); s = b; }
  String(float v, int dec = 2) : String((double)v, dec) {}
  const char *c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  char operator[](size_t i) const { return i < s.size() ? s[i] : 0; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  bool operator==(const char *o) const { return s == o; }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator!=(const char *o) const { return s != o; }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  void trim() {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    s = s.substr(a, b - a);
  }
  void toLowerCase() { for (auto &c : s) c = (char)tolower((unsigned char)c); }
};
static inline String operator+(const String &a, const String &b) { String r = a; r += b; return r; }
static inline String operator+(const String &a, const char *b) { String r = a; r += b; return r; }
static inline String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
static inline String operator+(const String &a, char b) { String r = a; r += b; return r; }

struct SerialPort {
  void begin(int) {}
  void println(const char *m) { puts(m); }
  void println(const String &m) { puts(m.c_str()); }
  template <class... A> void printf(const char *f, A... a) { ::printf(f, a...); }
};
extern SerialPort Serial;

struct EspClass {
  uint32_t getFreeHeap() { return 0; }
  uint32_t getMinFreeHeap() { return 0; }
  void restart();
};
extern EspClass ESP;
typedef enum { ESP_RST_UNKNOWN = 0, ESP_RST_POWERON = 1 } esp_reset_reason_t;
static inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }
