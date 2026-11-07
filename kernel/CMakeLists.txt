cmake_minimum_required(VERSION 3.16)
set(CMAKE_ASC_ARCHITECTURES "dav-2201" CACHE STRING "NPU architecture")
find_package(ASC REQUIRED)
project(pearl_hlc2 LANGUAGES ASC CXX)
set(CMAKE_CXX_STANDARD 17)
set(NINNER "1" CACHE STRING "inner matmuls per fold-step")
set(NFOLD "32" CACHE STRING "fold-steps")
set(PEARL_PIPE_DEPTH "4" CACHE STRING "host strip queue depth")
set(BUFS "4" CACHE STRING "slot ring depth")
set(BACKOFF "32" CACHE STRING "spin backoff iters")
set(VEC2X "2" CACHE STRING "vec consumers per cube producer")
set(SOC_VERSION "Ascend910B4" CACHE STRING "NPU SoC for fatbin")

# Generate a single config header in the source tree so standalone fatbin
# compilation receives identical shape values even with CANN's custom rules.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pearl_build_config.h.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/pearl_build_config_generated.h" @ONLY)

# CANN 9.1 SDK non-hybrid compile mode, using the actual installed Toolkit layout.
# Note: installed CANN layout is commonly tools/tikcpp or compiler/tikcpp,
# NOT $ASCEND_HOME_PATH/tikcpp. This resolver also handles CI path aliases.
if(NOT ASCEND_CANN_PACKAGE_PATH)
  if(DEFINED ENV{ASCEND_HOME_PATH} AND NOT "$ENV{ASCEND_HOME_PATH}" STREQUAL "")
    set(ASCEND_CANN_PACKAGE_PATH "$ENV{ASCEND_HOME_PATH}")
  endif()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ResolveAscendCFatbin.cmake")
include("${ASCENDC_CMAKE_DIR}/ascendc.cmake")
ascendc_fatbin_library(pearl_cube pearl_hlc2_cube_device.cpp)
ascendc_fatbin_library(pearl_vec  pearl_hlc2_vec_device.cpp)

# Only ordinary Host C++ goes into libpearl_hlc2.so. No <<< >>> calls or registration stubs.
add_library(pearl_hlc2 SHARED pearl_hlc2_host.cpp pearl_kernel_loader.cpp)
set_target_properties(pearl_hlc2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(pearl_hlc2 PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_link_libraries(pearl_hlc2 PRIVATE tiling_api platform ascendcl m dl pthread)
# Runtime 8.5 must provide all required exported functions; detect unresolved at link time.
target_link_options(pearl_hlc2 PRIVATE -Wl,-z,defs)
