# Paginotto

A compact pocket e-book reader for the Heltec Wireless Paper e-ink board.
Books are stored on-device, reading position is remembered, and the device
can be managed from any browser over Wi-Fi.

**Current release: v2.0.0** · [MIT License](LICENSE)

**Supported hardware:**
- Heltec Wireless Paper V1.1
- Heltec Wireless Paper V1.2

---

## Features

- Read TXT books; convert EPUB to TXT directly in the browser
- Library with folders; persistent reading position per book
- UTF-8 word-wrap paginator with configurable font size and line spacing
- Optional bionic reading mode
- Custom screensavers (upload your own images)
- Embedded WebUI — accessible via device hotspot or home Wi-Fi
- Wi-Fi provisioning over USB with Improv Serial
- OTA firmware updates over HTTPS (stable and dev channels, signed images)
- Light sleep and deep sleep for battery saving
- Device lock

---

## Hardware

Paginotto runs on the **Heltec Wireless Paper**, an ESP32-S3-based board with
a 2.13" e-ink display and integrated LiPo charging. A single 3.7 V LiPo cell
plugs directly into the board.

The battery indicator is voltage-based and filtered to reduce noise while the
ESP32 is active. The board has no fuel gauge or coulomb counter, so the
percentage is an estimate.

---

## Reading books

Books are organised in a library. Folders are supported. The device remembers
the current page of every book, including after a restart or deep sleep.

EPUB files are not stored directly — convert them to TXT in the WebUI upload
panel before transferring them to the device.

### Library navigation

| Gesture | Action |
|---|---|
| Short press | Move to next item |
| Double press | Open selected book or folder |
| Triple press | Return to library root |
| Hold | Lock device (default) |

### Reader navigation

| Gesture | Action |
|---|---|
| Short press | Next page |
| Double press | Previous page |
| Triple press | Return to library |
| Hold | Lock reader and enter standby |

The hold gesture can be reassigned in the WebUI. A click-then-hold gesture is
also available and can be left unassigned.

When a large book is opened for the first time, Paginotto builds a page index.
The display shows progress; the device stays awake until indexing completes.
Subsequent openings, bookmarks, and backward navigation use the saved index.

---

## WebUI

Connect to the device hotspot (`Paginotto-XXXXXX`) or to the same Wi-Fi
network as your phone or computer, then open the address shown on the device
display.

From the WebUI you can:

- Browse the library and folders
- Upload TXT books; convert EPUB to TXT in the browser before upload
- Rename, move, download, or delete books
- Search inside a book and jump to a match or page number
- Edit and save book text
- Configure font size, line spacing, bionic reading, and sleep behaviour
- Configure button gestures
- Upload, preview, rotate, and delete custom screensaver images
- Configure Wi-Fi networks
- Inspect device diagnostics
- Install firmware updates

The layout is responsive and works on both desktop and mobile browsers.

---

## Screensaver

Upload your own images from the WebUI screensaver panel. Uploaded images are
stored on the device and rendered in greyscale on the e-ink display.

You can preview, rotate, and delete screensavers from the same panel. The
active screensaver is selected in Settings.

---

## Firmware updates

### OTA (over-the-air)

From the library, open **Firmware Update**, select a saved Wi-Fi network, and
choose a channel:

- **Stable** — recommended releases
- **Dev** — latest development builds

The update is downloaded over HTTPS, validated, and written to the inactive
OTA partition. The device reboots into the new firmware on success. The
previous firmware remains in the other slot and can be restored by the
bootloader if the new image fails to boot.

### First-time setup

On a fresh device, Wi-Fi can be configured in two ways:

1. **Device hotspot** — connect to the Paginotto hotspot and open the WebUI.
2. **Improv Serial** — connect via USB and use a compatible Improv Serial
   client (e.g. the [web installer](https://esphome.github.io/esp-web-tools/))
   to push Wi-Fi credentials over the serial port.

---

## Installing from source

The recommended installation path is OTA. Developers can build and flash
directly with PlatformIO:

```bash
pio run -e wireless-paper-v1_2
pio run -e wireless-paper-v1_2 -t upload
```

Use `wireless-paper-v1_1` for a V1.1 board.

Host-side unit tests run without the ESP32 toolchain:

```bash
cmake -S test -B test/build
cmake --build test/build --config Release
ctest --test-dir test/build -C Release --output-on-failure
```

---

## Project notes

Paginotto started from the [Pala One](https://github.com/PaulLagier/pala-one-firmware)
firmware by Paul Lagier. The codebase has since been substantially rewritten —
the paginator, storage system, WebUI, OTA, screensaver engine, sleep
management, and input handling are all Paginotto-specific implementations.
Paginotto is now maintained as an independent project.

The Paginotto-authored source is licensed under the [MIT License](LICENSE).
Historical upstream material is documented in [UPSTREAM.md](UPSTREAM.md).
Third-party dependency notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
