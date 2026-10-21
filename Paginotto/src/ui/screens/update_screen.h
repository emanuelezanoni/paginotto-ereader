#ifndef PALA_UI_SCREENS_UPDATE_SCREEN_H
#define PALA_UI_SCREENS_UPDATE_SCREEN_H

#include <Arduino.h>
#include "src/hal/wifi.h"
#include "src/ui/screen.h"

class UpdateScreen : public Screen {
public:
  void onEnter() override;
  void onButton(const ButtonEvent& e) override;
  void draw() override;

  // Block sleep only while the Wi-Fi session is active; once wifiEnd() is
  // called (after flash or on exit) the device may sleep normally.
  bool allowSleep() const override { return !wifiStarted_; }

  // Called from the OTA progress callback during the blocking download.
  // Redraws at every 10 % to limit e-ink refresh overhead.
  void setProgress(int pct);

  // Takes ownership of an already connected station session. Used by the
  // proven Wi-Fi picker so Firmware Update never scans or reconnects twice.
  void beginWithConnectedWifi(const WifiSession& session);

private:
  enum class Phase {
    NoCreds,          // reached without the UploadScreen handoff
    Idle,             // connected — ready to check
    Checking,         // probe + manifest fetch running (drawn then blocks)
    ServerFail,       // probe or manifest fetch failed
    UpToDate,         // versions match
    UpdateAvailable,  // newer version found — shows Install button
    Downloading,      // binary streaming to OTA partition
    DownloadFailed,   // write/validation error — shows Install button for retry
    RebootPrompt      // binary flashed — waiting for reboot confirmation
  };
  Phase       phase_         = Phase::NoCreds;
  WifiSession net_;
  bool        stableChan_    = true;
  bool        wifiStarted_   = false;  // true only between wifiStaBegin() and wifiEnd()
  bool        connectedHandoff_ = false;
  WifiSession handoffSession_;
  int         focusItem_     = 0;  // 0=Stable 1=Dev
  String      stableVersion_;
  String      devCommit_;
  bool        stableAvailable_ = false;
  bool        devAvailable_ = false;
  bool        stableReachable_ = false;
  bool        devReachable_ = false;
  int         progress_      = 0;

  void loadChannel();
  void saveChannel();
  void checkForUpdates();
  void installSelectedUpdate();
  void exitToLibrary();
};

extern UpdateScreen g_updateScreen;

#endif  // PALA_UI_SCREENS_UPDATE_SCREEN_H
