#include "src/web/wifi.h"

#include <WiFi.h>

#include "src/hal/wifi.h"
#include "src/state.h"
#include "src/storage/wifi_creds.h"
#include "src/web/chrome.h"
#include "src/web/upload.h"  // notifyUploadActivity

static bool validSsid(const String& value) { return value.length() > 0 && value.length() <= 32; }
static bool validPass(const String& value) { return value.length() == 0 || (value.length() >= 8 && value.length() <= 63); }
static bool canAddNetwork(const String& ssid) {
  if (WifiCreds::count() < WifiCreds::kMaxNetworks) return true;
  for (uint8_t i = 0; i < WifiCreds::count(); i++) {
    if (WifiCreds::network(i).ssid == ssid) return true;
  }
  return false;
}
static constexpr uint32_t kWebScanTimeoutMs = 60000;
static bool s_scanActive = false;
static uint32_t s_scanStartedMs = 0;

static String notice() { return server.hasArg("msg") ? htmlEscape(server.arg("msg")) : ""; }
static String urlEncode(const String& in) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out += c;
    else { out += '%'; out += hex[(uint8_t)c >> 4]; out += hex[(uint8_t)c & 15]; }
  }
  return out;
}
static void redirect(const String& msg) {
  server.sendHeader("Location", String("/wifi?msg=") + urlEncode(msg));
  server.send(302, "text/plain", "");
}

static String jsonEscape(const String& in) {
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

static void startNearbyScan() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
  s_scanActive = true;
  s_scanStartedMs = millis();
}

static void handleWifiScan() {
  notifyUploadActivity();
  if (!s_scanActive) startNearbyScan();

  int found = WiFi.scanComplete();
  if (found == -1) {
    if ((uint32_t)(millis() - s_scanStartedMs) >= kWebScanTimeoutMs) {
      WiFi.scanDelete();
      s_scanActive = false;
      server.send(200, "application/json", "{\"state\":\"timeout\",\"networks\":[]}");
      return;
    }
    server.send(200, "application/json", "{\"state\":\"scanning\",\"networks\":[]}");
    return;
  }
  if (found < 0) {
    WiFi.scanDelete();
    s_scanActive = false;
    server.send(200, "application/json", "{\"state\":\"error\",\"networks\":[]}");
    return;
  }

  String json = "{\"state\":\"complete\",\"networks\":[";
  bool first = true;
  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    if (!first) json += ',';
    first = false;
    json += "\"" + jsonEscape(ssid) + "\"";
  }
  json += "]}";
  WiFi.scanDelete();
  s_scanActive = false;
  server.send(200, "application/json", json);
}

static void handleWifi() {
  // Start asynchronously. The page must render immediately; the browser
  // polls /wifi/scan and fills the visible list when results arrive.
  startNearbyScan();
  String out = webPageStart("Wi-Fi", "Configure hotspot and saved networks.", webNav("wifi"));
  String msg = notice();
  if (msg.length()) out += "<div class='banner-ok'>" + msg + "</div>";

  out += "<div class='card'><h2>Device hotspot</h2><p class='muted'>Used for WebUI. Password is hidden until changed.</p>"
         "<form method='POST' action='/wifi/hotspot' class='stack' style='margin-top:12px'>"
         "<div><label for='apssid'>Hotspot name</label><input id='apssid' name='ssid' maxlength='32' value='" + htmlEscape(hotspotSsid()) + "' required></div>"
         "<div><label for='appass'>Hotspot password</label><input id='appass' type='password' name='pass' minlength='8' maxlength='63' placeholder='Leave unchanged'></div>"
         "<div class='hint'>8–63 characters. Changing network settings may disconnect this browser.</div>"
         "<div class='actions'><button type='submit'>Save hotspot</button></div></form></div>";

  out += "<div class='card'><h2>Wi-Fi networks</h2><p class='muted'>Manage saved networks, connect to a nearby network, or add a hidden network manually. Passwords stay masked.</p>"
         "<h3>Saved networks</h3>";
  uint8_t n = WifiCreds::count();
  if (!n) out += "<p class='status idle'>No saved networks.</p>";
  else {
    out += "<ul class='list'>";
    for (uint8_t i = 0; i < n; i++) {
      WifiCreds::Network net = WifiCreds::network(i);
      out += "<li><div class='row'><div><b>" + htmlEscape(net.ssid) + "</b><div class='muted'>Password saved</div></div>"
             "<form method='POST' action='/wifi/remove'><input type='hidden' name='index' value='" + String(i) + "'><button class='danger' type='submit' onclick=\"return confirm('Remove saved network?')\">Remove</button></form></div></li>";
    }
    out += "</ul>";
  }
  out += "<h3>Nearby networks</h3>"
         "<form method='POST' action='/wifi/add' class='stack' style='margin-top:12px'>"
         "<div><label for='ssid'>Network name (SSID)</label><select id='ssid' name='ssid'><option value=''>Loading nearby networks…</option></select></div>"
         "<div id='wifi-scan-status' class='status idle'>Searching for nearby networks…</div>"
         "<div class='hint'>Only networks found nearby can be selected and tested.</div>"
         "<div><label for='pass'>Network password</label><input id='pass' type='password' name='pass' maxlength='63' autocomplete='new-password'></div>"
         "<div class='actions'><button type='submit'>Test and save network</button></div></form>"
         "<script>(function(){const s=document.getElementById('ssid'),st=document.getElementById('wifi-scan-status');"
         "async function poll(){try{const r=await fetch('/wifi/scan',{cache:'no-store'}),d=await r.json();"
         "if(d.networks){const old=s.value;s.innerHTML='';d.networks.forEach(function(n){const o=document.createElement('option');o.value=n;o.textContent=n;s.appendChild(o);});if(old)s.value=old;}"
         "if(d.state==='scanning'){st.textContent='Searching for nearby networks…';st.className='status idle';setTimeout(poll,1000);}"
         "else if(d.state==='complete'){st.textContent=d.networks.length?'Networks found.':'No nearby networks found.';st.className='status '+(d.networks.length?'ok':'idle');}"
         "else if(d.state==='timeout'){st.textContent='Search timed out. No network selected.';st.className='status idle';}"
         "else{st.textContent='Network scan failed. Try again.';st.className='status idle';}"
         "}catch(e){st.textContent='Network scan unavailable. Try again.';st.className='status idle';}}poll();})();</script>"
         "<h3>Add manually</h3>"
         "<p class='muted'>Use this for hidden networks or a phone hotspot. The connection is not tested now.</p>"
         "<form method='POST' action='/wifi/add-manual' class='stack' style='margin-top:12px'>"
         "<div><label for='manual-ssid'>Network name (SSID)</label><input id='manual-ssid' name='ssid' maxlength='32' required></div>"
         "<div><label for='manual-pass'>Network password</label><input id='manual-pass' type='password' name='pass' maxlength='63'></div>"
         "<div class='actions'><button type='submit'>Save network</button></div></form></div>";
  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleHotspot() {
  String ssid = server.arg("ssid"); ssid.trim();
  String pass = server.arg("pass");
  if (!pass.length()) pass = hotspotPass();
  if (!saveHotspotConfig(ssid, pass)) { redirect("Invalid hotspot name or password (name: 1–32; password: 8–63 characters)."); return; }
  redirect("Hotspot saved. It will be used next time WebUI starts.");
}

static void handleAdd() {
  String ssid = server.arg("ssid"); ssid.trim();
  String pass = server.arg("pass");
  if (!validSsid(ssid) || !validPass(pass)) { redirect("Invalid network name or password."); return; }
  if (!canAddNetwork(ssid)) { redirect("Saved network limit reached. Remove one first."); return; }

  // Keep SoftAP alive while the shared station test runs. Credentials are
  // written only after the attempt succeeds.
  WifiStaResult result = wifiStaTest(ssid, pass);
  if (result == WifiStaResult::NoSsidAvailable) {
    redirect("Network not found. Credentials were not saved."); return;
  }
  if (result == WifiStaResult::AuthenticationFailed) {
    redirect("Password rejected or connection refused. Credentials were not saved."); return;
  }
  if (result == WifiStaResult::TimedOut) {
    redirect("Connection timed out. Credentials were not saved."); return;
  }
  if (result != WifiStaResult::Connected) {
    redirect("Network test failed. Credentials were not saved."); return;
  }
  WifiCreds::add(ssid, pass);
  redirect("Network connected and saved.");
}

static void handleAddManual() {
  String ssid = server.arg("ssid"); ssid.trim();
  String pass = server.arg("pass");
  if (!validSsid(ssid) || !validPass(pass)) { redirect("Invalid network name or password."); return; }
  if (!canAddNetwork(ssid)) { redirect("Saved network limit reached. Remove one first."); return; }
  if (!WifiCreds::add(ssid, pass)) { redirect("Could not save network."); return; }
  redirect("Network saved. Test it from the device.");
}

static void handleRemove() {
  int index = server.arg("index").toInt();
  if (index < 0 || index >= WifiCreds::count()) { redirect("Saved network not found."); return; }
  WifiCreds::remove((uint8_t)index);
  redirect("Saved network removed.");
}

void registerWifiRoutes() {
  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/hotspot", HTTP_POST, handleHotspot);
  server.on("/wifi/add", HTTP_POST, handleAdd);
  server.on("/wifi/add-manual", HTTP_POST, handleAddManual);
  server.on("/wifi/remove", HTTP_POST, handleRemove);
}
