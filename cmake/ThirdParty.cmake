# Copyright (c) 2026 Stanislav SAVELIEV
# SPDX-License-Identifier: Apache-2.0

# Vendored third-party libraries: crc32c and LevelDB (both BSD-3-Clause,
# see their LICENSE files). They build with their own warning set off.

include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)

#=============================
# crc32c
#=============================
check_cxx_source_compiles("int main() { char c = 0; __builtin_prefetch(&c, 0, 0); }" CYBOU_HAVE_BUILTIN_PREFETCH)
check_cxx_source_compiles("
  #if defined(_MSC_VER)
  #include <intrin.h>
  #else
  #include <xmmintrin.h>
  #endif
  int main() { char c = 0; _mm_prefetch(&c, _MM_HINT_NTA); }" CYBOU_HAVE_MM_PREFETCH)
check_cxx_source_compiles("
  #include <sys/auxv.h>
  int main() { return getauxval(AT_HWCAP) == 0; }" CYBOU_HAVE_GETAUXVAL)

if(MSVC)
  set(crc32c_sse42_flags /arch:AVX)
else()
  set(crc32c_sse42_flags -msse4.2)
endif()
set(CMAKE_REQUIRED_FLAGS ${crc32c_sse42_flags})
check_cxx_source_compiles("
  #include <cstdint>
  #if defined(_MSC_VER)
  #include <intrin.h>
  #else
  #include <nmmintrin.h>
  #endif
  int main() { std::uint64_t v = _mm_crc32_u64(0, 0); return static_cast<int>(_mm_crc32_u8(static_cast<unsigned>(v), 0)); }"
  CYBOU_HAVE_SSE42)
set(crc32c_arm64_flags -march=armv8-a+crc+crypto)
set(CMAKE_REQUIRED_FLAGS ${crc32c_arm64_flags})
check_cxx_source_compiles("
  #include <arm_acle.h>
  #include <arm_neon.h>
  #ifndef __aarch64__
  #error 64-bit ARM only
  #endif
  int main() { __crc32cd(0, 0); vmull_p64(0, 0); }" CYBOU_HAVE_ARM64_CRC32C)
unset(CMAKE_REQUIRED_FLAGS)

set(crc32c_dir ${PROJECT_SOURCE_DIR}/src/crc32c)
add_library(crc32c STATIC EXCLUDE_FROM_ALL
  ${crc32c_dir}/src/crc32c.cc
  ${crc32c_dir}/src/crc32c_portable.cc
)
target_compile_definitions(crc32c PRIVATE
  HAVE_BUILTIN_PREFETCH=$<BOOL:${CYBOU_HAVE_BUILTIN_PREFETCH}>
  HAVE_MM_PREFETCH=$<BOOL:${CYBOU_HAVE_MM_PREFETCH}>
  HAVE_STRONG_GETAUXVAL=$<BOOL:${CYBOU_HAVE_GETAUXVAL}>
  BYTE_ORDER_BIG_ENDIAN=$<STREQUAL:${CMAKE_CXX_BYTE_ORDER},BIG_ENDIAN>
  HAVE_SSE42=$<BOOL:${CYBOU_HAVE_SSE42}>
  HAVE_ARM64_CRC32C=$<BOOL:${CYBOU_HAVE_ARM64_CRC32C}>
)
target_include_directories(crc32c PUBLIC ${crc32c_dir}/include)
target_link_libraries(crc32c PRIVATE core_interface)
if(CYBOU_HAVE_SSE42)
  target_sources(crc32c PRIVATE ${crc32c_dir}/src/crc32c_sse42.cc)
  set_source_files_properties(${crc32c_dir}/src/crc32c_sse42.cc PROPERTIES COMPILE_OPTIONS "${crc32c_sse42_flags}")
endif()
if(CYBOU_HAVE_ARM64_CRC32C)
  target_sources(crc32c PRIVATE ${crc32c_dir}/src/crc32c_arm64.cc)
  set_source_files_properties(${crc32c_dir}/src/crc32c_arm64.cc PROPERTIES COMPILE_OPTIONS "${crc32c_arm64_flags}")
endif()

#=============================
# LevelDB
#=============================
check_cxx_symbol_exists(fdatasync "unistd.h" CYBOU_HAVE_FDATASYNC)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" CYBOU_HAVE_FULLFSYNC)
check_cxx_symbol_exists(O_CLOEXEC "fcntl.h" CYBOU_HAVE_O_CLOEXEC)

set(leveldb_dir ${PROJECT_SOURCE_DIR}/src/leveldb)
set(leveldb_sources)
foreach(source IN ITEMS
    db/builder db/db_impl db/db_iter db/dbformat db/filename db/log_reader
    db/log_writer db/memtable db/repair db/table_cache db/version_edit
    db/version_set db/write_batch
    table/block table/block_builder table/filter_block table/format
    table/iterator table/merger table/table table/table_builder
    table/two_level_iterator
    util/arena util/bloom util/cache util/coding util/comparator util/crc32c
    util/env util/filter_policy util/hash util/logging util/options util/status
    helpers/memenv/memenv)
  list(APPEND leveldb_sources ${leveldb_dir}/${source}.cc)
endforeach()
if(WIN32)
  list(APPEND leveldb_sources ${leveldb_dir}/util/env_windows.cc)
else()
  list(APPEND leveldb_sources ${leveldb_dir}/util/env_posix.cc)
endif()

add_library(leveldb STATIC EXCLUDE_FROM_ALL ${leveldb_sources})
target_compile_definitions(leveldb PRIVATE
  HAVE_SNAPPY=0
  HAVE_CRC32C=1
  HAVE_FDATASYNC=$<BOOL:${CYBOU_HAVE_FDATASYNC}>
  HAVE_FULLFSYNC=$<BOOL:${CYBOU_HAVE_FULLFSYNC}>
  HAVE_O_CLOEXEC=$<BOOL:${CYBOU_HAVE_O_CLOEXEC}>
  FALLTHROUGH_INTENDED=[[fallthrough]]
  $<IF:$<BOOL:${WIN32}>,LEVELDB_PLATFORM_WINDOWS,LEVELDB_PLATFORM_POSIX>
  $<$<BOOL:${WIN32}>:_UNICODE>
  $<$<BOOL:${WIN32}>:UNICODE>
  $<$<BOOL:${MINGW}>:__USE_MINGW_ANSI_STDIO=1>
)
target_include_directories(leveldb PRIVATE ${leveldb_dir} PUBLIC ${leveldb_dir}/include)
target_link_libraries(leveldb PRIVATE core_interface crc32c)
if(MSVC)
  target_compile_options(leveldb PRIVATE /wd4722)
  target_compile_definitions(leveldb PRIVATE _CRT_NONSTDC_NO_WARNINGS)
endif()

unset(crc32c_dir)
unset(leveldb_dir)
unset(leveldb_sources)
