#!/usr/bin/env bash
# Run on the CANN 8.5.0 execution host, after deploying all three artifacts.
set -euo pipefail
cd "$(dirname "$0")/.."
for f in build/libpearl_hlc2.so build/pearl_cube.o build/pearl_vec.o; do
  if [[ ! -s "$f" ]]; then echo "[FAIL] missing $f" >&2; exit 2; fi
done
bad='rtRegisterFuncSymbol|rtRegisterVariable|aclrtGetFuncBySymbol|rtFunctionGetMetaInfoSize'
if readelf --dyn-syms -W build/libpearl_hlc2.so | grep -E "${bad}"; then
  echo '[FAIL] Host .so still references 9.1 registration symbols' >&2
  exit 3
fi
echo '[OK] No known 9.1 stub registration references'
# ldd -r may return failure; keep output to diagnose every unresolved symbol.
set +e
output=$(ldd -r build/libpearl_hlc2.so 2>&1)
code=$?
set -e
printf '%s\n' "$output"
if (( code != 0 )) || grep -Eq 'undefined symbol|not found' <<<"$output"; then
  echo '[FAIL] Host ELF is not yet loadable on this Runtime 8.5 host' >&2
  exit 4
fi
echo '[OK] Host ELF dynamic linker check passed; next test actual NPU binary load + result correctness'
