#ifndef PALA_UI_SCREENS_UPLOAD_SCREEN_H
#define PALA_UI_SCREENS_UPLOAD_SCREEN_H

#include "src/hal/wifi.h"    // WifiSession (cached for draw())
#include "src/ui/screen.h"
#include "src/ui/wifi_network_scanner.h"

class UploadScreen : public Screen {
public:
  void onEnter() override;
  void onButton(const ButtonEvent& e) override;
  void draw() override;
  void onIdleTick() override;
  void requestWebUiExit();
  void openForFirmwareUpdate();

  // The Wi-Fi session (AP or STA) can't keep running while the device
  // deep-sleeps.
  bool allowSleep() const override { return false; }

private:
  enum class Phase { Ready, ConnectingSta, Goodbye };

  Phase       phase_         = Phase::Ready;
  uint32_t    startedMs_     = 0;    // for the auto-exit timer (set on entry to Ready)
  uint32_t    staStartedMs_  = 0;    // when wifiStaBegin() was called (for the 5s timeout)
  WifiSession net_;                  // cached session info shown by draw()
  uint8_t selectedItem_ = 0;         // 0=hotspot, 1..=scanner entries
  WifiNetworkScanner scanner_;
  bool connectionFailed_ = false;
  uint32_t connectionFailedUntilMs_ = 0;
  bool sessionActive_ = false;
  bool firmwareMode_ = false;
  bool exitRequested_ = false;
  uint32_t goodbyeStartedMs_ = 0;

  void beginSession();
  void enterReady();
  void beginConfiguredScan();
  void startSelectedStation();
  void returnToSelection(bool failed);
  void useHotspot();
  void stopSessionToLibrary();
  void beginGoodbye();
};

extern UploadScreen g_uploadScreen;

#endif  // PALA_UI_SCREENS_UPLOAD_SCREEN_H
