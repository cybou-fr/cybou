// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/operation_work.h>

#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>

#include <openssl/evp.h>

#include <bit>
#include <memory>
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

struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept { EVP_MD_CTX_free(ctx); }
};
using UniqueEvpMdCtx = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

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

    static constexpr std::string_view DOMAIN{"CYBOU/OP-WORK"};

    UniqueEvpMdCtx base_ctx{EVP_MD_CTX_new()};
    UniqueEvpMdCtx work_ctx{EVP_MD_CTX_new()};
    if (!base_ctx || !work_ctx) return std::nullopt;

    if (EVP_DigestInit_ex2(base_ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(base_ctx.get(), reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size()) != 1 ||
        EVP_DigestUpdate(base_ctx.get(), network_binding.begin(), network_binding.size()) != 1 ||
        EVP_DigestUpdate(base_ctx.get(), operation_id.begin(), operation_id.size()) != 1) {
        return std::nullopt;
    }

    const uint32_t full_zero_bytes = required_bits / 8;
    const uint32_t rem_bits = required_bits % 8;
    const unsigned char rem_mask = rem_bits == 0 ? 0 : static_cast<unsigned char>(0xff << (8 - rem_bits));

    // A random start keeps two submitters of the same operation from racing one sequence.
    uint64_t nonce = std::random_device{}();
    nonce = (nonce << 32) ^ std::random_device{}();

    unsigned char nonce_bytes[8];
    unsigned char hash[32];

    for (uint64_t tries{0};; ++tries, ++nonce) {
        if ((tries & 0xffff) == 0 && stop.stop_requested()) return std::nullopt;

        for (unsigned i{0}; i < 8; ++i) {
            nonce_bytes[i] = static_cast<unsigned char>(nonce >> (8 * i));
        }

        if (EVP_MD_CTX_copy_ex(work_ctx.get(), base_ctx.get()) != 1) return std::nullopt;
        if (EVP_DigestUpdate(work_ctx.get(), nonce_bytes, sizeof(nonce_bytes)) != 1) return std::nullopt;
        unsigned int output_len{0};
        if (EVP_DigestFinal_ex(work_ctx.get(), hash, &output_len) != 1 || output_len != 32) return std::nullopt;

        bool match{true};
        for (uint32_t b{0}; b < full_zero_bytes; ++b) {
            if (hash[b] != 0) {
                match = false;
                break;
            }
        }
        if (match && rem_mask != 0 && (hash[full_zero_bytes] & rem_mask) != 0) {
            match = false;
        }
        if (match) {
            return nonce;
        }
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
