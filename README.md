# Paginotto firmware

Firmware for the Paginotto pocket e-book reader, built around the Heltec
Wireless Paper display.

Paginotto is a compact TXT reader with an e-ink UI, local storage, a browser
WebUI and wireless firmware updates. The firmware is built with PlatformIO.

Paginotto started from the Pala One firmware project by **Paul Lagier**. Since
the initial import, it has become a substantially different project: the code
was heavily debloated, the UI and reader interaction model were redesigned,
the storage and library systems were reworked, and a new WebUI, folder-based
navigation and GitHub Pages OTA pipeline were developed for the Paginotto
device.

## Project status

The repository supports two board variants:

- Heltec Wireless Paper V1.1
- Heltec Wireless Paper V1.2

The display revision is normally printed on the back of the board. Select the
matching PlatformIO environment when building or uploading firmware.

## What is included

- TXT reading with adjustable font size and line spacing
- Reading progress persistence
- Bionic reading mode
- Button gesture navigation and configurable reader actions
- Library tree with folders and book management
- Browser-side book reading, text search and editing
- WebUI book upload, download, rename, move and deletion
- Wi-Fi provisioning through Improv Serial
- Device SoftAP WebUI for configuration and file management
- Custom screensaver upload, preview, deletion and rotation modes
- Configurable sleep and deep-sleep behaviour
- OTA firmware updates through GitHub Pages

## Wi-Fi and WebUI

The device WebUI can be reached through the device SoftAP or through a saved
network. It provides pages for:

- books and folders;
- book reading, search and editing;
- Wi-Fi configuration and saved credentials;
- device settings;
- screensaver management.

Wi-Fi credentials can also be provisioned through Improv Serial over USB-CDC.
Saved credentials are stored on the device and are used by the firmware-update
flow.

## OTA firmware updates

The device has two OTA channels:

- **Stable** — firmware published from a `vX.Y.Z` Git tag;
- **Dev** — firmware published from the `dev` branch.

The update flow is:

1. Open **Firmware Update** from the library.
2. Select a nearby network whose credentials are already stored on the device.
3. Connect to it. The screen performs one automatic update check after the
   connection succeeds.
4. Select **Stable** or **Dev** from the resulting list.
5. Double-press an available channel to download and install it.
6. Double-press at the reboot prompt to start the new firmware.

There is no hotspot fallback in the firmware-update flow. The normal WebUI
flow can still use the device SoftAP for initial provisioning.

The update screen shows the current firmware version and the status of both
channels. Dev manifests include the source commit and commit timestamp; a Dev
update is offered only when its commit timestamp is newer than the timestamp
embedded in the running firmware. This prevents the device from accepting an
older development build after local, not-yet-pushed work.

OTA files are served from:

```text
https://emanuelezanoni.github.io/paginotto-ereader/<channel>/
```

Each channel contains one manifest and one firmware binary for each display
variant. The firmware downloads over HTTPS and writes to the inactive OTA
partition. The image is validated before it is selected for the next boot.

### GitHub Actions publishing

`.github/workflows/publish-ota.yml` builds both board variants and publishes
firmware-only bundles to the `gh-pages` branch.

- Push to `dev` publishes the `dev` channel.
- Push a `v*` tag to `main` publishes the `stable` channel and creates a
  matching GitHub Release.
- `workflow_dispatch` can publish a selected channel manually.
- A failed build does not publish a new OTA bundle.

`.github/workflows/static-analysis.yml` runs `cppcheck` through PlatformIO on
every push and pull request.

For the device to use OTA, the repository must be public and GitHub Pages must
serve the `gh-pages` branch from its root directory. GitHub Actions also needs
write permission for repository contents so it can update that branch.

## Building with PlatformIO

Install PlatformIO Core, then run commands from the repository root:

```bash
# Build V1.2
pio run -e wireless-paper-v1_2

# Build V1.1
pio run -e wireless-paper-v1_1

# Upload over USB
pio run -e wireless-paper-v1_2 -t upload
pio run -e wireless-paper-v1_1 -t upload

# Serial monitor
pio device monitor

# Static analysis
pio check -e wireless-paper-v1_2 --fail-on-defect low
```

The two environments share the source tree, libraries and OTA-capable
partition table. `scripts/build_info.py` injects the firmware version, source
commit and commit timestamp at build time.

## Host-side tests

The pure firmware modules and selected storage code have host-side tests under
`test/`. They use CMake and a C++17 compiler; no ESP32 or PlatformIO toolchain
is required for these tests.

```bash
cmake -S test -B test/build
cmake --build test/build --config Release
ctest --test-dir test/build -C Release --output-on-failure
```

See [`test/README.md`](test/README.md) for platform-specific prerequisites.

## Repository layout

```text
Paginotto/
├── Paginotto.ino          # firmware entry point
├── partitions.csv         # OTA-capable ESP32 partition table
└── src/
    ├── config.h           # compile-time configuration
    ├── hal/                # display, input, Wi-Fi and OTA adapters
    ├── pure/               # host-testable logic
    ├── storage/            # filesystem and persistent data
    ├── ui/                 # reader, menus, screens and widgets
    └── web/                # embedded WebUI and HTTP routes
scripts/                   # PlatformIO build and OTA helpers
test/                      # CMake host-side tests
```

## Origin and licensing

Paginotto started from the Pala One firmware project by Paul Lagier. The
upstream repository, imported commit and provenance are documented in
[`UPSTREAM.md`](UPSTREAM.md). Since that import, the codebase has been heavily
debloated and substantially redesigned for the Paginotto device.

This repository does not currently contain a standard MIT, GPL or Apache
license. The upstream project does not provide one, so the complete derivative
cannot be relicensed as if all of its code were new. The repository is intended
to be used and forked as an independent Paginotto project; changes belonging to
another version should live in that user's fork or repository.
