# HOT — host audit

Every host (physical, VM, container base image) must pass this audit
before rollout stage 5 (`docs/05-rollout.md`). Run
`scripts/host-audit.sh` and file the output under the host's inventory
entry. The script checks items 1–8 below; items 9–11 are manual review,
since no local probe can answer them.

## Checklist

| # | item | why | pass criteria |
|---|------|-----|---------------|
| 1 | architecture | determines native `time_t` width | 64-bit, or 32-bit with items 2–5 all passing |
| 2 | kernel release | 32-bit userspace needs the `*_time64` syscall family | ≥ 5.1 for 32-bit targets |
| 3 | C library | `_TIME_BITS=64` support | glibc ≥ 2.34 or musl ≥ 1.2.0 |
| 4 | `sizeof(time_t)` | actual compiled width of the default toolchain | 8 |
| 5 | `_TIME_BITS=64` probe | libc honors the macro (`__TIME_BITS__ == 64`) | honored |
| 6 | 32-bit userspace artifacts | binaries that cannot be fixed in place | none, or inventoried for stage 6 |
| 7 | filesystems in active use | on-disk timestamp width | no 2038-limited filesystem on any hot path |
| 8 | time synchronization client | NTP-era-rollover (2036) posture | NTS-capable client or documented era handling |
| 9 | epoch state files | DHCP leases, caches, queues storing epoch seconds | 64-bit or HOT-tagged (E3) |
| 10 | database schemas | `TIMESTAMP`-capped and INT-epoch columns | migrated or scheduled (E2) |
| 11 | wire protocols in use | internal + third-party time fields | versioned/64-bit (F1, F4) |

## Known filesystem time ranges (for item 7)

| filesystem | stored timestamp range | status |
|------------|----------------------|--------|
| ext4 (256-byte inodes) | 1901–2446 | OK |
| XFS, Btrfs, F2FS, tmpfs, ZFS | 64-bit-class | OK |
| ext2, ext3 | 32-bit seconds → 2038 | migrate hot paths |
| vfat/msdos | 2107 | migrate or contain |
| squashfs | 32-bit → 2106 | verify embedded images |
| NFS/CIFS | server-dependent | audit the server |
| overlayfs, bind mounts | passthrough | audit the underlying FS |

(Values from kernel source docs; verify per deployment — see
`docs/07-review-log.md` R7.)

## Snapshot — this host

The audit script was run during planning (2026-10-21) on the development
host that motivated this plan. Output:

```
HOT host audit
date: 2026-10-21T21:09:10Z
host: 0115D8CF.tplinkdns.com

1) architecture: x86_64
  [PASS] 64-bit architecture; native time_t is 64-bit wide

2) kernel release: 7.2.7-lynnicks
  [PASS] kernel >= 5.1; *_time64 syscall family available for 32-bit userspace

3) C library
  glibc 2.44
  [PASS] glibc >= 2.34; _TIME_BITS=64 supported for 32-bit builds

4) time_t width and _TIME_BITS support (checklist items 4-5)
  sizeof(time_t) = 8 bytes
  [PASS] default toolchain time_t is 64-bit
  [PASS] _TIME_BITS=64 honored by this libc (64)
  [WARN] multilib toolchain present (-m32 links); 32-bit binaries are buildable here

6) 32-bit userspace artifacts
  [WARN] 32-bit userspace present: /lib/ld-linux.so.2 — inventory for rollout stage 5

7) filesystems in active use
  mounted types: binfmt_misc bpf cgroup2 configfs debugfs devpts devtmpfs efivarfs ext4 fuse fusectl fuse.portal mqueue nsfs proc pstore securityfs sysfs tmpfs tracefs vfat
  [WARN] time-limited filesystems mounted: vfat(2107) — see docs/01 table

8) time synchronization
  [info] client present: chronyd
  [PASS] time daemon running: chronyd

summary: 6 pass, 3 warn, 0 fail
result: PASS — no blocking findings; manual items 9-11 of docs/01 remain
```

Host: Pentoo Linux 2.18 (Gentoo-based), 12th Gen Intel Core i7-12700H,
custom kernel `7.2.7-lynnicks`, glibc 2.44.

## Findings for this host

Result: **PASS, no blocking findings.** The type system on this host is
already 64-bit end to end; per `docs/00-scope.md` this host's patch work
is the enforcement and artifact track:

1. **Native `time_t` is 64-bit** (glibc 2.44, x86_64): patch classes A1,
   C3, D-series apply to code *we* build here — not to the platform.
2. **`_TIME_BITS=64` honored**: with `_FILE_OFFSET_BITS=64` paired, as
   patch A1 mandates. glibc's feature macros are `__USE_TIME_BITS64` and
   `__USE_TIME64_REDIRECTS` (the audit probe was corrected accordingly
   during planning — see `docs/07-review-log.md` R11).
3. **Multilib toolchain + 32-bit loader present**
   (`-m32` links, `/lib/ld-linux.so.2`): 32-bit binaries can run here.
   Stage 0 must inventory what actually uses them (WINE stems, Steam
   runtime, legacy tools); everything found enters stage 6.
4. **vfat at `/boot`** (the ESP, `/dev/nvme0n1p1`): FAT timestamps are
   2107-limited, 2-second granularity, local-time semantics. Kernel and
   initramfs files there will carry wrapped timestamps at 2107 — a
   monitoring/known-issue item, not a 2038 blocker (bootloaders do not
   consume those timestamps).
5. **ext4 root filesystem**: timestamp range 1901–2446, fine.
6. **chrony running**: NTP era rollover (2036) posture is a test item
   (T9), not a host defect.
7. **OpenRC, no systemd**: `timedatectl` unavailable — the audit falls
   back to daemon detection, which succeeded (chronyd).

Manual checklist items 9–11 (state files, schemas, wire protocols) are
unverified on this host and are stage 0 work.

Two script defects were found and fixed while validating the plan
against this host (see `docs/07-review-log.md`): the `_TIME_BITS` probe
originally omitted the required `_FILE_OFFSET_BITS=64` and checked a
macro glibc does not define. The snapshot above is from the corrected
script.
