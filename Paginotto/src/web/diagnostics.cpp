#include "src/web/diagnostics.h"

#include "src/state.h"
#include "src/web/chrome.h"

namespace {

constexpr uint8_t kSlots = 12;

String diagnosticsText() {
  String out;
  uint8_t count = prefs.getUChar("count", 0);
  uint8_t next = prefs.getUChar("next", 0) % kSlots;
  uint8_t first = (count < kSlots) ? 0 : next;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t slot = (uint8_t)((first + i) % kSlots);
    char key[8];
    snprintf(key, sizeof(key), "l%u", slot);
    String line = prefs.getString(key, "");
    if (line.length()) out += line + "\n";
  }
  return out;
}

void handleDiagnostics() {
  String out = webPageStart(
      "Diagnostics", "Persistent warnings and error records.", webNav("diagnostics"));
  out += "<div class='card'><h2>Device diagnostics</h2>"
         "<p class='muted'>The device keeps the latest 12 diagnostic records. "
         "Records contain timing, reset/error stage, free heap and Wi-Fi status; "
         "credentials and download URLs are not stored.</p>"
         "<div class='actions'><a class='btn secondary' href='/diagnostics.txt'>Download log</a>"
         "<form method='POST' action='/diagnostics/clear' style='display:inline'>"
         "<button type='submit'>Clear log</button></form></div>"
         "<pre style='white-space:pre-wrap;overflow:auto;max-height:60vh'>";
  out += htmlEscape(diagnosticsText());
  out += "</pre></div>";
  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

void handleDiagnosticsText() {
  server.send(200, "text/plain; charset=utf-8", diagnosticsText());
}

void handleDiagnosticsClear() {
  for (uint8_t i = 0; i < kSlots; i++) {
    char key[8];
    snprintf(key, sizeof(key), "l%u", i);
    prefs.remove(key);
  }
  prefs.remove("next");
  prefs.remove("count");
  prefs.remove("boot_count");
  server.sendHeader("Location", "/diagnostics");
  server.send(303, "text/plain", "");
}

}  // namespace

void registerDiagnosticsRoutes() {
  server.on("/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/diagnostics.txt", HTTP_GET, handleDiagnosticsText);
  server.on("/diagnostics/clear", HTTP_POST, handleDiagnosticsClear);
}
