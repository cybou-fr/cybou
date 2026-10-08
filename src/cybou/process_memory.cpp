// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/process_memory.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <cstdio>
#include <limits>
#include <unistd.h>
#endif
namespace cybou {
std::optional<uint64_t> ReadProcessResidentBytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) return std::nullopt;
    return static_cast<uint64_t>(memory.WorkingSetSize);
#elif defined(__linux__)
    // Fixed-size proc record, no process enumeration or persistent handle.
    auto* file = std::fopen("/proc/self/statm", "r");
    if (!file) return std::nullopt;
    unsigned long long virtual_pages{0}, resident_pages{0};
    const auto count = std::fscanf(file, "%llu %llu", &virtual_pages, &resident_pages);
    std::fclose(file);
    const auto page_size = ::sysconf(_SC_PAGESIZE);
    if (count != 2 || page_size <= 0 || resident_pages > std::numeric_limits<uint64_t>::max() / page_size)
        return std::nullopt;
    return resident_pages * static_cast<uint64_t>(page_size);
#else
    return std::nullopt;
#endif
}
}
