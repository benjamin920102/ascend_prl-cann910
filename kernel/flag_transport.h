#pragma once

// Producer and consumer always use direct GM scalar transport (CANN 9.1.x).
// Include AFTER kernel_operator.h from both .asc translation units.
// Bypass DCache is mandatory; no build-time switch or DCache fallback.

// On dav-2201 the bypass WRITE is valid only in the first 32 bytes of each
// 128-byte aligned region.  The host aligns the bases of both flag arrays to
// 128 bytes, and every actual flag is at the start of a 512-byte slot.
static_assert((FSTRIDE * sizeof(int32_t)) % 128 == 0,
              "bypass flags must start at a 128-byte boundary");

__aicore__ inline void FlagStore(__gm__ int32_t* p, int32_t v)
{
    AscendC::WriteGmByPassDCache<int32_t>(p, v);
}

__aicore__ inline int32_t FlagLoad(__gm__ int32_t* p)
{
    return AscendC::ReadGmByPassDCache<int32_t>(p);
}

__aicore__ inline void FlagSpinGE(__gm__ int32_t* p, int32_t v)
{
    // No DCache invalidate/flush on the bypass fast path.  The only remaining
    // wait is the real producer/consumer dependency: a slot cannot be reused
    // until its old GM partials have actually been fetched.
    int32_t delay = BACKOFF < 8 ? BACKOFF : 8;
    while (FlagLoad(p) < v) {
        for (volatile int32_t i = 0; i < delay; ++i) {}
        if (delay < BACKOFF) delay = delay > BACKOFF / 2 ? BACKOFF : delay * 2;
    }
}
