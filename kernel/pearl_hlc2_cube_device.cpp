// Standalone device-only Cube kernel; no host launch stub/registration linked.
/* Ascend cube producer and host API. Independent cube/vector streams share
 * monotonic producer/consumer flags; production returns only the hashed hit.
 */
#include "pearl_build_config_generated.h"
#include "../src/pearl_pipeline.h"
#include "kernel_tiling/kernel_tiling.h"
#include "kernel_operator.h"
#define ASCENDC_CUBE_ONLY
#include "lib/matmul_intf.h"

#ifndef NINNER
#define NINNER 1
#endif
#ifndef NFOLD
#define NFOLD 32
#endif
#ifndef BUFS
#define BUFS 4
#endif
#ifndef BACKOFF
#define BACKOFF 32
#endif
#ifndef VEC2X
#define VEC2X 2
#endif
#ifndef BASEN
#define BASEN 0      // L0C N-tile for the matmul; 0 => use full MM_N (one big tile)
#endif

constexpr int32_t R = 128, BN = 64, KSTEP = 128;
constexpr int32_t NINNER_C = NINNER, NFOLD_C = NFOLD;
constexpr int32_t NKT = NFOLD_C * NINNER_C;
constexpr int64_t KFULL = (int64_t)NKT * KSTEP;
// Each cube submission consumes one [R, K] strip and produces [BN, R] partials.
constexpr int32_t MM_M = BN, MM_N = R, MM_K = NINNER_C * KSTEP;
constexpr int32_t GRID = PEARL_GRID;
static_assert(BUFS >= 2, "one-band-late publication requires at least two buffers");
static_assert(VEC2X >= 1, "VEC2X must be positive");
static_assert(BACKOFF >= 0, "BACKOFF must be nonnegative");
constexpr size_t HIT_BYTES = PEARL_HIT_WORDS(VEC2X) * sizeof(uint32_t);
constexpr int32_t PART_ELEMS = BN * R;
constexpr int32_t SLOT_ELEMS = MM_M * MM_N;
constexpr int32_t WORDS = 16;
constexpr int32_t FSTRIDE = 128;
// Flags live at (lc * BUFS + buf) * FSTRIDE; VEC2X only multiplies vec launches, not flag slots.
constexpr int32_t NFLAGS = GRID * BUFS * FSTRIDE;

__aicore__ inline uint64_t Slot(int32_t lc, int32_t buf, int32_t p)
{
    return (((uint64_t)lc * BUFS + buf) * NFOLD_C + p) * SLOT_ELEMS;
}

__aicore__ inline void FlagStore(__gm__ volatile int32_t* p, int32_t v)
{
    *p = v;
    dcci((__gm__ int32_t*)p, cache_line_t::SINGLE_CACHE_LINE, dcci_dst_t::CACHELINE_OUT);
}
__aicore__ inline void FlagSpinGE(__gm__ volatile int32_t* p, int32_t v)
{
    // Start with a short poll interval for near-ready buffers; BACKOFF remains
    // the cap for sustained waits. Every poll still refreshes the GM cache line.
    int32_t delay = BACKOFF < 8 ? BACKOFF : 8;
    while (true) {
        dcci((__gm__ int32_t*)p, cache_line_t::SINGLE_CACHE_LINE, dcci_dst_t::CACHELINE_OUT);
        if (*p >= v) break;
        for (volatile int32_t i = 0; i < delay; ++i) {}
        if (delay < BACKOFF) delay = delay > BACKOFF / 2 ? BACKOFF : delay * 2;
    }
}

extern "C" __global__ __cube__ void pearl_hlc2_cube(__gm__ uint8_t* bt, __gm__ uint8_t* at, __gm__ uint8_t* part,
    __gm__ int32_t* prodF, __gm__ int32_t* consF, __gm__ uint8_t* workspace,
    AscendC::tiling::TCubeTiling tiling, int32_t nbands, int32_t gridDim, int32_t strip)
{
    AscendC::TPipe pipe;
    AscendC::GlobalTensor<int8_t> aGM, bGM;
    aGM.SetGlobalBuffer((__gm__ int8_t*)bt);
    bGM.SetGlobalBuffer((__gm__ int8_t*)at);
    AscendC::GlobalTensor<int32_t> partG; partG.SetGlobalBuffer((__gm__ int32_t*)part);
    // The input A operand is transposed logically by matmul, not on the host.
    matmul::Matmul<matmul::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, int8_t, false>,
                   matmul::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, int8_t, true>,
                   matmul::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, int32_t>> mm;
    REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling);
    mm.SetOrgShape(MM_M, MM_N, MM_K, NFOLD_C * MM_K);
    const int32_t lc = AscendC::GetBlockIdx();
    __gm__ volatile int32_t* prod = (__gm__ volatile int32_t*)prodF;
    __gm__ volatile int32_t* cons = (__gm__ volatile int32_t*)consF;
    // Flags are monotonic per producer lane: each lane processes a different
    // number of bands, so a global iteration offset would leave gaps.
    // Publishing one band late permits cube/vec overlap with slot back-pressure.
    const int32_t iterBase = strip * ((nbands - lc + gridDim - 1) / gridDim);
    int32_t iter = 0;
    for (int32_t pair = lc; pair < nbands; pair += gridDim, ++iter) {
        const int32_t giter = iterBase + iter;
        const int32_t buf = giter % BUFS;
        if (giter >= BUFS) FlagSpinGE(&cons[(lc * BUFS + buf) * FSTRIDE], giter - BUFS + 1);
        const uint64_t aBase = (uint64_t)pair * NFOLD_C * MM_M * MM_K;
        for (int32_t p = 0; p < NFOLD_C; ++p) {
            mm.SetTensorA(aGM[aBase + (uint64_t)p * MM_M * MM_K], false);
            mm.SetTensorB(bGM[(uint64_t)p * MM_K], true);
            mm.IterateAll(partG[Slot(lc, buf, p)]);
        }
        if (giter >= 1) FlagStore(&prod[(lc * BUFS + (giter - 1) % BUFS) * FSTRIDE], giter);
    }
    mm.End();
    if (iter >= 1) FlagStore(&prod[(lc * BUFS + (iterBase + iter - 1) % BUFS) * FSTRIDE], iterBase + iter);
}

