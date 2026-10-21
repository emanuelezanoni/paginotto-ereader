# Paginotto firmware

Firmware for the Paginotto pocket e-book reader: a small, distraction-free
TXT reader built around the Heltec Wireless Paper.

The goal of the project was to create a simple, distraction-free reading device that feels minimal, portable and easy to build while still looking and behaving more like a real product than a typical DIY electronics project.

This repository contains the firmware, device UI, WebUI, storage layer and the
GitHub-based OTA deployment pipeline.

## Project origin and attribution

Paginotto began from the Pala One firmware
(https://github.com/PaulLagier/pala-one-firmware), originally created by
**Paul Lagier**. The original import and provenance are documented in
UPSTREAM.md, including the imported upstream commit and the upstream Git
remote.

Since the import, Paginotto has been substantially redesigned and extended:

- heavily debloated by removing unused features, dependencies and legacy
  paths that are not part of the Paginotto product;
- new product identity, UI hierarchy and e-ink interaction model;
- reworked reader, gestures, sleep behaviour and library navigation;
- redesigned WebUI for books, folders, editing, search and settings;
- one-level folders with device-side folder navigation;
- streaming text editing with automatic repagination;
- GitHub Pages dual-channel OTA updates;
- many storage, reliability and display-flow changes specific to Paginotto.

Paginotto is maintained as its own project by Emanuele Zanoni.

## Board Versions

There are currently two supported Heltec Wireless Paper versions:
- `Heltec V1.1`
- `Heltec V1.2`

The board version is usually printed on the back of the PCB.

Pick your board's revision by selecting the matching PlatformIO environment.

## Wi-Fi provisioning (Improv)

Besides the SoftAP captive portal, the firmware supports **Improv Serial** Wi-Fi provisioning ([improv-wifi.com](https://www.improv-wifi.com)) over the USB-CDC port, using the [`jnthas/Improv-WiFi-Library`](https://github.com/jnthas/Improv-WiFi-Library).

Saved credentials let the device join your network in **Station mode**. The
Firmware Update screen only uses saved networks visible nearby and never
falls back to hotspot mode. The WebUI can still use the device hotspot for
initial provisioning when no network has been saved.

## OTA firmware updates

Once the device has Wi-Fi credentials stored (see [Wi-Fi provisioning](#wi-fi-provisioning-improv)), firmware updates can be installed wirelessly — no USB cable, no computer required.

### How to update

1. Navigate to **Firmware Update** at the bottom of the library menu.
2. The device shows nearby networks for which credentials are already saved.
   Select one with **1×** and connect with **2×**. There is no hotspot option
   in this screen.
3. Select Stable or Dev and choose **[ Check for update ]**.
4. The device checks both channels and displays either:
   - the stable transition, for example **v3.0 → v3.1.0**;
   - **Dev: update to commit abc1234**;
   - or that each channel is already up to date.
5. Select the channel to install, then confirm **[ Install update ]** with **2×**.
   The binary streams directly into the idle OTA partition (~5–30 s depending on your network).
6. When flashing is complete, press **2×** to reboot into the new firmware.

### Requirements

- Wi-Fi credentials must be provisioned first (see below). If none are stored
  the screen asks you to configure Wi-Fi through the WebUI first.
- The device must be able to reach the configured update server over HTTPS. A local network without internet access will be reported as *"Cannot reach update server"*.


## Strings

The firmware ships with one built-in string table in `Paginotto/src/lang/strings.h`.

Glyph coverage: Latin Extended (`á é í ó ú ñ Ñ ¿ ¡ ü Ü`) is provided by `u8g2_font_helv*_te` for body, bold, app-large and toast roles. The small bitmap fonts used for the battery percentage and page-number indicator stay on ASCII-only `_tf` tables — they only render digits / `%`, and any translation routed through them would render missing-glyph boxes. Web responses declare `charset=utf-8`.

## Building the firmware

### PlatformIO

1. Install [PlatformIO Core](https://platformio.org/install/cli) (CLI) or the PlatformIO IDE extension for VS Code.
2. From the repo root:
   ```
   pio run -e wireless-paper-v1_2 -t upload    # V1.2 panel
   pio run -e wireless-paper-v1_1 -t upload    # V1.1 panel
   ```
3. Serial monitor:
   ```
   pio device monitor
   ```

Both envs share libraries and partition table via `platformio.ini`. The PIO build also runs `scripts/build_info.py` to inject:

- `FW_VERSION` from `git describe --tags --always --dirty` (e.g. `v2.1`, `v2.1-3-gabc1234`, `…-dirty`)
- `BUILD_GIT_HASH` from the current short SHA

### OTA publishing (channels & CI)

Firmware-only OTA bundles are published to the `gh-pages` branch by
`.github/workflows/publish-ota.yml`. Two channels live side-by-side:

| Trigger                       | Channel  | URL path     | `DEBUG_BUILD` | Manifest version |
|-------------------------------|----------|--------------|---------------|------------------|
| push to `dev`                 | `dev`    | `/dev/`      | `1` (git hash visible on device) | `dev-<sha>` |
| tag `v*`                      | `stable` | `/stable/`   | `0` (clean UI) | `vX.Y.Z` |
| `workflow_dispatch`           | choose   | matches      | depends on channel | `dev-<sha>` / `manual-<sha>` |

Merges to `main` do **not** auto-publish. Tagging is the explicit release event, so `/stable/` never carries an arbitrary mid-release snapshot. To cut a release: merge to `main`, then `git tag vX.Y.Z && git push origin vX.Y.Z`.

Every run publishes only firmware binaries and manifests under the relevant
`gh-pages/<channel>/` subdirectory. The `Firmware Update` screen on the device
reads these files directly over HTTPS; there is no browser flashing workflow.

## Codebase layout

```
Paginotto/
├── Paginotto.ino     # Sketch entry: board selection + setup()/loop()
├── partitions.csv       # ESP32 partition table
└── src/                 # Firmware modules
    ├── config.h         # Compile-time constants
    ├── state.{h,cpp}    # Globals (display, WiFi server, prefs)
    ├── pure/            # Pure C++ — host-testable
    ├── hal/             # Hardware adapters (display, battery, input)
    ├── storage/         # KV store + on-disk persistence
    ├── ui/              # Screens, fonts, sleep, toasts, widgets
    │   └── screens/     # One file per screen
    └── web/             # Captive-portal HTTP server (route groups)
docs/                    # Architecture notes + refactor journal
scripts/                 # PlatformIO pre-build helpers
test/                    # Host-side CMake unit tests for pure/ + storage/
archive/                 # Past firmware revisions, kept for reference
```

The intent of the layering is **pure → storage → hal → ui**: pure modules avoid hardware-specific headers, so they compile into the host test build verbatim. Storage adds an on-disk persistence shim behind `KeyValueStore`, hal isolates the hardware, and the UI sits on top.

## Host-side tests

Pure modules and KV-backed storage have host-side unit tests under [`test/`](test/). They build with CMake and run on your laptop — no board required.

See [test/README.md](test/README.md) for prerequisites (CMake + a C++17 compiler) and per-platform setup / run instructions for Windows, Linux, and macOS.


## Features

- TXT book support
- Adjustable font size and line spacing
- Bionic reading mode
- Reading progress saving
- In-book text search (web UI)
- Remappable button gestures
- Custom screensaver image
- Adjustable idle sleep timeout
- Wi-Fi provisioning (Improv) + captive-portal web UI
- OTA firmware updates over Wi-Fi (see [OTA firmware updates](#ota-firmware-updates))
- Deep sleep mode
- USB-C charging
- Lightweight portable design
- Firmware source available for inspection and modification


## License and use

Paginotto is a public, independent repository. You may use it, fork it, and
modify it in your own repository. Changes and alternative versions should be
maintained in the corresponding fork or upstream repository.

The project does not currently include a standard license file; its original
provenance is documented in [UPSTREAM.md](UPSTREAM.md).
