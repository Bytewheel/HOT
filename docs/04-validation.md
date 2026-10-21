# HOT — validation plan

## Test matrix

| ID | test | how | pass criteria |
|----|------|-----|---------------|
| T1 | exact wrap crossing | libfaketime stepping through 2038-01-19T03:14:06Z → 03:14:08Z; full QEMU VM with RTC base at the wrap running the integration suite | no mis-ordering, no truncation; system status bit flips exactly at 03:14:07Z on 32-bit fixtures |
| T2 | era round-trip property | property test over int32/u32 boundary values ±2 and random samples, all eras 0–3: `hot_seconds(hot_from_32(v, e)) == e*2^32 + v` and word ordering equals time ordering. Seed: `tests/spec_vet.c` (already 11/11 green). | 100% on fixed seeds + fuzz corpus |
| T3 | decode fuzz | random `uint64_t` words through `hot_seconds`/predicates/policies under ASan/UBSan | no UB; tagged/untagged classification exact; reserved forms rejected |
| T4 | clock-step races | era context updates under NTP step/slew, threaded and multi-process | monotonic results, no torn reads |
| T5 | mixed-fleet interop | 32-bit container peers (pre- and post-wrap clocks) exchanging v1/v2 messages with 64-bit hosts | v1 refusal is loud (V-G4); v2 tagged values convert exactly |
| T6 | storage round-trip | write HOT words, simulate reboot (reload era state, dead-RTC case), read back | bit-exact or correct `-HOT_EAMBIGUOUS` |
| T7 | golden bytes | wire/serialization dumps of tagged/untagged words | byte-exact fixtures committed |
| T8 | performance | tag/strip/convert microbenchmarks | tag ops branchless, < 2 ns/op; no fleet benchmark regression |
| T9 | NTP era rollover | chrony/timesyncd at 2036 rollover with correct and dead RTC | resync correct, or loud failure — never a silent wrong lock |
| T10 | audit self-test | `scripts/host-audit.sh` against fixture hosts/containers with known profiles | expected PASS/FAIL classification for each |

## Harness

- In-process time travel: `libfaketime`. Whole-system: QEMU `-rtc
  base=2038-01-19T03:14:06Z`. Quarterly full-dress "2038 GameDay" VM
  image (stage 6) until GA.
- CI: armv7 (Debian trixie, glibc ≥ 2.34), i386, x86_64, aarch64 — all
  with `-D_TIME_BITS=64` (A1), ASan/UBSan on fuzz targets.
- Gates required to merge once enabled: A3 cast gate, A5 matrix, T1,
  T2, T3.

## Patch → test mapping

| patch classes | exercised by |
|---------------|--------------|
| A1–A6 | V-A1/A5 in CI matrix; T10 |
| B1–B3 | V-B1/B2 review gate; V-B3 via 32-bit module test rig |
| C1–C3 | V-C1 artifact manifest; T1 (V-C3 boundary years) |
| D1–D6 | V-A3 gate, T1, T3; V-D6 via wtmp fixture |
| E1–E4 | T10 + per-format round-trips (T6/T7) |
| F1–F5 | T5, T9 |
| G1–G4 | T2, T3, T6, T7; V-G4 negative tests in T5 |

## Acceptance

Rollout stage N may not proceed until every `V-<patch>` test for the
surfaces that stage touches is green (`docs/05-rollout.md`).
