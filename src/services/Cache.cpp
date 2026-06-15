#include "Cache.h"
#include <LittleFS.h>

bool Cache::begin() {
  if (!LittleFS.exists("/cache")) LittleFS.mkdir("/cache");
  return true;
}

String Cache::sanitize(const char* key) {
  String s(key);
  for (auto& c : s) if (!isalnum((int)c) && c != '_' && c != '-') c = '_';
  return s;
}

String Cache::bodyPath(const char* key) { return "/cache/" + sanitize(key) + ".bin"; }
String Cache::metaPath(const char* key) { return "/cache/" + sanitize(key) + ".m";   }

bool Cache::put(const char* key, const String& body, int status, uint32_t nowEpoch) {
  File b = LittleFS.open(bodyPath(key), "w");
  if (!b) return false;
  b.print(body);
  b.close();

  File m = LittleFS.open(metaPath(key), "w");
  if (!m) return false;
  m.printf("%u %d %u", nowEpoch, status, (unsigned)body.length());
  m.close();
  return true;
}

CacheMeta Cache::stat(const char* key) {
  CacheMeta meta;
  File m = LittleFS.open(metaPath(key), "r");
  if (!m) return meta;
  unsigned t = 0, sz = 0; int st = 0;
  if (sscanf(m.readString().c_str(), "%u %d %u", &t, &st, &sz) >= 2) {
    meta.found = true; meta.fetchedAt = t; meta.status = st; meta.size = sz;
  }
  m.close();
  return meta;
}

bool Cache::get(const char* key, String& outBody, CacheMeta& outMeta) {
  outMeta = stat(key);
  if (!outMeta.found) return false;
  File b = LittleFS.open(bodyPath(key), "r");
  if (!b) { outMeta.found = false; return false; }
  outBody = b.readString();
  b.close();
  return true;
}
