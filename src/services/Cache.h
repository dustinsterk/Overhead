#pragma once
#include <Arduino.h>

// services/Cache — LittleFS blob cache (spec §4, §9). Each entry stores the raw
// body plus a tiny sidecar of {fetchedAt, status} so every provider can serve a
// last-good value with an age + a stale badge when the network is down.
struct CacheMeta {
  bool     found     = false;
  uint32_t fetchedAt = 0;   // unix seconds
  int      status    = 0;   // last HTTP status
  size_t   size      = 0;
};

class Cache {
public:
  bool begin();             // ensure the /cache directory exists

  bool put(const char* key, const String& body, int status, uint32_t nowEpoch);
  bool get(const char* key, String& outBody, CacheMeta& outMeta);
  CacheMeta stat(const char* key);

private:
  String sanitize(const char* key);
  String bodyPath(const char* key);
  String metaPath(const char* key);
};
