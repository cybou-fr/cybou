// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_ECONOMICS_QUOTE_H
#define CYBOU_ECONOMICS_QUOTE_H

#include <cybou/root_publication.h>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace cybou {
/// Current-parameter arithmetic only; not an authorization or balance guarantee.
struct PublicationCostQuote {
    std::uint64_t publication_fee{0};
    std::uint64_t storage_escrow{0};
    std::uint64_t other_fees{0};
    std::uint64_t total_system_debit{0};
    std::uint32_t billing_units{0};
    std::uint32_t lease_periods{0};
    std::uint8_t remote_replicas{0};
};

/// Caller supplies full canonical operation size and authorized chunk count,
/// including metadata chunks. An estimate remains an estimate until preparation.
std::optional<PublicationCostQuote> QuotePublicationCost(const CybouProtocolParameters& params,
    std::size_t canonical_operation_bytes, std::uint32_t chunk_count, std::uint32_t lease_periods);
/// Serialize the actual authorized operation for an exact immediate debit quote.
std::optional<PublicationCostQuote> QuotePublicationCost(const CybouProtocolParameters& params,
    const AuthorizedRootPublication& operation);

struct StorageLeaseCostQuote {
    std::uint64_t protocol_fee{0};
    std::uint64_t storage_escrow{0};
    std::uint64_t total_system_debit{0};
};
/// Future renewal under current parameters, not an already paid obligation.
std::optional<StorageLeaseCostQuote> QuoteStorageLeaseCost(const CybouProtocolParameters& params,
    std::uint32_t billing_units, std::uint8_t remote_replicas, std::uint32_t periods);
}
#endif
