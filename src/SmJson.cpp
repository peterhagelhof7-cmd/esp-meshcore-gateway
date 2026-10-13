#include "SmJson.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int smFlatten(const char* json, size_t len, SmValue out[], int maxOut, char* err, size_t errLen) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) {
    snprintf(err, errLen, "JSON: %s", e.c_str());
    return -1;
  }
  JsonObject root = doc.as<JsonObject>();
  if (root.isNull()) {
    snprintf(err, errLen, "JSON: kein Objekt");
    return -1;
  }
  int n = 0;
  for (JsonPair sensor : root) {
    JsonObject obj = sensor.value().as<JsonObject>();
    if (obj.isNull()) continue;
    JsonVariant valid = obj["valid"];
    if (valid.is<bool>() && !valid.as<bool>()) continue;
    const char* sname = obj["name"] | "";
    for (JsonPair field : obj) {
      const char* key = field.key().c_str();
      if (!strcmp(key, "valid") || !strcmp(key, "name")) continue;
      JsonVariant v = field.value();
      if (!(v.is<float>() || v.is<int>() || v.is<long>())) continue;   // null, Text, bool
      float f = v.as<float>();
      if (isnan(f) || isinf(f)) continue;
      if (n >= maxOut) return n;
      snprintf(out[n].path, sizeof(out[n].path), "%s.%s", sensor.key().c_str(), key);
      snprintf(out[n].sensorName, sizeof(out[n].sensorName), "%s", sname);
      out[n].value = f;
      n++;
    }
  }
  return n;
}
