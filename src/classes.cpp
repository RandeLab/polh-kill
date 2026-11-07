// Class registry: 5 built-in classes + user-defined ones (name + emoji),
// persisted as /classes.txt on the dataset partition (survives UI/firmware updates
// and "wipe dataset", which only clears /ds).
#include <LittleFS.h>
#include "classifier.h"
#include "dataset.h"

namespace {
struct ClassDef {
  char name[17];
  char emoji[33];
};
const ClassDef BUILTIN[] = {{"snap", "💥"}, {"bump", "🤜"}, {"footstep", "👣"}, {"mouse", "🐭"}, {"other", "❓"}};
constexpr int NBUILTIN = sizeof(BUILTIN) / sizeof(BUILTIN[0]);
constexpr const char* FILE_PATH = "/classes.txt";

ClassDef defs[MAX_CLASSES];
volatile int count = NBUILTIN;

void save() {
  File f = dataFS.open(FILE_PATH, "w");
  if (!f) return;
  for (int i = NBUILTIN; i < count; i++) f.printf("%s %s\n", defs[i].name, defs[i].emoji);
  f.close();
}

bool validName(const String& n) {
  if (n.length() < 1 || n.length() > 16) return false;
  for (char c : n)
    if (!(islower(c) || isdigit(c) || c == '-' || c == '_')) return false;
  return true;
}

// emoji: 1..32 bytes, only non-ASCII bytes plus keycap characters (digits, # and *)
bool validEmoji(const String& e) {
  if (e.length() < 1 || e.length() > 32) return false;
  for (unsigned char c : e)
    if (c < 0x80 && !isdigit(c) && c != '#' && c != '*') return false;
  return true;
}
}  // namespace

void classesBegin() {
  for (int i = 0; i < NBUILTIN; i++) defs[i] = BUILTIN[i];
  count = NBUILTIN;
  File f = dataFS.open(FILE_PATH, "r");
  if (!f) return;
  while (f.available() && count < MAX_CLASSES) {
    String line = f.readStringUntil('\n');
    line.trim();
    const int sp = line.indexOf(' ');
    if (sp <= 0) continue;
    const String n = line.substring(0, sp), e = line.substring(sp + 1);
    if (!validName(n) || !validEmoji(e) || classFind(n) >= 0) continue;
    strlcpy(defs[count].name, n.c_str(), sizeof(defs[count].name));
    strlcpy(defs[count].emoji, e.c_str(), sizeof(defs[count].emoji));
    count = count + 1;
  }
  f.close();
  Serial.printf("[Classes 🏷️] %d classes (%d custom)\n", (int)count, (int)count - NBUILTIN);
}

int classCount() { return count; }
int builtinClassCount() { return NBUILTIN; }
const char* className(int c) { return (c >= 0 && c < count) ? defs[c].name : "?"; }
const char* classEmoji(int c) { return (c >= 0 && c < count) ? defs[c].emoji : "❔"; }

int classFind(const String& name) {
  for (int i = 0; i < count; i++)
    if (name.equalsIgnoreCase(defs[i].name)) return i;
  return -1;
}

int classAdd(String name, String emoji, String& err) {
  name.trim();
  name.toLowerCase();
  emoji.trim();
  if (!validName(name)) { err = "name: 1-16 chars, a-z 0-9 - _"; return -1; }
  if (!validEmoji(emoji)) { err = "emoji: one emoji please"; return -1; }
  if (classFind(name) >= 0) { err = "class exists"; return -1; }
  if (count >= MAX_CLASSES) { err = "too many classes"; return -1; }
  strlcpy(defs[count].name, name.c_str(), sizeof(defs[count].name));
  strlcpy(defs[count].emoji, emoji.c_str(), sizeof(defs[count].emoji));
  count = count + 1;
  save();
  return count - 1;
}

// caller (app::deleteClass) has already moved the labels of higher classes down
bool classRemove(int idx, String& err) {
  if (idx < NBUILTIN || idx >= count) { err = "built-in classes cannot be deleted"; return false; }
  for (int i = idx; i < count - 1; i++) defs[i] = defs[i + 1];
  count = count - 1;
  save();
  return true;
}
