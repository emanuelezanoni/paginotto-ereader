#include "src/web/web.h"

#include "src/web/chrome.h"
#include "src/web/diagnostics.h"
#include "src/web/files.h"
#include "src/web/find.h"
#include "src/web/reset.h"
#include "src/web/screensavers.h"
#include "src/web/settings.h"
#include "src/web/upload.h"
#include "src/web/wifi.h"
#include "src/ui/screens/upload_screen.h"

static void handleWebUiExit() {
  g_uploadScreen.requestWebUiExit();
  String page = webPageStart("Paginotto", "Taking a page break.", "");
  page += "<div class='card'><div class='banner-ok'><h2>Paginotto is taking a page break.</h2>"
          "<p>Wi-Fi off. See you soon.</p></div>"
          "<p class='muted'>You can close this tab now.</p></div>";
  page += webPageEnd();
  server.send(200, "text/html; charset=utf-8", page);
}

// ============================================================================
//  Web routes — split per topic across the files in this directory. This
//  function is the single entry point called from setup() to mount them all.
//  Each `register*Routes` is an `HTTP_GET`/`HTTP_POST` registration on the
//  shared global `server`.
// ============================================================================
void registerWebRoutes() {
  registerChromeRoutes();      // /style.css
  registerFilesRoutes();       // / redirect, /books, /del, /mkdir, /rmdir, /move, /rename, /jumppage
  registerSettingsRoutes();    // /settings
  registerWifiRoutes();        // /wifi
  registerFindRoutes();        // /read, /readbook-text, /jumpoffset
  registerScreensaverRoutes(); // /screensavers and subroutes
  registerUploadRoutes();      // /upload, /upload-sleep (legacy)
  registerResetRoutes();       // /settings/reset
  registerDiagnosticsRoutes(); // /diagnostics and /diagnostics.txt
  server.on("/exit", HTTP_GET, handleWebUiExit);
}
