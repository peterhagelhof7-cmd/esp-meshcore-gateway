#include "OtaUpdate.h"

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>

#include "FwMarker.h"
#include "LogBuffer.h"

#ifndef GATEWAY_PROJECT_ID
  #error "GATEWAY_PROJECT_ID fehlt (platformio.ini)"
#endif

// Das Merkmal muss im Binary landen: in begin() einmal ausgeben, damit der
// Linker es nicht verwirft.
static const char FW_MARKER[] = "MCGW-FW-ID:" GATEWAY_PROJECT_ID ":" GATEWAY_VERSION ":MCGW-FW-END";

// Arduino-Core-Hook (esp32-hal-misc.c): true = die App markiert sich selbst
// als gültig, statt dass der Core das sofort beim Start tut. extern "C" ist
// Pflicht: der Core deklariert die schwache Funktion in C, ohne C-Linkage
// entsteht ein anderes (C++-)Symbol und der Core bestätigt jede neue Firmware
// sofort (auf HW gefunden: "gültig" nach 8 s statt "Prüflauf").
extern "C" bool verifyRollbackLater() { return true; }

namespace {
struct UploadState {
  AsyncWebServerRequest* owner = nullptr;   // laufender Upload (nur einer)
  uint32_t lastDataMs = 0;
  bool active = false;
  bool failed = false;
  bool finished = false;   // Update.end() erfolgreich
  bool force = false;
  size_t written = 0;
  String error;
  FwMarkerScanner scanner;
};
UploadState s_up;
bool s_markedValid = false;
}

void ota_update::begin() {
  Serial.print("[OTA] ");
  Serial.println(FW_MARKER);
  const esp_partition_t* run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
    console_log.push('s', 0, "Neue Firmware im Prüflauf - wird nach 60 s stabiler Laufzeit bestätigt");
  }
  // Rollback erkennen: der Bootloader hat die andere Partition verworfen und
  // dieser Start folgt auf einen Absturz -> die neue Firmware ist abgestürzt.
  const esp_partition_t* bad = esp_ota_get_last_invalid_partition();
  esp_reset_reason_t rr = esp_reset_reason();
  if (bad && (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT)) {
    char msg[100];
    snprintf(msg, sizeof(msg), "ROLLBACK: neue Firmware auf %s ist im Prüflauf abgestürzt, vorherige Version läuft", bad->label);
    console_log.push('s', 0, msg);
    Serial.printf("[OTA] %s\n", msg);
  }
}

const char* ota_update::projectId() { return GATEWAY_PROJECT_ID; }

const char* ota_update::runningPartition() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  return run ? run->label : "?";
}

const char* ota_update::imageState() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK) return "unbekannt";
  switch (st) {
    case ESP_OTA_IMG_NEW: return "neu";
    case ESP_OTA_IMG_PENDING_VERIFY: return "Prüflauf";
    case ESP_OTA_IMG_VALID: return "gültig";
    case ESP_OTA_IMG_INVALID: return "ungültig";
    case ESP_OTA_IMG_ABORTED: return "abgebrochen";
    default: return "undefiniert";
  }
}

void ota_update::loop() {
  if (s_markedValid || millis() < OTA_VALID_AFTER_MS) return;
  s_markedValid = true;
  const esp_partition_t* run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    console_log.push('s', 0, "Firmware nach 60 s stabiler Laufzeit als gültig bestätigt");
    Serial.println("[OTA] Firmware als gültig markiert");
  } else {
    // Per USB geflasht oder schon bestätigt: Aufruf ist harmlos.
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

static void fail(const String& why) {
  if (!s_up.failed) {
    s_up.failed = true;
    s_up.error = why;
    if (Update.isRunning()) Update.abort();
    Serial.println("[OTA] Abbruch: " + why);
  }
}

void ota_update::onUpload(AsyncWebServerRequest* req, const String& filename, size_t index,
                          uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    // Hängt ein alter Upload (Client weg ohne Disconnect-Meldung)? Nach 30 s
    // ohne Daten freigeben.
    if (s_up.active && millis() - s_up.lastDataMs > 30000) {
      if (Update.isRunning()) Update.abort();
      s_up.active = false;
      s_up.owner = nullptr;
    }
    if (s_up.active) return;   // paralleler Upload: wird in onRequestDone abgelehnt
    s_up.active = true;
    s_up.owner = req;
    s_up.lastDataMs = millis();
    req->onDisconnect([req]() {
      // Verbindungsabbruch mitten im Upload: Partition freigeben.
      if (s_up.owner == req) {
        if (Update.isRunning()) Update.abort();
        s_up.active = false;
        s_up.owner = nullptr;
      }
    });
    s_up.failed = false;
    s_up.finished = false;
    s_up.error = "";
    s_up.written = 0;
    s_up.scanner.reset();
    // Das Feld "force" steht im Formular VOR der Datei (Multipart wird in
    // Reihenfolge geparst, siehe Memory "OTA identity + downgrade check").
    s_up.force = req->hasParam("force", true) && req->getParam("force", true)->value() == "1";
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      fail(String("Update.begin: ") + Update.errorString());
    } else {
      Serial.printf("[OTA] Upload %s gestartet\n", filename.c_str());
    }
  }
  if (s_up.owner != req || s_up.failed) return;
  s_up.lastDataMs = millis();

  if (len) {
    s_up.scanner.feed(data, len);
    if (Update.write(data, len) != len) {
      fail(String("Schreibfehler: ") + Update.errorString());
      return;
    }
    s_up.written += len;
  }

  if (final) {
    if (!s_up.scanner.found()) {
      fail("kein Gateway-Firmware-Merkmal gefunden (falsche Datei?)");
      return;
    }
    if (strcmp(s_up.scanner.projectId(), GATEWAY_PROJECT_ID) != 0) {
      fail(String("falsche Firmware: ") + s_up.scanner.projectId() + ", dieses Gerät ist " + GATEWAY_PROJECT_ID);
      return;
    }
    if (fwVersionCompare(s_up.scanner.version(), GATEWAY_VERSION) < 0 && !s_up.force) {
      fail(String("Downgrade ") + GATEWAY_VERSION + " -> " + s_up.scanner.version() +
           " abgelehnt (\"Downgrade erzwingen\" setzen)");
      return;
    }
    if (!Update.end(true)) {
      fail(String("Update.end: ") + Update.errorString());
      return;
    }
    s_up.finished = true;
    Serial.printf("[OTA] OK, %u Bytes, Version %s\n", (unsigned)s_up.written, s_up.scanner.version());
  }
}

bool ota_update::onRequestDone(AsyncWebServerRequest* req) {
  if (s_up.active && s_up.owner != req) {
    req->send(409, "application/json", "{\"error\":\"Es läuft bereits ein Update\"}");
    return false;
  }
  bool ok = s_up.active && s_up.owner == req && !s_up.failed && s_up.finished;
  String body;
  if (ok) {
    body = String("{\"ok\":true,\"version\":\"") + s_up.scanner.version() + "\",\"bytes\":" + s_up.written + "}";
    char line[96];
    snprintf(line, sizeof(line), "Firmware-Update auf %s geschrieben, Neustart", s_up.scanner.version());
    console_log.push('s', 0, line);
  } else {
    String e = s_up.active ? s_up.error : String("keine Datei empfangen");
    if (e.isEmpty()) e = "Upload unvollständig";
    e.replace("\"", "'");
    body = "{\"error\":\"" + e + "\"}";
    if (Update.isRunning()) Update.abort();
  }
  if (s_up.owner == req || !s_up.active) {
    s_up.active = false;
    s_up.owner = nullptr;
  }
  req->send(ok ? 200 : 400, "application/json", body);
  return ok;
}
