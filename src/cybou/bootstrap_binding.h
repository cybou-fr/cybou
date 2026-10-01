// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_BINDING_H
#define CYBOU_BOOTSTRAP_BINDING_H

#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>
#include <cybou/recovery_phrase.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cybou {

inline constexpr size_t MAX_BOOTSTRAP_NETWORK_FILE_BYTES{16 * 1024 * 1024};
inline constexpr size_t MAX_BOOTSTRAP_DISPLAY_NAME_BYTES{128};

/** Signed public network binding. It contains no operator address or NodeID. */
struct BootstrapNetworkBinding {
    uint64_t generation{0};
    std::string display_name;
    uint256 network_id;
    std::array<unsigned char, 32> network_file_sha256{};
    std::vector<unsigned char> network_file;
    IdentityHybridSignature authority_signature;
};

std::optional<BootstrapNetworkBinding> CreateBootstrapNetworkBinding(
    uint64_t generation, std::string display_name, std::span<const unsigned char> exact_network_file,
    const RecoveryEntropy& poa_recovery_entropy);
bool VerifyBootstrapNetworkBinding(const BootstrapNetworkBinding& binding);
std::optional<std::vector<unsigned char>> EncodeBootstrapNetworkBinding(
    const BootstrapNetworkBinding& binding);
std::optional<BootstrapNetworkBinding> DecodeBootstrapNetworkBinding(
    std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_BINDING_H
