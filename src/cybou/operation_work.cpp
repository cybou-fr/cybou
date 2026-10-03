// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/operation_work.h>

#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>

#include <bit>
#include <random>
#include <string_view>

namespace cybou {

cybou::Hash256 ComputeOperationWorkHash(const cybou::Hash256& network_binding,
    const cybou::Hash256& operation_id, uint64_t nonce)
{
    static constexpr std::string_view DOMAIN{"CYBOU/OP-WORK"};
    unsigned char nonce_bytes[8];
    for (unsigned i{0}; i < 8; ++i) nonce_bytes[i] = static_cast<unsigned char>(nonce >> (8 * i));
    crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_binding.begin(), network_binding.size());
    hasher.Write(operation_id.begin(), operation_id.size());
    hasher.Write(nonce_bytes, sizeof(nonce_bytes));
    cybou::Hash256 hash;
    hasher.Finalize(hash.begin());
    return hash;
}

namespace {
unsigned LeadingZeroBits(const cybou::Hash256& hash)
{
    unsigned count{0};
    for (const unsigned char byte : hash) {
        if (byte != 0) return count + static_cast<unsigned>(std::countl_zero(byte));
        count += 8;
    }
    return count;
}
} // namespace

bool CheckOperationWork(const cybou::Hash256& network_binding, const cybou::Hash256& operation_id,
    uint64_t nonce, uint32_t required_bits)
{
    if (required_bits == 0) return true;
    if (required_bits > 256 || network_binding.IsNull() || operation_id.IsNull()) return false;
    return LeadingZeroBits(ComputeOperationWorkHash(network_binding, operation_id, nonce)) >= required_bits;
}

std::optional<uint64_t> SolveOperationWork(const cybou::Hash256& network_binding,
    const cybou::Hash256& operation_id, uint32_t required_bits, std::stop_token stop)
{
    if (required_bits == 0) return uint64_t{0};
    if (required_bits > 64 || network_binding.IsNull() || operation_id.IsNull()) return std::nullopt;
    // A random start keeps two submitters of the same operation from racing one sequence.
    uint64_t nonce = std::random_device{}();
    nonce = (nonce << 32) ^ std::random_device{}();
    for (uint64_t tries{0};; ++tries, ++nonce) {
        if ((tries & 0xffff) == 0 && stop.stop_requested()) return std::nullopt;
        if (CheckOperationWork(network_binding, operation_id, nonce, required_bits)) return nonce;
    }
}

uint32_t RequiredOperationWorkBits(const ProtocolOperation& operation, const CybouState& finalized)
{
    const auto account = AuthorizingAccount(operation);
    if (!account) return 0;
    const auto found = finalized.accounts.find(*account);
    const auto limits = ComputeAuthorityTierLimits(found == finalized.accounts.end() ? 0 : found->second.authority);
    const bool name = std::holds_alternative<AuthorizedNameCommit>(operation) ||
        std::holds_alternative<AuthorizedNameReveal>(operation);
    return limits.operation_work_bits + (name ? NAME_OPERATION_EXTRA_WORK_BITS : 0);
}

} // namespace cybou
