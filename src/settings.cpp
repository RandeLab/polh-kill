#include "settings.h"
#include <Preferences.h>

Settings settings;

void Settings::load() {
  Preferences p;
  p.begin("polh", true);
  wifiSsid = p.getString("ssid", "");
  wifiPass = p.getString("pass", "");
  tgToken = p.getString("tg_token", "");
  tgChat = p.getString("tg_chat", "");
  deviceName = p.getString("name", "polh-kill");
  adminPass = p.getString("admin", "");
  ghToken = p.getString("gh_token", "");
  ghRepo = p.getString("gh_repo", ghRepo);
  ghBranch = p.getString("gh_branch", ghBranch);
  ghEveryMin = p.getInt("gh_every", ghEveryMin);
  kSigma = p.getFloat("k_sigma", kSigma);
  minG = p.getFloat("min_g", minG);
  snapPeakG = p.getFloat("snap_g", snapPeakG);
  alertProb = p.getFloat("alert_p", alertProb);
  unsureProb = p.getFloat("unsure_p", unsureProb);
  armed = p.getBool("armed", armed);
  notifyUnsure = p.getBool("notify_u", notifyUnsure);
  dailyReport = p.getBool("daily", dailyReport);
  p.end();
}

void Settings::save() const {
  Preferences p;
  p.begin("polh", false);
  p.putString("ssid", wifiSsid);
  p.putString("pass", wifiPass);
  p.putString("tg_token", tgToken);
  p.putString("tg_chat", tgChat);
  p.putString("name", deviceName);
  p.putString("admin", adminPass);
  p.putString("gh_token", ghToken);
  p.putString("gh_repo", ghRepo);
  p.putString("gh_branch", ghBranch);
  p.putInt("gh_every", ghEveryMin);
  p.putFloat("k_sigma", kSigma);
  p.putFloat("min_g", minG);
  p.putFloat("snap_g", snapPeakG);
  p.putFloat("alert_p", alertProb);
  p.putFloat("unsure_p", unsureProb);
  p.putBool("armed", armed);
  p.putBool("notify_u", notifyUnsure);
  p.putBool("daily", dailyReport);
  p.end();
}

void Settings::clearNetwork() {
  wifiSsid = "";
  wifiPass = "";
  save();
}
