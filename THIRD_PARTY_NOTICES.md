# Third-party notices

This file records dependencies used by the current Paginotto firmware. It is
not a license for Paginotto source and does not relicense historical upstream
code. The dependency versions and revisions below are the values declared in
`platformio.ini`.

## Firmware and board dependencies

- **pioarduino platform-espressif32 55.03.38-1** — Arduino-ESP32 and
  ESP-IDF framework, board support and bundled framework components. The
  framework's own license files apply to the distributed binaries.
- **Heltec E-Ink Modules v4.6.0** — Heltec Wireless Paper display and board
  support. The package contains separate notices for bundled Adafruit/GFX
  material and fonts; no standalone top-level license file was found in the
  resolved package, so its upstream terms must be retained with the package
  when redistributing binaries.
- **Adafruit GFX Library 1.12.6** — BSD license; see the dependency's
  `license.txt`.
- **Adafruit BusIO** — BSD license; used transitively by Adafruit GFX.
- **U8g2 for Adafruit GFX 1.8.0** — BSD 2-Clause license; see `LICENSE`.
- **Improv WiFi Library** at commit
  `e37c244e0080349e191252acf4bce146eca118e0` — MIT license; see `LICENSE`.
- **ArduinoJson 7.2.1** — MIT license; see `LICENSE.txt`.
- **QRCode** at commit
  `eafbde494979abc2445c363cc2602230bcbe299c` — MIT license, including the
  Project Nayuki-derived components; see `LICENSE.txt`.

## Fonts and embedded assets

- The firmware uses font data exposed by U8g2 (`helvR`, `helvB`, `5x8` and
  `6x10` families). Those font terms are part of the U8g2 distribution and
  are not Paginotto-authored assets.
- `Paginotto/src/ui/default_screensaver.h` is a Paginotto product meme asset,
  intentionally embedded in the firmware. It is not treated as third-party
  artwork or as a dependency license item.
- No standalone font, icon or image source files were found outside the
  embedded default screensaver and dependency packages.

## Upstream source

The repository history originated from Pala One. That provenance is documented
in `UPSTREAM.md`; because the upstream snapshot has no standard license file,
this notice does not claim a license for inherited material. The Paginotto
license boundary remains to be defined after the manual inventory.
