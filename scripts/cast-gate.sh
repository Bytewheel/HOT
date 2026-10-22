#!/usr/bin/env bash
# HOT A3 cast gate: refuse narrowing casts of time values and time
# bitfields. Sanctioned legacy-boundary truncations carry the inline
# marker "hot-cast-gate: allow". Greppable idioms only; typed coverage
# is clang-tidy bugprone-narrowing-conversions (scripts/ci.sh).
set -u

CAST_RE='\([[:space:]]*(u?int(8|16|32)_t|unsigned([[:space:]]+(int|short))?|signed([[:space:]]+(int|short))?|int|short)[[:space:]]*\)'
TIME_RE='\b(time_t|hot_time_t|tv_sec|now|seconds|epoch|mtime|atime|ctime|stamp|boottime)\b'
BITFIELD_RE='\b(time_t|hot_time_t)\b[^;{}]*:[[:space:]]*[0-9]+'

ROOTS=("$@")
[ ${#ROOTS[@]} -eq 0 ] && ROOTS=(src include tests)

files=()
for root in "${ROOTS[@]}"; do
  while IFS= read -r f; do
    files+=("$f")
  done < <(find "$root" -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null | sort)
done
if [ ${#files[@]} -eq 0 ]; then
  echo "cast-gate: no sources found under ${ROOTS[*]}"
  exit 1
fi

viol=0
for f in "${files[@]}"; do
  while IFS= read -r hit; do
    printf 'cast-gate: %s: narrowing cast of time value: %s\n' "$f" "$hit"
    viol=$((viol + 1))
  done < <(grep -nE "$CAST_RE" "$f" | grep -E "$TIME_RE" | grep -vE 'hot-cast-gate:[[:space:]]*allow')
  while IFS= read -r hit; do
    printf 'cast-gate: %s: time bitfield: %s\n' "$f" "$hit"
    viol=$((viol + 1))
  done < <(grep -nE "$BITFIELD_RE" "$f" | grep -vE 'hot-cast-gate:[[:space:]]*allow')
done

if [ "$viol" -gt 0 ]; then
  printf 'cast-gate: FAIL: %d violation(s) in %d files\n' "$viol" "${#files[@]}"
  exit 1
fi
printf 'cast-gate: PASS: %d files clean\n' "${#files[@]}"
exit 0