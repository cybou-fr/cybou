# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Build CYBOU in a separate directory: cmake -S . -B build")
endif()

project(CYBOU
  DESCRIPTION "CYBOU peer-to-peer network software"
  HOMEPAGE_URL "https://cybou.fr/"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#=============================
# Options
#=============================
option(BUILD_GUI "Build the CYBOU desktop executable." OFF)
option(BUILD_TESTS "Build the CYBOU tests and developer tools." ON)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(CYBOU_ENABLE_TEST_HOOKS "Enable deterministic crash hooks in CYBOU test builds." OFF)
option(WITH_CCACHE "Use ccache when it is installed." ON)
set(SANITIZERS "" CACHE STRING "Comma-separated -fsanitize values, e.g. address,undefined.")
if(CYBOU_ENABLE_TEST_HOOKS AND NOT BUILD_TESTS)
  message(FATAL_ERROR "CYBOU_ENABLE_TEST_HOOKS requires BUILD_TESTS=ON.")
endif()
include(CMakeDependentOption)
cmake_dependent_option(BUILD_GUI_TESTS "Build the CYBOU desktop tests." ON "BUILD_GUI;BUILD_TESTS" OFF)

include(CybouBuildType)
include(CybouFlags)

#=============================
# Dependencies
#=============================
find_package(OpenSSL 3.5 REQUIRED COMPONENTS Crypto SSL)
find_package(ZLIB REQUIRED)
find_package(blake3 1.8.1 EXACT CONFIG REQUIRED)
find_package(Threads REQUIRED)
# Header-only use: Asio and Beast for networking, Boost.Test for the tests.
find_package(Boost 1.74.0 REQUIRED CONFIG)
set_target_properties(Boost::headers PROPERTIES IMPORTED_GLOBAL TRUE)
if(DEFINED VCPKG_TARGET_TRIPLET)
  # vcpkg's boost/config/user.hpp selects DLL linkage; CYBOU uses Boost header-only.
  target_compile_definitions(Boost::headers INTERFACE BOOST_NO_USER_CONFIG)
  if(BUILD_TESTS)
    # vcpkg ships the header-only Boost.Test runner as a separate package.
    find_package(boost_included_unit_test_framework ${Boost_VERSION} EXACT REQUIRED CONFIG)
  endif()
endif()
if(BUILD_GUI)
  set(qt_components Core Gui Widgets Svg LinguistTools)
  if(BUILD_GUI_TESTS)
    list(APPEND qt_components Test)
  endif()
  find_package(Qt6 6.2 REQUIRED COMPONENTS ${qt_components})
  unset(qt_components)
endif()

if(WITH_CCACHE AND NOT MSVC)
  find_program(CCACHE_EXECUTABLE ccache)
  mark_as_advanced(CCACHE_EXECUTABLE)
  if(CCACHE_EXECUTABLE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE})
  endif()
endif()

#=============================
# Common compile and link settings
#=============================
# Every CYBOU target links core_interface for platform definitions,
# hardening and warnings.
add_library(core_interface INTERFACE)
target_link_libraries(core_interface INTERFACE Threads::Threads)
target_compile_definitions(core_interface INTERFACE $<$<CONFIG:Debug>:DEBUG>)

if(WIN32)
  target_compile_definitions(core_interface INTERFACE
    _WIN32_WINNT=0x0A00
    _WIN32_IE=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_definitions(core_interface INTERFACE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(core_interface INTERFACE /utf-8 /Zc:preprocessor /Zc:__cplusplus /sdl /W3 $<$<CONFIG:Debug>:/bigobj> $<$<BOOL:${WARNINGS_AS_ERRORS}>:/WX>)
    target_link_options(core_interface INTERFACE /MANIFEST:NO /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT $<$<BOOL:${WARNINGS_AS_ERRORS}>:/WX>)
  else()
    target_compile_definitions(core_interface INTERFACE WIN32 _WINDOWS _MT)
    # MinGW: unaligned vector moves (GCC bug 54412), large debug objects, static runtime.
    cybou_add_compile_flag(core_interface -Wa,-muse-unaligned-vector-move)
    cybou_add_compile_flag(core_interface $<$<CONFIG:Debug>:-Wa,-mbig-obj> CHECK -Wa,-mbig-obj)
    cybou_add_link_flag(core_interface -static)
  endif()
endif()

if(SANITIZERS)
  target_compile_options(core_interface INTERFACE -fsanitize=${SANITIZERS})
  target_link_options(core_interface INTERFACE -fsanitize=${SANITIZERS})
endif()

if(NOT MSVC)
  if(WARNINGS_AS_ERRORS)
    cybou_add_compile_flag(core_interface -Werror)
  endif()
  foreach(flag IN ITEMS
      -Wall -Wextra -Wformat -Wformat-security -Wvla -Wredundant-decls -Wdate-time
      -Wduplicated-branches -Wduplicated-cond -Wlogical-op -Woverloaded-virtual
      -Wsuggest-override -Wimplicit-fallthrough -Wunreachable-code -Wundef
      -Wthread-safety -Wloop-analysis -Wself-assign -Wconditional-uninitialized
      -Wno-unused-parameter -Wno-missing-field-initializers
      -fno-extended-identifiers -fstack-reuse=none
      -Wstack-protector -fstack-protector-all -fcf-protection=full)
    cybou_add_compile_flag(core_interface ${flag})
  endforeach()
  cybou_add_compile_flag(core_interface -fmacro-prefix-map=${PROJECT_SOURCE_DIR}/src=.)
  if(NOT MINGW)
    cybou_add_compile_flag(core_interface -fstack-clash-protection)
  endif()
  # _FORTIFY_SOURCE needs optimisation; Debug builds compile with -O0.
  target_compile_options(core_interface INTERFACE $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3>)
  foreach(flag IN ITEMS
      -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va
      -Wl,-z,relro -Wl,-z,now -Wl,-z,separate-code)
    cybou_add_link_flag(core_interface ${flag})
  endforeach()
endif()

if(BUILD_TESTS)
  enable_testing()
endif()

include(CybouTargets)
include(ThirdParty)
add_subdirectory(src)

message(STATUS "CYBOU: build type ${CMAKE_BUILD_TYPE}, GUI ${BUILD_GUI}, tests ${BUILD_TESTS}, "
  "compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
