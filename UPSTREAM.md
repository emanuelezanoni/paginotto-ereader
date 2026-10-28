# Upstream provenance

Paginotto firmware started from the Pala One firmware project:

- Upstream repository: https://github.com/PaulLagier/pala-one-firmware
- Upstream author: Paul Lagier
- Imported branch: `main`
- Imported commit: `2c82e59e0f6e87afd204606b930f15cf7b5b7b8a`

## What has been done

The codebase has been substantially rewritten since the initial import.
Every high-risk module — the paginator, book storage, WebUI, OTA system,
screensaver engine, sleep management, and input handling — has been
reimplemented or refactored at clear product-policy boundaries.
The provenance boundary is established with the v2.0.0 release.

## What remains historical upstream

The following elements are retained from or directly derived from upstream:

- `Paginotto/partitions.csv` — board partition configuration; functionally
  identical to upstream, with added Paginotto documentation comments.
- General project structure (Arduino/PlatformIO layout, `Paginotto/` source
  directory, `platformio.ini` environment names).

## Licensing

Paginotto-authored source code is licensed under the MIT License
(see `LICENSE`). This license applies to the code written for Paginotto
and does not retroactively relicense inherited upstream material, which
was imported without an explicit license.

External dependencies have their own licenses, documented in
`THIRD_PARTY_NOTICES.md`.
