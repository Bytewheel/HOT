# ByteWheel / HOT — Handover Of Time

Y2038 remediation program: make 64-bit Unix time work end-to-end on modern
Linux systems (including this host), plus a compatibility layer that keeps
32-bit-epoch data usable and unambiguous across the 2038 wrap.

Status: **implementation, stages 1–3** (`docs/05-rollout.md`). Stage 1:
A3 cast gate (`scripts/cast-gate.sh`) + A5 matrix entry point
(`scripts/ci.sh`). Stage 2: `libhotcompat` (`include/hot/time.h`,
`src/hot.c`), fuzz (T3) + golden wire fixtures (T7). Stage 3: wire v2 +
storage formats behind version negotiation (`include/hot/wire.h`,
`src/hotwire.c`, `include/hot/store.h`, `src/hotstore.c`; flags-byte
layout normative in `docs/03`). `make check` runs the full gate — all
suites green on x86_64 and i686 (`-m32 -D_TIME_BITS=64`). Stage 0
audit of this host: PASS (`docs/01`). Remaining stages per rollout
order.

## The HOT compat bit

Every value that crosses a HOT boundary is a 64-bit **HOT time word**
(`uint64_t`). Its top bit is the **HOT compat bit**:

| bit 63 | meaning |
|--------|---------|
| 1 (ON)  | the word holds **32-bit time in compatibility mode** (era-encoded legacy epoch) |
| 0 (OFF) | the word holds **true 64-bit Unix time** |

The compatibility layer engages at the 32-bit epoch wrap
(2038-01-19T03:14:07Z): before the wrap the 32-bit and 64-bit readings
coincide; at and after the wrap any 32-bit-derived value is tagged ON and
can never be confused with true 64-bit time. Normative spec:
`docs/03-compat-layer.md`.

## Repository layout

| path | contents |
|------|----------|
| `docs/00-scope.md` | background, goals, success criteria, key dates |
| `docs/01-host-audit.md` | per-host audit checklist + snapshot of this host |
| `docs/02-patch-plan.md` | the patch inventory (build, kernel, libc, app, data, protocol, compat) |
| `docs/03-compat-layer.md` | normative design of the compat layer and the bit |
| `docs/04-validation.md` | test matrix, time-travel harness, CI gates |
| `docs/05-rollout.md` | rollout order, monitoring, risk register |
| `docs/06-open-questions.md` | decisions needed; assumptions used meanwhile |
| `docs/07-review-log.md` | findings from plan review (vetting) |
| `include/hot/time.h` | `libhotcompat` public API (normative, `docs/03`) |
| `include/hot/wire.h` | wire v1/v2 + HELLO codec API (F1, G2, G4) |
| `include/hot/store.h` | storage header + legacy32 reader API (D5, E3) |
| `src/hot.c` | `libhotcompat` implementation |
| `src/hotwire.c` | wire codec implementation (stage 3) |
| `src/hotstore.c` | storage codec implementation (stage 3) |
| `Makefile` | build + `make check`/`fuzz`/`golden`/`gate`/`ci`; A1 flags (`_TIME_BITS=64`) on by default |
| `scripts/host-audit.sh` | runnable host capability probe (see `docs/01`) |
| `scripts/cast-gate.sh` | A3 gate: refuse narrowing casts of time values (allow-marker escape) |
| `scripts/ci.sh` | A5 matrix entry point: gate + x86_64/i686 legs + clang-tidy |
| `tests/test_hot.c` | T2 seed against the library (round-trip, era, ordering, policies) |
| `tests/test_wire.c` | stage-3 wire tests: negotiation, golden bytes, loud refusal (G4) |
| `tests/test_store.c` | stage-3 storage tests: header validation, legacy32 era conversion |
| `tests/fuzz_hot.c` | T3 decode-fuzz harness (ASan/UBSan, deterministic PRNG) |
| `tests/golden_hot.c` | T7 golden-fixture generator/checker (`--update` regenerates) |
| `tests/golden_wire.txt` | T7 committed byte-exact wire fixtures |
| `tests/spec_vet.c` | numeric verification of the `docs/03` spec (kept as stage-0 evidence) |

## Quick start

    make gate
    make check
    scripts/host-audit.sh

`make gate` = A3 cast gate. `make check` runs the `libhotcompat` suite
(63 checks) + spec evidence + golden fixtures + fuzz under ASan/UBSan;
`scripts/ci.sh` adds the i686 leg and clang-tidy. `scripts/host-audit.sh`
exit 0 = no blocking findings for 64-bit time readiness.

## The one forbidden failure mode

Silent misinterpretation of time. Anything that cannot represent or
understand a time value must fail loudly, never guess.
