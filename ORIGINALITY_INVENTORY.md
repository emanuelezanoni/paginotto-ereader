# Paginotto provenance inventory

This is a first-pass engineering inventory of the current tree against the
imported upstream snapshot `2c82e59e0f6e87afd204606b930f15cf7b5b7b8a`.
It is a provenance aid, not a legal determination. Similarity percentages
come from Git's rename/similarity detection and must be followed by a manual
review before a licensing boundary is declared.

## Action summary

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

## Unchanged or byte-for-byte-renamed files

These files currently match an upstream file byte for byte, even where the
path or project name changed. The upstream counterpart is shown after `←`.

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

These are provenance-review candidates. Some are small interfaces or hardware
adapters, so byte identity alone does not mean that every line needs a
creative rewrite; it does mean they cannot yet be covered automatically by a
new Paginotto-only copyright notice.

## Substantially similar files to inspect or modify

Git detected the following files as largely derived from upstream:

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

## Next review order

1. Reimplement `src/web/screensavers.cpp` and `src/ui/screensavers.cpp` from
   behaviour and requirements, without using the upstream files as a coding
   template.
2. Review the unchanged and highly similar utility/UI files one by one and
   mark each as `rewrite`, `retain as generic interface`, or `third-party /
   upstream provenance required`.
3. Audit HTML, CSS, JavaScript, fonts, icons and generated assets separately.
4. Only after those reviews, define the exact scope of the Paginotto license.
