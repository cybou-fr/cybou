// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_H
#define CYBOU_STORAGE_ASSIGNMENT_H

#include <cybou/private_application_store.h>
#include <array>
#include <compare>
#include <optional>
#include <vector>

namespace cybou {
using StorageAssignmentId = std::array<unsigned char, 32>;
inline constexpr std::size_t MAX_STORAGE_ASSIGNMENT_CANDIDATES{1024};

struct StorageAssignmentContext {
    StorageAssignmentId network_binding{}, publication{}, chunk{}, finalized_seed{}, payer{};
    std::uint64_t epoch{0}, term_start{0}, term_end{0};
    std::uint8_t replicas{2};
    friend bool operator==(const StorageAssignmentContext&, const StorageAssignmentContext&) = default;
};
struct StorageAssignmentProvider {
    StorageAssignmentId storage_id{}, payout_account{};
    auto operator<=>(const StorageAssignmentProvider&) const = default;
};
struct StorageAssignmentPlan {
    StorageAssignmentContext context;
    std::vector<StorageAssignmentProvider> eligible, selected;
    StorageAssignmentId commitment{};
    friend bool operator==(const StorageAssignmentPlan&, const StorageAssignmentPlan&) = default;
};

/// Pure off-chain preparation. The caller must independently verify finalized
/// seed/publication/lease and payout bindings/eligibility. This does not confer
/// authority, prove physical independence, sign an assignment or authorize pay.
std::optional<StorageAssignmentPlan> PrepareStorageAssignment(StorageAssignmentContext context,
    std::vector<StorageAssignmentProvider> eligible);

/// Immutable encrypted record in the existing application store, not a registry.
/// A different plan for the same publication/chunk/epoch/term fails closed.
bool FreezeStorageAssignment(PrivateApplicationStore& db, const StorageAssignmentPlan& plan);
std::optional<StorageAssignmentPlan> LoadStorageAssignment(PrivateApplicationStore& db,
    const StorageAssignmentContext& context);
}
#endif
