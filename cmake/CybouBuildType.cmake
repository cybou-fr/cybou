# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

# RelWithDebInfo by default, and assertions stay enabled in every configuration:
# CYBOU uses assert() for invariants that must hold in release builds too.
get_property(cybou_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT cybou_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS RelWithDebInfo Release Debug MinSizeRel)

foreach(config IN ITEMS RELEASE RELWITHDEBINFO MINSIZEREL)
  string(REGEX REPLACE "(^| )[/-]DNDEBUG( |$)" " " CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
endforeach()
if(NOT MSVC)
  string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(PREPEND CMAKE_CXX_FLAGS_DEBUG "-O0 ")
endif()
unset(cybou_multi_config)
