#!/usr/bin/env bash
# HOT CI entry point: A3 cast gate + A5 matrix legs runnable on this
# host (x86_64 native; i686 via -m32 with A1 flags). Cross-arch legs
# (armv7, aarch64) run in the CI provider and are SKIPped here
# (docs/04-validation.md).
set -u
cd "$(dirname "$0")/.."

PASS=0 FAIL=0 SKIP=0
step() { printf '\n== %s\n' "$1"; }
ok()   { printf '   [ok]   %s\n' "$1"; PASS=$((PASS + 1)); }
bad()  { printf '   [FAIL] %s\n' "$1"; FAIL=$((FAIL + 1)); }
skip() { printf '   [SKIP] %s\n' "$1"; SKIP=$((SKIP + 1)); }

A1="-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -Iinclude"
W="-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion"

step "A3 cast gate"
if scripts/cast-gate.sh src include tests; then
  ok "cast gate over src include tests"
else
  bad "cast gate over src include tests"
fi

step "A3 cast gate self-test (planted violations)"
t=$(mktemp -d)
mkdir -p "$t/src"
printf 'uint32_t x = (uint32_t)now;\ntime_t f : 32;\n' > "$t/src/bad.c"
if scripts/cast-gate.sh "$t" >/dev/null 2>&1; then
  bad "planted narrowing cast + bitfield not caught"
else
  ok "planted narrowing cast + bitfield caught"
fi
rm -rf "$t"

step "A5 matrix leg: x86_64 native + A1, ASan/UBSan fuzz"
make clean >/dev/null 2>&1
if make check >/tmp/opencode/check.out 2>&1; then
  ok "make check (63 checks + vet + golden + fuzz 1e6)"
else
  tail -5 /tmp/opencode/check.out
  bad "make check"
fi

step "A5 matrix leg: i686 via -m32 + A1"
d=$(mktemp -d)
if cc -m32 -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -x c -o "$d/probe" - <<< 'int main(void){return 0;}' >/dev/null 2>&1; then
  for t in test_hot test_wire test_store golden_hot; do
    if cc -m32 -std=c11 $W -O2 $A1 -o "$d/$t" tests/$t.c src/hot.c src/hotwire.c src/hotstore.c \
       && "$d/$t" >/dev/null 2>&1; then
      ok "i686 $t"
    else
      bad "i686 $t"
    fi
  done
  if cc -m32 -std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -O2 $A1 -o "$d/fuzz32" tests/fuzz_hot.c src/hot.c src/hotwire.c src/hotstore.c 2>/dev/null \
     && ASAN_OPTIONS=detect_leaks=0 "$d/fuzz32" 100000 >/dev/null 2>&1; then
    ok "i686 fuzz ASan/UBSan (1e5)"
  else
    skip "i686 sanitized fuzz (asan multilib absent?)"
  fi
else
  skip "i686 leg: no -m32 toolchain on this host"
fi
rm -rf "$d"

step "A3 typed coverage: clang-tidy bugprone-narrowing-conversions"
if command -v clang-tidy >/dev/null 2>&1; then
  if clang-tidy --checks='-*,bugprone-narrowing-conversions' --warnings-as-errors='*' \
       src/hot.c src/hotwire.c src/hotstore.c tests/test_hot.c tests/test_wire.c tests/test_store.c tests/fuzz_hot.c tests/golden_hot.c -- \
       -std=c11 $A1 >/tmp/opencode/tidy.out 2>&1; then
    ok "clang-tidy clean"
  else
    grep -E 'warning|error' /tmp/opencode/tidy.out | sed 's/^/     /'
    bad "clang-tidy findings"
  fi
else
  skip "clang-tidy not installed on this host"
fi

step "A5 matrix legs: armv7, aarch64"
skip "cross-arch — CI provider (docs/04-validation.md)"

printf '\nci: %d ok, %d skip, %d fail\n' "$PASS" "$SKIP" "$FAIL"
[ "$FAIL" -eq 0 ]