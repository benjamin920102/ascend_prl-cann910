# Find headers needed by the ordinary Host C++ (not the ASC Device compiler).
# CANN releases/installers use different folder layouts, e.g. the public
# include tree and the arch-specific ascendc highlevel_api include tree.
# This script intentionally checks real files instead of guessing a versioned path.
function(pearl_resolve_cann_host_includes result)
  set(_roots "")
  foreach(_name IN ITEMS ASCEND_CANN_PACKAGE_PATH ASCEND_HOME_PATH
                         ASCEND_INSTALL_PATH ASCEND_TOOLKIT_HOME CANN_HOME)
    if(DEFINED ${_name} AND NOT "${${_name}}" STREQUAL "")
      list(APPEND _roots "${${_name}}")
    endif()
    if(DEFINED ENV{${_name}} AND NOT "$ENV{${_name}}" STREQUAL "")
      list(APPEND _roots "$ENV{${_name}}")
    endif()
  endforeach()

  # The fatbin CMake directory was already resolved by ResolveAscendCFatbin.
  # Walk up from it to cover incomplete environment variables in CI.
  if(DEFINED ASCENDC_CMAKE_DIR AND NOT "${ASCENDC_CMAKE_DIR}" STREQUAL "")
    get_filename_component(_p "${ASCENDC_CMAKE_DIR}" DIRECTORY)
    foreach(_unused RANGE 0 4)
      list(APPEND _roots "${_p}")
      get_filename_component(_parent "${_p}" DIRECTORY)
      if(_parent STREQUAL _p)
        break()
      endif()
      set(_p "${_parent}")
    endforeach()
  endif()
  list(REMOVE_DUPLICATES _roots)

  # The required include is `acl/acl.h`, so the path passed to -I must be
  # the PARENT of acl/, not the acl/ directory itself.
  set(_common_incs "")
  set(_highlevel_incs "")
  foreach(_root IN LISTS _roots)
    foreach(_suffix IN ITEMS include aarch64-linux/include x86_64-linux/include
                             runtime/include aarch64-linux/runtime/include
                             x86_64-linux/runtime/include)
      if(IS_DIRECTORY "${_root}/${_suffix}")
        list(APPEND _common_incs "${_root}/${_suffix}")
      endif()
    endforeach()
    foreach(_suffix IN ITEMS include/ascendc/highlevel_api
                             aarch64-linux/ascendc/include/highlevel_api
                             x86_64-linux/ascendc/include/highlevel_api
                             aarch64-linux/include/ascendc/highlevel_api
                             x86_64-linux/include/ascendc/highlevel_api
                             include/tikcpp/tikcfw
                             tools/tikcpp/tikcfw
                             compiler/tikcpp/tikcfw)
      if(IS_DIRECTORY "${_root}/${_suffix}")
        list(APPEND _highlevel_incs "${_root}/${_suffix}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _common_incs)
  list(REMOVE_DUPLICATES _highlevel_incs)

  set(_acl_dir "")
  set(_tiling_dir "")
  set(_cube_dir "")
  foreach(_inc IN LISTS _common_incs)
    if(_acl_dir STREQUAL "" AND EXISTS "${_inc}/acl/acl.h")
      set(_acl_dir "${_inc}")
    endif()
    if(_tiling_dir STREQUAL "" AND
       EXISTS "${_inc}/tiling/platform/platform_ascendc.h" AND
       EXISTS "${_inc}/tiling/tiling_api.h")
      set(_tiling_dir "${_inc}")
    endif()
  endforeach()
  foreach(_inc IN LISTS _highlevel_incs)
    if(_cube_dir STREQUAL "" AND EXISTS "${_inc}/kernel_tiling/kernel_tiling.h")
      set(_cube_dir "${_inc}")
    endif()
  endforeach()
  # Some installs put kernel_tiling directly under another standard include root.
  if(_cube_dir STREQUAL "")
    foreach(_inc IN LISTS _common_incs)
      if(EXISTS "${_inc}/kernel_tiling/kernel_tiling.h")
        set(_cube_dir "${_inc}")
        break()
      endif()
    endforeach()
  endif()

  if(_acl_dir STREQUAL "" OR _tiling_dir STREQUAL "" OR _cube_dir STREQUAL "")
    string(REPLACE ";" "\n  " _root_dump "${_roots}")
    message(FATAL_ERROR
      "CANN Host C++ header resolution failed.\n"
      "acl/acl.h: ${_acl_dir}\n"
      "tiling/platform/platform_ascendc.h and tiling/tiling_api.h: ${_tiling_dir}\n"
      "kernel_tiling/kernel_tiling.h: ${_cube_dir}\n"
      "CANN roots inspected:\n  ${_root_dump}\n"
      "On the compiler host run:\n"
      "  find \"${ASCEND_CANN_PACKAGE_PATH}\" -type f \\( -name acl.h -o -name kernel_tiling.h -o -name platform_ascendc.h \\) -print 2>/dev/null\n"
      "Check that a complete CANN 9.1 Toolkit (including Host Tiling headers) is installed."
    )
  endif()

  # Tiling/Matmul may include other high-level headers using both short and
  # relative paths. Include existing SDK roots but never let a missing include
  # silently pass CMake configuration.
  set(_includes "${_acl_dir}" "${_tiling_dir}" "${_cube_dir}")
  foreach(_inc IN LISTS _highlevel_incs)
    list(APPEND _includes "${_inc}")
    if(IS_DIRECTORY "${_inc}/lib")
      list(APPEND _includes "${_inc}/lib")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _includes)
  set(${result} "${_includes}" PARENT_SCOPE)
  message(STATUS "CANN Host acl include: ${_acl_dir}")
  message(STATUS "CANN Host tiling include: ${_tiling_dir}")
  message(STATUS "CANN Host kernel_tiling include: ${_cube_dir}")
endfunction()
