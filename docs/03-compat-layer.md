# HOT — compat layer (`libhotcompat`)

Normative design. Any implementation must satisfy every MUST here.

## Purpose

After 2038-01-19T03:14:07Z, systems still producing or storing 32-bit
epoch seconds continue in a wrapped regime. HOT keeps such data usable
and unambiguous by defining a single 64-bit **HOT time word** whose top
bit records the representation. The layer also defines a host-level
**compat status bit** that reports whether the host itself is still
operating on 32-bit time past the wrap.

Extensions — subsecond (ns/as) fraction fields, deep-past records,
big-endian opt-in — extend this word in 12/16-byte records without
altering anything in this document: `docs/08-extensions.md`.

## The HOT time word (normative)

A HOT time word is `uint64_t`, host byte order in structs, little-endian
on the wire.

```
63         62              32 31                     0
+----------+------------------+------------------------+
| 1 (ON)   |    era (31 bits) |  low 32 bits of t      |   32-bit compat mode
+----------+------------------+------------------------+

63         62                                          0
+----------+-------------------------------------------+
| 0 (OFF)  |        true 64-bit Unix time, seconds     |   true 64-bit time
+----------+-------------------------------------------+
```

**Bit 63 is the HOT compat bit.** ON means the word holds 32-bit time in
compatibility mode; OFF means it holds true 64-bit time.

### Rules

- R1 (untagged words): bit 63 = 0; bits 62..0 hold true 64-bit Unix
  time in seconds. Restricted to **non-negative** values [0, 2^63):
  instants before 1970 MUST use the tagged form, so the tag bit can
  never collide with a sign bit of a negative value.
- R2 (tagged words): bit 63 = 1; bits 31..0 hold the 32-bit reading
  exactly as legacy code produced it (two's complement for signed
  sources); bits 62..32 hold the **era**, where
  `era = floor((t + 2^31) / 2^32)`. Era 0 covers 1901-12-13..2038-01-19,
  era 1 covers 2038-01-19..2106-02-07, and so on for 68-year windows.
  Extraction MUST mask the tag bit: `era = (w >> 32) & 0x7FFFFFFF` —
  a bare `w >> 32` drags bit 63 into the era field.
- R3 (ordering): within tagged words, numeric order equals temporal
  order, because the era sits above the legacy bits. Sorting and
  comparing tagged words is always safe. The two classes are not
  directly comparable as raw words — cross-class comparisons MUST go
  through `hot_seconds()` only.
- R4 (era ambiguity): a bare 32-bit value is ambiguous after the wrap
  within a 68-year window. Converting one without an era context or
  explicit bounds MUST fail with `-HOT_EAMBIGUOUS`. The library never
  guesses silently.
- R5 (single encoding): the tagged layout always uses the signed-era
  partition of R2. Helpers for unsigned 32-bit sources exist, but the
  on-word encoding never changes.
- R6 (loud failure): any reader that cannot interpret a word MUST
  reject it. A v1 peer receiving a HOT word (bit 63 set) MUST error,
  not guess. Untagged words with values beyond a v1 peer's expected
  range cannot be detected by the peer — which is why version
  negotiation (patch F1) gates every v2 exchange, and why untagged
  values are never sent to un-negotiated peers.

### Worked examples

| instant (UTC) | true 64-bit (untagged) | legacy int32 reading | tagged word |
|---------------|------------------------|----------------------|-------------|
| 2038-01-19 03:14:07 | `0x000000007FFFFFFF` | `0x7FFFFFFF` (era 0) | `0x800000007FFFFFFF` |
| 2038-01-19 03:14:08 | `0x0000000080000000` | `0x80000000` = −2^31 (era 1) | `0x8000000180000000` |
| 1970-01-01 00:00:00 | `0x0000000000000000` | `0x00000000` (era 0) | `0x8000000000000000` |
| 1901-12-13 20:45:52 | (not representable untagged) | `0x80000000` (era 0) | `0x8000000080000000` |

### The system status bit

The host-level compat status bit engages at the wrap:

```
hot_compat_active() == (native time_t width < 64) && (now >= 0x7FFFFFFF)
```

- **ON**: this host is still operating on 32-bit time and is at or past
  the wrap — the compatibility layer is actively carrying the host.
- **OFF**: true 64-bit time — either natively 64-bit, or migrated.

It is exposed via `hot_compat_active()` and, for scripts/monitoring, as
`/run/hot/time-status` (one line: `compat=<0|1> era=<n> width=<32|64>`),
refreshed on boot and daily. This bit describes the *host*; the word tag
describes a *value*. Do not conflate them.

## When tagging applies

The word tag is valid at all times: before the wrap, tagged and untagged
forms coincide numerically, so tagging 32-bit-derived words from day one
is safe, keeps a single code path (no clock-dependent encoder switch),
and preserves provenance in stored data. The wrap is when the tag stops
being redundant and becomes the only thing standing between correct and
corrupted data. The wrap-triggered engagement in the request is realized
by the system status bit above. Pre-wrap 32-bit-derived words therefore
always carry the tag (decided 2026-10-21; see `docs/06` Q2).

## Era context and state

- Primary era source: the system clock (`hot_now_word()`), which the
  host audit guarantees is 64-bit (natively or via time64 syscalls).
- Persisted hint: `/var/lib/hot/era`, written atomically (era + boot id
  + last-seen 64-bit time). On cold boot with a dead RTC it bounds the
  guess. If hint and clock disagree by more than half an era, bounded
  conversions fail with `-HOT_EAMBIGUOUS` and log loudly.
- Bounded conversions: callers may supply bounds — "mtime cannot be in
  the future", "log records are monotonically ordered" — which the
  library uses to select the era; the selected era is recorded with the
  result.

## API sketch

```c
#ifndef HOT_TIME_H
#define HOT_TIME_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t hot_time_t;

#define HOT_BIT_32COMPAT  (UINT64_C(1) << 63)
#define HOT_VALUE_MASK    (UINT64_C(0x7FFFFFFFFFFFFFFF))
#define HOT_ERA_SHIFT     32
#define HOT_ERA_MASK      (UINT64_C(0x7FFFFFFF))

enum hot_wrap_policy { HOT_WRAP, HOT_SATURATE, HOT_ERROR };

hot_time_t hot_now_word(void);
hot_time_t hot_from_64(int64_t seconds);
hot_time_t hot_from_32(int32_t legacy, uint32_t era);
hot_time_t hot_from_u32(uint32_t legacy, uint32_t era);
int64_t    hot_seconds(const hot_time_t word);
bool       hot_is_compat(const hot_time_t word);
uint32_t   hot_era_of(const hot_time_t word);
int        hot_compat_active(void);
int        hot_from_32_bounded(int32_t legacy, int64_t lower, int64_t upper,
                              hot_time_t *out);
int        hot_to_32(int64_t seconds, enum hot_wrap_policy policy,
                    int32_t *out);

#endif
```

Semantics:

- `hot_now_word()`: current wall clock as an untagged word; never
  returns bit 63 set (clamps at 0 for pre-1970 clocks).
- `hot_from_64(seconds)`: untagged word; clamps negatives to 0
  (pre-1970 must use `hot_from_32`).
- `hot_from_32(v, era)` / `hot_from_u32(u, era)`: tagged words. The
  unsigned form computes `t = era * 2^32 + u` and re-encodes per R2/R5,
  so unsigned era 0 (1970–2106) spans signed eras 0 and 1 exactly.
- `hot_seconds(word)`: strips the tag; untagged → value; tagged →
  `era * 2^32 + (int32_t)low`. May be negative for era-0 tagged words.
- `hot_era_of(word)`: the R2 era of the instant, for both classes
  (untagged words derive it: `floor((value + 2^31) / 2^32)`).
- `hot_from_32_bounded(...)`: bounds are true seconds (`lower` may be
  negative); `-HOT_EAMBIGUOUS` when bounds do not select exactly one
  era; otherwise like `hot_from_32`.
- Error codes: `HOT_EAMBIGUOUS`, `HOT_ERANGE` are library-defined
  negative return values (not errno constants).
- `hot_to_32(seconds, policy)`: output to legacy sinks. `HOT_WRAP`
  reproduces legacy truncation; `HOT_SATURATE` clamps to INT32_MIN/MAX;
  `HOT_ERROR` returns `-HOT_ERANGE` when not representable.

Thread-safety: all functions async-signal-safe; era state is a single
atomic `uint64_t`. Wire order: little-endian.

## Interop rules

- Storage: after rollout stage 3, any stored epoch is a HOT word. Old
  untagged legacy values are read via `hot_from_32`/`hot_from_u32` with
  an era context.
- Wire: new protocol revisions carry a flags byte including the compat
  bit adjacent to the time field. v1 peers receiving HOT words fail
  loudly (patch G4). Never send an untagged 32-bit truncation to a peer.
- Old binaries cannot be fixed by `LD_PRELOAD`: interposition cannot
  widen `time_t` inside an existing 32-bit process. A preload shim may
  be used for auditing (logging legacy time syscalls) only, never for
  correction. Fixing requires recompile; containment uses translating
  proxies (patch F4).
- Signed vs unsigned sources: one tagged space (R5). Callers must know
  their source's signedness; the helpers encode that knowledge.

## Serialization

| medium | representation |
|--------|----------------|
| binary | HOT word, little-endian `uint64_t` |
| protobuf | `int64 seconds = 1;` + `Tag hot_tag = 2;` (or packed u64) |
| JSON | `{"t": <int>, "hot32": true/false}` |
| text logs | ISO 8601 + `hot32compat` marker when tagged |

### Wire v2 frame (normative layout)

```
offset  size  field
0       2     magic "H2" (0x48 0x32)
2       2     payload length, uint16 LE
4       1     flags byte (below) -- adjacent to the time field
5       8     HOT time word, uint64 LE
13      len   payload
```

Flags byte: bit 0 = compat bit (MUST equal bit 63 of the time word);
bits 1..7 reserved, MUST be zero on send and MUST be rejected nonzero on
receive. The v1 frame ("H1", len, int32 tstamp) encodes only representable
times — `HOT_ERROR` policy, never an untagged truncation. A decoder fed a
frame of the wrong version MUST error loudly (G4/R6).

### Storage header (normative layout)

```
offset  size  field
0       4     magic "HOTS" (0x48 0x4F 0x54 0x53)
4       1     version = 1
5       1     flags = 0x01 (entries are HOT words)
6       1     time unit = 0 (seconds)
7       1     time width = 8 (bytes per entry)
8       4     entry count, uint32 LE
12      8*n   HOT time words, uint64 LE each
```

Every field is validated on read; any deviation is `-HOT_EMALFORMED`,
never a guess. Legacy 32-bit state files are read through
`hot_from_32_bounded` with caller bounds (R4).

Reference implementation (stage 3): `include/hot/wire.h`,
`include/hot/store.h`, `src/hotwire.c`, `src/hotstore.c`.

## Limitations

- 63-bit range (~292 billion years); era field 31 bits — both ample.
- Cannot fix un-recompilable binaries (see above).
- The status bit is per-host; the word tag is per-value.
- Cold boot with dead RTC and stale hint degrades to `-HOT_EAMBIGUOUS`
  refusal — a manual runbook, by design, not silent corruption.
