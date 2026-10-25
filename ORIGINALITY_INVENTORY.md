# Paginotto provenance inventory

This is a first-pass engineering inventory of the current tree against the
imported upstream snapshot `2c82e59e0f6e87afd204606b930f15cf7b5b7b8a`.
It is a provenance aid, not a legal determination. Similarity percentages
come from Git's rename/similarity detection and must be followed by a manual
review before a licensing boundary is declared.

## Unchanged or byte-for-byte-renamed files

These files currently match an upstream file byte for byte, even where the
path or project name changed:

- `Paginotto/partitions.csv`
- `src/hal/battery.h`
- `src/hal/display.cpp`
- `src/hal/display.h`
- `src/hal/file_stream.h`
- `src/hal/wifi_provisioning.h`
- `src/pure/arduino_compat.h`
- `src/pure/library_nav.cpp`
- `src/pure/paginator.cpp`
- `src/pure/paginator.h`
- `src/pure/stream.h`
- `src/pure/text_util.cpp`
- `src/storage/fs_util.cpp`
- `src/storage/fs_util.h`
- `src/storage/kv_store.h`
- `src/storage/library.h`
- `src/storage/preferences_store.h`
- `src/ui/screen.h`
- `src/ui/screens/library_screen.h`
- `src/ui/screensavers.h`
- `src/ui/toast.cpp`
- `src/ui/toast.h`
- `src/web/find.h`
- `src/web/reset.h`
- `src/web/screensavers.h`
- `src/web/web.h`

These are the first rewrite or explicit provenance-review candidates. Some
are small interfaces or hardware adapters, so byte identity alone does not
mean that every line needs a creative rewrite; it does mean they cannot yet
be covered automatically by a new Paginotto-only copyright notice.

## Substantially similar files

Git detected the following files as largely derived from upstream:

- `src/web/screensavers.cpp` — 98%
- `src/ui/screensavers.cpp` — 96%
- `src/pure/text_util.h` — 96%
- `src/ui/sleep.h` — 96%
- `src/hal/wifi_provisioning.cpp` — 95%
- `src/ui/widgets.cpp` — 95%
- `src/ui/lock.h` — 94%
- `src/state.h` — 91%
- `src/hal/ota.cpp` — 91%
- `src/storage/library.cpp` — 91%
- `src/ui/widgets.h` — 91%
- `src/ui/screens/reader_screen.h` — 92%

The screensaver WebUI and runtime are the clearest functional rewrite target.
The remaining files need manual classification: some may be generic APIs or
hardware glue, while others may retain expressive upstream implementation
details.

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

## Next review order

1. Reimplement `src/web/screensavers.cpp` and `src/ui/screensavers.cpp` from
   behaviour and requirements, without using the upstream files as a coding
   template.
2. Review the unchanged and highly similar utility/UI files one by one and
   mark each as `rewrite`, `retain as generic interface`, or `third-party /
   upstream provenance required`.
3. Audit HTML, CSS, JavaScript, fonts, icons and generated assets separately.
4. Only after those reviews, define the exact scope of the Paginotto license.
