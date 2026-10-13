// ESP-MeshCore-Gateway - Einstiegspunkt.
//
// Basis ist examples/simple_repeater/main.cpp aus MeshCore repeater-v1.17.1
// (Ablauf von setup()/loop() unverändert übernommen, plattformfremde Zweige
// für nRF52/RP2040/STM32/Ethernet/SenseCAP entfernt). Ergänzt um:
//   - Gateway-Konfig (NVS), WLAN-Manager mit Installer-AP, Webserver
//   - Status-Snapshot für den Web-Task (docs/entscheidungen.md E4)
//   - Sleep-Sperre: mit aktivem WLAN darf der Knoten nie schlafen

#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "GatewayMesh.h"

#include "ChannelStore.h"
#include "CliBridge.h"
#include "GatewayConfig.h"
#include "LogBuffer.h"
#include "MeshActions.h"
#include "Profile.h"
#include "GatewayStatus.h"
#include "NetworkManager.h"
#include "WebUi.h"

#if !defined(ESP32)
  #error "Das Gateway ist nur für ESP32-S3 (Heltec V4) gedacht"
#endif
#if ESP32_CPU_FREQ != 240
  #error "ESP32_CPU_FREQ muss 240 sein (build_unflags in platformio.ini prüfen)"
#endif
// Werks-Defaults nach hansemesh.de (E15), müssen zum Profil passen.
#if LORA_TX_POWER != 17 || LORA_CR != 8
  #error "LORA_TX_POWER muss 17 und LORA_CR 8 sein (build_unflags in platformio.ini prüfen)"
#endif

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(board, display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

GatewayMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];

// stats-core liest die Akkuspannung; auf dem V4 blockiert das 10 ms
// (HeltecV4Board::getBattMilliVolts, delay(10)) -> nicht öfter als nötig.
static const unsigned long STATUS_UPDATE_INTERVAL_MS = 5000;
static unsigned long next_status_update = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Früh, damit schon die ersten Pakete/Meldungen im Web-Log landen.
  console_log.begin();
  packet_log.begin();
  channel_log.begin();
  cli_bridge::begin();
  mesh_actions::begin();
  channel_store::begin();

  board.begin();
  Serial.printf("MeshCore Gateway %s (MeshCore %s), CPU %u MHz\n",
                GATEWAY_VERSION, FIRMWARE_VERSION, getCpuFrequencyMhz());

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    Serial.println("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;

  sensors.begin();

  // Erststart = noch keine MeshCore-Prefs auf dem Dateisystem (gleiche Prüfung
  // wie CommonCLI::loadPrefs). Dann wird nach dem Start das Empfehlungsprofil
  // angewendet (docs/entscheidungen.md E15).
  bool first_boot = !SPIFFS.exists("/prefs.json") && !SPIFFS.exists("/com_prefs");

  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

  // --- Gateway-Erweiterungen ---
  board.setInhibitSleep(true);   // WLAN + Webserver vertragen keinen Light-Sleep
  gateway_status::begin();
  gateway_status::update();      // erster Snapshot, bevor der Webserver Anfragen annimmt
  gw_config.begin();
  network.begin();
  web_ui::begin();
  console_log.push('s', the_mesh.getRTCClock()->getCurrentTime(), "Gateway gestartet, MeshCore " FIRMWARE_VERSION);

  if (first_boot) {
    const char* cmds[cli_bridge::MAX_BATCH];
    size_t n = 0;
    for (size_t i = 0; i < profile::COUNT && n < cli_bridge::MAX_BATCH; i++) cmds[n++] = profile::ENTRIES[i].cmd;
    console_log.push('s', the_mesh.getRTCClock()->getCurrentTime(), "Erststart: wende Profil an");
    console_log.push('s', the_mesh.getRTCClock()->getCurrentTime(), profile::NAME);
    cli_bridge::submit(cmds, n, false);
  }

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();
}

void loop() {
  // Handle Serial CLI (unverändert aus simple_repeater)
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    Serial.print('\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    reply[0] = 0;
    // Serielle Befehle zusätzlich in der Webkonsole mitprotokollieren.
    uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
    console_log.push('c', now, command);
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
      console_log.pushLines('r', now, reply);
    }

    command[0] = 0;  // reset command buffer
  }

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();

  // --- Gateway-Erweiterungen ---
  mesh_actions::process();
  if (cli_bridge::process()) {
    next_status_update = millis();   // geänderte Einstellungen sofort im Snapshot
  }
  network.loop();
  web_ui::loop();
  if (network.takeTimeSynced()) {
    // ESP32RTCClock nutzt die Systemzeit, SNTP hat sie schon gestellt. Bei einer
    // externen RTC (AutoDiscoverRTCClock, z. B. DS3231) diese mitziehen.
    uint32_t t = (uint32_t)time(nullptr);
    rtc_clock.setCurrentTime(t);
    static bool logged = false;   // SNTP meldet sich stündlich, nur das erste Mal loggen
    if (!logged) {
      console_log.push('s', t, "Uhr per NTP gestellt");
      logged = true;
    }
  }
  if ((long)(millis() - next_status_update) >= 0) {
    gateway_status::update();
    next_status_update = millis() + STATUS_UPDATE_INTERVAL_MS;
  }

  // Powersaving: board.sleep() kehrt wegen setInhibitSleep(true) sofort zurück
  // (ESP32Board::sleep -> delay(1)), der Upstream-Aufruf bleibt trotzdem drin.
  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasPendingWork()) {
    board.sleep(30);
  }
}
