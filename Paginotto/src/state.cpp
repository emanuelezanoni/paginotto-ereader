#include "src/state.h"

// ============================================================================
//  Display + adapter (definitions; adapter class lives in display.h)
// ============================================================================
EInkDisplay display;

// ============================================================================
//  Globals — instances of the shared state structs declared in state.h.
//  Pure definitions; no logic lives in this file.
// ============================================================================
WebServer server(80);
Preferences prefs;

static String s_defaultApSsid = "PAGINOTTO-";

String hotspotSsid() { return prefs.getString("ap_ssid", s_defaultApSsid); }
String hotspotPass() { return prefs.getString("ap_pass", "paginotto"); }

bool saveHotspotConfig(const String& ssid, const String& pass) {
  if (ssid.length() < 1 || ssid.length() > 32 || pass.length() < 8 || pass.length() > 63) return false;
  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", pass);
  return true;
}
