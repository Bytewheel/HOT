# HOT — rollout

## Order (each stage gated by `docs/04-validation.md` acceptance)

| stage | action | reversibility |
|-------|--------|---------------|
| 0 | Baseline `scripts/host-audit.sh` on all hosts; inventory 32-bit artifacts (binaries, schemas, protocols, appliances) | none needed |
| 1 | A1/A3/A5 build flags + CI matrix — no-op on 64-bit hosts, flushes latent bugs | trivial |
| 2 | Land `libhotcompat` with unit/property/fuzz suites and golden fixtures (T2, T3, T7) | additive |
| 3 | Format bumps: wire v2 + storage width/version headers behind version negotiation (D3, D5, E3, F1, G2) | dual-stack window |
| 4 | Extensions per `docs/08`: subsecond (ns/as) records, deep-past class, byte-order opt-in — `hot_word` API (`include/hot/ext.h`), wire v3 + CAPS, storage v2 (D3, D5, F1, G2); tests T11–T14. Docs landed 2026-10-28 (Q9); implementation pending | dual-stack window |
| 5 | Storage migrations (E1–E4) with backup/restore scripts | backed up |
| 6 | Kernel modules `kmod/hot_time` (B1–B3, Q4); rebuild/replace 32-bit binaries (A4, C1); contain irreducibles behind translating proxies (F4) | per artifact |
| 7 | Quarterly 2038 GameDay (T1, T5 full-dress) until GA | n/a |
| 8 | GA: monitoring G3 armed; any compat activation is an incident | monitored |
| 9 | Post-GA: retire v1 endpoints and legacy binaries; era state becomes read-mostly | staged |

Reconciliation (2026-10-28, per Q9 pickup note): Q4's kernel-module
work had been recorded as "stage 5" while the table assigned stage 5
to binary rebuild — kmod now rides the rebuild stage (6); the
extensions stage was inserted as stage 4 (ahead of storage migrations,
so migrated data lands in the final formats). Stages ≥ 4 shifted by
one; Q7's "stage 9 before 2037-01-01" is now literal.

Stage 9 must complete before the 2037-01-01 milestone (`docs/00`).

## Monitoring

Alert on:

- `hot_compat_active() == 1` anywhere (a host is running on 32-bit time
  past the wrap — should be impossible after stage 6),
- any `-HOT_EAMBIGUOUS` return in production,
- tagged words arriving from post-GA writers,
- non-canonical or reserved-class records rejected in production
  (indicates a buggy or hostile writer; `docs/08` X3/X4),
- era hint vs clock disagreement at boot.

Dashboard per host: time source, native width, persisted era, last
audit result.

## Risk register

| risk | likelihood | impact | mitigation |
|------|-----------|--------|------------|
| Mixed 32/64 fleet mis-ordering events | medium | high | stage ordering, T5, F5 skew alerts |
| Era mis-resolution on cold boot with dead RTC | low | high | persisted hint + refusal policy (`docs/03`); manual runbook |
| Un-upgradeable third-party peer stuck at 32-bit time | high | medium | F4 proxying; contained blast radius |
| Unsigned (2106) artifacts missed | medium | medium | audit item 7; `hot_from_u32`; E1 squashfs/vfat |
| Old binaries misreading HOT words as huge integers | medium | medium | loud-failure design (G4); staged rollout |
| Fraction/class-bit codec bugs corrupt subsecond data | low | medium | canonical-only decode (`docs/08` X2–X4); T11/T12; golden bytes T13 |
| LE/BE confusion in mixed-endian fleets | low | medium | scaffolding always LE (`docs/08` X9); BE only by negotiated opt-in; T13 |
| Perf regression in hot paths | low | low | T8 gate |
| Leap-second / RTC quirks during GameDays | low | low | documented per-deployment policy |

## Rollback

Version-negotiated formats (stages 3–4) allow dual-stack windows; storage
migrations (stage 5) are backed up and restorable. Compat tags are
retained in readers even after full migration, so old data stays
readable forever — the reader is never removed, only writers are.
