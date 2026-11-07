#!/bin/bash
# Build kernel, Rust proof FFI, and miner on the CANN 9.1.0 build host (non-hybrid fatbin).
set -e
RANK="${RANK:-128}"
source /usr/local/Ascend/cann-9.1.0/set_env.sh
export ASC_MODULES=/usr/local/Ascend/cann-9.1.0/aarch64-linux/tikcpp/ascendc_kernel_cmake/asc_modules
export ASCENDC_CMAKE_DIR=/usr/local/Ascend/cann-9.1.0/aarch64-linux/tikcpp/ascendc_kernel_cmake

# container clock skews ~200s vs the host; touch so make/cmake don't see future mtimes
find . -type f -exec touch {} +

make RANK="$RANK" kernel
make RANK="$RANK" proof
make RANK="$RANK" miner

echo "=== built (RANK=$RANK) ==="
ls -la build/
