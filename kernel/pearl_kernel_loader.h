#pragma once
#include <cstdint>
#include "acl/acl.h"
#include "kernel_tiling/kernel_tiling.h"

namespace pearl {
// Binary paths: PEARL_KERNEL_DIR override, otherwise directory of libpearl_hlc2.so.
bool LoadKernels();
void UnloadKernels();
aclError LaunchCube(aclrtStream stream, uint32_t blockDim,
                    void* bt, void* at, void* part, void* prodF, void* consF,
                    void* workspace, const AscendC::tiling::TCubeTiling& tiling,
                    int32_t nbands, int32_t gridDim, int32_t strip);
aclError LaunchVector(aclrtStream stream, uint32_t blockDim,
                      void* part, void* trans, void* prodF, void* consF,
                      int32_t nbands, int32_t gridDim, int32_t strip,
                      void* hitResult, void* key, void* target,
                      int32_t n_hi, int32_t emitTranscript);
}
