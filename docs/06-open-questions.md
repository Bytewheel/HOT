# HOT — open questions

Q1–Q3 were decided on 2026-10-21, Q4 on 2026-10-22, and Q9 on
2026-10-28 (decisions recorded inline below). Q5–Q8 remain open; each
lists the assumption currently baked into the plan so work can proceed
while decisions are pending.

| # | question | current assumption |
|---|----------|--------------------|
| Q1 | ~~Is "the bit" per-value (tag inside each time word), a single global mode flag, or both?~~ **DECIDED (2026-10-21): both**, as drafted — per-word tag (bit 63) normative and load-bearing; system status bit advisory. | — |
| Q2 | ~~Should 32-bit-derived words carry the tag even before the wrap?~~ **DECIDED (2026-10-21): always tag** — one code path, no clock-dependent encoder switch, provenance from day one; the literal wrap-triggered engagement is carried by the system status bit. | — |
| Q3 | ~~Existing inventory of 32-bit artifacts to seed stage 0?~~ **DECIDED (2026-10-21): none exists** — stage 0 builds the inventory via `scripts/host-audit.sh` plus manual items 9–11 of `docs/01`. | — |
| Q4 | ~~Do we ship kernel modules/patches? (activates patch class B)~~ **DECIDED (2026-10-22): yes** — patch class B ships; `kmod/hot_time` (B1/B2/B3) + V-B3 32-bit rig landed as rollout stage 5. | — |
| Q5 | Where does the era hint persist if `/var/lib` is ephemeral (containers)? | Host-level bind mount, or derive-only with reduced confidence; refusal policy unchanged. |
| Q6 | Any unsigned-32-bit (2106) data sources in the fleet? | Assume yes; `hot_from_u32` provided. |
| Q7 | Authority to force peers to protocol v2 by a fixed date? | Assumed yes: stage 9 before 2037-01-01. |
| Q8 | Pre-1970 data in 64-bit form (untagged words are non-negative by design)? | Rare; represented via tagged era-0 words or an out-of-band type. If 64-bit pre-1970 is a real need, revisit an out-of-band tag (R1 was drafted to avoid the sign-bit collision). |
| Q9 | ~~How do subsecond precision, deep-past instants, and big-endian readability land in HOT?~~ **DECIDED (2026-10-28): docs first, then code** — normative `docs/08-extensions.md` plus `docs/04`/`docs/05` amendments land before any implementation. Parameters: TAI64N/TAI64NA-style append-only fraction fields, **both ns and as levels now** (12/16-byte records; fraction ∈ [0, 10^9) from the floored second — valid for negative instants; canonical-only, reject non-canonical; fractions attach to both word classes). Deep past **built now**: TAI64-style +2^62 bias in the 63-bit value field, selected by class bits 30–31 of the ns field — representable only in 12/16-byte records, mutually exclusive with compat bit 63. Byte order: **LE default, BE opt-in via header flags bit** in storage v2 / wire v3. New APIs carry a struct-wrapped **`hot_word`** strong typedef (existing `hot_time_t` surface untouched); `_HOT_TIME_BITS` ladder (1 = ns, 2 = ns+as) selects the API surface, mirroring `_TIME_BITS=64`. New formats ride HELLO negotiation with fixed-width entries; new tests take T11+ (T6/T7 extend naturally). **Progress for pickup: the bit-1 mask prerequisite has landed (R15 in `docs/07` — flags bit 1 is now genuinely reserved and free to become the ns-fraction flag). Next step: write `docs/08-extensions.md`; when amending `docs/05`, reconcile the stage table first (Q4's kmod landed as "stage 5", which the table assigns to binary rebuild) before inserting the extensions stage.** | — |
