# HOT — plan review log

Review of the v0.1 plan: design self-review, a live run of the audit
script on the development host, and a numeric verification of the
compat-layer spec (`tests/spec_vet.c`, all 11 checks passing).

## Findings and dispositions

| # | finding | severity | disposition |
|---|---------|----------|------------|
| R1 | A single in-band tag bit collides with the sign bit of negative true time (pre-1970). | blocker if unaddressed | Untagged words restricted to [0, 2^63); pre-1970 instants use tagged era-0 form; Q8 tracks the residual. |
| R2 | Bare 32-bit values are era-ambiguous post-wrap; any "smart guess" heuristic is silent corruption waiting to happen. | high | Library refuses without era context or bounds (`-HOT_EAMBIGUOUS`); persisted era hint; bounded-conversion API. |
| R3 | `LD_PRELOAD` cannot widen `time_t` inside a legacy 32-bit process; "shim" fixes are false comfort. | high | Preload demoted to audit-only; recompile or translating-proxy containment instead (`docs/03` interop rules). |
| R4 | Tagged words must not lose ordering across the wrap, or sorting/indexing breaks. | high | Era encoded in bits 62..32 so tagged-word numeric order equals temporal order; verified in `tests/spec_vet.c`. |
| R5 | Old v1 peers receiving HOT words would misread bit-63-set words as huge integers. | medium | Loud-failure rule R6 + version negotiation gates all v2 exchanges (patch F1/G4); staged rollout. |
| R6 | `utmp`/`wtmp` are frozen 32-bit formats that would silently store wrapped times. | medium | Patch D6 stops writes; readers route through the compat layer. |
| R7 | Third-party claims need verification: MySQL/MariaDB `TIMESTAMP` 2038 cap, NTP 2036 era, ext4 2446 range, FAT 2107. | low | Kept with explicit verify notes (E2, F2); audit item 7 + host snapshot confirm the filesystem posture on this host. |
| R8 | Signed and unsigned 32-bit sources could be double-counted into separate eras. | medium | Single signed-era encoding (R5 in `docs/03`); `hot_from_u32` re-encodes — verified numerically (u32 0x80000000 era 0 → signed era 1, seconds 2147483648). |
| R9 | Perf of tagging on hot paths. | low | Branchless masks; T8 gate. |
| R10 | Plan correctness depends on the audit script itself. | low | Script run live on the dev host (PASS); fixture-based self-test T10 planned. |
| R11 | Audit script defect (found on first host run): the `_TIME_BITS` probe omitted the required `_FILE_OFFSET_BITS=64` and tested `__TIME_BITS__`, a macro glibc does not define. | high (tool defect) | Fixed against `/usr/include/features-time64.h` on this host (glibc 2.44): correct macros are `__USE_TIME_BITS64` and `__USE_TIME64_REDIRECTS`. Snapshot in `docs/01` is from the fixed script. |
| R12 | Worked example for 2038-01-19 03:14:08 in `docs/03` had the era nibble misplaced (`0x8000010080000000` instead of `0x8000000180000000`). | high (spec defect) | Caught by compiling the spec's examples (`tests/spec_vet.c`); doc corrected. |
| R13 | Era extraction with a bare `w >> 32` drags the tag bit into the era field. | high (implementation trap) | Caught by the same vet run; normative extraction rule added to R2: `era = (w >> 32) & 0x7FFFFFFF`. |
| R14 | Draft R3 claimed untagged and tagged words relate by a shift — false; and duplicate/misspelled API bullets (`-EAMBIGUOUS` vs library codes). | medium | Corrected during review; error codes unified as `HOT_EAMBIGUOUS`/`HOT_ERANGE`. |
| R15 | Wire v2 reserved-flags mask was `0xFC`, leaving bit 1 outside the rejected set: a frame with flags = 0x02 decoded silently with the bit ignored, contradicting `docs/03` ("bits 1..7 reserved, MUST be rejected nonzero"). Found while designating the flags byte for the subsecond extension (bit 1 is the natural ns-fraction flag). The two pre-existing rejection tests passed vacuously — cap 15 against 14-byte frames, so the size check short-circuited before flags were examined. | high (implementation defect) | Fixed: `HOT_WIRE_F_RESERVED` `0xFC` → `0xFE` (`include/hot/wire.h`); regression test (flags = 0x02 alone must fail); vacuous caps repaired (15 → 14) so the reserved-flags and compat-mismatch checks are genuinely reached. `tests/test_wire.c` 57/57. |
| R16 | `docs/08` contradicted itself on the level-2 record width: the records table, the Purpose section ("nanoseconds + attoseconds (16-byte records)", TAI64NA-style), the wire v3 layout (`W in {8, 12, 16}`), and the storage v2 layout (`W in {8, 12, 16}`) all said 16 bytes, but the field description said "as field, 8 bytes" (which makes 8+4+8 = 20). Caught by compiling the level-2 worked frame at stage-4 implementation — the golden array sized itself to 23 bytes while the field list produced 27 (the R12 lesson, again). | high (spec defect) | Resolved in favor of 16-byte records: the as field is 4 bytes (`31..0`), exactly TAI64NA's layout (8 + 4 + 4), cited by name in the Purpose section. One erratum line fixed in `docs/08`; four consistent normative mentions stand. The in-memory `uint64_t as` of the API sketch is unchanged — canonical values (< 10^9 < 2^30) pack into the 32-bit field exactly, and `hot_word_as_pack`/`_unpack` remain the field boundary. |

## Verification evidence

- `scripts/host-audit.sh`: live run on the development host
  (Pentoo 2.18, kernel 7.2.7, glibc 2.44, x86_64) — 6 pass, 3 warn,
  0 fail; see `docs/01-host-audit.md` snapshot.
- `tests/spec_vet.c`: `gcc -O2 -Wall -Wextra` clean, 11/11 checks pass —
  validates every worked example in `docs/03`, the era formula,
  cross-wrap ordering, and unsigned-source encoding. This program is
  the seed of test T2 (`docs/04-validation.md`) and should be replaced
  by the real `libhotcompat` test suite at stage 2.
- Stage 2 (2026-10-21): `libhotcompat` landed (`include/hot/time.h`,
  `src/hot.c`) — `tests/test_hot.c` 63/63 green on x86_64 and, via the
  multilib toolchain, on i686 with `-m32 -D_TIME_BITS=64`
  (`sizeof(time_t)` 8 with the flag, 4 without — A1 is load-bearing on
  32-bit). `spec_vet.c` retained as stage-0 evidence.
- Stage 2 continued (T3/T7, 2026-10-21): `tests/fuzz_hot.c` — 10^6
  iterations under ASan/UBSan (detect_leaks=0 under this sandbox),
  invariants: classification exactness vs independent reference decode,
  era masking, `from_32`/`from_64` round-trips, `from_u32` re-encoding,
  tagged ordering, bounded-window uniqueness/refusal (R4), wrap/saturate/
  error policies vs independent formulas — green on x86_64 and i686
  (2·10^5 iters). `tests/golden_hot.c` + `tests/golden_wire.txt` — 2446
  bytes byte-exact on x86_64 and i686 (`_TIME_BITS=64` keeps the ISO 8601
  rendering stable across width); `make golden-update` regenerates.
- Stage 1 (2026-10-21): A3 gate landed as `scripts/cast-gate.sh`
  (`make gate`) — greppable narrowing-cast idioms on time identifiers +
  time-bitfield rule, `hot-cast-gate: allow` inline escape for the
  sanctioned legacy boundary (`hot_to_32`, `spec_vet` fixtures). The gate
  caught 3 lines on first run (all sanctioned, all resolved). A5 entry
  point `scripts/ci.sh` (`make ci`): gate self-test (planted violations
  rejected), x86_64 leg (`make check`), i686 leg (`-m32 -D_TIME_BITS=64`:
  63 checks + golden byte-exact + ASan/UBSan fuzz 10^5), clang-tidy
  `bugprone-narrowing-conversions` clean (typed coverage for the gate's
  ident-list blind spots), armv7/aarch64 legs SKIPped to the CI provider.
  7 ok / 1 skip / 0 fail on this host.
- Stage 3 (2026-10-21): wire v2 + storage format landed
  (`include/hot/wire.h`, `src/hotwire.c`, `include/hot/store.h`,
  `src/hotstore.c`). The deferred flags-byte layout (see Remaining
  weaknesses) is now normative in `docs/03`: bit 0 = compat bit (must
  equal word bit 63), bits 1..7 reserved-zero, rejected nonzero.
  Tests: `tests/test_wire.c` 56/56 (negotiation matrix incl. refusal
  cases, HELLO round-trip, v2/v1 golden bytes, loud rejection of
  reserved flags / compat mismatch / truncation / trailing bytes /
  wrong-version frames — G4, and post-wrap v1 encode refusal
  `-HOT_ERANGE` — "never an untagged truncation"); `tests/test_store.c`
  32/32 (header round-trip + golden bytes, loud rejection of every
  header deviation, legacy32 era-bounded conversion incl. era-2
  selection and `-HOT_EAMBIGUOUS` refusals). New error codes
  centralized: `HOT_ENOSPC`, `HOT_EMALFORMED` (`include/hot/time.h`).
  i686 leg green (all four suites + sanitized fuzz); clang-tidy clean;
  cast gate clean (12 files).
- Post-stage-3 fix (2026-10-28, R15): v2 reserved-flags mask corrected
  (`HOT_WIRE_F_RESERVED` 0xFC → 0xFE, `include/hot/wire.h`); bit 1 was
  silently ignored on decode. Regression test added (flags = 0x02 alone
  → `-HOT_EMALFORMED`); the pre-existing reserved-flags and
  compat-mismatch rejection tests were repaired to non-vacuous caps
  (15 → 14 against 14-byte frames — the size check had short-circuited
  both). `tests/test_wire.c` 57/57 on x86_64 and on i686 via
  `-m32 -D_TIME_BITS=64`.
- Stage 4 docs (2026-10-28, Q9): `docs/08-extensions.md` landed — the
  normative extension spec: 12/16-byte append-only fraction records
  (ns/as; canonical-only; floored-second semantics valid for negative
  instants), deep-past class (+2^62 bias, ns-field class bits 30–31,
  mutually exclusive with the compat bit), LE-default/BE-opt-in byte
  order (records only; scaffolding stays LE), wire v3 + CAPS
  negotiation with fixed-width sessions, storage v2, `hot_word` strong
  typedef + `_HOT_TIME_BITS` ladder, worked examples with byte dumps.
  `docs/04` amended (T11–T14; T6/T7 extended; `_HOT_TIME_BITS` CI
  rungs); `docs/05` stage table reconciled (Q4's kmod rides the
  rebuild stage 6; extensions inserted as stage 4; retirement now
  stage 9, making Q7's "stage 9" reference literal); stage references
  in `docs/01` and `scripts/host-audit.sh` updated to the reconciled
  numbering (the 2026-10-21 audit snapshot in `docs/01` keeps the old
  wording as a historical record). No code changed in this step —
  implementation order is in `docs/08`; its worked examples must be
  numerically vetted when the code lands (the R12 lesson).

## Remaining weaknesses

- Cold-boot era refusal is a manual runbook; the fleet policy for it is
  TBD (Q5).
- The word tag vs system status bit distinction is easy to conflate in
  dashboards; enforce a naming convention at implementation.
- ~~Wire v2 flags-byte layout is deferred to stage 3 design.~~ Resolved
  at stage 3: normative layout in `docs/03` (compat bit + reserved-zero
  upper bits), validated byte-exact in `tests/test_wire.c`.
- Filesystem/DB/protocol claims in `docs/02` are vendor-documented but
  not yet exercised against fixtures; T9/T10 cover NTP and the audit
  script; schema and format audits are stage 0 field work.
