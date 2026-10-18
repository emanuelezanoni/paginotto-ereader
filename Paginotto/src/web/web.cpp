#include "src/web/web.h"

#include "src/web/chrome.h"
#include "src/web/files.h"
#include "src/web/find.h"
#include "src/web/reset.h"
#include "src/web/screensavers.h"
#include "src/web/settings.h"
#include "src/web/upload.h"

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
  registerFindRoutes();        // /read, /readbook-text, /jumpoffset
  registerScreensaverRoutes(); // /screensavers and subroutes
  registerUploadRoutes();      // /upload, /upload-sleep (legacy)
  registerResetRoutes();       // /settings/reset
}
