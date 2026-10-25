# Paginotto

Paginotto is a compact pocket e-book reader built around the Heltec Wireless
Paper e-ink board. Books are stored locally, reading position is remembered,
and the device can be managed from a browser when needed.

Current release: **v1.3.0**

Supported boards:

- Heltec Wireless Paper V1.1
- Heltec Wireless Paper V1.2

## Reading books

Books are organised in a simple library with optional folders. The device
remembers the current position of every book, including after a restart or
standby.

### Library navigation

- **Short press** — move to the next item.
- **Double press** — open the selected book or folder.
- **Triple press** — leave the current folder and return to the library root.
- **Hold** — lock the device by default.

Inside a folder, the same controls are used. The selected row is shown on the
display before opening it.

### Reader navigation

- **Short press** — next page.
- **Double press** — previous page.
- **Triple press** — return to the library.
- **Hold** — lock the reader and enter standby.

The hold gestures can be configured from the WebUI. A click-then-hold gesture
is also available and can be left unassigned.

When a large book is opened for the first time, Paginotto may need to build
its page index. The display shows the progress and the operation does not
enter the screensaver. Later openings, bookmarks and backward page turns use
the saved index instead of scanning the whole book again. The index is stored
on the device and does not impose a fixed page-count limit through RAM.

## Standby and screensaver

The normal library and reading screens follow the sleep timeout selected in
**Settings**. Any physical input resets the inactivity timer.

Depending on the settings, the device can:

- enter light sleep while waiting for input;
- show the selected screensaver after the configured timeout;
- enter deep sleep to preserve battery;
- lock itself when it enters sleep.

Book indexing temporarily keeps the device awake. Standalone Wi-Fi selection,
network scanning and firmware-update screens have a separate five-minute
timeout. When it expires, the active Wi-Fi session is closed and the device
returns to the library; the normal library sleep rules then apply.

## Battery

Paginotto is designed for a single 3.7 V LiPo battery. The board provides the
charging and battery-protection hardware.

The battery percentage shown by the firmware is an estimate based on battery
voltage. It is filtered to avoid sudden jumps while the ESP32 is working, but
it is not a measurement of remaining mAh: this board does not provide a
dedicated fuel gauge or coulomb counter.

## WebUI

The WebUI is available through the device hotspot or through a configured Wi-Fi
network. It is designed to work on both desktop and mobile browsers.

Open the WebUI to:

- browse the library and folders;
- upload TXT books or convert EPUB books in the browser;
- rename, move, download or delete books;
- search inside a book and jump to a match or page;
- edit and save book text;
- configure font size, line spacing, bionic reading and sleep behaviour;
- configure button gestures;
- upload, preview, rotate and delete custom screensavers;
- configure Wi-Fi and inspect diagnostics;
- install firmware updates.

The mobile layout keeps search controls together, prevents the browser from
jumping away from the active controls, and uses a consistent size for text
fields and buttons. Text fields use a mobile-safe font size so iOS does not
zoom the page when the keyboard opens.

## Firmware updates

From the library, open **Firmware Update**, choose a saved Wi-Fi network and
select the desired channel:

- **Stable** — recommended releases;
- **Dev** — latest development firmware.

The update is downloaded over HTTPS and installed in the inactive OTA
partition. The device validates the image before rebooting.

For first-time setup, Wi-Fi can be configured through the device hotspot or
provisioned over USB with Improv Serial.

## Installing from source

The normal installation path is OTA. Developers can build and upload the
firmware with PlatformIO:

```bash
pio run -e wireless-paper-v1_2
pio run -e wireless-paper-v1_2 -t upload
```

Use `wireless-paper-v1_1` for a V1.1 board. Host-side tests can be run without
the ESP32 toolchain:

```bash
cmake -S test -B test/build
cmake --build test/build --config Release
ctest --test-dir test/build -C Release --output-on-failure
```

## Project notes

Paginotto is maintained as its own project for the Paginotto device. The
repository contains the firmware, embedded WebUI, storage system, OTA support
and host-side tests in one place.

The repository history originated from Pala One and has been substantially
modified. See [UPSTREAM.md](UPSTREAM.md) for historical provenance and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency notices. These
documents do not grant a blanket license over inherited upstream material; the
Paginotto licensing boundary is documented separately in the originality
inventory and remains subject to the final manual review.

The board-specific source tree is under `Paginotto/`; build helpers are under
`scripts/`; host-side tests are under `test/`.
