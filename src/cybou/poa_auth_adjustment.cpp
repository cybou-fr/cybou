// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_auth_adjustment.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {
namespace {

constexpr size_t BODY_SIZE{1 + 32 + 8 + 8};

void Write64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t Read64(std::span<const unsigned char> bytes)
{
    uint64_t value{0};
    for (unsigned i{0}; i < 8; ++i) value |= uint64_t{bytes[i]} << (8 * i);
    return value;
}

bool ValidAction(PoaAuthAction action)
{
    return action == PoaAuthAction::GRANT || action == PoaAuthAction::BURN;
}

std::optional<std::vector<unsigned char>> SerializeBody(const PoaAuthAdjustment& adjustment)
{
    if (!ValidAction(adjustment.action) || adjustment.target_account_id.IsNull() ||
        adjustment.amount == 0 || adjustment.block_height == 0) return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(POA_AUTH_ADJUSTMENT_SIZE);
    out.push_back(static_cast<unsigned char>(adjustment.action));
    out.insert(out.end(), adjustment.target_account_id.Value().begin(), adjustment.target_account_id.Value().end());
    Write64(out, adjustment.amount);
    Write64(out, adjustment.block_height);
    return out;
}

} // namespace

std::optional<std::array<unsigned char, 32>> ComputePoaAuthAdjustmentDigest(
    const uint256& network_id, const PoaAuthAdjustment& adjustment)
{
    constexpr std::string_view domain{"CYBOU/POA-AUTH-ADJUSTMENT/V1"};
    const auto body = SerializeBody(adjustment);
    if (network_id.IsNull() || !body) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain),
            std::span<const unsigned char>{network_id.begin(), 32}, std::span<const unsigned char>{*body}},
            digest.data())) return std::nullopt;
    return digest;
}

std::optional<std::vector<unsigned char>> SerializePoaAuthAdjustment(const PoaAuthAdjustment& adjustment)
{
    auto out = SerializeBody(adjustment);
    if (!out || adjustment.poa_signature.ml_dsa.size() != 3309) return std::nullopt;
    out->insert(out->end(), adjustment.poa_signature.ed25519.begin(), adjustment.poa_signature.ed25519.end());
    out->insert(out->end(), adjustment.poa_signature.ml_dsa.begin(), adjustment.poa_signature.ml_dsa.end());
    return out;
}

std::optional<PoaAuthAdjustment> DeserializePoaAuthAdjustment(std::span<const unsigned char> bytes)
{
    if (bytes.size() != POA_AUTH_ADJUSTMENT_SIZE) return std::nullopt;
    const auto target = AccountId::FromBytes(bytes.subspan(1, 32));
    if (!target) return std::nullopt;
    PoaAuthAdjustment adjustment{
        .action = static_cast<PoaAuthAction>(bytes[0]),
        .target_account_id = *target,
        .amount = Read64(bytes.subspan(33, 8)),
        .block_height = Read64(bytes.subspan(41, 8)),
    };
    std::copy_n(bytes.begin() + BODY_SIZE, 64, adjustment.poa_signature.ed25519.begin());
    adjustment.poa_signature.ml_dsa.assign(bytes.begin() + BODY_SIZE + 64, bytes.end());
    if (!SerializeBody(adjustment)) return std::nullopt;
    return adjustment;
}

PoaAuthAdjustmentError ApplyPoaAuthAdjustment(const PoaAuthAdjustment& adjustment,
    const uint256& network_id, uint64_t block_height,
    const IdentityHybridPublicKey& poa_key, CybouState& state)
{
    const auto digest = ComputePoaAuthAdjustmentDigest(network_id, adjustment);
    if (!digest) return PoaAuthAdjustmentError::INVALID_PAYLOAD;
    if (adjustment.block_height != block_height) return PoaAuthAdjustmentError::WRONG_HEIGHT;
    if (poa_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        !VerifyIdentityMessage(poa_key, adjustment.poa_signature, *digest)) {
        return PoaAuthAdjustmentError::INVALID_SIGNATURE;
    }
    const auto account = state.accounts.find(adjustment.target_account_id);
    if (account == state.accounts.end()) return PoaAuthAdjustmentError::TARGET_NOT_FOUND;
    auto& authority = account->second.authority;
    if (adjustment.action == PoaAuthAction::GRANT) {
        if (authority > std::numeric_limits<uint64_t>::max() - adjustment.amount) {
            return PoaAuthAdjustmentError::AUTHORITY_OVERFLOW;
        }
        authority += adjustment.amount;
    } else {
        authority -= std::min(authority, adjustment.amount);
    }
    return PoaAuthAdjustmentError::NONE;
}

} // namespace cybou
