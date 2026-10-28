# HOT — extensions: subsecond, deep past, byte order

Normative addendum to `docs/03-compat-layer.md`, decided per Q9
(2026-10-28, `docs/06`). Any implementation must satisfy every MUST
here, and every MUST of `docs/03` remains in force: the first 8 bytes
of every record below are a `docs/03` HOT word, except in the
deep-past class, which reinterprets the value field per X6. Docs land
before any implementation (rollout stage 4, `docs/05`).

## Purpose

`docs/03` carries seconds only, cannot represent pre-1970 instants in
untagged form (R1), and is little-endian only. Three extensions close
those gaps without touching the 8-byte word or any deployed v1/v2
format:

1. **Subsecond precision** — append-only fraction fields,
   TAI64N/TAI64NA-style: nanoseconds (12-byte records) and
   nanoseconds + attoseconds (16-byte records).
2. **Deep past** — a TAI64-style +2^62 bias in the 63-bit value
   field, selected by class bits in the ns field; representable only
   in 12/16-byte records, mutually exclusive with the compat bit.
3. **Byte order** — little-endian default, big-endian opt-in via a
   header flags bit (storage v2, wire v3).

## Records and levels

| level | record | fields | width |
|-------|--------|--------|-------|
| 0 | HOT word | `w` | 8 |
| 1 | word + ns | `w`, `ns` | 12 |
| 2 | word + ns + as | `w`, `ns`, `as` | 16 |

```
ns field, 4 bytes:
31..30  class: 00 = plain, 01 = deep-past, 10/11 = reserved
29..0   nanoseconds within the second, < 10^9

as field, 8 bytes:
63..0   attoseconds within the nanosecond, < 10^9
```

### Rules

- X1 (append-only levels): a record's level is its width. For levels
  M < N, the first width(M) bytes of a canonical level-N record are
  exactly the canonical level-M record of the same instant floored to
  level M's resolution — fraction fields carry no length prefix and
  never alter earlier bytes.
- X2 (fraction reference): a record denotes the instant
  `hot_seconds(w) + ns*10^-9 + as*10^-18`, and canonical form
  requires `hot_seconds(w) = floor(instant)`. The fraction is measured
  from the **floored** second, which makes it well-defined for
  negative instants (era-0 tagged words and deep-past words).
  Truncation toward zero (C integer division on negative seconds)
  selects the wrong second and MUST NOT be used to build the fraction.
- X3 (canonical-only): `ns` (class bits removed) MUST be < 10^9 and
  `as` MUST be < 10^9. Since 10^9 < 2^30, out-of-range fractions
  still fit their fields; decoders MUST check the bound, not the field
  width, and reject with `-HOT_EMALFORMED`.
- X4 (class bits): ns-field bits 31..30 select the record class:
  00 = plain, 01 = deep-past. Classes 10 and 11 are reserved — zero
  on send, rejected on receive.
- X5 (plain records): the word is any `docs/03` word — tagged or
  untagged. Fractions attach to both word classes; R1–R6 apply to the
  word unchanged.
- X6 (deep-past encoding): in class-01 records the word's bit 63 MUST
  be 0 — the deep-past class is mutually exclusive with the compat
  bit — and bits 62..0 hold `seconds + 2^62` (the TAI64-style bias),
  for seconds in [-2^62, 2^62) (about ±146.2 billion years). A
  class-01 record with word bit 63 set MUST be rejected.
- X7 (deep-past scope): deep past is representable only in 12/16-byte
  records — the selector lives in the ns field. 8-byte words keep R1:
  untagged, non-negative. Pre-1970 instants in 8-byte form remain
  tagged era-0 words (`docs/03`); deep past serves 64-bit-native
  pre-1970 data (Q8).
- X8 (ordering): within one class, record order — compare `w`, then
  `ns`, then `as` — equals temporal order. Raw comparison across
  classes (plain-untagged vs tagged vs deep-past) is invalid and MUST
  go through conversion to seconds (extends `docs/03` R3). Worked
  hazard: deep-past 1901-12-13 (`w = 0x3FFFFFFF80000000`, ~4.6e18)
  compares greater than plain 2038 (`w = 0x7FFFFFFF`, ~2.1e9)
  although 1901 < 2038.
- X9 (byte order): records are little-endian by default. A big-endian
  flag (wire v3 flags bit 3, storage v2 flags bit 3) selects
  big-endian **time records only**; framing scaffolding (wire payload
  length, storage entry count) is little-endian in every mode.
- X10 (loud failure): decoders reject wrong-version, non-canonical,
  contradictory, over-level, and reserved-bit inputs with
  `-HOT_EMALFORMED`, never a guess (extends R6/G4). Unrepresentable
  conversions fail `-HOT_ERANGE`.
- X11 (strong types and ladder): new APIs carry struct-wrapped
  `hot_word` / `hot_word_ns` / `hot_word_as` values; the existing
  `hot_time_t` surface of `docs/03` is untouched. The `_HOT_TIME_BITS`
  ladder selects the API surface a consumer compiles against,
  mirroring `_TIME_BITS=64`: undefined/0 = seconds only; 1 = +
  nanosecond records; 2 = + attosecond records. The ladder is
  cumulative; the library ships built at rung 2.
- X12 (explicit truncation): reading a level-N record at level M < N
  via the typed API is exact — the appended fields are simply not read
  (X1). Decode paths MUST NOT drop or synthesize fraction fields
  silently: a width change is a format/negotiation event (new storage
  header, new wire session), never decoder behavior. In particular, a
  wire-v3 encode at session level 0 requires the record's fraction
  fields to be zero and rejects the record otherwise.

## Worked examples

| instant (UTC) | class | word (hex) | ns raw field | 12-byte LE record |
|---------------|-------|------------|--------------|-------------------|
| 2038-01-19 03:14:07.5 | plain untagged | `000000007FFFFFFF` | `1DCD6500` | `FF FF FF 7F 00 00 00 00 00 65 CD 1D` |
| 2038-01-19 03:14:08.25 | tagged, era 1 | `8000000180000000` | `0EE6B280` | `00 00 00 80 01 00 00 80 80 B2 E6 0E` |
| 1901-12-13 20:45:52.75 | deep-past | `3FFFFFFF80000000` | `6CB41780` | `00 00 00 80 FF FF FF 3F 80 17 B4 6C` |
| 1969-12-31 23:59:59.5 | deep-past | `3FFFFFFFFFFFFFFF` | `5DCD6500` | `FF FF FF FF FF FF FF 3F 00 65 CD 5D` |

The ns raw field is class bits (31..30) OR fraction (29..0):
500,000,000 = `0x1DCD6500`, 250,000,000 = `0x0EE6B280`,
750,000,000 = `0x2CB41780` (deep adds `0x40000000`).

- 1969-12-31 23:59:59.5 is instant −0.5 s: floor = −1 s, fraction
  0.5 s (X2). Tagged alternative: word `80000000FFFFFFFF` (era 0,
  low32 `FFFFFFFF` = −1) + ns `1DCD6500`.
- 1901-12-13 20:45:52.75 is also representable as the tagged era-0
  word `8000000080000000` + ns `2CB41780` (the `docs/03` worked
  example plus fraction). The classes carry provenance — legacy-32
  vs 64-bit-native — the instant does not change; compare via
  conversion only (X8).

Wire v3 frame for the first row, empty payload, LE session (flags
`0x02` = ns):

    48 33 00 00 02 FF FF FF 7F 00 00 00 00 00 65 CD 1D

Same frame in a BE session (flags `0x0A` = ns + BE):

    48 33 00 00 0A 00 00 00 00 7F FF FF FF 1D CD 65 00

Storage v2 header for two BE copies of the deep-past row (flags
`0x0B` = words + ns + BE, width 12, count 2 LE):

    48 4F 54 53 02 0B 00 0C 02 00 00 00
    3F FF FF FF 80 00 00 00 6C B4 17 80   (x2)

Rejections (each `-HOT_EMALFORMED`): ns raw `3B9ACA00` (fraction
exactly 10^9 — fits 30 bits; X3), as = 10^9, ns raw ≥ `80000000`
(class 10/11; X4), deep class with word bit 63 set (X6).

## Wire v3 frame (normative layout)

```
offset  size  field
0       2     magic "H3" (0x48 0x33)
2       2     payload length, uint16 LE (always LE — X9)
4       1     flags byte (below)
5       W     time record, W in {8, 12, 16} = session level width,
              LE or BE per session byte order
5+W     len   payload
```

Flags byte: bit 0 = compat bit, MUST equal bit 63 of the word
(carried from v2); bit 1 = ns field present (W = 12, or 16 with
bit 2); bit 2 = as field present (W = 16; MUST NOT be set without
bit 1); bit 3 = time record is big-endian; bits 4..7 reserved — zero
on send, rejected nonzero on receive, mask `0xF0`. The v2 reserved
mask stays `0xFE` (R15): a decoder applies the mask of the version it
actually decoded, and the magic gates the version.

A v3 session at level 0 in LE mode is byte-compatible with v2 except
for the magic and the reserved mask. A v2-only decoder fed an H3 frame
MUST reject it (magic mismatch) — G4/R6 unchanged.

### CAPS exchange

```
offset  size  field
0       2     magic "HC" (0x48 0x43)
2       1     fraction level: 0, 1, or 2 (this peer's maximum)
3       1     big-endian acceptance: 0 or 1
```

HELLO is unchanged (4 bytes, `H I vmin vmax`); advertising vmax = 3
means v3-capable. If the negotiated version is 3, both sides
immediately exchange one CAPS message (the two may cross); no data
frame before both CAPS messages are seen. Session level = min of the
two levels; session byte order = BE iff both peers set be = 1, else
LE. Every data frame's flags MUST match the session exactly (bits 1–2
= session level, bit 3 = session byte order) — the record width is
fixed for the whole session; changing width requires a new
negotiation. CAPS with any other field value, a CAPS message in a
sub-v3 session, or a data frame arriving before CAPS completes MUST
be rejected with `-HOT_EMALFORMED`.

## Storage v2 header (normative layout)

```
offset  size  field
0       4     magic "HOTS" (0x48 0x4F 0x54 0x53)
4       1     version = 2
5       1     flags (below)
6       1     time unit = 0 (seconds)
7       1     time width W in {8, 12, 16}
8       4     entry count, uint32 LE (always LE — X9)
12      W*n   time records, LE or BE per flags
```

Flags byte: bit 0 = entries are HOT time records (carried from v1;
MUST be 1); bit 1 = ns field present; bit 2 = as field present (MUST
NOT be set without bit 1); bit 3 = time records are big-endian; bits
4..7 reserved — zero on send, rejected nonzero, mask `0xF0`.

The flags/width pair MUST be consistent — W = 8 with neither fraction
bit, W = 12 with bit 1 only, W = 16 with bits 1 and 2 — any other
combination is `-HOT_EMALFORMED`. All entries share the declared width
(fixed-width entries), and every entry is validated per X3/X4/X6: one
bad entry rejects the decode.

Version-1 files remain readable through the existing `docs/03` path —
readers are never removed (`docs/05` rollback). Version-1 readers
reject version 2 loudly.

## API sketch

```c
#ifndef HOT_EXT_H
#define HOT_EXT_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "hot/time.h"

typedef struct { uint64_t w; } hot_word;          /* strong typedef */

#define HOT_NS_MAX         UINT32_C(1000000000)  /* exclusive bound */
#define HOT_AS_MAX         UINT64_C(1000000000)  /* exclusive bound */
#define HOT_NS_CLASS_SHIFT 30
#define HOT_NS_CLASS_MASK  UINT32_C(0xC0000000)
#define HOT_NS_CLASS_PLAIN UINT32_C(0x00000000)
#define HOT_NS_CLASS_DEEP  UINT32_C(0x40000000)
#define HOT_DEEP_BIAS      (UINT64_C(1) << 62)

hot_word   hot_word_adopt(hot_time_t word);
hot_time_t hot_word_raw(const hot_word w);
int64_t    hot_word_seconds(const hot_word w);   /* docs/03 semantics */

#if _HOT_TIME_BITS >= 1
typedef struct { uint64_t w; uint32_t ns; } hot_word_ns;

int      hot_word_ns_adopt(hot_time_t word, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_plain(int64_t seconds, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_deep(int64_t seconds, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_now(hot_word_ns *out);      /* CLOCK_REALTIME */
int      hot_word_ns_unpack(uint64_t w, uint32_t ns_raw, hot_word_ns *out);
void     hot_word_ns_pack(const hot_word_ns *r, uint64_t *w, uint32_t *ns_raw);
int64_t  hot_word_ns_seconds(const hot_word_ns *r); /* floored second */
uint32_t hot_word_ns_class(const hot_word_ns *r);
bool     hot_word_ns_is_deep(const hot_word_ns *r);
bool     hot_word_ns_is_compat(const hot_word_ns *r);
int      hot_word_ns_cmp(const hot_word_ns *a, const hot_word_ns *b);
int      hot_word_ns_from_timespec(const struct timespec *ts, hot_word_ns *out);
int      hot_word_ns_timespec(const hot_word_ns *r, struct timespec *ts);
#endif

#if _HOT_TIME_BITS >= 2
typedef struct { uint64_t w; uint32_t ns; uint64_t as; } hot_word_as;
/* same surface with the as field: _adopt/_plain/_deep/_now/_unpack/
 * _pack/_seconds/_cmp take and return hot_word_as; no timespec forms
 * (attoseconds do not fit struct timespec). */
#endif

#endif
```

Wire and storage additions ride their existing headers:

```c
/* include/hot/wire.h */
enum { HOT_WIRE_V3 = 3 };
#define HOT_WIRE_F_NS          UINT8_C(0x02)
#define HOT_WIRE_F_AS          UINT8_C(0x04)
#define HOT_WIRE_F_BE          UINT8_C(0x08)
#define HOT_WIRE_F_RESERVED_V3 UINT8_C(0xF0)     /* v2 keeps 0xFE */
#define HOT_WIRE_CAPS_SZ       ((size_t)4)

int hot_wire_caps_encode(uint8_t *buf, size_t cap, uint8_t level,
                         uint8_t be, size_t *out);
int hot_wire_caps_decode(const uint8_t *buf, size_t cap,
                         uint8_t *level, uint8_t *be);
int hot_wire_v3_encode(uint8_t *buf, size_t cap, const hot_word_ns *rec,
                       uint8_t level, uint8_t be, const uint8_t *payload,
                       uint16_t len, size_t *out);
int hot_wire_v3_decode(const uint8_t *buf, size_t cap, uint8_t level,
                       uint8_t be, hot_word_ns *rec,
                       const uint8_t **payload, uint16_t *len);
/* level-2 variants are named *_as and take hot_word_as; they are
 * gated by the same _HOT_TIME_BITS ladder. */

/* include/hot/store.h */
#define HOT_STORE_VER_V2        UINT8_C(2)
#define HOT_STORE_F_NS          UINT8_C(0x02)
#define HOT_STORE_F_AS          UINT8_C(0x04)
#define HOT_STORE_F_BE          UINT8_C(0x08)
#define HOT_STORE_F_RESERVED_V2 UINT8_C(0xF0)
```

Names may be refined at implementation; the semantics above are
normative.

Semantics:

- `hot_word_ns_adopt(word, ns)`: any `docs/03` word (tagged or
  untagged) plus a fraction; validates `ns` canonicality only.
- `hot_word_ns_plain(seconds, ns)`: plain untagged record; requires
  `0 <= seconds < 2^63` (R1), else `-HOT_ERANGE`.
- `hot_word_ns_deep(seconds, ns)`: deep-past record; requires
  `-2^62 <= seconds < 2^62`, else `-HOT_ERANGE`.
- `hot_word_ns_now()`: CLOCK_REALTIME as a plain untagged record;
  clamps pre-1970 clocks at second 0 like `hot_now_word()`;
  `tv_nsec` is canonical by construction.
- `hot_word_ns_seconds()`: the floored second of the record's instant,
  either class (untag or unbias).
- `hot_word_ns_cmp()`: total order over records of one level, all
  classes, via (seconds, fraction) conversion — cross-class safe; a
  level-2 record with `as = 0` compares equal to the level-1 record
  of the same instant.
- `hot_word_ns_unpack()`: raw record fields to typed value; validates
  X3, X4, X6 and returns `-HOT_EMALFORMED`. `hot_word_ns_pack()` is
  total.
- `hot_word_ns_from_timespec()`: picks the class — `tv_sec >= 0`
  plain, `tv_sec < 0` deep-past — with floor semantics per X2.
  `hot_word_ns_timespec()` is exact for both classes.
- Wire v3 encode/decode take the session level and byte order and
  enforce the frame/session match (X11, X12). Storage v2 entry arrays
  are typed per level; width comes from the header.
- Thread-safety: as `docs/03` (async-signal-safe; `clock_gettime` is
  async-signal-safe).

## Error-code mapping

| violation | return |
|-----------|--------|
| non-canonical fraction (X3), reserved class (X4), class/compat conflict (X6), flags/width/CAPS contradiction, reserved flag bits, wrong version | `-HOT_EMALFORMED` |
| unrepresentable conversion: negative seconds into a plain untagged record; `\|seconds\| >= 2^62` deep; v1/v2 encode past int32 (`HOT_ERROR` policy) | `-HOT_ERANGE` |
| output buffer too small | `-HOT_ENOSPC` |
| era ambiguity in bounded conversions (unchanged, `docs/03`) | `-HOT_EAMBIGUOUS` |

No new error codes.

## Interop and migration

- v1/v2 peers: unchanged. A v3-capable host negotiates down via
  HELLO and speaks v2/v1 exactly as before; H3 frames never reach a
  sub-v3 decoder except as a loud magic-mismatch rejection.
- Dual-stack window: writers choose v2 or v3 per peer; readers accept
  both forever — the reader is never removed, only writers move on
  (`docs/05` rollback philosophy).
- Storage: v1 and v2 files coexist; v2-aware readers take both.
- Proxies (F4): emit at the negotiated level; deep-past records are
  the correct carrier for 64-bit-native pre-1970 sources.
- Serialization beyond the binary forms (guidance, not load-bearing):
  protobuf `fixed64 w = 1; fixed32 ns = 2; fixed64 as = 3;`; JSON
  `{"t": <sec>, "ns": <n>, "as": <n>, "hot32": <bool>, "deep": <bool>}`;
  text logs ISO 8601 with up to 9 (ns) or 18 (as) fractional digits
  plus the `docs/03` markers.

## Implementation order (stage 4)

1. `include/hot/ext.h` + `src/hotext.c` — types, constants,
   constructors, pack/unpack, comparators; built at rung 2.
2. Wire v3 + CAPS (`include/hot/wire.h`, `src/hotwire.c`); storage
   v2 (`include/hot/store.h`, `src/hotstore.c`).
3. Tests T11–T14 (`docs/04`): canonicality, deep-past, byte order,
   negotiation matrix; byte-exact vetting of every worked example
   above (the R12 lesson: examples are compiled, not eyeballed); fuzz
   extension over 12/16-byte records; golden fixtures for wire v3 +
   storage v2 (T7 extension).
4. CI: `_HOT_TIME_BITS` rungs 0/1/2 on x86_64 and i686; clang-tidy;
   cast gate over the new sources.
5. README / repository layout update at landing.

## Limitations

- Resolution floor 10^-18 s; no leap-second representation
  (`docs/03` non-goal unchanged).
- Deep-past range ±2^62 s (about ±146.2 billion years).
- 8-byte words are unchanged in every respect; pre-1970 stays out of
  untagged 8-byte form (R1).
- Big-endian is per format instance, never per record; no
  mixed-endian files.
- The class field consumes 2 of the 32 fraction bits; 10^9 < 2^30, so
  no nanosecond precision is lost.
- CAPS adds one round-trip to v3 session setup.
