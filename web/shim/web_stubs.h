// Web demo: WiFi, OTA, mDNS and flash settings do nothing in the browser. The web server keeps its handlers,
// so the page beside the arena can call the firmware's own /api/... routes (demo_api in web_main.cpp).
#pragma once
#include "Arduino.h"
#include <map>
#include <vector>

// ---- WiFi
#define WL_CONNECTED 3
struct IPAddress { String toString() const { return "browser"; } };
struct WiFiClass {
  IPAddress localIP() { return {}; }
  int status() { return WL_CONNECTED; }
  String SSID() { return "browser demo"; }
  int RSSI() { return 0; }
  void reconnect() {}
  void setAutoReconnect(bool) {}
};
extern WiFiClass WiFi;

// ---- OTA / mDNS
struct ArduinoOTAClass {
  void setHostname(const char *) {}
  template <class F> void onStart(F) {}
  template <class F> void onProgress(F) {}
  void begin() {}
  void handle() {}
};
extern ArduinoOTAClass ArduinoOTA;
struct MDNSClass { bool begin(const char *) { return true; } void addService(const char *, const char *, int) {} };
extern MDNSClass MDNS;

// ---- web server: handlers are kept and run on request from the page
enum HTTPMethod { HTTP_GET, HTTP_ANY };
class WebServer {
 public:
  explicit WebServer(int) {}
  void on(const char *uri, HTTPMethod, std::function<void()> fn) { routes[uri] = fn; }
  void begin() {}
  void handleClient() {}
  bool hasArg(const char *n) const { return args.count(n) > 0; }
  String arg(const char *n) const { auto i = args.find(n); return i == args.end() ? String() : i->second; }
  void send(int code, const char *, const String &body) { status = code; reply = body; }
  void send_P(int code, const char *, const char *body, size_t = 0) { status = code; reply = String(body); }
  void sendHeader(const char *, const String &) {}
  void setContentLength(size_t) {}
  void sendContent(const char *, size_t) {}

  std::map<std::string, std::function<void()>> routes;
  std::map<std::string, String> args;
  int status = 0;
  String reply;
};
struct ElegantOTAClass {
  void begin(WebServer *) {}
  void loop() {}
  template <class F> void onStart(F) {}
  template <class F> void onProgress(F) {}
};
extern ElegantOTAClass ElegantOTA;

// ---- settings (in memory; the page could keep them in localStorage later)
class Preferences {
 public:
  bool begin(const char *, bool) { return true; }
  int getInt(const char *k, int d) { auto i = kv.find(k); return i == kv.end() ? d : atoi(i->second.c_str()); }
  bool getBool(const char *k, bool d) { auto i = kv.find(k); return i == kv.end() ? d : i->second == "1"; }
  String getString(const char *k, const String &d) { auto i = kv.find(k); return i == kv.end() ? d : String(i->second); }
  void putInt(const char *k, int v) { kv[k] = std::to_string(v); }
  void putBool(const char *k, bool v) { kv[k] = v ? "1" : "0"; }
  void putString(const char *k, const String &v) { kv[k] = v.s; }
  std::map<std::string, std::string> kv;
};
