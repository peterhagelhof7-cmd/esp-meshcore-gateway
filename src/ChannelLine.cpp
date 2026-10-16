#include "ChannelLine.h"

#include <stdlib.h>
#include <string.h>

namespace chan_line {

bool parse(char* buf, Parsed& out) {
  memset(&out, 0, sizeof(out));
  char* f[5];
  char* p = buf;
  for (int i = 0; i < 4; i++) {
    char* t = strchr(p, '\t');
    if (!t) return false;
    *t = 0;
    f[i] = p;
    p = t + 1;
  }
  f[4] = p;   // Rest = Nachricht (darf weitere Tabs nicht enthalten, GatewayMesh ersetzt sie)

  char* end;
  long idx = strtol(f[0], &end, 10);
  if (end == f[0] || *end) return false;
  out.idx = (int)idx;
  out.channel = f[1];
  out.hasSnr = f[2][0] != 0;
  out.snr = out.hasSnr ? strtof(f[2], nullptr) : 0;
  out.hops = atoi(f[3]);

  char* sep = strstr(f[4], ": ");
  if (sep) {
    *sep = 0;
    out.sender = f[4];
    out.text = sep + 2;
  } else {
    out.sender = "";
    out.text = f[4];
  }
  return true;
}

size_t utf8Cut(const char* text, size_t maxBytes) {
  if (!text) return 0;
  size_t n = strlen(text);
  if (n <= maxBytes) return n;
  n = maxBytes;
  while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) n--;   // Folgebyte -> zurück zum Zeichenanfang
  return n;
}

}  // namespace chan_line
