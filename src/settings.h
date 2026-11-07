// Persistent settings in NVS (Preferences). Secrets live only here, never in git.
#pragma once
#include <Arduino.h>

struct Settings {
  // network & bot (set through the captive portal)
  String wifiSsid, wifiPass;
  String tgToken, tgChat;   // chat id empty = pair with the first /start
  String deviceName = "polh-kill";
  String adminPass;         // optional: protects /update, OTA and destructive API calls

  // detection & alerts (set in the web UI)
  float kSigma = 6.0f;
  float minG = 0.05f;
  float snapPeakG = 1.0f;   // heuristic while the model is still empty
  float alertProb = 0.70f;  // >= -> "trap snapped" alert
  float unsureProb = 0.35f; // between unsure and alert -> ask for a label
  bool armed = true;        // send alerts at all
  bool notifyUnsure = true;
  bool dailyReport = true;  // 20:00 diary entry like RandeSolar

  void load();
  void save() const;
  void clearNetwork();
};

extern Settings settings;
