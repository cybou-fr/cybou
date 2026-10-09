// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_USAGE_H
#define CYBOU_STORAGE_USAGE_H
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
namespace cybou {
struct StorageUsage {
    std::array<unsigned char, 32> storage_id{};
    uint64_t capacity_bytes{0}, stored_bytes{0};
};
struct StorageUsageSample {
    StorageUsage usage;
    std::chrono::steady_clock::time_point sampled_at;
};
struct NetworkStorageUsage {
    uint64_t capacity_bytes{0}, stored_bytes{0}, oldest_age_ms{0};
    size_t nodes{0}, responding_endpoints{0}, known_endpoints{0};
    uint64_t peak_capacity_bytes{0}, peak_stored_bytes{0};
};
// Each storage key contributes once. Copies on distinct nodes count as physical
// bytes. Only fresh direct samples count; no sum of another node's aggregate.
inline std::optional<NetworkStorageUsage> SumStorageUsage(
    const StorageUsage& local, std::span<const StorageUsageSample> samples,
    size_t known_endpoints, std::chrono::steady_clock::time_point now)
{
    std::map<std::array<unsigned char, 32>, StorageUsageSample> unique;
    unique.emplace(local.storage_id, StorageUsageSample{local, now});
    NetworkStorageUsage result;
    result.known_endpoints = known_endpoints;
    for (const auto& sample : samples) {
        if (sample.sampled_at > now || now - sample.sampled_at > std::chrono::seconds{90} ||
            sample.usage.storage_id == local.storage_id) continue;
        ++result.responding_endpoints;
        auto [it, inserted] = unique.emplace(sample.usage.storage_id, sample);
        if (!inserted && sample.sampled_at > it->second.sampled_at) it->second = sample;
    }
    for (const auto& [id, sample] : unique) {
        const auto& usage = sample.usage;
        if (usage.capacity_bytes > std::numeric_limits<uint64_t>::max() - result.capacity_bytes ||
            usage.stored_bytes > std::numeric_limits<uint64_t>::max() - result.stored_bytes) return std::nullopt;
        result.capacity_bytes += usage.capacity_bytes;
        result.stored_bytes += usage.stored_bytes;
        result.oldest_age_ms = std::max(result.oldest_age_ms, static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - sample.sampled_at).count()));
    }
    result.nodes = unique.size();
    return result;
}
} // namespace cybou
#endif
