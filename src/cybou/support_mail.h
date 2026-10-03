// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Правила поддержки support mail поверх обычной RootPublication.

#ifndef CYBOU_SUPPORT_MAIL_H
#define CYBOU_SUPPORT_MAIL_H

#include <cybou/identity_registry.h>
#include <cybou/root_publication.h>
#include <cybou/state.h>

#include <openssl/rand.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace cybou {

/// Метка имени поддержки сети.
inline constexpr std::string_view SUPPORT_NAME_LABEL{CENTRAL_AUTHORITY_NAME};
/// Во сколько раз минимальная комиссия support mail выше базовой публикации.
inline constexpr std::uint64_t SUPPORT_MAIL_FEE_MULTIPLIER{5};

/// Вычисляет минимальную комиссию support mail.
inline std::uint64_t SupportMailMinimumFee(const CybouProtocolParameters& params)
{
    return SUPPORT_MAIL_FEE_MULTIPLIER * (5 * params.root_publication_fee_per_started_kib + params.root_publication_fee_per_chunk);
}

/// Возвращает AccountID, владеющий support-именем после его claim.
inline std::optional<AccountId> SupportAccount(const CybouState& state)
{
    const auto* allocation = FindCentralAuthorityAllocation(state);
    return allocation ? allocation->claimed_by : std::nullopt;
}

/// Вычисляет точную сетевую комиссию RootPublication.
inline std::optional<std::uint64_t> RootPublicationOperationFee(
    const CybouProtocolParameters& params, const RootPublication& publication)
{
    const auto payload = SerializeRootPublication(publication);
    if (!payload) return std::nullopt;
    return ComputeRootPublicationFee(params, 2 + IDENTITY_OPERATION_AUTH_SIZE + payload->size(), publication.chunk_count);
}

/// Добавляет padding-capsules, пока публикация не достигнет `minimum_fee`.
inline bool PadPublicationToFee(const CybouProtocolParameters& params, RootPublication& publication,
    std::uint64_t minimum_fee)
{
    while (true) {
        const auto fee = RootPublicationOperationFee(params, publication);
        if (!fee) return false;
        if (*fee >= minimum_fee) return true;
        if (publication.recipient_capsules.size() >= ROOT_PUBLICATION_MAX_CAPSULES) return false;
        RootRecipientCapsule padding;
        if (RAND_bytes(padding.encapsulation.data(), static_cast<int>(padding.encapsulation.size())) != 1 ||
            RAND_bytes(padding.wrapped_content_key.data(), static_cast<int>(padding.wrapped_content_key.size())) != 1) {
            return false;
        }
        padding.key_epoch = publication.recipient_capsules.empty() ? 0 : publication.recipient_capsules.front().key_epoch;
        publication.recipient_capsules.push_back(padding);
    }
}

} // namespace cybou

#endif // CYBOU_SUPPORT_MAIL_H
