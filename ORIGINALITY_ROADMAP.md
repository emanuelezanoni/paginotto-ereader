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
   - [ ] `library_nav` and the remaining stream interfaces review/rewrite.

   The streaming compaction API intentionally retains its existing contract:
   a chunk may return a trailing space when `trimTail=false`. Therefore a
   space at the end of one independently returned chunk cannot be removed
   retroactively when the next chunk starts with a newline. Fixing that would
   require a new buffered-stream API or changing the caller's write model; it
   is not silently claimed as solved by a source refactor.

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

- [x] Upstream license/assignment check completed.
- [x] Initial comparison completed.
- [x] First-pass automated provenance inventory.
- [ ] Full manual provenance inventory.
- [x] Paginator characterization and rewrite.
- [ ] Remaining core reader algorithms review/rewrite.
- [ ] Persistence and library storage review/rewrite.
- [ ] Device UI and hardware adapter review/rewrite.
- [ ] Connectivity and maintenance review/rewrite.
- [ ] Screensaver WebUI rewrite.
- [ ] Remaining inherited utility review.
- [ ] Dependency and asset license inventory.
- [ ] Provenance-boundary commit.
- [ ] Paginotto license and third-party notices.
- [ ] Final verification and `v2.0.0` release.
