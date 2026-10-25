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

Priority order:

1. WebUI screensaver editor and slot manager.
2. Runtime screensaver management where it remains substantially inherited.
3. Unchanged or nearly unchanged utility modules that are not merely trivial
   interfaces or standard hardware adapters.

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
- [ ] Screensaver WebUI rewrite.
- [ ] Remaining inherited utility review.
- [ ] Dependency and asset license inventory.
- [ ] Provenance-boundary commit.
- [ ] Paginotto license and third-party notices.
- [ ] Final verification and `v2.0.0` release.
