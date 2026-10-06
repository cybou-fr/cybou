# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

include(GNUInstallDirs)

# cybou_install(<target> [INTERNAL]): user-facing executables go to bin,
# test executables (INTERNAL) to libexec.
function(cybou_install target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "INTERNAL" "" "")
  if(ARG_INTERNAL)
    set(destination ${CMAKE_INSTALL_LIBEXECDIR})
  else()
    set(destination ${CMAKE_INSTALL_BINDIR})
  endif()
  install(TARGETS ${target} RUNTIME DESTINATION ${destination} COMPONENT ${target})
endfunction()

# cybou_windows_manifest(<target> [<extra .rc files>...]): embeds the CYBOU
# application manifest (UTF-8 active code page, asInvoker) and any
# extra resource scripts into a Windows executable. No-op elsewhere.
function(cybou_windows_manifest target)
  if(NOT WIN32)
    return()
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/cmake/windows-app.manifest.in ${target}.manifest)
  file(CONFIGURE OUTPUT ${target}-manifest.rc
    CONTENT "1 24 \"${target}.manifest\"\n")
  target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}-manifest.rc ${ARGN})
endfunction()
