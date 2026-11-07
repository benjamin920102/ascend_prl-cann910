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

# CANN 9.1 SDK non-hybrid compile mode; each target installs a standalone .o.
if(NOT ASCENDC_CMAKE_DIR)
  if(DEFINED ENV{ASCENDC_CMAKE_DIR})
    set(ASCENDC_CMAKE_DIR "$ENV{ASCENDC_CMAKE_DIR}")
  elseif(DEFINED ENV{ASCEND_HOME_PATH})
    set(ASCENDC_CMAKE_DIR "$ENV{ASCEND_HOME_PATH}/tikcpp/ascendc_kernel_cmake")
  elseif(DEFINED ENV{ASC_MODULES})
    get_filename_component(ASCENDC_CMAKE_DIR "$ENV{ASC_MODULES}" DIRECTORY)
  endif()
endif()
if(NOT EXISTS "${ASCENDC_CMAKE_DIR}/ascendc.cmake")
  message(FATAL_ERROR "ascendc.cmake not found at ${ASCENDC_CMAKE_DIR}; export ASCENDC_CMAKE_DIR from CANN 9.1 Toolkit")
endif()
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
