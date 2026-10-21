# ByteWheel / HOT — Handover Of Time

Y2038 remediation program: make 64-bit Unix time work end-to-end on modern
Linux systems (including this host), plus a compatibility layer that keeps
32-bit-epoch data usable and unambiguous across the 2038 wrap.

Status: **planning, v0.1**. This repository contains the plan only — patch
inventory, normative design, validation plan, rollout order, and open
questions. No production code yet.

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
| `scripts/host-audit.sh` | runnable host capability probe (see `docs/01`) |
| `tests/spec_vet.c` | numeric verification of the `docs/03` spec (seed of test T2) |

## Quick start

    scripts/host-audit.sh

Exit code 0 = no blocking findings for 64-bit time readiness.

## The one forbidden failure mode

Silent misinterpretation of time. Anything that cannot represent or
understand a time value must fail loudly, never guess.
