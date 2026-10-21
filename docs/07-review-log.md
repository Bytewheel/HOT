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

## Verification evidence

- `scripts/host-audit.sh`: live run on the development host
  (Pentoo 2.18, kernel 7.2.7, glibc 2.44, x86_64) — 6 pass, 3 warn,
  0 fail; see `docs/01-host-audit.md` snapshot.
- `tests/spec_vet.c`: `gcc -O2 -Wall -Wextra` clean, 11/11 checks pass —
  validates every worked example in `docs/03`, the era formula,
  cross-wrap ordering, and unsigned-source encoding. This program is
  the seed of test T2 (`docs/04-validation.md`) and should be replaced
  by the real `libhotcompat` test suite at stage 2.

## Remaining weaknesses

- Cold-boot era refusal is a manual runbook; the fleet policy for it is
  TBD (Q5).
- The word tag vs system status bit distinction is easy to conflate in
  dashboards; enforce a naming convention at implementation.
- Wire v2 flags-byte layout is deferred to stage 3 design.
- Filesystem/DB/protocol claims in `docs/02` are vendor-documented but
  not yet exercised against fixtures; T9/T10 cover NTP and the audit
  script; schema and format audits are stage 0 field work.
