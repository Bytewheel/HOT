# HOT — patch plan

Patch inventory for making 64-bit time work on modern Linux systems.
IDs are stable and referenced by `docs/04-validation.md` (as `V-<ID>`)
and `docs/05-rollout.md`.

Applicability: **[64]** = applies to 64-bit hosts (enforcement/interop),
**[32]** = applies to 32-bit targets, **[all]** = both. On a modern
64-bit host, most patches are enforcement and artifact work — the type
system is already 64-bit there (see `docs/00-scope.md`).

## A. Build system and toolchain

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| A1 | Define `_TIME_BITS=64` (with `_FILE_OFFSET_BITS=64`) in every build | all | No-op on 64-bit hosts; full 64-bit `time_t` on 32-bit glibc/musl. Keeps source portable and flushes hidden assumptions early. | V-A1 |
| A2 | Raise the 32-bit minimum kernel to 5.1; boot-time capability check | [32] | `clock_gettime64` and friends first appear in 5.1; older kernels cannot serve 64-bit time to 32-bit userspace (this also fixes 32-bit `CLOCK_MONOTONIC` wrap in 2106). | V-A2 |
| A3 | Cast gate: ban narrowing of `time_t` | all | CI grep for casts of time values to `int`/`uint32_t`/bitfields + clang-tidy `bugprone-narrowing-conversions` on time APIs. | V-A3 |
| A4 | Rebuild all static binaries with A1 | [32] | Static 32-bit binaries cannot be fixed in place; there is no runtime remediation. | V-A4 |
| A5 | CI matrix: 32-bit (armv7, i386) + 64-bit (x86_64, aarch64), all with A1, ASan/UBSan on fuzz targets | all | Time bugs are width-dependent; the matrix is where they reliably surface. | V-A5 |
| A6 | Container base images: glibc ≥ 2.34 or musl ≥ 1.2.0 | all | Base-image age is the usual reason 32-bit containers stay broken. | V-A6 |

## B. Kernel-side (active — Q4 decided yes, 2026-10-22: ByteWheel ships kernel modules/patches)

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| B1 | Replace `get_seconds()` / `do_gettimeofday()` with `ktime_get_real_seconds()` / `ktime_get_*()` | all kernels we ship | Legacy 32-bit in-kernel time interfaces are deprecated/removed on recent kernels. | V-B1 |
| B2 | Use `struct timespec64` (or `time64_t`) in module ABIs | all | Never raw `time_t` in ioctls or module interfaces. | V-B2 |
| B3 | Version time-bearing ioctls: new numbers with 64-bit fields; legacy numbers return `-EOVERFLOW` past 2038 | [32] | An old ioctl number cannot change meaning; it must fail loudly instead of lying. | V-B3 |

## C. libc and runtimes

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| C1 | Rebuild all 32-bit shared artifacts against glibc ≥ 2.34 / musl ≥ 1.2.0 with `_TIME_BITS=64` | [32] | Works via libc symbol redirection (`clock_gettime` → `__clock_gettime64` etc.). Old binaries keep old semantics — documented and contained, not silently mixed. | V-C1 |
| C2 | Audit embedded scripting runtimes | [32] | Mainline runtimes (JVM, Go, Rust, Python ≥ 3, Node) are 64-bit internally; verify anything shipped small (e.g. embedded Lua) is built with a 64-bit `time_t`. | V-C2 |
| C3 | Overflow-checked wall-clock decomposition (`gmtime_r`/`localtime_r`/`timegm`) wherever years are formatted | all | These return NULL/`EOVERFLOW` outside the representable range; unhandled NULL is the classic crash mode at the wrap. | V-C3 |

## D. Application code patterns

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| D1 | Remove/replace `time_t`→32-bit casts | all | Use `int64_t` and the compat layer's explicit conversion helpers (`docs/03`). | V-D1 (same gate as A3) |
| D2 | `PRId64`/`PRIu64` for printing time values | all | Format-specifier mismatches are UB and hide truncation. | V-D2 |
| D3 | No `time_t` in packed structs, wire structs, or on-disk structs | all | Serialization uses explicit `int64_t`, or HOT words per `docs/03`. | V-D3 |
| D4 | Overflow-safe delta comparisons; monotonic clocks for intervals | all | `now - then > timeout` written so overflow cannot invert it; `CLOCK_MONOTONIC` for intervals, wall clock only for timestamps. | V-D4 |
| D5 | Version + width + unit metadata in every file/state format that stores epochs | all | A bare number epoch cannot be fixed later in place; stamps are cheap now. | V-D5 |
| D6 | Stop writing legacy `utmp`/`wtmp`/`btmp` records (or route them explicitly through the compat layer) | all | `struct utmp` is a frozen 32-bit-time format; post-2038 writes would store wrapped values unreadable as-is. | V-D6 |

## E. Storage and data

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| E1 | Filesystem matrix: no 2038-limited filesystem on any host hot path | all | See table in `docs/01`. ext2/ext3 (2038), vfat (2107), squashfs (2106) are limited; ext4/XFS/Btrfs/F2FS are wide. NFS/CIFS: audit servers. | V-E1 |
| E2 | Schema audit: `TIMESTAMP`-capped columns and INT-epoch columns → 64-bit | all | MySQL/MariaDB `TIMESTAMP` is documented as capped at 2038 (verify per server version); epoch-in-INT columns → BIGINT; SQLite/Postgres are 64-bit internally but application casts may not be. | V-E2 |
| E3 | State files storing epoch seconds (DHCP leases, caches, queues) get width+version headers or HOT words | all | Text epoch fields wrap exactly as hard as binary ones. | V-E3 |
| E4 | Archive/backup format audit | all | Verify every producer and consumer; tar pax headers are fine, custom formats usually are not. | V-E4 |

## F. Network and protocols

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| F1 | Internal RPC: 32-bit time fields → 64-bit, behind protocol version negotiation | all | Old peers must be rejected loudly, never guessed at (G4). | V-F1 |
| F2 | NTP posture: NTS-capable clients; era-rollover (2036) exercised in tests | all | chrony/systemd-timesyncd handle era rollover only if the system clock is itself right — which the audit (item 4) guarantees. | V-F2 |
| F3 | Kerberos audit | all | MIT krb5 uses a documented post-2038 encoding for 2038–2106; verify all realms/KDCs understand it. | V-F3 |
| F4 | Third-party/appliance protocols with hard 32-bit time | all | Cannot be patched — isolate behind translating proxies that emit tagged words (`docs/03`). | V-F4 |
| F5 | Mixed-fleet skew monitoring | all | Nodes disagreeing about the current epoch is the first symptom of a half-fixed fleet. | V-F5 |

## G. Compat layer integration

| ID | patch | applies | notes | validation |
|----|-------|---------|-------|------------|
| G1 | Implement `libhotcompat` to the normative spec in `docs/03` | all | Single implementation, no forks. | V-G1 |
| G2 | Wire/storage formats carry the HOT word and the compat bit | all | The bit is normative (`docs/03`). | V-G2 |
| G3 | Monitoring on compat-mode activation and `HOT_EAMBIGUOUS` returns | all | After GA, any compat activation is an incident signal, not a normal state. | V-G3 |
| G4 | Old-peer refusal: a v1 peer receiving a HOT word must fail loudly | all | Silent misinterpretation is the one forbidden failure mode. | V-G4 |

## What this plan does NOT fix

- In-place widening of a 32-bit binary's internal `time_t`: impossible
  without recompile (A4/C1). Un-recompilable binaries are contained (F4,
  `docs/05-rollout.md`).
- The 2106 unsigned wrap in formats we do not own.
- Anything inside devices we cannot reflash.
