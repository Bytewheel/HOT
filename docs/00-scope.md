# HOT — scope and goals

## Background

Unix time counts seconds since 1970-01-01T00:00:00Z. A signed 32-bit
counter overflows at **2038-01-19T03:14:07Z** (0x7FFFFFFF); an unsigned
32-bit counter wraps at **2106-02-07T06:28:16Z**. NTP's wire era wraps
earlier still: **2036-02-07T06:28:16Z**.

On modern 64-bit Linux hosts (x86_64, aarch64, ...), `time_t` is already
64 bits and the kernel has always passed 64-bit times. The residual Y2038
exposure on such hosts is therefore not the type system — it is:

1. **32-bit artifacts** still built, shipped, or executed (multilib
   binaries, 32-bit containers and appliances, legacy firmware in the
   fleet),
2. **data and storage** with hard-coded 32-bit epochs (filesystems,
   schemas, state files, archives),
3. **wire protocols** with 32-bit time fields,
4. **code that truncates**: casts of `time_t` to 32-bit widths, packed
   structs, naive serialization, format-specifier misuse.

HOT patches all four classes, and adds a compatibility layer so that
32-bit-epoch data produced or consumed across the wrap is never silently
misinterpreted.

## Goals

- G1: all ByteWheel code builds and runs correctly with 64-bit time on
  modern Linux, and with `_TIME_BITS=64` on 32-bit Linux.
- G2: 32-bit-epoch data (historic, current, and post-wrap) is handled
  through one normative compat layer with a machine-checkable provenance
  bit.
- G3: correctness is enforced by a repeatable test matrix (time travel,
  fuzzing, property tests, mixed-fleet interop).
- G4: rollout is ordered so no step can lose or mis-order data.

## Non-goals

- Fixing third-party firmware or appliances we cannot rebuild (we isolate
  and contain them instead).
- Non-Linux platforms (Linux-first plan).
- Changing the Unix epoch or redefining `time_t` semantics.
- Leap-second policy (only that behavior is documented per deployment).
- Monotonic/boottime clock domains (no epoch wrap; covered indirectly via
  patch class A2).

## Success criteria

- `scripts/host-audit.sh` exits 0 on every production host.
- CI gates: zero `time_t`-narrowing casts (A3); 32-bit builds with
  `_TIME_BITS=64` compile clean and pass tests (A5).
- All HOT-tagged wire/storage formats carry explicit width, unit, and tag;
  all validation matrix tests green (`docs/04-validation.md`).
- After rollout stage 3 (`docs/05-rollout.md`), no untagged 32-bit epoch
  crosses any process or host boundary.

## Key dates

| date (UTC) | event |
|------------|-------|
| 2036-02-07 06:28:16 | NTP era 0 wraps (protocol-level) |
| 2038-01-19 03:14:07 | signed 32-bit epoch wraps (Y2038) |
| 2038-01-19 + 68 years | era-ambiguity window of any bare 32-bit timestamp |
| 2106-02-07 06:28:16 | unsigned 32-bit epoch wraps |

Milestone: all ByteWheel surfaces true-64-bit before **2037-01-01**,
with the compat layer in production before the wrap.
