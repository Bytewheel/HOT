# HOT — open questions

Q1–Q3 were decided on 2026-10-21 (decisions recorded inline below).
Q4–Q8 remain open; each lists the assumption currently baked into the
plan so work can proceed while decisions are pending.

| # | question | current assumption |
|---|----------|--------------------|
| Q1 | ~~Is "the bit" per-value (tag inside each time word), a single global mode flag, or both?~~ **DECIDED (2026-10-21): both**, as drafted — per-word tag (bit 63) normative and load-bearing; system status bit advisory. | — |
| Q2 | ~~Should 32-bit-derived words carry the tag even before the wrap?~~ **DECIDED (2026-10-21): always tag** — one code path, no clock-dependent encoder switch, provenance from day one; the literal wrap-triggered engagement is carried by the system status bit. | — |
| Q3 | ~~Existing inventory of 32-bit artifacts to seed stage 0?~~ **DECIDED (2026-10-21): none exists** — stage 0 builds the inventory via `scripts/host-audit.sh` plus manual items 9–11 of `docs/01`. | — |
| Q4 | Do we ship kernel modules/patches? (activates patch class B) | No; B is documented but out of first scope. |
| Q5 | Where does the era hint persist if `/var/lib` is ephemeral (containers)? | Host-level bind mount, or derive-only with reduced confidence; refusal policy unchanged. |
| Q6 | Any unsigned-32-bit (2106) data sources in the fleet? | Assume yes; `hot_from_u32` provided. |
| Q7 | Authority to force peers to protocol v2 by a fixed date? | Assumed yes: stage 8 before 2037-01-01. |
| Q8 | Pre-1970 data in 64-bit form (untagged words are non-negative by design)? | Rare; represented via tagged era-0 words or an out-of-band type. If 64-bit pre-1970 is a real need, revisit an out-of-band tag (R1 was drafted to avoid the sign-bit collision). |
