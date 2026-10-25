# Differential compatibility oracle

Status: **PASS**
Upstream ref: `upstream/main`

This is a reproducible engineering check, not a legal originality certificate.
The upstream firmware has no equivalent host black-box harness, so the oracle
checks contract-to-test coverage, persistent-format markers and exact-match
allowlisting. Runtime hardware behaviour still requires device testing.

## Checks
- exact-match allowlist: Paginotto/partitions.csv only.
- reader core: 3 sources mapped to 3 host tests.
- storage and persistence: 2 sources mapped to 2 host tests.
- reader integration: 4 sources mapped to 4 host tests.
- device and UI policy: 5 sources mapped to 5 host tests.
- connectivity and maintenance: 2 sources mapped to 2 host tests.
- persistent page-state markers are present.

## Current exact matches
- `Paginotto/partitions.csv`

## Limitations
- This oracle does not prove source-code originality or licensing rights.
- It does not replace hardware testing of display, input, radio, sleep and OTA.
- It intentionally preserves the upstream-compatible persistent keys and formats.
