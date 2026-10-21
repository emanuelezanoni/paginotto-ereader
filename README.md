# Paginotto firmware

Firmware for the Paginotto pocket e-book reader: a small, distraction-free
TXT reader built around the Heltec Wireless Paper.

The goal of the project was to create a simple, distraction-free reading device that feels minimal, portable and easy to build while still looking and behaving more like a real product than a typical DIY electronics project.

This repository contains the firmware, device UI, WebUI, storage layer and the
GitHub-based installer/OTA deployment pipeline.

## Project origin and attribution

Paginotto is an independent derivative of the Pala One firmware
(https://github.com/PaulLagier/pala-one-firmware), originally created by
**Paul Lagier**. The original import and provenance are documented in
UPSTREAM.md, including the imported upstream commit and the upstream Git
remote.

Since the import, Paginotto has been substantially redesigned and extended:

- new product identity, UI hierarchy and e-ink interaction model;
- reworked reader, gestures, sleep behaviour and library navigation;
- redesigned WebUI for books, folders, editing, search and settings;
- one-level folders with device-side folder navigation;
- streaming text editing with automatic repagination;
- GitHub Pages installer and dual-channel OTA updates;
- many storage, reliability and display-flow changes specific to Paginotto.

The upstream project remains the origin of the inherited code and deserves
its original attribution. Paginotto-specific work is maintained by Emanuele
Zanoni and contributors to this repository.

## Install (no toolchain needed)

The web installer is published through GitHub Pages at:

https://emanuelezanoni.github.io/paginotto-ereader/

The easiest way to flash a board is via the web installer. Plug your Heltec Wireless Paper into a desktop computer running Chrome, Edge, or Opera, then open the installer page and pick a channel:

- **Stable** — latest tagged release (`vX.Y.Z`).
- **Development** — latest build from `dev`; new features, may break.

Each channel page lists both display revisions (V1.1 / V1.2). Pick the one that matches your board and click **Install**. The installer keeps existing reading progress and uploaded books across re-flashes.

## Board Versions

There are currently two supported Heltec Wireless Paper versions:
- `Heltec V1.1`
- `Heltec V1.2`

The board version is usually printed on the back of the PCB.

Pick your board's revision in the build step below — either by uncommenting the matching `#define` at the top of `Paginotto/Paginotto.ino` (Arduino IDE), or by selecting the matching env (PlatformIO).

## Wi-Fi provisioning (Improv)

Besides the SoftAP captive portal, the firmware supports **Improv Serial** Wi-Fi provisioning ([improv-wifi.com](https://www.improv-wifi.com)) over the USB-CDC port, using the [`jnthas/Improv-WiFi-Library`](https://github.com/jnthas/Improv-WiFi-Library). When the board is plugged into a computer, a browser can hand it Wi-Fi credentials directly; the Paginotto installer will handle the post-flash redirect once deployed.

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

## Device lock

The device can be locked to stop accidental input (page turns, navigation) while it rests in a bag or pocket. The locked state is persisted to NVS (`cfg_locked`), so a device that fully powers down comes back locked.

Locking is a remappable button action. In the web UI under **Settings → Buttons** you can bind click-hold to *None* or *Lock device*. Out-of-box default:

- **Click-hold** → Lock device

Unlocking uses click-hold and shows an "Unlocked" toast. While locked, sleep screen shows a small padlock badge in top-left corner.

---

## Contributing

If you improve the firmware, add features or fix bugs, feel free to open a pull request.
Please clearly mention:
- which board version(s) you tested on (V1.1, V1.2, or both)
- what was changed
- how it was tested


## Building the firmware

The same sources build under either toolchain.

### PlatformIO (recommended)

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

### Arduino IDE 2 (outdated)

1. Install the **esp32 by Espressif Systems** board package (Boards Manager) and select the **Heltec WiFi LoRa 32 V3** board.
2. Install these libraries via Library Manager (or by URL):
   - [`heltec-eink-modules`](https://github.com/todd-herbert/heltec-eink-modules) (todd-herbert fork)
   - **Adafruit GFX Library** (Adafruit)
   - **U8g2_for_Adafruit_GFX** (olikraus)
   - [`Improv-WiFi-Library`](https://github.com/jnthas/Improv-WiFi-Library) (jnthas) — serial Wi-Fi provisioning; PlatformIO installs it automatically, Arduino IDE users add it by URL
3. Open `Paginotto/Paginotto.ino`. Uncomment exactly one of `BOARD_V1_1` / `BOARD_V1_2` at the top.
4. Tools → Partition Scheme → **Custom** (the sketch ships its own `partitions.csv`).
5. Verify / Upload.

Arduino IDE / host-test builds skip the script and fall back to `"dev"` and `"unknown"` respectively — those toolchains are for developer iteration; releases go through the PIO + tagged-CI flow where the real values get injected.


### Web Installer site (channels & CI)

The web installer is published to the gh-pages branch by
the workflow at .github/workflows/deploy-installer.yml. Two channels live
side-by-side and never overwrite each other:

| Trigger                       | Channel  | URL path     | `DEBUG_BUILD` | Manifest version |
|-------------------------------|----------|--------------|---------------|------------------|
| push to `dev`                 | `dev`    | `/dev/`      | `1` (git hash visible on device) | `dev-<sha>` |
| tag `v*`                      | `stable` | `/stable/`   | `0` (clean UI) | `vX.Y.Z` |
| `workflow_dispatch`           | choose   | matches      | depends on channel | `dev-<sha>` / `manual-<sha>` |

Merges to `main` do **not** auto-publish. Tagging is the explicit release event, so `/stable/` never carries an arbitrary mid-release snapshot and the install dialog always shows a clean version name. To cut a release: merge to `main`, then `git tag vX.Y.Z && git push origin vX.Y.Z`.

Every run rebuilds the channel it owns and publishes only the corresponding `gh-pages/<channel>/` subdirectory (the workflow uses `keep_files: true`, so the other channel's directory is preserved). A small landing page at the gh-pages root links to both — it is republished on every run with identical content, so the cross-write is safe. The Improv post-provisioning `connected.html` also lives at the gh-pages root (its content is channel-agnostic and the URL is baked into the firmware).

Source HTML/manifests live in `install/` on the normal branches. The `gh-pages` branch is fully generated; do not edit it by hand.


#### Local development

To iterate on the installer page (HTML, Improv Serial provisioning flow, manifest tweaks) without CI:

1. Build both board environments at least once so the firmware binaries exist:
   ```
   pio run -e wireless-paper-v1_1
   pio run -e wireless-paper-v1_2
   ```
2. Assemble the bundle. Two layouts are supported:
   ```
   python scripts/assemble_site.py                       # flat layout in site/
   python scripts/assemble_site.py --channel dev         # site/index.html + site/dev/
   ```
3. Serve it. Web Serial works on `localhost` without HTTPS:
   ```
   python -m http.server 8000 --directory site
   ```
4. Open <http://localhost:8000> in Chrome, Edge, or Opera. With `--channel`, the landing page is served; without, the installer is served directly.

Optional flags: --version <string>, --commit <short-sha> and --out <dir>.
The channel-aware layout produced locally is bit-identical to what the
workflow uploads to gh-pages.

## Codebase layout

```
Paginotto/
├── Paginotto.ino     # Sketch entry: board selection + setup()/loop()
├── partitions.csv       # ESP32 partition table
└── src/                 # Firmware modules
    ├── config.h         # Compile-time constants
    ├── state.{h,cpp}    # Globals (display, WiFi server, prefs)
    ├── pure/            # Pure C++, no Arduino headers — host-testable
    ├── hal/             # Hardware adapters (display, battery, input)
    ├── storage/         # KV store + on-disk persistence
    ├── ui/              # Screens, fonts, sleep, toasts, widgets
    │   └── screens/     # One file per screen
    └── web/             # Captive-portal HTTP server (route groups)
docs/                    # Architecture notes + refactor journal
scripts/                 # PlatformIO pre-build helpers
test/                    # Host-side CMake unit tests for pure/ + storage/
install/                 # ESP Web Tools installer page (deployed to GitHub Pages by CI)
archive/                 # Past firmware revisions, kept for reference
```

The intent of the layering is **pure → storage → hal → ui**: pure modules never include `Arduino.h`, so they compile into the host test build verbatim. Storage adds an on-disk persistence shim behind `KeyValueStore`, hal isolates the hardware, and the UI sits on top.

### Include style

Firmware sources include each other by the full path from the sketch root, e.g. `#include "src/hal/display.h"`, not the shorter `#include "hal/display.h"`.

This is to stay compatible with Arduino IDE. The IDE recursively compiles files inside the `src/` subfolder of a sketch, **but does not add `src/` to the compiler's include path** — only the sketch folder root is on it. So `#include "config.h"` from a file at sketch root fails (the file actually lives at `src/config.h`), whereas `#include "src/config.h"` resolves correctly. PlatformIO is happy either way; we picked the Arduino-IDE-compatible form so the same `#include` lines work in both build systems with no extra `-I` flags.

## Host-side tests

Pure modules and KV-backed storage have host-side unit tests under [`test/`](test/). They build with CMake and run on your laptop — no board required.

See [test/README.md](test/README.md) for prerequisites (CMake + a C++17 compiler) and per-platform setup / run instructions for Windows, Linux, and macOS.


## Features

- TXT book support
- Adjustable font size and line spacing
- Bionic reading mode
- Reading progress saving
- In-book text search (web UI)
- Device lock (see [Device lock](#device-lock))
- Remappable button gestures
- Custom screensaver image
- Adjustable idle sleep timeout
- Wi-Fi provisioning (Improv) + captive-portal web UI
- OTA firmware updates over Wi-Fi (see [OTA firmware updates](#ota-firmware-updates))
- Deep sleep mode
- USB-C charging
- Lightweight portable design
- Firmware source available for inspection and modification subject to the attribution terms above


## Community & Modifications

Community improvements, forks and firmware modifications are welcome.
If you build your own version or improve the project, feel free to share it with the community.

## License and copyright

This repository does not currently declare a new standard open-source license.
The inherited upstream code and its original terms remain attributable to Paul
Lagier; see UPSTREAM.md. A permissive license cannot be added to the complete
derivative without explicit permission from the upstream copyright holder.

The firmware in this repository is provided for personal and educational use.

Please do not:
- reupload paid project files
- redistribute complete download packages
- resell the project files
- commercially redistribute modified versions of paid assets

The design, branding, documentation and paid project assets remain copyright © Paul Lagier.

---

Paginotto-specific code, documentation and design changes are maintained by
Emanuele Zanoni and contributors, subject to the same attribution boundary.
Do not treat this repository as a grant to redistribute upstream or paid
project assets.
