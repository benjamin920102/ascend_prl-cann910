# Locate the CANN-provided ascendc.cmake (NOT an application-local copy).
# This file is deliberately usable from `cmake -P` for path-only regression tests.
# Common Toolkit layouts include tools/tikcpp and compiler/tikcpp; the older
# <ASCEND_HOME_PATH>/tikcpp-only assumption is not generally valid.
set(_pearl_roots "")
foreach(_var IN ITEMS ASCEND_CANN_PACKAGE_PATH ASCEND_HOME_PATH ASCEND_INSTALL_PATH
                       ASCEND_TOOLKIT_HOME CANN_HOME)
  if(DEFINED ${_var} AND NOT "${${_var}}" STREQUAL "")
    list(APPEND _pearl_roots "${${_var}}")
  endif()
  if(DEFINED ENV{${_var}} AND NOT "$ENV{${_var}}" STREQUAL "")
    list(APPEND _pearl_roots "$ENV{${_var}}")
  endif()
endforeach()

# `ASC_MODULES` usually points at .../ascendc_kernel_cmake/asc_modules,
# so its parent may itself be the directory containing ascendc.cmake.
if(DEFINED ENV{ASC_MODULES} AND NOT "$ENV{ASC_MODULES}" STREQUAL "")
  get_filename_component(_pearl_module_parent "$ENV{ASC_MODULES}" DIRECTORY)
  list(APPEND _pearl_roots "${_pearl_module_parent}")
endif()
if(DEFINED CMAKE_MODULE_PATH)
  foreach(_p IN LISTS CMAKE_MODULE_PATH)
    get_filename_component(_pearl_module_parent "${_p}" DIRECTORY)
    list(APPEND _pearl_roots "${_pearl_module_parent}")
  endforeach()
endif()

list(REMOVE_DUPLICATES _pearl_roots)
set(_pearl_candidates "")

# Explicit overrides take precedence ONLY if they contain the CMake file.
foreach(_explicit IN ITEMS "${ASCENDC_CMAKE_DIR}" "$ENV{ASCENDC_CMAKE_DIR}")
  if(NOT "${_explicit}" STREQUAL "")
    list(APPEND _pearl_candidates "${_explicit}")
  endif()
endforeach()

foreach(_root IN LISTS _pearl_roots)
  foreach(_sub IN ITEMS
      "tools/tikcpp/ascendc_kernel_cmake"
      "compiler/tikcpp/ascendc_kernel_cmake"
      "aarch64-linux/tikcpp/ascendc_kernel_cmake"
      "x86_64-linux/tikcpp/ascendc_kernel_cmake"
      "aarch64-linux/tools/tikcpp/ascendc_kernel_cmake"
      "aarch64-linux/compiler/tikcpp/ascendc_kernel_cmake"
      "x86_64-linux/tools/tikcpp/ascendc_kernel_cmake"
      "x86_64-linux/compiler/tikcpp/ascendc_kernel_cmake"
      "ascendc_devkit/tikcpp/samples/cmake"
      "tikcpp/ascendc_kernel_cmake"
      "")
    if(_sub STREQUAL "")
      list(APPEND _pearl_candidates "${_root}")
    else()
      list(APPEND _pearl_candidates "${_root}/${_sub}")
    endif()
  endforeach()
endforeach()
list(REMOVE_DUPLICATES _pearl_candidates)

set(_pearl_found "")
foreach(_candidate IN LISTS _pearl_candidates)
  if(EXISTS "${_candidate}/ascendc.cmake")
    get_filename_component(_pearl_found "${_candidate}" REALPATH)
    break()
  endif()
endforeach()

if(_pearl_found STREQUAL "")
  string(REPLACE ";" "\n  " _searched "${_pearl_candidates}")
  message(FATAL_ERROR
    "Cannot find ascendc.cmake in the installed CANN Toolkit.\n"
    "ASCEND_HOME_PATH='$ENV{ASCEND_HOME_PATH}'\n"
    "ASC_MODULES='$ENV{ASC_MODULES}'\n"
    "ASCENDC_CMAKE_DIR='$ENV{ASCENDC_CMAKE_DIR}'\n"
    "Searched:\n  ${_searched}\n"
    "Find it on the build machine with:\n"
    "  find \"$ENV{ASCEND_HOME_PATH}\" -name ascendc.cmake -print 2>/dev/null\n"
    "If present, set ASCENDC_CMAKE_DIR to the directory containing that file."
  )
endif()
set(ASCENDC_CMAKE_DIR "${_pearl_found}")
message(STATUS "Using AscendC fatbin CMake: ${ASCENDC_CMAKE_DIR}/ascendc.cmake")
