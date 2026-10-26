# Paginotto originality and licensing roadmap

This document tracks the work needed to separate the current Paginotto code
from the inherited upstream implementation while preserving the existing
development history.

## Starting point

- The upstream repository has no explicit license.
- No separate copyright assignment or license grant has been found in the
  available project documentation.
- The upstream commit and provenance remain documented in `UPSTREAM.md`.
- The existing Git history is intentionally preserved; it is not treated as
  a claim that every historical commit is Paginotto-original.

## 2. Provenance inventory

- Compare every current source file with the imported upstream commit.
- Classify each file as:
  - Paginotto-original;
  - modified but still derived;
  - unchanged or nearly unchanged upstream code;
  - third-party dependency or asset.
- Record the result in a private working report or in this document when the
  classification is stable.
- Give special attention to copied JavaScript, HTML, CSS, comments and assets,
  not only to C++ files.

### First-pass result

- [x] Automated similarity/rename scan completed against upstream commit
  `2c82e59e0f6e87afd204606b930f15cf7b5b7b8a`.
- [x] Exact matches and high-similarity files recorded in
  `ORIGINALITY_INVENTORY.md`.
- [x] Manual file-by-file classification completed in
  `ORIGINALITY_INVENTORY.md`.

The scan confirms that the screensaver implementation is the main functional
rewrite target, but also found unchanged or nearly unchanged utility, UI,
storage and hardware-support files. No licensing conclusion is drawn from the
similarity percentages alone.

### Brand and namespace cleanup

- [x] Replace inherited `PALA_*` include-guard names in active source with
  `PAGINOTTO_*`.
- [x] Replace the inherited screensaver JavaScript flag name with a Paginotto
  namespace.
- [x] Keep upstream names in `UPSTREAM.md` and provenance reports only, where
  they are required for attribution and history.

This cleanup removes old branding from the product source, but is not counted
as a substantive originality rewrite.

## 3. Rewrite the remaining derived code

The work is intentionally split into two tracks. The core reader track comes
first; the cosmetic screensaver track is postponed until the reader core has a
clean provenance decision.

### Core-first track

1. **Pure reader algorithms**
   - `src/pure/paginator.cpp`
   - `src/pure/paginator.h`
   - `src/pure/text_util.cpp`
   - `src/pure/text_util.h`
   - `src/pure/library_nav.cpp`
   - `src/pure/stream.h`
   - `src/hal/file_stream.h`

   Freeze the current behaviour in host tests first, then reimplement only
   the files that retain expressive upstream implementation. The paginator,
   text normalisation and library-tree rules are the highest-risk part because
   they directly affect page breaks and navigation.

   Current progress:

   - [x] Paginator behaviour characterized with additional edge-case tests.
   - [x] `src/pure/paginator.cpp` reimplemented with the existing API and
     offset semantics preserved.
   - [x] `text_util.cpp` reviewed and reimplemented with the existing API.
   - [x] UTF-8 validation strengthened: overlong encodings, surrogate halves,
     out-of-range code points and invalid lead bytes now fall back safely one
     byte at a time. Added regression coverage for these cases.
   - [x] Typography normalization and streaming compaction were kept
     behaviour-compatible while being simplified and documented.
   - [x] `library_nav.cpp` reviewed and reimplemented with the existing menu
     ordering and catalog references preserved.
   - [x] Added defensive handling for null output/state pointers, invalid
     counts, zero capacity and malformed empty folders.
   - [x] `stream.h` contract characterized with host tests for byte order,
     EOF, absolute seek, exact-EOF seek and invalid seeks.
   - [x] `StringReadStream` confirmed as a small, self-contained test adapter;
     no unnecessary redesign was introduced.
   - [x] `file_stream.h` classified as a thin Arduino/HAL adapter rather than
     core reader logic. It remains subject to the later hardware-layer audit.

   The streaming compaction API intentionally retains its existing contract:
   a chunk may return a trailing space when `trimTail=false`. Therefore a
   space at the end of one independently returned chunk cannot be removed
   retroactively when the next chunk starts with a newline. Fixing that would
   require a new buffered-stream API or changing the caller's write model; it
   is not silently claimed as solved by a source refactor.

   Storage progress:

   - [x] Pure path and filename utilities reimplemented with explicit
     UTF-8-safe truncation and expanded boundary tests.
   - [x] Filesystem helpers hardened so existing files are not mistaken for
     directories and `/books` is never removed implicitly.
   - [x] Catalog ordering made deterministic when books share a display name.
   - [x] Catalog insertion, duplicate handling, fixed-buffer truncation and
     deterministic ordering isolated into `pure/library_catalog.cpp` and
     covered by host tests.
   - [x] LittleFS-backed path compiled successfully for both firmware boards.

   Persistence progress:

   - [x] Existing per-book keys (`_p`, `_off`, `_anc`, `_anc_at`) preserved.
   - [x] Offset storage now supports the complete `uint32_t` range while
     retaining a fallback reader for legacy integer values.
   - [x] Negative pages, empty keys, invalid anchors, self-renames and orphan
     anchor fields covered by host tests.
   - [x] Metadata helpers compiled successfully for both firmware boards.

   Reader-state/cache progress:

   - [x] Page-cache header encoding and validation extracted into the pure
     `page_cache_format` module and covered by host tests.
   - [x] Existing cache magic, layout stamp and file-size compatibility kept
     unchanged.
   - [x] Malformed cache sizes, stale layout stamps, null output pointers and
     self-renames are handled safely.
   - [x] Cache integration compiled successfully for both firmware boards.
   - [x] Final audit completed: the cache writer now rejects source files too
     large for its fixed-width on-disk size field instead of truncating the
     value during serialization.
   - [x] Clean rebuild verified both firmware environments after the audit.

   Hardware/dependency boundary:

   - The board/framework layer is external and is not a rewrite target:
     pioarduino/Arduino-ESP32/ESP-IDF, Heltec E-Ink Modules, Adafruit GFX,
     U8g2, Improv WiFi Library, ArduinoJson and QRCode are declared in
     `platformio.ini` and retain their own licenses.
   - Project files that call those APIs are not automatically third-party.
     `display`, `input`, `battery`, `sleep`, `widgets`, `lock`, OTA and Wi-Fi
     provisioning contain Paginotto adapters, policy and user-visible
     behaviour; they require provenance review where upstream similarity is
     high.
   - The review must separate three things: external API/driver code,
     Paginotto wrapper or product policy, and any inherited implementation
     still present inside the wrapper. Hardware-specific code is therefore
     reviewed file by file, not blindly rewritten or blindly classified as
     vendor code.

2. **Persistence and library storage**
   - `src/storage/fs_util.cpp`
   - `src/storage/fs_util.h`
   - `src/storage/kv_store.h`
   - `src/storage/preferences_store.h`
   - `src/storage/library.cpp`
   - `src/storage/library.h`
   - `src/state.h`

   Preserve on-device paths, keys and file formats unless a migration is
   explicitly designed. Verify library loading, page cache interaction and
   existing books after each change.

3. **Device UI and hardware adapters**
   - `src/hal/display.cpp`
   - `src/hal/display.h`
   - `src/ui/screen.h`
   - `src/ui/screens/library_screen.h`
   - `src/ui/screens/reader_screen.h`
   - `src/ui/widgets.cpp`
   - `src/ui/widgets.h`
   - `src/ui/sleep.h`

   These should be changed only after the pure and storage layers are stable.
   Hardware-shaped interfaces may remain structurally conventional; copied
   implementation logic must still be rewritten or explicitly attributed.

4. **Connectivity and maintenance services**
   - `src/hal/wifi_provisioning.cpp`
   - `src/hal/wifi_provisioning.h`
   - `src/hal/ota.cpp`
   - `src/ui/lock.h`
   - `src/hal/battery.h`

   These are important but not prerequisites for local book pagination. They
   come after the reader core is safe.

### Cosmetic/optional track — postponed

1. WebUI and device screensaver implementations.
2. Toast/feedback interfaces.
3. Optional WebUI find/reset interfaces.

Priority order for the eventual full separation:

1. Core reader algorithms.
2. Persistence and library storage.
3. Device UI and hardware adapters.
4. Connectivity and maintenance services.
5. WebUI screensaver editor and slot manager.
6. Runtime screensaver management where it remains substantially inherited.

The replacement must be implemented from the required behaviour and public
interfaces, without copying upstream source structure, comments or distinctive
implementation details. Preserve user-visible behaviour where practical, but
allow the implementation and internal organisation to be Paginotto-specific.

## 4. Audit dependencies and assets

- List PlatformIO, Arduino, ESP32 and browser-side dependencies.
- Identify the license for each dependency.
- Check fonts, icons, default screensavers, JavaScript snippets and generated
  assets separately.
- Do not include third-party code or assets in the Paginotto copyright claim.

## 5. Establish the provenance boundary

- Run the host tests and both board builds.
- Perform a final source comparison against the upstream commit.
- Create a dedicated commit documenting that the remaining derived blocks have
  been replaced.
- Update `UPSTREAM.md` to state what was inherited historically and what was
  replaced before the boundary.

## 6. Add project licensing documents

- Add a license for Paginotto-authored code, after choosing the intended
  license (MIT or Apache-2.0 are candidates).
- Add `THIRD_PARTY_NOTICES` with dependency and asset licenses.
- State clearly that the license applies to Paginotto-authored material, not
  to external dependencies or historical upstream blobs.

## 7. Document the release boundary

- Update the README with a short, user-appropriate ownership/provenance note.
- Record the first release whose current source is covered by the Paginotto
  license.
- Keep `UPSTREAM.md` as historical provenance.
- Do not rewrite Git history unless a completely clean public history becomes
  a separate requirement.

## 8. Release and verification

- Use a major version for the licensing boundary, likely `v2.0.0`.
- Verify WebUI, screensavers, OTA, reading, storage and both board variants.
- Confirm that the repository contains no accidental upstream license claim or
  copied attribution that should not be present in newly written code.
- Publish the boundary commit and release only after the audit is complete.

## Current status

### What is complete

- **Core reader:** paginator, text/UTF-8 handling, library navigation and
  stream contracts are characterized, rewritten or hardened.
- **Storage and persistence:** paths, filesystem helpers, catalog, metadata,
  reading position, persistent keys and page-cache integration are complete.
- **Reader integration:** opening/resume, legacy fallback, incremental scan,
  cancellation, forward/backward boundaries, sleep/resume intent, layout
  remapping and WebUI jumps are covered by host tests and firmware builds.
  No reader rewrite is currently pending.
- **Verification:** the last reader audit passed 110 tests / 412 checks,
  `pio check`, and clean builds for `wireless-paper-v1_1` and
  `wireless-paper-v1_2`.
- **Hardware boundary classification:** `display`, `file_stream`, `input`,
  `font`, `sleep`, `battery` and `lock` are repository adapters or product
  policy, not vendor source. Heltec, Arduino/ESP-IDF, Adafruit GFX, U8g2 and
  the other declared dependencies remain external.
- **Connectivity 2A:** Wi-Fi credential validation is now a pure, tested
  policy. Empty/oversized SSIDs and oversized WPA passphrases are rejected
  before NVS writes; open-network empty passwords remain valid. Host tests
  pass 112 tests / 418 checks, `pio check` is clean, and both board builds
  pass.

### What remains — ordered by importance

1. **Targeted rewrite/review of derived device modules — complete.**
   `display`/`file_stream`, `input`, `font`, `sleep`, `battery` and `lock`
   were refactored at their product-policy boundaries while preserving the
   hardware contracts. Vendor APIs and vendor implementations remain external.
2. **Connectivity and maintenance — complete.** Credential validation,
   provisioning ownership/cleanup, association failure handling, OTA manifest
   and channel validation, stalled/truncated downloads, image validation and
   rollback confirmation are covered. Keep Arduino/ESP-IDF and library code
   external.
3. **Remaining UI and screensavers — complete as an implementation phase.**
   The screensaver runtime/editor, slot validation, shared widget list policy
   and toast deadline policy were refactored and tested. The generic `screen`
   interface remains an API boundary, not a substantive implementation.
4. **Final provenance/licensing audit — complete.** The dependency inventory,
   `THIRD_PARTY_NOTICES.md`, current-tree rescan, manual retained-interface
   review and differential compatibility oracle are complete. The default
   screensaver is recorded as a Paginotto meme asset. The audit scope is now
   explicit: materially rewritten product logic is Paginotto material;
   generic contracts and compatibility shims are boundary material; historical
   configuration and third-party dependencies/assets are excluded from a
   Paginotto-only claim. Choosing and applying the project license remains in
   the boundary-release phase.
5. **Boundary and release.** Update `UPSTREAM.md` and README, document the
   exact boundary between inherited history, external material and
   Paginotto-authored code, choose the Paginotto license, run the full matrix,
   and prepare the planned `v2.0.0` release.

## Updated execution plan

This is the authoritative order for the remaining work. A phase is complete
only when its behaviour is tested where practical, `pio check` passes, and
both firmware environments compile. No phase below requires rewriting vendor
libraries or changing the already-stable reader/storage formats.

### Phase 1 — Rewrite inherited device logic

**Status: complete.** The derived device modules have been rewritten or
refactored at their product-policy boundaries. Vendor APIs and board drivers
remain external dependencies; no vendor library was rewritten.

Work in this order, one bounded subsystem at a time:

1. `file_stream` + `display`: preserve `IReadStream`, framebuffer geometry,
   rotation and refresh semantics; remove inherited implementation structure.
   **`file_stream` and `display` complete.**
2. `input`: preserve ISR safety, event timings and public `ButtonEvent`
   behaviour; pure edge/debounce and click-count policy extracted, queue and
   classifier transitions refactored.
3. `font`: preserve font roles, layout metrics, cache stamps and bionic mode;
   keep U8g2 as the rendering dependency; bionic UTF-8 policy extracted.
4. `sleep` + `lock`: preserve NVS keys, wake behaviour, lock lifecycle and
   radio shutdown; timeout policy and lock persistence centralized.
5. `battery`: preserve board ADC wiring and visible percentage behaviour;
   test sampling, invalid readings, filtering, hysteresis and UI bounds; the
   voltage curve and display slew policy are pure.

### Phase 2 — Finish Wi-Fi and OTA

**Status: complete.** Credential validation, provisioning lifecycle and OTA
failure/rollback handling are implemented and tested. Provisioning now keeps
the upload SoftAP alive while associating on STA, times out deterministically,
and cleans up failed associations without overwriting saved credentials. OTA
accepts only known channels, rejects malformed manifests and oversized or
truncated images, aborts stalled downloads, validates the image through the
ESP-IDF OTA API, and confirms a successfully booted image for rollback safety.

The remaining work is outside this phase: UI/screensaver provenance, the final
manual provenance/licensing audit, and the boundary/release documentation.

### Phase 3 — Finish non-reader UI

**Status: complete as an implementation phase.** Screensaver runtime and
WebUI slot management were rewritten at their product-policy boundaries;
malformed slot arguments are rejected, uploads are size-checked before
promotion, and shared widget/toast policies are pure and host-tested. The
remaining provenance work is the global manual inventory and licensing audit.

### Phase 4 — Provenance and licensing closure

**Status: complete.** The dependency inventory, QRCode pin,
`THIRD_PARTY_NOTICES.md`, current exact-match scan, manual boundary review and
differential oracle are complete. The default screensaver is classified as a
Paginotto meme asset. The final scope is documented in
`ORIGINALITY_INVENTORY.md`; license selection and application are deferred to
the boundary-release phase.

### Phase 5 — Boundary release

**Status: not started.** Update `UPSTREAM.md` and README, state the exact
boundary between historical upstream code, third-party material and rewritten
Paginotto code, choose the project license, run the complete verification
matrix, and prepare `v2.0.0`.

### Explicitly closed and not to be reopened

- paginator, text/UTF-8, library navigation and stream core;
- filesystem, catalog, metadata, persistent keys and page-cache;
- reader opening/resume, scan, cancellation, navigation boundaries,
  sleep/resume intent, layout remapping and WebUI jumps;
- hardware/vendor boundary classification;
- Wi-Fi credential validation (2A).

## Detailed completion checklist

1. **Reader integration and state** — audit `state`, `reader_screen` and the
   hand-off between persistent position, page cache and the active book. This
   is the next functional priority because it can break opening/resuming a
   book even though the pure core and storage layers pass independently.
   - [x] Extracted and tested the pure resume decision contract: canonical
     byte offset, legacy page fallback, missing/stale cache and asynchronous
     scan setup.
   - [x] Integrated the resolver without changing persistence keys, cache
     format or public reader APIs.
   - [x] Fixed legacy resume when the requested page was already cached: the
     cursor and render offset now advance together.
   - [x] Corrected the leaving-state documentation and removed the obsolete
     full-table cache flush call; the cache is streaming and incremental.
   - [x] Extracted and integrated the pure incremental-scan transitions for
     target reached, forward progress and non-progressing EOF/corrupt input.
     The reached page is still appended to the streaming cache before the
     scan completes.
   - [x] Extracted and integrated forward-navigation boundary decisions for
     valid pages, empty files, EOF, and non-progressing offsets.
   - [x] Added explicit scan-cancellation reset semantics and backward
     navigation boundary decisions; both are covered by host tests.
   - [x] Complete transition tests for sleep/resume, layout changes and
     WebUI jumps. Wake intent is single-shot, layout changes preserve the
     canonical byte offset, invalidate stale volatile page hints, and WebUI
     jumps normalize signed/oversized input.
2. **Hardware boundary classification** — inspect display, input, sleep,
   battery, font and file-stream files and mark each block as external API,
   Paginotto wrapper/policy or inherited implementation. Do this before any
   rewrite so vendor code is not duplicated and copied product logic is not
   excused as vendor code. **Blocks 1A, 1B and 1C complete:** `display`,
   `file_stream`, `input`, `font`, `sleep`, `battery` and `lock` are
   classified as derived repository adapters/policy; their vendor
   dependencies are external and are not candidates for rewriting.
3. **Reader-facing UI/HAL implementation** — rewrite only the inherited
   product logic found in step 2, preserving the Heltec/ESP32 contracts;
   compile the supported board variants at phase closure. **Complete:**
   `file_stream`, `display`, `input`, `font`, `sleep`, `lock` and `battery`.
4. **Connectivity and maintenance** — Wi-Fi provisioning, OTA and lock
   lifecycle. **Complete:** credential validation, provisioning ownership and
   cleanup, OTA validation, stalled/truncated download handling and rollback
   confirmation.
5. **Cosmetic/UI remainder** — widgets, toast and screensaver runtime/WebUI.
   Screensavers are the clearest remaining derived feature but are not core
   reader functionality.
6. **Final provenance and licensing audit** — refresh the inventory from the
   current tree, audit dependencies/assets, add third-party notices, define
   the provenance boundary, then choose and add the Paginotto license.

- [x] Upstream license/assignment check completed.
- [x] Initial comparison completed.
- [x] First-pass automated provenance inventory.
- [x] Full manual provenance inventory; retained interfaces, adapters,
  historical configuration and external material were classified manually in
  `ORIGINALITY_INVENTORY.md`.
- [x] Paginator characterization and rewrite.
- [x] Text utility review/rewrite and stricter UTF-8 handling.
- [x] Library navigation review/rewrite.
- [x] Stream contract characterization and pure adapter tests.
- [x] Audit fix in paginator: invalid starting offsets and zero-line layouts
  now terminate safely instead of reading from an unintended cursor position.
- [x] Core/provenance audit performed; stale inventory counts corrected and
  remaining high-similarity modules listed explicitly.
- [x] Differential compatibility oracle against the imported implementation;
  the reproducible check is documented in `DIFFERENTIAL_ORACLE.md` and its
  hardware-test limitation is explicit.
- [x] Remaining core reader algorithms review/rewrite; reader integration,
  position recovery, incremental scan, navigation boundaries, sleep/resume,
  layout remapping and WebUI jumps are covered by host tests and firmware CI.
- [x] Persistence and library storage review/rewrite (paths, catalog,
  metadata and page-cache integration completed).
- [x] Classify device UI/HAL files into external API, Paginotto wrapper and
  inherited implementation before deciding what to rewrite. Blocks 1A
  (`display`, `file_stream`), 1B (`input`, `font`) and 1C (`sleep`,
  `battery`, `lock`) are complete. Vendor APIs remain external; repository
  adapters and product policies are classified as derived where the
  upstream comparison supports that conclusion.
- [x] Wi-Fi credential policy extracted and tested; invalid SSIDs and
  passphrases are rejected before persistence, while open-network passwords
  remain supported.
- [x] Dependency inventory completed for the declared PlatformIO libraries;
  QRCode is pinned to an immutable commit and `THIRD_PARTY_NOTICES.md` records
  the applicable package notices.
- [x] Device UI and hardware adapter review/rewrite. Boundary classification
  and targeted rewrites are complete for display, file-stream, input, font,
  sleep, battery and lock; vendor APIs remain external.
- [x] Connectivity and maintenance review/rewrite. Credential validation,
  provisioning lifecycle, upload-radio ownership, OTA channel/manifest/image
  validation, download abort paths and rollback confirmation are complete.
- [x] Screensaver runtime/WebUI and shared UI policy rewrite. Runtime slot
  selection, upload/delete argument validation, atomic image promotion,
  widgets list-window calculation and toast expiry are covered by the current
  implementation and host tests.
- [x] Remaining inherited utility review; retained candidates are either
  generic contracts/compatibility shims, product adapters/policy, historical
  configuration or external dependencies.
- [x] Default screensaver asset classified as a Paginotto meme; dependency
  notices are recorded in `THIRD_PARTY_NOTICES.md`.
- [x] Complete license inventory and final Paginotto license scope.
- [ ] Provenance-boundary commit.
- [ ] Paginotto license and third-party notices.
- [ ] Final verification and `v2.0.0` release.
