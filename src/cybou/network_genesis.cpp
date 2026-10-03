// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_genesis.h>
#include <cybou/signing.h>
#include <cybou/root_publication.h>
#include <limits>
#include <cybou/official_networks.h>
#include <cybou/crypto/sha256.h>
#include <cybou/root_publication.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace cybou {

bool ValidateProtocolParameters(const CybouProtocolParameters& params)
{
    if (params.account_creation_work_bits > cybou::Hash256::size() * 8) {
        return false;
    }
    if (params.max_account_creates_per_block == 0) {
        return false;
    }
    if (params.epoch_blocks == 0) {
        return false;
    }
    const auto max_fee_kib = (ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1023) / 1024;
    const auto per_kib = params.root_publication_fee_per_started_kib;
    const auto per_chunk = params.root_publication_fee_per_chunk;
    if ((per_kib != 0 && max_fee_kib > std::numeric_limits<uint64_t>::max() / per_kib) ||
        (per_chunk != 0 && MAX_PUBLICATION_CHUNKS > std::numeric_limits<uint64_t>::max() / per_chunk)) {
        return false;
    }
    const auto max_byte_fee = static_cast<uint64_t>(max_fee_kib) * per_kib;
    const auto max_chunk_fee = static_cast<uint64_t>(MAX_PUBLICATION_CHUNKS) * per_chunk;
    if (max_chunk_fee > std::numeric_limits<uint64_t>::max() - max_byte_fee) {
        return false;
    }
    if (params.name_claim_work_bits > cybou::Hash256::size() * 8 ||
        params.name_commit_min_depth == 0 ||
        params.name_commit_max_lifetime < params.name_commit_min_depth ||
        params.max_pending_name_commits == 0 ||
        params.max_pending_name_commits > DEFAULT_MAX_PENDING_NAME_COMMITS) {
        return false;
    }
    return true;
}

cybou::Hash256 ComputeNetworkBinding(const IdentityHybridPublicKey& network_public_key)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NETWORK-ID"};
    const auto key = CanonicalSerializeNetworkPublicKey(network_public_key);
    cybou::Hash256 result;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(key.data(), key.size());
    hasher.Finalize(result.begin());
    return result;
}

CybouState CreateDevGenesisState()
{
    return CybouState{
        .onboarding_pool = DEV_ONBOARDING_POOL,
        .accounts = {},
        .identities = {},
        .names = {},
    };
}

namespace {

void WriteU8(std::vector<unsigned char>& out, uint8_t value) {
    out.push_back(value);
}

void WriteU32LE(std::vector<unsigned char>& out, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
    }
}

void WriteU64LE(std::vector<unsigned char>& out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
    }
}

void WriteHash(std::vector<unsigned char>& out, const cybou::Hash256& hash) {
    out.insert(out.end(), hash.begin(), hash.end());
}

} // namespace

std::vector<unsigned char> CanonicalSerializeHybridPublicKey(const IdentityHybridPublicKey& key)
{
    std::vector<unsigned char> out;
    out.reserve(1 + ED25519_PUBLIC_KEY_SIZE + 4 + key.ml_dsa.size());
    WriteU8(out, static_cast<uint8_t>(key.purpose));
    out.insert(out.end(), key.ed25519.begin(), key.ed25519.end());
    WriteU32LE(out, static_cast<uint32_t>(key.ml_dsa.size()));
    out.insert(out.end(), key.ml_dsa.begin(), key.ml_dsa.end());
    return out;
}

std::optional<IdentityHybridPublicKey> CanonicalDeserializeHybridPublicKey(
    std::span<const unsigned char> bytes,
    std::optional<IdentityKeyPurpose> expected_purpose)
{
    if (bytes.size() < 1 + ED25519_PUBLIC_KEY_SIZE + 4) return std::nullopt;
    IdentityHybridPublicKey key;
    key.purpose = static_cast<IdentityKeyPurpose>(bytes[0]);
    if (expected_purpose && key.purpose != *expected_purpose) return std::nullopt;

    size_t expected_ml_dsa_size = 0;
    switch (key.purpose) {
    case IdentityKeyPurpose::AUTHORIZATION:
    case IdentityKeyPurpose::STORAGE:
        expected_ml_dsa_size = MLDSA44_PUBLIC_KEY_SIZE;
        break;
    case IdentityKeyPurpose::RECOVERY_ROOT:
    case IdentityKeyPurpose::POA_FINALIZER:
    case IdentityKeyPurpose::NETWORK_ROOT:
        expected_ml_dsa_size = MLDSA65_PUBLIC_KEY_SIZE;
        break;
    default:
        return std::nullopt;
    }

    size_t pos = 1;
    std::copy_n(bytes.begin() + pos, ED25519_PUBLIC_KEY_SIZE, key.ed25519.begin());
    pos += ED25519_PUBLIC_KEY_SIZE;

    uint32_t ml_dsa_len = static_cast<uint32_t>(bytes[pos]) |
                          (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
                          (static_cast<uint32_t>(bytes[pos + 2]) << 16) |
                          (static_cast<uint32_t>(bytes[pos + 3]) << 24);
    pos += 4;
    if (pos + ml_dsa_len != bytes.size()) return std::nullopt;
    if (ml_dsa_len != expected_ml_dsa_size) return std::nullopt;
    key.ml_dsa.assign(bytes.begin() + pos, bytes.end());
    return key;
}

std::vector<unsigned char> CanonicalSerializeNetworkPublicKey(const IdentityHybridPublicKey& key)
{
    return CanonicalSerializeHybridPublicKey(key);
}

std::optional<IdentityHybridPublicKey> CanonicalDeserializeNetworkPublicKey(std::span<const unsigned char> bytes)
{
    return CanonicalDeserializeHybridPublicKey(bytes, IdentityKeyPurpose::NETWORK_ROOT);
}

std::vector<unsigned char> SerializeNetworkGenesisPayload(const NetworkGenesis& genesis)
{
    std::vector<unsigned char> out;

    const auto net_key_bytes = CanonicalSerializeNetworkPublicKey(genesis.network_public_key);
    WriteU32LE(out, static_cast<uint32_t>(net_key_bytes.size()));
    out.insert(out.end(), net_key_bytes.begin(), net_key_bytes.end());

    WriteHash(out, genesis.genesis_state_root);

    WriteU8(out, static_cast<uint8_t>(genesis.poa_finalizer_public_key.purpose));
    out.insert(out.end(), genesis.poa_finalizer_public_key.ed25519.begin(), genesis.poa_finalizer_public_key.ed25519.end());
    WriteU32LE(out, static_cast<uint32_t>(genesis.poa_finalizer_public_key.ml_dsa.size()));
    out.insert(out.end(), genesis.poa_finalizer_public_key.ml_dsa.begin(), genesis.poa_finalizer_public_key.ml_dsa.end());

    // Protocol parameters
    const auto& p = genesis.protocol_parameters;
    WriteU32LE(out, p.account_creation_work_bits);
    WriteU64LE(out, p.account_creation_epoch_lag);
    WriteU32LE(out, p.max_account_creates_per_block);
    WriteU64LE(out, p.onboarding_bonus);
    WriteU64LE(out, p.epoch_blocks);
    WriteU64LE(out, p.payment_fee);
    WriteU64LE(out, p.root_publication_fee_per_started_kib);
    WriteU64LE(out, p.root_publication_fee_per_chunk);
    WriteU32LE(out, p.name_claim_work_bits);
    WriteU64LE(out, p.name_commit_min_depth);
    WriteU64LE(out, p.name_commit_max_lifetime);
    WriteU32LE(out, p.max_pending_name_commits);
    WriteU8(out, p.identity_kem_xwing_enabled ? 1 : 0);

    return out;
}

cybou::Hash256 ComputeNetworkGenesisDigest(const NetworkGenesis& genesis)
{
    const auto payload = SerializeNetworkGenesisPayload(genesis);
    cybou::Hash256 digest;
    crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(CYBOU_GENESIS_SIGNING_DOMAIN.data()),
                 CYBOU_GENESIS_SIGNING_DOMAIN.size());
    hasher.Write(payload.data(), payload.size());
    hasher.Finalize(digest.begin());
    return digest;
}

std::vector<unsigned char> SerializeSignedNetworkGenesis(const NetworkGenesis& genesis)
{
    auto out = SerializeNetworkGenesisPayload(genesis);

    // Signature
    out.insert(out.end(), genesis.signature.ed25519.begin(), genesis.signature.ed25519.end());
    WriteU32LE(out, static_cast<uint32_t>(genesis.signature.ml_dsa.size()));
    out.insert(out.end(), genesis.signature.ml_dsa.begin(), genesis.signature.ml_dsa.end());

    return out;
}

std::optional<NetworkGenesis> DeserializeSignedNetworkGenesis(std::span<const unsigned char> bytes)
{
    size_t pos{0};
    const auto read_u8 = [&]() -> std::optional<uint8_t> {
        if (pos >= bytes.size()) return std::nullopt;
        return bytes[pos++];
    };
    const auto read_u32le = [&]() -> std::optional<uint32_t> {
        if (pos + 4 > bytes.size()) return std::nullopt;
        uint32_t v = static_cast<uint32_t>(bytes[pos]) |
                    (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
                    (static_cast<uint32_t>(bytes[pos + 2]) << 16) |
                    (static_cast<uint32_t>(bytes[pos + 3]) << 24);
        pos += 4;
        return v;
    };
    const auto read_u64le = [&]() -> std::optional<uint64_t> {
        if (pos + 8 > bytes.size()) return std::nullopt;
        uint64_t v = 0;
        for (unsigned i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(bytes[pos + i]) << (8 * i);
        }
        pos += 8;
        return v;
    };
    const auto read_hash = [&]() -> std::optional<cybou::Hash256> {
        if (pos + cybou::Hash256::size() > bytes.size()) return std::nullopt;
        cybou::Hash256 val;
        std::copy_n(bytes.begin() + pos, cybou::Hash256::size(), val.begin());
        pos += cybou::Hash256::size();
        return val;
    };


    const auto net_key_len = read_u32le();
    if (!net_key_len || pos + *net_key_len > bytes.size()) return std::nullopt;
    const auto net_key = CanonicalDeserializeNetworkPublicKey(bytes.subspan(pos, *net_key_len));
    if (!net_key) return std::nullopt;
    pos += *net_key_len;

    const auto state_root = read_hash();
    if (!state_root) return std::nullopt;

    const auto poa_purpose = read_u8();
    if (!poa_purpose || pos + ED25519_PUBLIC_KEY_SIZE + 4 > bytes.size()) return std::nullopt;
    IdentityHybridPublicKey poa_key;
    poa_key.purpose = static_cast<IdentityKeyPurpose>(*poa_purpose);
    std::copy_n(bytes.begin() + pos, ED25519_PUBLIC_KEY_SIZE, poa_key.ed25519.begin());
    pos += ED25519_PUBLIC_KEY_SIZE;
    const auto poa_ml_dsa_len = read_u32le();
    if (!poa_ml_dsa_len || pos + *poa_ml_dsa_len > bytes.size()) return std::nullopt;
    poa_key.ml_dsa.assign(bytes.begin() + pos, bytes.begin() + pos + *poa_ml_dsa_len);
    pos += *poa_ml_dsa_len;

    // Protocol parameters
    CybouProtocolParameters p;
    const auto work_bits = read_u32le();
    const auto epoch_lag = read_u64le();
    const auto max_creates = read_u32le();
    const auto onboarding_bonus = read_u64le();
    const auto epoch_blocks = read_u64le();
    const auto payment_fee = read_u64le();
    const auto per_kib = read_u64le();
    const auto per_chunk = read_u64le();
    const auto name_work = read_u32le();
    const auto name_min_depth = read_u64le();
    const auto name_max_life = read_u64le();
    const auto max_pending_names = read_u32le();
    const auto kem_xwing = read_u8();
    if (!work_bits || !epoch_lag || !max_creates || !onboarding_bonus || !epoch_blocks ||
        !payment_fee || !per_kib || !per_chunk || !name_work || !name_min_depth ||
        !name_max_life || !max_pending_names || !kem_xwing || *kem_xwing > 1) return std::nullopt;

    p.account_creation_work_bits = *work_bits;
    p.account_creation_epoch_lag = *epoch_lag;
    p.max_account_creates_per_block = *max_creates;
    p.onboarding_bonus = *onboarding_bonus;
    p.epoch_blocks = *epoch_blocks;
    p.payment_fee = *payment_fee;
    p.root_publication_fee_per_started_kib = *per_kib;
    p.root_publication_fee_per_chunk = *per_chunk;
    p.name_claim_work_bits = *name_work;
    p.name_commit_min_depth = *name_min_depth;
    p.name_commit_max_lifetime = *name_max_life;
    p.max_pending_name_commits = *max_pending_names;
    p.identity_kem_xwing_enabled = (*kem_xwing == 1);

    if (pos + 64 + 4 > bytes.size()) return std::nullopt;
    IdentityHybridSignature sig;
    std::copy_n(bytes.begin() + pos, 64, sig.ed25519.begin());
    pos += 64;
    const auto sig_ml_dsa_len = read_u32le();
    if (!sig_ml_dsa_len || pos + *sig_ml_dsa_len != bytes.size()) return std::nullopt;
    sig.ml_dsa.assign(bytes.begin() + pos, bytes.end());

    NetworkGenesis result;
    result.network_public_key = *net_key;
    result.genesis_state_root = *state_root;
    result.poa_finalizer_public_key = poa_key;
    result.protocol_parameters = p;
    result.signature = std::move(sig);

    return result;
}

NetworkGenesisError VerifySignedNetworkGenesis(const NetworkGenesis& genesis)
{
    if (genesis.network_public_key.purpose != IdentityKeyPurpose::NETWORK_ROOT ||
        genesis.network_public_key.ml_dsa.size() != MLDSA65_PUBLIC_KEY_SIZE ||
        std::all_of(genesis.network_public_key.ed25519.begin(), genesis.network_public_key.ed25519.end(), [](unsigned char b){ return b == 0; })) {
        return NetworkGenesisError::INVALID_NETWORK_KEY;
    }
    if (genesis.genesis_state_root.IsNull()) {
        return NetworkGenesisError::NULL_GENESIS_STATE_ROOT;
    }
    if (genesis.poa_finalizer_public_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        genesis.poa_finalizer_public_key.ml_dsa.size() != MLDSA65_PUBLIC_KEY_SIZE ||
        std::all_of(genesis.poa_finalizer_public_key.ed25519.begin(), genesis.poa_finalizer_public_key.ed25519.end(), [](unsigned char b){ return b == 0; })) {
        return NetworkGenesisError::INVALID_POA_KEY;
    }

    if (!ValidateProtocolParameters(genesis.protocol_parameters)) {
        return NetworkGenesisError::INVALID_PROTOCOL_PARAMETERS;
    }

    // Cryptographic signature check under Network Key
    const auto digest = ComputeNetworkGenesisDigest(genesis);
    if (!VerifyIdentityMessage(genesis.network_public_key, genesis.signature,
                              std::span<const unsigned char>{digest.begin(), digest.size()})) {
        return NetworkGenesisError::INVALID_SIGNATURE;
    }

    return NetworkGenesisError::NONE;
}

VerifiedNetworkGenesis::VerifiedNetworkGenesis(NetworkGenesis genesis, std::vector<unsigned char> network_id_bytes, cybou::Hash256 genesis_digest)
    : m_genesis(std::move(genesis)), m_network_id_bytes(std::move(network_id_bytes)), m_genesis_digest(genesis_digest)
{
}

std::optional<VerifiedNetworkGenesis> VerifiedNetworkGenesis::Create(NetworkGenesis genesis)
{
    if (VerifySignedNetworkGenesis(genesis) != NetworkGenesisError::NONE) {
        return std::nullopt;
    }
    auto id_bytes = CanonicalSerializeNetworkPublicKey(genesis.network_public_key);
    const auto digest = ComputeNetworkGenesisDigest(genesis);
    return VerifiedNetworkGenesis(std::move(genesis), std::move(id_bytes), digest);
}


} // namespace cybou
