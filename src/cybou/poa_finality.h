// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_POA_FINALITY_H
#define CYBOU_POA_FINALITY_H

#include <cybou/identity_crypto.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct CybouBlock;

inline constexpr uint8_t POA_FINALITY_CERTIFICATE_VERSION{1};
inline constexpr size_t POA_FINALITY_CERTIFICATE_SIZE{1 + 32 + 32 + 8 + 32 + 64 + 3309};

/** Single-operator hybrid signature authorizing one canonical block as final. */
struct PoaFinalityCertificate {
    uint8_t version{POA_FINALITY_CERTIFICATE_VERSION};
    uint256 network_id;
    uint256 block_id;
    uint64_t height{0};
    uint256 parent_block_id;
    IdentityHybridSignature signature;

    friend bool operator==(const PoaFinalityCertificate&, const PoaFinalityCertificate&) = default;
};

uint256 ComputePoaFinalityDigest(const uint256& network_id, const uint256& block_id,
    uint64_t height, const uint256& parent_block_id);

bool VerifyPoaFinalityCertificate(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const uint256& expected_network_id, const uint256& expected_block_id,
    uint64_t expected_height, const uint256& expected_parent_block_id);

/** Verify the certificate against the exact canonical block it finalizes. */
bool VerifyPoaCertificateForBlock(const PoaFinalityCertificate& certificate,
    const IdentityHybridPublicKey& genesis_finalizer_key,
    const uint256& expected_network_id, const CybouBlock& block);

std::optional<std::vector<unsigned char>> SerializePoaFinalityCertificate(
    const PoaFinalityCertificate& certificate);
std::optional<PoaFinalityCertificate> DeserializePoaFinalityCertificate(
    std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_POA_FINALITY_H
