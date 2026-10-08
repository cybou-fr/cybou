// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_PROCESS_MEMORY_H
#define CYBOU_PROCESS_MEMORY_H
#include <cstdint>
#include <optional>
namespace cybou {
/// Instantaneous resident process bytes (Windows working set / Linux RSS).
/// Includes the entire executable, shared pages included; unavailable is not zero.
std::optional<uint64_t> ReadProcessResidentBytes();
}
#endif
