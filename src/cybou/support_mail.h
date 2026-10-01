// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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

/**
 * Support mail (application rule, not consensus).
 *
 * The recipient is not explicitly encoded in the publication. A message to
 * the network authority's support name pays a higher RootPublication fee on
 * purpose: the sender adds undecryptable padding capsules until the fee
 * reaches SupportMailMinimumFee. This public fee and capsule pattern can act
 * as a statistical support-mail traffic fingerprint. The fee goes to the
 * network pools like any fee; the authority's client verifies it from the
 * finalized publication and marks messages that paid less.
 */
inline constexpr std::string_view SUPPORT_NAME_LABEL{"cybou"};
inline constexpr std::uint64_t SUPPORT_MAIL_FEE_MULTIPLIER{5};

/** About 5x a short message (5 KiB, one chunk). */
inline std::uint64_t SupportMailMinimumFee(const CybouProtocolParameters& params)
{
    return SUPPORT_MAIL_FEE_MULTIPLIER * (5 * params.root_publication_fee_per_started_kib + params.root_publication_fee_per_chunk);
}

/** The Identity that holds the genesis-granted support name, once claimed. */
inline std::optional<AccountId> SupportAccount(const CybouState& state)
{
    for (const auto& [recovery_id, allocation] : state.genesis_allocations) {
        if (allocation.label == SUPPORT_NAME_LABEL && allocation.claimed_by) return allocation.claimed_by;
    }
    return std::nullopt;
}

/**
 * Exact network fee of a publication: its canonical operation is the
 * two-byte operation header, the fixed-size authorization and the payload.
 */
inline std::optional<std::uint64_t> RootPublicationOperationFee(
    const CybouProtocolParameters& params, const RootPublication& publication)
{
    const auto payload = SerializeRootPublication(publication);
    if (!payload) return std::nullopt;
    return ComputeRootPublicationFee(params, 2 + IDENTITY_OPERATION_AUTH_SIZE + payload->size(), publication.chunk_count);
}

/**
 * Adds padding capsules (random bytes no key opens) until the publication
 * pays at least `minimum_fee`. False if the capsule limit is reached first.
 */
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
