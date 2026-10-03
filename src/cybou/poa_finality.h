// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_FINALITY_H
#define CYBOU_POA_FINALITY_H

#include <cybou/identity_crypto.h>
#include <cybou/hash256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct CybouBlock;

inline constexpr size_t POA_FINALITY_CERTIFICATE_SIZE{ 32 + 32 + 8 + 32 + 64 + 3309};

/** Single-operator hybrid signature authorizing one canonical block as final. */
struct PoaFinalityCertificate {
    cybou::Hash256 network_binding;
    cybou::Hash256 block_id;
    uint64_t height{0};
    cybou::Hash256 parent_block_id;
    IdentityHybridSignature signature;

    friend bool operator==(const PoaFinalityCertificate&, const PoaFinalityCertificate&) = default;
};

cybou::Hash256 ComputePoaFinalityDigest(const cybou::Hash256& network_binding, const cybou::Hash256& block_id,
    uint64_t height, const cybou::Hash256& parent_block_id);

bool VerifyPoaFinalityCertificate(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const cybou::Hash256& expected_network_binding, const cybou::Hash256& expected_block_id,
    uint64_t expected_height, const cybou::Hash256& expected_parent_block_id);

/** Verify the certificate against the exact canonical block it finalizes. */
bool VerifyPoaCertificateForBlock(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const cybou::Hash256& expected_network_binding, const CybouBlock& block);

std::optional<std::vector<unsigned char>> SerializePoaFinalityCertificate(
    const PoaFinalityCertificate& certificate);
std::optional<PoaFinalityCertificate> DeserializePoaFinalityCertificate(
    std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_POA_FINALITY_H
