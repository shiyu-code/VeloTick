#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libuv::uv_a" for configuration "MinSizeRel"
set_property(TARGET libuv::uv_a APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(libuv::uv_a PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/libuv.lib"
  )

list(APPEND _cmake_import_check_targets libuv::uv_a )
list(APPEND _cmake_import_check_files_for_libuv::uv_a "${_IMPORT_PREFIX}/lib/libuv.lib" )

# Import target "libuv::uv" for configuration "MinSizeRel"
set_property(TARGET libuv::uv APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(libuv::uv PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/uv.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/uv.dll"
  )

list(APPEND _cmake_import_check_targets libuv::uv )
list(APPEND _cmake_import_check_files_for_libuv::uv "${_IMPORT_PREFIX}/lib/uv.lib" "${_IMPORT_PREFIX}/bin/uv.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
