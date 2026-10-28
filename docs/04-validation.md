# HOT — validation plan

## Test matrix

| ID | test | how | pass criteria |
|----|------|-----|---------------|
| T1 | exact wrap crossing | libfaketime stepping through 2038-01-19T03:14:06Z → 03:14:08Z; full QEMU VM with RTC base at the wrap running the integration suite | no mis-ordering, no truncation; system status bit flips exactly at 03:14:07Z on 32-bit fixtures |
| T2 | era round-trip property | property test over int32/u32 boundary values ±2 and random samples, all eras 0–3: `hot_seconds(hot_from_32(v, e)) == e*2^32 + v` and word ordering equals time ordering. Seed: `tests/spec_vet.c` (already 11/11 green). | 100% on fixed seeds + fuzz corpus |
| T3 | decode fuzz | random `uint64_t` words through `hot_seconds`/predicates/policies under ASan/UBSan | no UB; tagged/untagged classification exact; reserved forms rejected |
| T4 | clock-step races | era context updates under NTP step/slew, threaded and multi-process | monotonic results, no torn reads |
| T5 | mixed-fleet interop | 32-bit container peers (pre- and post-wrap clocks) exchanging v1/v2 messages with 64-bit hosts | v1 refusal is loud (V-G4); v2 tagged values convert exactly |
| T6 | storage round-trip | write HOT words and 12/16-byte records (storage v2, both byte orders), simulate reboot (reload era state, dead-RTC case), read back | bit-exact or correct `-HOT_EAMBIGUOUS` |
| T7 | golden bytes | wire/serialization dumps of tagged/untagged words; wire v1/v2/v3 frames and storage v1/v2 headers in LE and BE | byte-exact fixtures committed |
| T8 | performance | tag/strip/convert microbenchmarks | tag ops branchless, < 2 ns/op; no fleet benchmark regression |
| T9 | NTP era rollover | chrony/timesyncd at 2036 rollover with correct and dead RTC | resync correct, or loud failure — never a silent wrong lock |
| T10 | audit self-test | `scripts/host-audit.sh` against fixture hosts/containers with known profiles | expected PASS/FAIL classification for each |
| T11 | subsecond canonicality (`docs/08`) | property test over fraction boundaries (0, 1, 10^9−1) ±1 and random samples at levels 1–2, plain/tagged/deep classes; floor semantics on negative instants; pack/unpack round-trip | 100% on fixed seeds; every non-canonical form (ns ≥ 10^9, as ≥ 10^9, class 10/11) rejected `-HOT_EMALFORMED` |
| T12 | deep-past records (`docs/08`) | bias round-trip over [−2^62, 2^62) boundaries ±1; class/compat mutual exclusion; 8-byte surfaces refuse pre-1970 untagged (`-HOT_ERANGE`); intra-class ordering; cross-class via conversion | exact seconds recovery; every contradiction rejected loudly |
| T13 | byte order (`docs/08`) | storage v2 + wire v3 golden bytes in LE and BE; scaffolding-stays-LE checks; BE sent to an LE-only peer refused | byte-exact fixtures; no silent order flip |
| T14 | v3 negotiation matrix (`docs/08`) | HELLO→CAPS exchange: level/BE min-selection, v1/v2 fallback, frame-vs-session mismatch, reserved flags (non-vacuous caps, per R15), fixed-width rule | full matrix green; every violation loud |

## Harness

- In-process time travel: `libfaketime`. Whole-system: QEMU `-rtc
  base=2038-01-19T03:14:06Z`. Quarterly full-dress "2038 GameDay" VM
  image (stage 7) until GA.
- CI: armv7 (Debian trixie, glibc ≥ 2.34), i386, x86_64, aarch64 — all
  with `-D_TIME_BITS=64` (A1), ASan/UBSan on fuzz targets; extensions
  legs add the `_HOT_TIME_BITS` rungs 0/1/2 (`docs/08` X11). Local entry
  point: `scripts/ci.sh` (gate + x86_64/i686 legs + clang-tidy);
  cross-arch legs run in the CI provider.
- Gates required to merge once enabled: A3 cast gate
  (`scripts/cast-gate.sh`), A5 matrix, T1, T2, T3.

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
| `docs/08` X-rules (extensions) | T11–T14; T6/T7 extended |

## Acceptance

Rollout stage N may not proceed until every `V-<patch>` test for the
surfaces that stage touches is green (`docs/05-rollout.md`). Stage-4
surfaces additionally require every MUST of `docs/08` (X1–X12 rules
and format layouts) covered by a green test, including byte-exact
vetting of the `docs/08` worked examples.
