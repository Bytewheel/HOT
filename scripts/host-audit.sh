#!/usr/bin/env bash
set -u

PASS=0
WARN=0
FAIL=0
ok()   { printf '  [PASS] %s\n' "$*"; PASS=$((PASS+1)); }
warn() { printf '  [WARN] %s\n' "$*"; WARN=$((WARN+1)); }
fail() { printf '  [FAIL] %s\n' "$*"; FAIL=$((FAIL+1)); }
note() { printf '  [info] %s\n' "$*"; }

echo "HOT host audit"
echo "date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "host: $(hostname 2>/dev/null || echo unknown)"
echo

ARCH=$(uname -m)
KERNEL=$(uname -r)

echo "1) architecture: $ARCH"
case "$ARCH" in
  x86_64|aarch64|arm64|ppc64le|ppc64|riscv64|s390x|loongarch64|mips64*)
    ok "64-bit architecture; native time_t is 64-bit wide" ;;
  i[3-6]86|armv[5-7]*|arm|mips|mipsel|powerpc|m68k|sh4)
    fail "32-bit architecture; needs _TIME_BITS=64 rebuild and kernel >= 5.1" ;;
  *)
    note "unrecognized architecture; verify sizeof(time_t) manually" ;;
esac

echo
echo "2) kernel release: $KERNEL"
if awk -v k="$KERNEL" 'BEGIN {
  n = split(k, p, ".")
  if (p[1]+0 > 5) exit 0
  if (p[1]+0 == 5 && p[2]+0 >= 1) exit 0
  exit 1
}'; then
  ok "kernel >= 5.1; *_time64 syscall family available for 32-bit userspace"
else
  fail "kernel < 5.1; 32-bit userspace cannot obtain 64-bit time via time64 syscalls"
fi

echo
echo "3) C library"
if getconf GNU_LIBC_VERSION >/dev/null 2>&1; then
  GLIBC=$(getconf GNU_LIBC_VERSION)
  echo "  $GLIBC"
  gv=${GLIBC#glibc }
  gmaj=${gv%%.*}
  vrest=${gv#*.}
  gmin=${vrest%%.*}
  if [ "${gmaj:-0}" -gt 2 ] 2>/dev/null || \
     { [ "${gmaj:-0}" -eq 2 ] 2>/dev/null && [ "${gmin:-0}" -ge 34 ] 2>/dev/null; }; then
    ok "glibc >= 2.34; _TIME_BITS=64 supported for 32-bit builds"
  else
    warn "glibc < 2.34 or unparsed; upgrade libc before 32-bit rebuilds"
  fi
else
  if command -v ldd >/dev/null 2>&1; then
    lddout=$(ldd --version 2>&1 || true)
    printf '  %s\n' "$(printf '%s\n' "$lddout" | sed -n '1,2p')"
    case "$lddout" in
      musl*)
        mver=$(printf '%s\n' "$lddout" | sed -n 's/^Version //p')
        if [ -n "$mver" ] && awk -v v="$mver" 'BEGIN{exit !(v+0 >= 1.2)}'; then
          ok "musl >= 1.2; 64-bit time_t on 32-bit targets"
        else
          warn "musl < 1.2; 64-bit time_t unavailable on 32-bit targets"
        fi ;;
      *)
        note "non-glibc, non-musl libc; verify 64-bit time support manually" ;;
    esac
  else
    note "cannot determine libc; install getconf or ldd"
  fi
fi

echo
echo "4) time_t width and _TIME_BITS support (checklist items 4-5)"
CCBIN=$(command -v cc || command -v gcc || true)
if [ -n "$CCBIN" ]; then
  TMP=$(mktemp -d)
  printf '%s\n' \
    '#include <stdio.h>' \
    '#include <time.h>' \
    'int main(void) { printf("%zu\n", sizeof(time_t)); return 0; }' > "$TMP/probe.c"
  if "$CCBIN" -o "$TMP/probe" "$TMP/probe.c" 2>/dev/null; then
    W=$("$TMP/probe")
    echo "  sizeof(time_t) = ${W} bytes"
    if [ "$W" -eq 8 ]; then
      ok "default toolchain time_t is 64-bit"
    else
      fail "default toolchain time_t is ${W} bytes; rebuild with _TIME_BITS=64"
    fi
  else
    note "probe compile failed; check toolchain"
  fi

  printf '%s\n' \
    '#include <stdio.h>' \
    '#include <time.h>' \
    'int main(void) {' \
    '#ifdef __USE_TIME_BITS64' \
    '  printf("64");' \
    '#else' \
    '  printf("unsupported");' \
    '#endif' \
    '#ifdef __USE_TIME64_REDIRECTS' \
    '  printf(" redirects");' \
    '#endif' \
    '  printf("\n");' \
    '  return 0; }' > "$TMP/tb.c"
  if "$CCBIN" -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -o "$TMP/tb" "$TMP/tb.c" 2>/dev/null; then
    TB=$("$TMP/tb")
    if [ "$TB" = "64" ] || [ "$TB" = "64 redirects" ]; then
      ok "_TIME_BITS=64 honored by this libc ($TB)"
    else
      note "_TIME_BITS=64 compiled but unsupported by this libc ($TB)"
    fi
  else
    note "_TIME_BITS=64 rejected by this libc/toolchain (needs _FILE_OFFSET_BITS=64 or newer libc)"
  fi

  printf '%s\n' \
    '#include <stdio.h>' \
    'int main(void) { return 0; }' > "$TMP/m32.c"
  if "$CCBIN" -m32 -o "$TMP/m32" "$TMP/m32.c" >/dev/null 2>&1; then
    warn "multilib toolchain present (-m32 links); 32-bit binaries are buildable here"
  else
    note "no -m32 link capability; accidental 32-bit builds unlikely on this host"
  fi
  rm -rf "$TMP"
else
  note "no C compiler found; skipped width and _TIME_BITS probes"
fi

echo
echo "6) 32-bit userspace artifacts"
found=""
for d in /lib/ld-linux.so.2 /usr/lib/ld-linux.so.2 /lib/i386-linux-gnu \
         /usr/lib/i386-linux-gnu /lib/arm-linux-gnueabihf \
         /usr/lib/arm-linux-gnueabihf /lib/ld-linux-armhf.so.3; do
  [ -e "$d" ] && found="$found $d"
done
if command -v dpkg >/dev/null 2>&1; then
  fa=$(dpkg --print-foreign-architectures 2>/dev/null || true)
  [ -n "$fa" ] && found="$found (dpkg foreign arch: $(printf '%s ' $fa))"
fi
if [ -n "$found" ]; then
  warn "32-bit userspace present:${found} — inventory for rollout stage 5"
else
  ok "no 32-bit userspace artifacts found in common locations"
fi

echo
echo "7) filesystems in active use"
fstypes=$(awk '{print $3}' /proc/mounts | sort -u | tr '\n' ' ')
echo "  mounted types: ${fstypes}"
limited=""
for fs in $fstypes; do
  case "$fs" in
    ext2|ext3)       limited="$limited $fs(2038)" ;;
    msdos|umsdos)    limited="$limited $fs(2038)" ;;
    vfat)            limited="$limited $fs(2107)" ;;
    squashfs)        limited="$limited $fs(2106)" ;;
    nfs|nfs4|cifs|smb3|smbfs) limited="$limited $fs(server-dependent)" ;;
  esac
done
if [ -n "$limited" ]; then
  warn "time-limited filesystems mounted:${limited} — see docs/01 table"
else
  ok "no known time-limited filesystems among mounted types"
fi

echo
echo "8) time synchronization"
for s in chronyd ntpd openntpd systemd-timesyncd; do
  if command -v "$s" >/dev/null 2>&1; then
    note "client present: $s"
  fi
done
running=""
for d in chronyd ntpd openntpd systemd-timesyncd; do
  pgrep -x "$d" >/dev/null 2>&1 && running="$running $d"
done
if [ -n "$running" ]; then
  ok "time daemon running:${running}"
else
  warn "no time daemon detected as running; NTP era handling is unverified here"
fi
if command -v timedatectl >/dev/null 2>&1; then
  timedatectl show -p NTP -p NTPSynchronization 2>/dev/null | sed 's/^/  timedatectl /'
fi

echo
echo "summary: $PASS pass, $WARN warn, $FAIL fail"
if [ "$FAIL" -gt 0 ]; then
  echo "result: FAIL — resolve [FAIL] items before rollout stage 4"
  exit 1
fi
echo "result: PASS — no blocking findings; manual items 9-11 of docs/01 remain"
exit 0
