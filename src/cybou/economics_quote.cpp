// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/economics_quote.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_lease.h>
#include <limits>

namespace cybou {
std::optional<PublicationCostQuote> QuotePublicationCost(const CybouProtocolParameters& params,
    const std::size_t bytes, const std::uint32_t chunks, const std::uint32_t periods)
{
    const auto fee = ComputeRootPublicationFee(params, bytes, chunks);
    if (!fee || periods > params.max_storage_lease_periods) return std::nullopt;
    const auto escrow = periods == 0 ? std::optional<std::uint64_t>{0} :
        ComputeStorageLeaseEscrow(params, chunks, params.storage_replica_target, periods);
    if (!escrow || *escrow > std::numeric_limits<std::uint64_t>::max() - *fee) return std::nullopt;
    return PublicationCostQuote{.publication_fee = *fee, .storage_escrow = *escrow,
        .total_system_debit = *fee + *escrow, .billing_units = chunks,
        .lease_periods = periods, .remote_replicas = params.storage_replica_target};
}

std::optional<PublicationCostQuote> QuotePublicationCost(const CybouProtocolParameters& params,
    const AuthorizedRootPublication& operation)
{
    const auto encoded = SerializeProtocolOperation(ProtocolOperation{operation});
    if (!encoded) return std::nullopt;
    return QuotePublicationCost(params, encoded->size(), operation.publication.chunk_count,
        operation.publication.lease_periods);
}

std::optional<StorageLeaseCostQuote> QuoteStorageLeaseCost(const CybouProtocolParameters& params,
    const std::uint32_t units, const std::uint8_t replicas, const std::uint32_t periods)
{
    const auto escrow = ComputeStorageLeaseEscrow(params, units, replicas, periods);
    if (!escrow || *escrow > std::numeric_limits<std::uint64_t>::max() - params.payment_fee) return std::nullopt;
    return StorageLeaseCostQuote{.protocol_fee = params.payment_fee, .storage_escrow = *escrow,
        .total_system_debit = params.payment_fee + *escrow};
}
}
