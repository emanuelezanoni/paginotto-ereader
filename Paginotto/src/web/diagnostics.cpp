#include "src/web/diagnostics.h"

#include "src/state.h"
#include "src/web/chrome.h"
#include "src/diagnostics/log.h"

namespace {

constexpr uint8_t kPersistentSlots = 24;

String diagnosticsText() {
  String out = "=== PERSISTENT LOG ===\n";
  out += Diagnostics::persistentText();
  out += "\n=== CURRENT SESSION TRACE ===\n";
  out += Diagnostics::runtimeText();
  return out;
}

void handleDiagnostics() {
  String out = webPageStart(
      "Diagnostics", "Persistent warnings and error records.", webNav("diagnostics"));
  out += "<div class='card'><h2>Device diagnostics</h2>"
      "<p class='muted'>The device keeps the latest 24 persistent records and "
         "a larger current-session trace in RAM. Records contain timing, "
         "reset/error stage, free heap and Wi-Fi status; "
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
  for (uint8_t i = 0; i < kPersistentSlots; i++) {
    char key[8];
    snprintf(key, sizeof(key), "l%u", i);
    prefs.remove(key);
  }
  prefs.remove("next");
  prefs.remove("count");
  prefs.remove("boot_count");
  prefs.remove("phase");
  Diagnostics::clearRuntime();
  server.sendHeader("Location", "/diagnostics");
  server.send(303, "text/plain", "");
}

}  // namespace

void registerDiagnosticsRoutes() {
  server.on("/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/diagnostics.txt", HTTP_GET, handleDiagnosticsText);
  server.on("/diagnostics/clear", HTTP_POST, handleDiagnosticsClear);
}
