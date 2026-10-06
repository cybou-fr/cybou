# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

# cybou_add_compile_flag(<target> <flag> [CHECK <flag-to-test>])
# Adds <flag> to the INTERFACE compile options of <target> when the compiler
# accepts it. CHECK names the plain flag to test when <flag> is a generator expression.
function(cybou_add_compile_flag target flag)
  cmake_parse_arguments(PARSE_ARGV 2 ARG "" "CHECK" "")
  set(tested "${flag}")
  if(ARG_CHECK)
    set(tested "${ARG_CHECK}")
  endif()
  string(MAKE_C_IDENTIFIER "CYBOU_CXX_ACCEPTS${tested}" result)
  check_cxx_compiler_flag("${tested}" ${result})
  if(${result})
    target_compile_options(${target} INTERFACE "${flag}")
  endif()
endfunction()

# cybou_add_link_flag(<target> <flag>)
# Adds <flag> to the INTERFACE link options of <target> when the linker accepts it.
function(cybou_add_link_flag target flag)
  string(MAKE_C_IDENTIFIER "CYBOU_LD_ACCEPTS${flag}" result)
  check_linker_flag(CXX "${flag}" ${result})
  if(${result})
    target_link_options(${target} INTERFACE "${flag}")
  endif()
endfunction()
