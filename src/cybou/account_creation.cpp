// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <bit>
#include <string_view>

namespace cybou {

std::vector<unsigned char> SerializeAccountCreationWork(const AccountCreationWorkV1& work)
{
    std::vector<unsigned char> out;
    out.reserve(ACCOUNT_CREATION_WORK_SERIALIZED_SIZE);
    const auto append_hash = [&out](const uint256& hash) {
        out.insert(out.end(), hash.data(), hash.data() + uint256::size());
    };
    const auto append_u64le = [&out](const uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };

    out.push_back(work.version);
    append_hash(work.network_id);
    append_hash(work.account_id.Value());
    append_hash(work.initial_authorization_commitment);
    append_u64le(work.work_epoch);
    append_u64le(work.nonce);
    return out;
}

std::optional<AccountCreationWorkV1> DeserializeAccountCreationWork(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != ACCOUNT_CREATION_WORK_SERIALIZED_SIZE) return std::nullopt;
    size_t offset{0};
    const auto read_u8 = [&]() -> uint8_t { return bytes[offset++]; };
    const auto read_hash = [&]() -> uint256 {
        uint256 value;
        std::copy_n(bytes.begin() + offset, uint256::size(), value.begin());
        offset += uint256::size();
        return value;
    };
    const auto read_u64le = [&]() -> uint64_t {
        uint64_t value{0};
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[offset++]} << (8 * i);
        return value;
    };

    AccountCreationWorkV1 work;
    work.version = read_u8();
    if (work.version != ACCOUNT_CREATION_WORK_VERSION) return std::nullopt;
    work.network_id = read_hash();
    work.account_id = AccountId{read_hash()};
    work.initial_authorization_commitment = read_hash();
    work.work_epoch = read_u64le();
    work.nonce = read_u64le();
    return work;
}

uint256 ComputeAccountCreationWorkHash(const AccountCreationWorkV1& work)
{
    static constexpr std::string_view DOMAIN{"CYBOU/ACCOUNT-CREATE-WORK/V1"};
    const auto bytes{SerializeAccountCreationWork(work)};
    uint256 result;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

unsigned int CountLeadingZeroBits(const uint256& hash)
{
    unsigned int bits{0};
    for (size_t i = 0; i < uint256::size(); ++i) {
        const uint8_t byte{hash.data()[i]};
        if (byte == 0) {
            bits += 8;
        } else {
            bits += static_cast<unsigned int>(std::countl_zero(byte));
            break;
        }
    }
    return bits;
}

bool CheckAccountCreationWork(const AccountCreationWorkV1& work, const unsigned int required_leading_zero_bits)
{
    if (required_leading_zero_bits == 0) return true;
    const uint256 hash{ComputeAccountCreationWorkHash(work)};
    return CountLeadingZeroBits(hash) >= required_leading_zero_bits;
}

std::vector<unsigned char> SerializeAccountCreateOp(const AccountCreateOpV1& op)
{
    std::vector<unsigned char> out;
    out.reserve(1 + uint256::size() + uint256::size() + ACCOUNT_CREATION_WORK_SERIALIZED_SIZE);
    const auto append_hash = [&out](const uint256& hash) {
        out.insert(out.end(), hash.data(), hash.data() + uint256::size());
    };

    out.push_back(op.version);
    append_hash(op.account_id.Value());
    append_hash(op.initial_authorization.authorization_descriptor);
    const auto work_bytes{SerializeAccountCreationWork(op.creation_work)};
    out.insert(out.end(), work_bytes.begin(), work_bytes.end());
    return out;
}

std::optional<AccountCreateOpV1> DeserializeAccountCreateOp(const std::span<const unsigned char> bytes)
{
    static constexpr size_t EXPECTED_SIZE{1 + uint256::size() + uint256::size() + ACCOUNT_CREATION_WORK_SERIALIZED_SIZE};
    if (bytes.size() != EXPECTED_SIZE) return std::nullopt;
    size_t offset{0};
    const auto read_u8 = [&]() -> uint8_t { return bytes[offset++]; };
    const auto read_hash = [&]() -> uint256 {
        uint256 value;
        std::copy_n(bytes.begin() + offset, uint256::size(), value.begin());
        offset += uint256::size();
        return value;
    };

    AccountCreateOpV1 op;
    op.version = read_u8();
    if (op.version != ACCOUNT_CREATE_OP_VERSION) return std::nullopt;
    op.account_id = AccountId{read_hash()};
    op.initial_authorization.authorization_descriptor = read_hash();
    const auto work{DeserializeAccountCreationWork(bytes.subspan(offset))};
    if (!work) return std::nullopt;
    op.creation_work = *work;
    return op;
}

AccountCreateValidationError ValidateAccountCreateOp(
    const AccountCreateOpV1& op,
    const uint256& expected_network_id,
    const uint64_t current_epoch,
    const CybouProtocolParameters& params)
{
    if (op.version != ACCOUNT_CREATE_OP_VERSION) return AccountCreateValidationError::UNSUPPORTED_VERSION;
    if (op.account_id.IsNull()) return AccountCreateValidationError::NULL_ACCOUNT_ID;
    if (op.creation_work.network_id.IsNull()) return AccountCreateValidationError::NULL_NETWORK_ID;
    if (op.creation_work.network_id != expected_network_id) return AccountCreateValidationError::NETWORK_MISMATCH;
    if (op.creation_work.account_id != op.account_id) return AccountCreateValidationError::ACCOUNT_ID_MISMATCH;
    if (op.creation_work.initial_authorization_commitment != ComputeAuthCommitment(op.initial_authorization)) {
        return AccountCreateValidationError::AUTH_COMMITMENT_MISMATCH;
    }
    if (op.creation_work.work_epoch > current_epoch) {
        return AccountCreateValidationError::FUTURE_WORK_EPOCH;
    }
    if (current_epoch - op.creation_work.work_epoch > params.account_creation_epoch_lag) {
        return AccountCreateValidationError::EXPIRED_WORK_EPOCH;
    }
    if (!CheckAccountCreationWork(op.creation_work, params.account_creation_work_bits)) {
        return AccountCreateValidationError::INSUFFICIENT_WORK;
    }
    return AccountCreateValidationError::NONE;
}

} // namespace cybou
