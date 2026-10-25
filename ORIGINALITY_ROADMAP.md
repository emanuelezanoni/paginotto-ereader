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
- [ ] Manual file-by-file classification completed.

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

## Remaining work — ordered by importance

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
   - [ ] Complete transition tests for scan cancellation, sleep/resume,
     layout changes, WebUI jumps and forward/backward boundary navigation.
2. **Hardware boundary classification** — inspect display, input, sleep,
   battery, font and file-stream files and mark each block as external API,
   Paginotto wrapper/policy or inherited implementation. Do this before any
   rewrite so vendor code is not duplicated and copied product logic is not
   excused as vendor code.
3. **Reader-facing UI/HAL implementation** — rewrite only the inherited
   product logic found in step 2, preserving the Heltec/ESP32 contracts;
   compile both board variants after each subsystem.
4. **Connectivity and maintenance** — Wi-Fi provisioning, OTA and lock
   lifecycle. These are important for the product but do not block local
   pagination and book reading.
5. **Cosmetic/UI remainder** — widgets, toast and screensaver runtime/WebUI.
   Screensavers are the clearest remaining derived feature but are not core
   reader functionality.
6. **Final provenance and licensing audit** — refresh the inventory from the
   current tree, audit dependencies/assets, add third-party notices, define
   the provenance boundary, then choose and add the Paginotto license.

- [x] Upstream license/assignment check completed.
- [x] Initial comparison completed.
- [x] First-pass automated provenance inventory.
- [ ] Full manual provenance inventory.
- [x] Paginator characterization and rewrite.
- [x] Text utility review/rewrite and stricter UTF-8 handling.
- [x] Library navigation review/rewrite.
- [x] Stream contract characterization and pure adapter tests.
- [x] Audit fix in paginator: invalid starting offsets and zero-line layouts
  now terminate safely instead of reading from an unintended cursor position.
- [x] Core/provenance audit performed; stale inventory counts corrected and
  remaining high-similarity modules listed explicitly.
- [ ] Differential oracle against the imported implementation (needed for
  stronger historical-behaviour compatibility evidence).
- [ ] Remaining core reader algorithms review/rewrite.
- [x] Persistence and library storage review/rewrite (paths, catalog,
  metadata and page-cache integration completed).
- [ ] Classify device UI/HAL files into external API, Paginotto wrapper and
  inherited implementation before deciding what to rewrite.
- [ ] Device UI and hardware adapter review/rewrite, starting with reader
  integration and only then display/input/sleep/battery.
- [ ] Connectivity and maintenance review/rewrite.
- [ ] Screensaver WebUI rewrite.
- [ ] Remaining inherited utility review.
- [ ] Dependency and asset license inventory.
- [ ] Provenance-boundary commit.
- [ ] Paginotto license and third-party notices.
- [ ] Final verification and `v2.0.0` release.
