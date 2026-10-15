#include "NetCheck.h"

#include <WiFi.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/etharp.h>
#include <lwip/ip_addr.h>
#include <lwip/tcpip.h>
#include <ping/ping_sock.h>

namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
volatile net_check::State s_state = net_check::State::IDLE;
char s_detail[48] = {0};
uint32_t s_target = 0;

void finish(net_check::State st, const char* detail) {
  portENTER_CRITICAL(&s_mux);
  strlcpy(s_detail, detail ? detail : "", sizeof(s_detail));
  s_state = st;
  portEXIT_CRITICAL(&s_mux);
}

// --- ARP im lwIP-Thread (etharp_* ist nicht thread-sicher) ---
struct ArpJob {
  struct netif* nif;
  ip4_addr_t ip;
  bool query;          // false = Anfrage senden, true = Tabelle lesen
  bool found;
  uint8_t mac[6];
  SemaphoreHandle_t done;
};

void arpInLwip(void* arg) {
  ArpJob* j = (ArpJob*)arg;
  if (!j->query) {
    etharp_request(j->nif, &j->ip);
  } else {
    struct eth_addr* eth = nullptr;
    const ip4_addr_t* ipr = nullptr;
    j->found = etharp_find_addr(j->nif, &j->ip, &eth, &ipr) >= 0 && eth;
    if (j->found) memcpy(j->mac, eth->addr, 6);
  }
  xSemaphoreGive(j->done);
}

bool runArp(ArpJob& j) {
  if (tcpip_callback(arpInLwip, &j) != ERR_OK) return false;
  return xSemaphoreTake(j.done, pdMS_TO_TICKS(1000)) == pdTRUE;
}

void probeTask(void* arg) {
  IPAddress ip((uint32_t)s_target);
  char detail[48] = {0};

  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    finish(net_check::State::UNCHECKABLE, "Gateway nicht im WLAN (Client)");
    vTaskDelete(nullptr);
    return;
  }
  IPAddress me = WiFi.localIP(), mask = WiFi.subnetMask();
  if (ip == me) {
    finish(net_check::State::OWN, "aktuelle Adresse des Gateways");
    vTaskDelete(nullptr);
    return;
  }
  if (((uint32_t)ip & (uint32_t)mask) != ((uint32_t)me & (uint32_t)mask)) {
    snprintf(detail, sizeof(detail), "anderes Subnetz als %s", me.toString().c_str());
    finish(net_check::State::UNCHECKABLE, detail);
    vTaskDelete(nullptr);
    return;
  }

  bool present = false;
  esp_netif_t* h = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  struct netif* nif = h ? (struct netif*)esp_netif_get_netif_impl(h) : nullptr;
  if (nif) {
    ArpJob j = {};
    j.nif = nif;
    IP4_ADDR(&j.ip, ip[0], ip[1], ip[2], ip[3]);
    j.done = xSemaphoreCreateBinary();
    if (j.done) {
      for (int k = 0; k < 3 && !present; k++) {
        j.query = false;
        runArp(j);
        vTaskDelay(pdMS_TO_TICKS(400));
        j.query = true;
        if (runArp(j) && j.found) {
          present = true;
          snprintf(detail, sizeof(detail), "antwortet auf ARP (MAC %02x:%02x:%02x:%02x:%02x:%02x)",
                   j.mac[0], j.mac[1], j.mac[2], j.mac[3], j.mac[4], j.mac[5]);
        }
      }
      vSemaphoreDelete(j.done);
    }
  }
  if (!present) {
    uint32_t rtt = 0;
    char ipStr[16], err[48];
    if (net_check::ping(ip.toString().c_str(), rtt, ipStr, sizeof(ipStr), err, sizeof(err))) {
      present = true;
      snprintf(detail, sizeof(detail), "antwortet auf Ping (%u ms)", (unsigned)rtt);
    }
  }
  finish(present ? net_check::State::PRESENT : net_check::State::ABSENT,
         present ? detail : "keine Antwort auf ARP und Ping");
  vTaskDelete(nullptr);
}

// --- Ping (aus SmPoller, E25) ---
struct PingCtx {
  SemaphoreHandle_t done;
  uint32_t replies;
  uint32_t bestRtt;
};

void onPingSuccess(esp_ping_handle_t hdl, void* args) {
  PingCtx* c = (PingCtx*)args;
  uint32_t t = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &t, sizeof(t));
  if (c->replies == 0 || t < c->bestRtt) c->bestRtt = t;
  c->replies++;
}
void onPingEnd(esp_ping_handle_t hdl, void* args) {
  xSemaphoreGive(((PingCtx*)args)->done);
}

}  // namespace

bool net_check::probe(IPAddress ip) {
  portENTER_CRITICAL(&s_mux);
  bool busy = s_state == State::RUNNING;
  if (!busy) {
    s_state = State::RUNNING;
    s_detail[0] = 0;
    s_target = (uint32_t)ip;
  }
  portEXIT_CRITICAL(&s_mux);
  if (busy) return false;
  // Core 0 wie der WLAN-Stack; 4 KB reichen für ARP + esp_ping
  if (xTaskCreatePinnedToCore(probeTask, "netcheck", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
    finish(State::UNCHECKABLE, "kein Speicher");
  }
  return true;
}

net_check::State net_check::result(char* detail, size_t detailLen) {
  portENTER_CRITICAL(&s_mux);
  State st = s_state;
  if (detail && detailLen) strlcpy(detail, s_detail, detailLen);
  portEXIT_CRITICAL(&s_mux);
  return st;
}

const char* net_check::stateName(State s) {
  switch (s) {
    case State::RUNNING: return "running";
    case State::PRESENT: return "present";
    case State::ABSENT: return "absent";
    case State::OWN: return "own";
    case State::UNCHECKABLE: return "uncheckable";
    default: return "idle";
  }
}

bool net_check::validMask(IPAddress m) {
  uint32_t v = ((uint32_t)m[0] << 24) | ((uint32_t)m[1] << 16) | ((uint32_t)m[2] << 8) | m[3];
  if (v == 0) return false;
  uint32_t inv = ~v;
  if ((inv & (inv + 1)) != 0) return false;   // Einsen nicht zusammenhängend
  int bits = 0;
  for (uint32_t x = v; x; x <<= 1) bits++;
  return bits >= 8 && bits <= 30;
}

// Blockiert bis ca. 1,5 s (3 Pakete à 200 ms Abstand, 1 s Timeout). true =
// mindestens eine Antwort.
bool net_check::ping(const char* host, uint32_t& rttMs, char* ipOut, size_t ipLen, char* err, size_t errLen) {
  IPAddress ip;
  if (!ip.fromString(host)) {
    if (!WiFi.hostByName(host, ip) || ip == IPAddress(0, 0, 0, 0)) {
      snprintf(err, errLen, "Name nicht auflösbar");
      return false;
    }
  }
  snprintf(ipOut, ipLen, "%s", ip.toString().c_str());

  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.count = 3;
  cfg.interval_ms = 200;
  cfg.timeout_ms = 1000;
  cfg.task_stack_size = 3072;
  IP_ADDR4(&cfg.target_addr, ip[0], ip[1], ip[2], ip[3]);

  PingCtx ctx;
  ctx.done = xSemaphoreCreateBinary();
  ctx.replies = 0;
  ctx.bestRtt = 0;
  if (!ctx.done) { snprintf(err, errLen, "kein Speicher"); return false; }

  esp_ping_callbacks_t cbs = {};
  cbs.cb_args = &ctx;
  cbs.on_ping_success = onPingSuccess;
  cbs.on_ping_end = onPingEnd;
  esp_ping_handle_t h = nullptr;
  if (esp_ping_new_session(&cfg, &cbs, &h) != ESP_OK) {
    vSemaphoreDelete(ctx.done);
    snprintf(err, errLen, "Ping-Sitzung fehlgeschlagen");
    return false;
  }
  esp_ping_start(h);
  bool ended = xSemaphoreTake(ctx.done, pdMS_TO_TICKS(5000)) == pdTRUE;
  esp_ping_stop(h);
  esp_ping_delete_session(h);
  vSemaphoreDelete(ctx.done);
  if (!ended) { snprintf(err, errLen, "Ping hängt"); return false; }
  if (ctx.replies == 0) { snprintf(err, errLen, "keine Antwort"); return false; }
  rttMs = ctx.bestRtt;
  err[0] = 0;
  return true;
}
