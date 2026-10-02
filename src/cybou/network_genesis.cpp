// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_genesis.h>
#include <cybou/crypto/sha256.h>
#include <cybou/root_publication.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace cybou {

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

void WriteHash(std::vector<unsigned char>& out, const uint256& hash) {
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
    case IdentityKeyPurpose::STORAGE_PROVIDER:
        expected_ml_dsa_size = MLDSA44_PUBLIC_KEY_SIZE;
        break;
    case IdentityKeyPurpose::RECOVERY_ROOT:
    case IdentityKeyPurpose::RELEASE_SIGNING:
    case IdentityKeyPurpose::TREASURY:
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
    WriteU8(out, genesis.version);

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

    // Initial Authority Assignments
    WriteU32LE(out, static_cast<uint32_t>(genesis.initial_authority.size()));
    for (const auto& a : genesis.initial_authority) {
        out.insert(out.end(), a.recovery_key_id.begin(), a.recovery_key_id.end());
        WriteU64LE(out, a.initial_authority);
    }

    return out;
}

uint256 ComputeNetworkGenesisDigest(const NetworkGenesis& genesis)
{
    const auto payload = SerializeNetworkGenesisPayload(genesis);
    uint256 digest;
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
    const auto read_hash = [&]() -> std::optional<uint256> {
        if (pos + uint256::size() > bytes.size()) return std::nullopt;
        uint256 val;
        std::copy_n(bytes.begin() + pos, uint256::size(), val.begin());
        pos += uint256::size();
        return val;
    };

    const auto version = read_u8();
    if (!version || *version != CYBOU_NETWORK_GENESIS_VERSION) return std::nullopt;

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
        !name_max_life || !max_pending_names || !kem_xwing) return std::nullopt;

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

    const auto auth_count = read_u32le();
    if (!auth_count || *auth_count > 128) return std::nullopt;
    std::vector<InitialAuthorityAssignment> initial_authority;
    initial_authority.reserve(*auth_count);
    for (uint32_t i = 0; i < *auth_count; ++i) {
        const auto key_id = read_hash();
        const auto val = read_u64le();
        if (!key_id || !val) return std::nullopt;
        IdentityKeyId id{};
        std::copy(key_id->begin(), key_id->end(), id.begin());
        initial_authority.push_back(InitialAuthorityAssignment{.recovery_key_id = id, .initial_authority = *val});
    }

    if (pos + 64 + 4 > bytes.size()) return std::nullopt;
    IdentityHybridSignature sig;
    std::copy_n(bytes.begin() + pos, 64, sig.ed25519.begin());
    pos += 64;
    const auto sig_ml_dsa_len = read_u32le();
    if (!sig_ml_dsa_len || pos + *sig_ml_dsa_len != bytes.size()) return std::nullopt;
    sig.ml_dsa.assign(bytes.begin() + pos, bytes.end());

    NetworkGenesis result;
    result.version = *version;
    result.network_public_key = *net_key;
    result.genesis_state_root = *state_root;
    result.poa_finalizer_public_key = poa_key;
    result.protocol_parameters = p;
    result.initial_authority = std::move(initial_authority);
    result.signature = std::move(sig);

    return result;
}

NetworkGenesisError VerifySignedNetworkGenesis(const NetworkGenesis& genesis)
{
    if (genesis.version != CYBOU_NETWORK_GENESIS_VERSION) {
        return NetworkGenesisError::UNSUPPORTED_VERSION;
    }
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

    for (size_t i = 0; i < genesis.initial_authority.size(); ++i) {
        const auto& entry = genesis.initial_authority[i];
        if (entry.initial_authority == 0 ||
            std::all_of(entry.recovery_key_id.begin(), entry.recovery_key_id.end(), [](unsigned char b){ return b == 0; })) {
            return NetworkGenesisError::INVALID_INITIAL_AUTHORITY;
        }
        if (i > 0 && entry.recovery_key_id <= genesis.initial_authority[i - 1].recovery_key_id) {
            return NetworkGenesisError::INVALID_INITIAL_AUTHORITY;
        }
    }

    // Cryptographic signature check under Network Key
    const auto digest = ComputeNetworkGenesisDigest(genesis);
    if (!VerifyIdentityMessage(genesis.network_public_key, genesis.signature,
                              std::span<const unsigned char>{digest.begin(), digest.size()})) {
        return NetworkGenesisError::INVALID_SIGNATURE;
    }

    return NetworkGenesisError::NONE;
}

VerifiedNetworkGenesis::VerifiedNetworkGenesis(NetworkGenesis genesis, std::vector<unsigned char> network_id_bytes, uint256 genesis_digest)
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

VerifiedNetworkGenesis CreateTestVerifiedGenesis(
    const CybouNetworkDefinition& definition,
    const std::vector<InitialAuthorityAssignment>& initial_auth)
{
    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x33);
    auto net_pub = DeriveIdentityPublicKey(net_secret, IdentityKeyPurpose::NETWORK_ROOT);
    NetworkGenesis spec;
    spec.version = CYBOU_NETWORK_GENESIS_VERSION;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = definition.genesis_state_root;
    spec.poa_finalizer_public_key = definition.poa_finalizer_public_key;
    spec.protocol_parameters = definition.protocol_parameters;
    spec.initial_authority = initial_auth;

    const auto digest = ComputeNetworkGenesisDigest(spec);
    auto sig = SignIdentityMessage(net_secret, IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    spec.signature = *sig;
    auto verified = VerifiedNetworkGenesis::Create(spec);
    return *verified;
}

std::optional<std::vector<unsigned char>> SerializeNetworkGenesisBundle(
    const NetworkGenesis& genesis,
    const CybouState& genesis_state)
{
    if (VerifySignedNetworkGenesis(genesis) != NetworkGenesisError::NONE) {
        return std::nullopt;
    }
    if (ValidateCybouState(genesis_state) != StateValidationError::NONE) {
        return std::nullopt;
    }
    const auto computed_root = CybouStateHash(genesis_state);
    if (!computed_root || *computed_root != genesis.genesis_state_root) {
        return std::nullopt;
    }
    for (const auto& entry : genesis.initial_authority) {
        if (!genesis_state.genesis_allocations.contains(entry.recovery_key_id)) {
            return std::nullopt;
        }
    }

    const auto signed_bytes = SerializeSignedNetworkGenesis(genesis);
    const auto state_bytes = SerializeCybouState(genesis_state);
    if (!state_bytes) return std::nullopt;

    if (signed_bytes.size() > 4 * 1024 * 1024 || state_bytes->size() > 16 * 1024 * 1024) {
        return std::nullopt;
    }

    std::vector<unsigned char> out;
    out.reserve(4 + 4 + signed_bytes.size() + 4 + state_bytes->size());
    out.insert(out.end(), {'C', 'Y', 'G', '1'});

    WriteU32LE(out, static_cast<uint32_t>(signed_bytes.size()));
    out.insert(out.end(), signed_bytes.begin(), signed_bytes.end());
    WriteU32LE(out, static_cast<uint32_t>(state_bytes->size()));
    out.insert(out.end(), state_bytes->begin(), state_bytes->end());

    return out;
}

std::optional<VerifiedNetworkBundle> VerifyNetworkGenesisBundle(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 12 || bytes.size() > 20 * 1024 * 1024) {
        return std::nullopt;
    }
    if (bytes[0] != 'C' || bytes[1] != 'Y' || bytes[2] != 'G' || bytes[3] != '1') {
        return std::nullopt;
    }

    const auto read_u32le = [&bytes](size_t offset) -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            val |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
        }
        return val;
    };

    const uint32_t genesis_len = read_u32le(4);
    if (genesis_len == 0 || 8 + genesis_len + 4 > bytes.size()) {
        return std::nullopt;
    }

    const size_t state_offset = 8 + genesis_len;
    const uint32_t state_len = read_u32le(state_offset);
    if (state_offset + 4 + state_len != bytes.size()) {
        return std::nullopt;
    }

    auto parsed_genesis = DeserializeSignedNetworkGenesis(bytes.subspan(8, genesis_len));
    if (!parsed_genesis) return std::nullopt;

    auto verified_genesis = VerifiedNetworkGenesis::Create(*parsed_genesis);
    if (!verified_genesis) return std::nullopt;

    auto state = DeserializeCybouState(bytes.subspan(state_offset + 4, state_len));
    if (!state) return std::nullopt;

    if (ValidateCybouState(*state) != StateValidationError::NONE) {
        return std::nullopt;
    }

    const auto state_hash = CybouStateHash(*state);
    if (!state_hash || *state_hash != verified_genesis->GetGenesisStateRoot()) {
        return std::nullopt;
    }

    for (const auto& entry : verified_genesis->GetInitialAuthority()) {
        if (!state->genesis_allocations.contains(entry.recovery_key_id)) {
            return std::nullopt;
        }
    }

    CybouNetworkDefinition def;
    def.protocol_version = CYBOU_NETWORK_DEFINITION_VERSION;
    def.genesis_state_root = verified_genesis->GetGenesisStateRoot();
    def.poa_finalizer_public_key = verified_genesis->GetPoaPublicKey();
    def.genesis_block_id = ComputeGenesisBlockId(def.genesis_state_root, def.poa_finalizer_public_key);
    def.protocol_parameters = verified_genesis->GetProtocolParameters();

    if (ValidateNetworkDefinition(def) != NetworkDefinitionError::NONE) {
        return std::nullopt;
    }

    const auto digest = verified_genesis->GetGenesisDigest();
    return VerifiedNetworkBundle{
        .genesis = std::move(*verified_genesis),
        .genesis_state = std::move(*state),
        .network_definition = std::move(def),
        .genesis_digest = digest,
    };
}

std::optional<VerifiedNetworkBundle> LoadNetworkGenesisBundle(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size < 12 || file_size > 20 * 1024 * 1024) return std::nullopt;

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return std::nullopt;

    std::vector<unsigned char> bytes(file_size);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(file_size));
    if (!stream) return std::nullopt;

    return VerifyNetworkGenesisBundle(bytes);
}

} // namespace cybou
