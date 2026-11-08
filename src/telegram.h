// Telegram bot (BotFather token), RandeSolar style: raw HTTPS, getUpdates polling.
// Runs in its own task so TLS handshakes never block sampling or the web UI.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "app.h"

namespace telegram {
void begin();
bool configured();
void send(const String& html, const String& replyMarkupJson = "");
void onEvent(const RecentEvent& e);  // decides: alert, ask for label, or stay quiet
void sendTest();
String botName();
String lastError();
// subscribers: owner first, others by approval button or join code
void usersToJson(JsonObject t);  // paired, join_code, max_users, pending, users[]
bool removeUser(const String& chat);
void unpairAll();
void newCode();
}  // namespace telegram
