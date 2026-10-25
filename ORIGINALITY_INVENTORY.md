# Paginotto provenance inventory

This is a first-pass engineering inventory of the current tree against the
imported upstream snapshot `2c82e59e0f6e87afd204606b930f15cf7b5b7b8a`.
It is a provenance aid, not a legal determination. Similarity percentages
come from Git's rename/similarity detection and must be followed by a manual
review before a licensing boundary is declared.

## Current audit snapshot — 2026-10-25

The original first-pass lists below were not automatically updated after the
core rewrites. This section is the authoritative current snapshot and
supersedes any older file count in this document.

### Current exact matches

Only these four tracked files are still byte-for-byte identical to their
mapped upstream counterparts:

- `Paginotto/partitions.csv`
- `Paginotto/src/hal/display.cpp`
- `Paginotto/src/storage/fs_util.cpp`
- `Paginotto/src/ui/toast.cpp`

The core files previously listed as exact matches are no longer exact matches:
`paginator`, `text_util`, `library_nav` and `stream` have all been changed.

### Current high-similarity and adjacent critical files requiring provenance review

These are engineering similarity indicators, not legal conclusions. The list
contains files at or near 90% line similarity, plus adjacent critical files
that need review even when their score is lower:

- `src/web/screensavers.cpp` — 98.8% — rewrite first
- `src/ui/screensavers.cpp` — 97.8% — rewrite first
- `src/pure/arduino_compat.h` — 98.2% — review; compatibility shim
- `src/ui/screensavers.h` — 96.3% — review with screensaver rewrite
- `src/hal/wifi_provisioning.cpp` — 96.6% — review/rewrite
- `src/storage/library.cpp` — 96.8% — important storage logic; review carefully
- `src/hal/ota.cpp` — 95.1% — maintenance service; review/rewrite
- `src/hal/wifi_provisioning.h` — 92.7% — interface/adapter review
- `src/ui/toast.h` — 92.7% — small interface; low functional risk
- `src/ui/font.cpp` — 93.3% — rendering adapter; audit assets and licenses
- `src/ui/lock.cpp` — 94.1% — feature service; review
- `src/ui/sleep.h` — 92.3% — interface; review with sleep implementation
- `src/ui/widgets.cpp` — 97.6% — shared UI implementation; review/rewrite
- `src/ui/widgets.h` — 91.5% — interface review
- `src/state.h` — 85.7% by the broad line matcher, but central and must be
  reviewed because it carries shared architecture and configuration
- `src/hal/display.h` — 93.2% — hardware interface; audit, not blind rewrite
- `src/hal/file_stream.h` — 88.0% — thin hardware adapter; not core algorithm
- `src/pure/stream.h` — 94.2% before the current test characterization;
  interface is generic, implementation is now explicitly tested
- `src/pure/text_util.h` — 93.1% — declarations and contracts; review is
  sufficient unless distinctive implementation comments remain
- `src/pure/paginator.h` — 92.5% — public API; implementation was rewritten
- `src/pure/library_nav.h` — 89.1% — data/API declarations; implementation
  was rewritten
- `src/storage/kv_store.h` — 88.5% — persistence contract; audit carefully

- `src/storage/preferences_store.h` — 91.9% — persistence contract
- `src/storage/library.h` — 91.4% — storage API review
- `src/ui/screen.h` — 90.6% — generic UI interface review
- `src/ui/screens/library_screen.h` — 88.0% — screen interface review
- `src/ui/screens/reader_screen.h` — 92.0% — reader screen interface review
- `src/ui/lock.h` — 90.5% — feature interface review
- `src/ui/sleep.cpp` — 86.1% — lifecycle implementation review
- `src/ui/text.cpp` — 90.1% — rendering/text integration review
- `src/hal/input.cpp` — 90.4% — input implementation review
- `src/hal/input.h` — 86.6% — interface review
- `src/hal/battery.cpp` — 92.8% — hardware service review
- `src/ui/font.h` — 89.0% — interface plus asset ownership review

### Hardware boundary audit — block 1A

- `src/hal/display.cpp` and `src/hal/display.h` are **repository adapter
  code**, not vendor source. They depend on Heltec E-Ink, Adafruit GFX and
  U8g2, but the adapter class, rotation policy and canvas setup belong to the
  firmware. The current implementation is functionally useful but remains
  substantially inherited from the upstream display adapter; it is therefore
  classified as **derived adapter — targeted rewrite pending**, not as
  third-party code.
- `src/hal/file_stream.h` is a **thin repository adapter** from Arduino
  `File` to the pure `IReadStream` contract. It is not supplied by Heltec or
  Arduino as a Paginotto component. Its implementation has now been rewritten
  with explicit invalid-handle, EOF and position semantics while preserving
  the public contract; it is classified as **targeted rewrite complete**.
  Host tests, static analysis and both firmware environments pass. No new
  algorithm or vendor code was introduced.
- The included `Adafruit_GFX`, `U8g2`, Heltec and Arduino/ESP-IDF APIs remain
  third-party dependencies and must not be counted as copied repository
  implementation.

### Hardware boundary audit — block 1B

- `src/hal/input.cpp` and `src/hal/input.h` are **repository input logic**,
  not Arduino/ESP-IDF vendor code. GPIO and timer calls are external API;
  the ISR ring buffer, timestamp handling, debounce, overflow recovery and
  click/hold state machine are product logic. The current implementation is
  substantially derived from upstream, although Paginotto has already changed
  the public event model and removed legacy app-facing counters. Classify it
  as **derived product logic — targeted rewrite/review pending**.
- `src/ui/font.cpp` and `src/ui/font.h` are **repository rendering policy**,
  not U8g2 source. U8g2 supplies font tables and drawing primitives; role
  selection, NVS settings, layout metrics, cache stamping and bionic reading
  belong to the product. The current file is modified from upstream (the
  Paginotto tree removed the upstream OpenDyslexic/statusbar family path), but
  the remaining policy and bionic implementation are still substantially
  derived. Classify it as **derived UI logic — targeted rewrite or clean-room
  review pending**.

### Hardware boundary audit — block 1C

- `src/ui/sleep.cpp` and `src/ui/sleep.h` are **repository lifecycle policy**,
  not ESP32, Wi-Fi or Bluetooth vendor source. Platform APIs provide sleep,
  wake sources and radio shutdown; the firmware owns settings, screensaver
  selection, lock-on-sleep, screen cleanup, button-edge recovery and shutdown
  ordering. The implementation is substantially inherited from upstream,
  with Paginotto-specific reader/screensaver integration changes. Classify it
  as **derived lifecycle logic — targeted rewrite/review pending**.
- `src/hal/battery.cpp` and `src/hal/battery.h` are **repository battery
  policy**, not ESP32 ADC vendor code. Arduino/ESP32 supplies ADC access and
  attenuation; the firmware owns switched measurement, trimmed sampling,
  voltage conversion, the LiPo lookup curve, filtering, hysteresis and icon
  rendering. The implementation is substantially inherited but has materially
  different calibration, lookup, smoothing and rendering policy. Classify it
  as **derived hardware service — targeted rewrite/review pending**.
- `src/ui/lock.cpp` and `src/ui/lock.h` are **repository feature logic**, not
  board-support code. NVS persistence, lock state and unlock gesture policy
  belong to Paginotto; preferences and the input event type are dependencies.
  The implementation is substantially inherited, with changed gesture
  semantics and lifecycle documentation. Classify it as **derived feature
  logic — targeted rewrite/review pending**.

The first-pass hardware boundary classification is now complete: vendor APIs
are external, while display/input/font/sleep/battery/lock remain repository
adapters or product policy. None of these files should be treated as vendor
source merely because they call board APIs.

The percentages above were recomputed against the imported commit using a
line-sequence matcher. They are useful for triage only; a high percentage can
be caused by a conventional adapter, while a short copied block can be more
important than a large generic header.

### Core audit conclusion

- The pure reader core has been rewritten or characterized: paginator,
  `text_util`, `library_nav` and the `StringReadStream` contract.
- The current implementation comparison is materially different from the
  header comparison: `paginator.cpp` is about 44%, `text_util.cpp` about 59%
  and `library_nav.cpp` about 40% similar to the mapped upstream files. These
  figures support that the implementations were substantially changed, but
  they are still not a legal originality certificate.
- Functional preservation is supported by the host suite, now **64 tests and
  227 checks**, including malformed UTF-8, pagination boundaries, invalid
  starts, catalog bounds, EOF and seek behavior.
- This is not yet a full differential test against the old implementation:
  the tests specify the intended/current contract, but we have not generated a
  side-by-side oracle for every historical behavior. That remains a quality
  gap, especially for storage and UI integration.
- The licensing/originality problem is **not solved globally**. The core is in
  much better shape, but storage (`library.cpp`, persistence headers), shared
  UI/widgets, connectivity/OTA, HAL and screensavers still require review or
  clean-room-style rewrites before a Paginotto-only license can honestly cover
  the whole current source tree.

### Why a plain diff is insufficient

A normal textual diff is useful for finding unchanged files and obvious
copying, but it is not an originality test. Renaming variables, reformatting,
reordering functions or translating comments can make a diff look different
while retaining the same expressive structure. Conversely, a generic header
or hardware adapter can legitimately remain similar because its interface is
dictated by the protocol.

For the remaining work, the stronger engineering procedure is:

1. write a behavioural specification from public APIs, file formats and tests;
2. freeze black-box characterization tests before editing;
3. reimplement from that specification without using upstream structure as a
   template;
4. compare normalized tokens/AST shape, not only raw text;
5. review comments, strings, HTML/CSS/JavaScript and assets separately;
6. keep the upstream mapping and the provenance boundary explicit.

This is a clean-room-style engineering process, not a formal legal clean room
because the same agent can inspect both implementations. It is nevertheless
the correct way to avoid mistaking cosmetic renaming for an independent
implementation.

## Historical first-pass action summary

The sections below preserve the original triage notes. Their file counts and
exact-match claims are historical; use the current audit snapshot above for
the present tree.

### Rewrite first

These two implementation files are the clearest remaining derived feature and
should be reimplemented before adding a Paginotto licence:

- `Paginotto/src/web/screensavers.cpp` — 98% similar to
  `Pala_One_2_1/src/web/screensavers.cpp`.
- `Paginotto/src/ui/screensavers.cpp` — 96% similar to
  `Pala_One_2_1/src/ui/screensavers.cpp`.

The replacement should be written from the required screensaver behaviour and
the current Paginotto UI, not by editing or mechanically translating the
upstream implementation.

### Review manually, then either rewrite or explicitly retain

The files below are unchanged or highly similar. They are not all equally
creative: a small interface, a hardware adapter and a complete implementation
should not be treated identically. Nevertheless, each needs an explicit
decision before the Paginotto-only licence is declared:

- **rewrite** if expressive upstream implementation remains;
- **retain as generic interface/adapter** only when the code is dictated by
  the hardware, protocol or a trivial public interface;
- **mark as third-party/upstream** if it cannot be cleanly attributed to
  Paginotto.

## Historical unchanged or byte-for-byte-renamed files

These were reported as byte-for-byte matches by the first-pass scan. The
current exact-match list is in the audit snapshot above; this historical list
is retained to show how the initial triage was produced.

- `Paginotto/partitions.csv` ← `Pala_One_2_1/partitions.csv`
- `src/hal/battery.h` ← `Pala_One_2_1/src/hal/battery.h`
- `src/hal/display.cpp` ← `Pala_One_2_1/src/hal/display.cpp`
- `src/hal/display.h` ← `Pala_One_2_1/src/hal/display.h`
- `src/hal/file_stream.h` ← `Pala_One_2_1/src/hal/file_stream.h`
- `src/hal/wifi_provisioning.h` ← `Pala_One_2_1/src/hal/wifi_provisioning.h`
- `src/pure/arduino_compat.h` ← `Pala_One_2_1/src/pure/arduino_compat.h`
- `src/pure/library_nav.cpp` ← `Pala_One_2_1/src/pure/library_nav.cpp`
- `src/pure/paginator.cpp` ← `Pala_One_2_1/src/pure/paginator.cpp`
- `src/pure/paginator.h` ← `Pala_One_2_1/src/pure/paginator.h`
- `src/pure/stream.h` ← `Pala_One_2_1/src/pure/stream.h`
- `src/pure/text_util.cpp` ← `Pala_One_2_1/src/pure/text_util.cpp`
- `src/storage/fs_util.cpp` ← `Pala_One_2_1/src/storage/fs_util.cpp`
- `src/storage/fs_util.h` ← `Pala_One_2_1/src/storage/fs_util.h`
- `src/storage/kv_store.h` ← `Pala_One_2_1/src/storage/kv_store.h`
- `src/storage/library.h` ← `Pala_One_2_1/src/storage/library.h`
- `src/storage/preferences_store.h` ← `Pala_One_2_1/src/storage/preferences_store.h`
- `src/ui/screen.h` ← `Pala_One_2_1/src/ui/screen.h`
- `src/ui/screens/library_screen.h` ← `Pala_One_2_1/src/ui/screens/library_screen.h`
- `src/ui/screensavers.h` ← `Pala_One_2_1/src/ui/screensavers.h`
- `src/ui/toast.cpp` ← `Pala_One_2_1/src/ui/toast.cpp`
- `src/ui/toast.h` ← `Pala_One_2_1/src/ui/toast.h`
- `src/web/find.h` ← `Pala_One_2_1/src/web/find.h`
- `src/web/reset.h` ← `Pala_One_2_1/src/web/reset.h`
- `src/web/screensavers.h` ← `Pala_One_2_1/src/web/screensavers.h`
- `src/web/web.h` ← `Pala_One_2_1/src/web/web.h`

These were provenance-review candidates at the initial scan. Some are small interfaces or hardware
adapters, so byte identity alone does not mean that every line needs a
creative rewrite; it does mean they cannot yet be covered automatically by a
new Paginotto-only copyright notice.

## Historical substantially similar files to inspect or modify

The initial Git scan detected the following files as largely derived from
upstream. The current recomputed list is in the audit snapshot above:

- `src/web/screensavers.cpp` ← `Pala_One_2_1/src/web/screensavers.cpp` — 98% — **rewrite first**
- `src/ui/screensavers.cpp` ← `Pala_One_2_1/src/ui/screensavers.cpp` — 96% — **rewrite first**
- `src/pure/text_util.h` ← `Pala_One_2_1/src/pure/text_util.h` — 96% — review/rewrite if implementation remains expressive
- `src/ui/sleep.h` ← `Pala_One_2_1/src/ui/sleep.h` — 96% — review; likely generic interface
- `src/hal/wifi_provisioning.cpp` ← `Pala_One_2_1/src/hal/wifi_provisioning.cpp` — 95% — review/rewrite
- `src/ui/widgets.cpp` ← `Pala_One_2_1/src/ui/widgets.cpp` — 95% — review/rewrite
- `src/ui/lock.h` ← `Pala_One_2_1/src/ui/lock.h` — 94% — review; likely interface/adapter
- `src/ui/screens/reader_screen.h` ← `Pala_One_2_1/src/ui/screens/reader_screen.h` — 92% — review/rewrite
- `src/state.h` ← `Pala_One_2_1/src/state.h` — 91% — review/rewrite
- `src/hal/ota.cpp` ← `Pala_One_2_1/src/hal/ota.cpp` — 91% — review/rewrite
- `src/storage/library.cpp` ← `Pala_One_2_1/src/storage/library.cpp` — 91% — review/rewrite
- `src/ui/widgets.h` ← `Pala_One_2_1/src/ui/widgets.h` — 91% — review/rewrite

The screensaver WebUI and runtime are the clearest functional rewrite target.
The remaining files need manual classification: some may be generic APIs or
hardware glue, while others may retain expressive upstream implementation
details. The suggested action above is provisional until that review is done.

## Current interpretation

- The current Paginotto application layer is substantially new: reader
  workflow, persistent indexing/cache, library WebUI, upload/update flows,
  diagnostics, battery estimation and responsive WebUI work are not listed as
  unchanged upstream files.
- That does not by itself clear the unchanged and highly similar modules.
- Deleted upstream files are not automatically an issue for the current
  source tree, but their presence in Git history is intentionally preserved
  and documented by `UPSTREAM.md`.
- Third-party libraries, board support packages, fonts and assets require a
  separate license/asset audit.

## Functional criticality of the flagged files

“Critical” here means critical to the firmware feature set, not “legally
owned by upstream”. A critical file can still be rewritten without changing
the product behaviour.

The classification below covers every file in the unchanged/high-similarity
lists above. It answers “what breaks for the user if this file is removed?”
It does not answer “can this file be licensed?” — provenance review is still
required for all of them.

### Complete classification

#### CORE — reader, persistence and device lifecycle

These are the files that must work for Paginotto to boot, open books, paginate
them, remember state or keep the display usable:

- `src/state.h` — central state and shared device services.
- `src/hal/display.cpp`, `src/hal/display.h` — display backend.
- `src/hal/file_stream.h`, `src/pure/stream.h` — book/file I/O.
- `src/pure/arduino_compat.h` — shared firmware types and compatibility layer.
- `src/pure/library_nav.cpp` — library tree/navigation model.
- `src/pure/paginator.cpp`, `src/pure/paginator.h` — page layout and
  pagination; **functionally critical**.
- `src/pure/text_util.cpp`, `src/pure/text_util.h` — text processing used by
  pagination; **functionally critical**.
- `src/storage/fs_util.cpp`, `src/storage/fs_util.h` — filesystem access.
- `src/storage/kv_store.h`, `src/storage/preferences_store.h` — persistent
  settings and state.
- `src/storage/library.cpp`, `src/storage/library.h` — catalogue and book
  management.
- `src/ui/screen.h` — base screen interface used by the device UI.
- `src/ui/screens/library_screen.h` — library screen interface.
- `src/ui/screens/reader_screen.h` — reader screen interface.
- `Paginotto/partitions.csv` — not runtime logic, but critical for the flash
  layout and safe firmware/data upgrades.

#### IMPORTANT — useful device services, but not the reading engine

The device remains a usable reader without some of these, but loses a major
operational capability or becomes harder to maintain:

- `src/hal/battery.h` — battery API; the device can read without the battery
  percentage UI, but power information is degraded.
- `src/hal/wifi_provisioning.cpp`, `src/hal/wifi_provisioning.h` — Wi-Fi setup.
- `src/hal/ota.cpp` — remote firmware updates.
- `src/ui/sleep.h` — sleep/wake contract; important for battery life and wake
  reliability, though not for pagination.
- `src/ui/widgets.cpp`, `src/ui/widgets.h` — shared controls and drawing
  helpers used across the device UI.
- `src/ui/lock.h` — lock-state interface; important only if the lock feature
  is retained.

#### OPTIONAL — WebUI tools and administrative features

These are not required for local reading with the physical device controls:

- `src/web/web.h` — WebUI framework interface.
- `src/web/find.h` — browser-side book search.
- `src/web/reset.h` — browser-side reset action.

The WebUI is valuable, but its removal does not prevent the reader firmware
from booting and displaying books.

#### COSMETIC — screensavers and feedback

These can be removed without affecting book storage, pagination, reading or
device boot. They are still worth rewriting because they are the most obvious
remaining upstream-derived feature:

- `src/ui/screensavers.cpp`, `src/ui/screensavers.h` — device screensaver.
- `src/web/screensavers.cpp`, `src/web/screensavers.h` — screensaver WebUI.
- `src/ui/toast.cpp`, `src/ui/toast.h` — temporary notifications.

The screensaver rewrite is therefore **low functional risk** and a good first
rewrite: it gives us a clean provenance win without touching the reader core.

## External dependencies

The following are declared in `platformio.ini` and do not belong to Paul or
to Paginotto. Their source and licence notices must be treated separately
from the Paginotto licence:

- **pioarduino `platform-espressif32` 55.03.38-1** — Arduino-ESP32 framework,
  ESP-IDF, board support and bundled framework components.
- **Heltec E-Ink Modules v4.6.0** — Heltec display/board support.
- **Adafruit GFX Library 1.12.6** — graphics primitives.
- **U8g2 for Adafruit GFX 1.8.0** — font/rendering integration.
- **Improv WiFi Library** at commit
  `e37c244e0080349e191252acf4bce146eca118e0` — Wi-Fi provisioning protocol.
- **ArduinoJson 7.2.1** — OTA manifest parsing.
- **QRCode** from `ricmoo/QRCode` — QR generation for the connection screen.

The QRCode dependency is currently requested as `#master`, so it is not
actually pinned to a fixed commit despite the nearby comment saying that it
is fixed. This must be corrected or documented during the dependency audit.

Framework headers such as `Arduino.h`, `WiFi.h`, `WebServer.h`,
`Preferences.h`, `LittleFS.h`, `HTTPClient.h` and ESP-IDF headers are covered
by the selected Arduino/ESP32 framework rather than being Paginotto source.

No file listed above as “identical” or “highly similar” is an external
library: those are files copied or derived into this repository and therefore
are the files that need provenance decisions.

## Next review order

1. Reimplement `src/web/screensavers.cpp` and `src/ui/screensavers.cpp` from
   behaviour and requirements, without using the upstream files as a coding
   template.
2. Review the unchanged and highly similar utility/UI files one by one and
   mark each as `rewrite`, `retain as generic interface`, or `third-party /
   upstream provenance required`.
3. Audit HTML, CSS, JavaScript, fonts, icons and generated assets separately.
4. Only after those reviews, define the exact scope of the Paginotto license.

## Brand cleanup decision

The active Paginotto source previously retained the upstream prefix `PALA_` in
many include guards and one browser-side initialisation flag:

- C/C++ include guards such as `PALA_PURE_PAGINATOR_H`.
- JavaScript flag `window.__palaSleepEditorInit` in the screensaver editor.

These names are historical residue, not independent external dependencies.
They have been renamed mechanically to the Paginotto namespace:

- `PALA_*` → `PAGINOTTO_*`.
- `__palaSleepEditorInit` → `__paginottoSleepEditorInit`.

This is a namespace/branding cleanup only. It does not by itself make an
implementation original and does not replace the provenance review above.
The upstream name remains intentionally present in `UPSTREAM.md` and this
inventory, where it documents historical provenance. It is not present in the
active product source, comments or identifiers after this cleanup.

Header similarity is handled proportionally: declarations, include guards,
plain data types and hardware-shaped interfaces may legitimately remain close
to an upstream API; copied implementation logic, distinctive comments and
product behaviour still require review or a clean rewrite.
