// CANN 8.5-compatible explicit kernel registration + dispatch.
// This translation unit MUST be compiled by a standard Host C++ compiler.
// Do not compile/link it together with an ASC <<< >>> launch stub.
#include "pearl_build_config_generated.h"
#include "pearl_kernel_loader.h"
#include "../src/pearl_pipeline.h"
#ifndef VEC2X
#define VEC2X 2
#endif
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <limits.h>
#include <string>

namespace pearl {
namespace {
aclrtBinHandle s_cubeBin = nullptr, s_vecBin = nullptr;
aclrtFuncHandle s_cubeFunc = nullptr, s_vecFunc = nullptr;

void Error(const char* name, aclError e) {
    std::cerr << "[pearl-hlc2] " << name << " failed: " << e << std::endl;
}

std::string ResolveBinary(const char* filename) {
    const char* overrideDir = std::getenv("PEARL_KERNEL_DIR");
    std::string dir;
    if (overrideDir && *overrideDir) {
        dir = overrideDir;
    } else {
        Dl_info info = {};
        if (!dladdr(reinterpret_cast<void*>(&LoadKernels), &info) || !info.dli_fname) {
            std::cerr << "[pearl-hlc2] dladdr could not locate libpearl_hlc2.so" << std::endl;
            return {};
        }
        std::string lib = info.dli_fname;
        const size_t slash = lib.find_last_of('/');
        dir = slash == std::string::npos ? "." : lib.substr(0, slash);
    }
    const std::string combined = dir + "/" + filename;
    // 8.5 aclrtBinaryLoadFromFile requires an absolute path.
    char absolute[PATH_MAX] = {};
    if (!realpath(combined.c_str(), absolute)) {
        std::cerr << "[pearl-hlc2] missing kernel binary " << combined
                  << " (set PEARL_KERNEL_DIR or deploy both .o beside .so)" << std::endl;
        return {};
    }
    return absolute;
}

template<class T>
aclError Append(aclrtArgsHandle args, const T& value) {
    aclrtParamHandle param = nullptr;
    return aclrtKernelArgsAppend(args, const_cast<void*>(static_cast<const void*>(&value)),
                                 sizeof(value), &param);
}

// The runtime owns the argument-list storage for aclrtKernelArgsInit.
// A fresh handle is used for each asynchronous launch to avoid editing data
// potentially still in use by a previously enqueued kernel.
template<class... T>
aclError Launch(aclrtFuncHandle func, uint32_t blocks, aclrtStream stream, const T&... vals) {
    if (!func) return ACL_ERROR_INVALID_PARAM;
    aclrtArgsHandle args = nullptr;
    aclError e = aclrtKernelArgsInit(func, &args);
    if (e != ACL_SUCCESS) return e;
    const aclError results[] = { Append(args, vals)... };
    for (aclError v : results) {
        if (v != ACL_SUCCESS) return v;
    }
    e = aclrtKernelArgsFinalize(args);
    if (e != ACL_SUCCESS) return e;
    return aclrtLaunchKernelWithConfig(func, blocks, stream, nullptr, args, nullptr);
}

} // namespace

bool LoadKernels() {
    if (s_cubeBin || s_vecBin) return s_cubeFunc && s_vecFunc;
    const std::string cube = ResolveBinary("pearl_cube.o");
    const std::string vec  = ResolveBinary("pearl_vec.o");
    if (cube.empty() || vec.empty()) return false;
    aclError e = aclrtBinaryLoadFromFile(cube.c_str(), nullptr, &s_cubeBin);
    if (e != ACL_SUCCESS) { Error("aclrtBinaryLoadFromFile(cube)", e); return false; }
    e = aclrtBinaryGetFunction(s_cubeBin, "pearl_hlc2_cube", &s_cubeFunc);
    if (e != ACL_SUCCESS) { Error("aclrtBinaryGetFunction(cube)", e); UnloadKernels(); return false; }
    e = aclrtBinaryLoadFromFile(vec.c_str(), nullptr, &s_vecBin);
    if (e != ACL_SUCCESS) { Error("aclrtBinaryLoadFromFile(vector)", e); UnloadKernels(); return false; }
    e = aclrtBinaryGetFunction(s_vecBin, "pearl_hlc2_vec", &s_vecFunc);
    if (e != ACL_SUCCESS) { Error("aclrtBinaryGetFunction(vector)", e); UnloadKernels(); return false; }
    return true;
}

void UnloadKernels() {
    // Caller has synchronized all streams; handles no longer used in outstanding tasks.
    if (s_vecBin) {
        const aclError e = aclrtBinaryUnLoad(s_vecBin);
        if (e != ACL_SUCCESS) Error("aclrtBinaryUnLoad(vector)", e);
        s_vecBin = nullptr;
    }
    if (s_cubeBin) {
        const aclError e = aclrtBinaryUnLoad(s_cubeBin);
        if (e != ACL_SUCCESS) Error("aclrtBinaryUnLoad(cube)", e);
        s_cubeBin = nullptr;
    }
    s_cubeFunc = nullptr;
    s_vecFunc = nullptr;
}

aclError LaunchCube(aclrtStream stream, uint32_t blocks,
                    void* bt, void* at, void* part, void* prodF, void* consF,
                    void* workspace, const AscendC::tiling::TCubeTiling& tiling,
                    int32_t nbands, int32_t gridDim, int32_t strip) {
    return Launch(s_cubeFunc, blocks, stream, bt, at, part, prodF, consF,
                  workspace, tiling, nbands, gridDim, strip);
}

aclError LaunchVector(aclrtStream stream, uint32_t blocks,
                      void* part, void* trans, void* prodF, void* consF,
                      int32_t nbands, int32_t gridDim, int32_t strip,
                      void* hitResult, void* key, void* target,
                      int32_t n_hi, int32_t emitTranscript) {
    return Launch(s_vecFunc, blocks, stream, part, trans, prodF, consF,
                  nbands, gridDim, strip, hitResult, key, target, n_hi, emitTranscript);
}
} // namespace pearl

// Preserve the original C ABI entrypoint used by the host pipeline.
extern "C" void LaunchVec(void* part, void* trans, void* prodF, void* consF,
                          int32_t nbands, int32_t gridDim, int32_t strip, void* stream,
                          void* hitResult, void* key, void* target, int32_t n_hi, int32_t emitTranscript) {
    const aclError e = pearl::LaunchVector(static_cast<aclrtStream>(stream),
                                           PEARL_GRID * VEC2X, part, trans, prodF, consF,
                                           nbands, gridDim, strip, hitResult, key, target,
                                           n_hi, emitTranscript);
    if (e != ACL_SUCCESS) {
        std::cerr << "[pearl-hlc2] launch vector failed: " << e << std::endl;
        std::exit(2);
    }
}
