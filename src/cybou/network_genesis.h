// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NETWORK_GENESIS_H
#define CYBOU_NETWORK_GENESIS_H

#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>
#include <cybou/protocol_params.h>
#include <cybou/state.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

struct OfficialNetworkProfile;

inline constexpr uint8_t CYBOU_NETWORK_GENESIS_VERSION{1};
inline constexpr std::string_view CYBOU_GENESIS_SIGNING_DOMAIN{"CYBOU/GENESIS/V1"};

/**
 * Baseline initial Authority assignment configured in genesis for a designated recovery key.
 * Used for bootstrap and official identities (e.g. DEV bootstrap baseline = 1,000,001).
 */
struct InitialAuthorityAssignment {
    IdentityKeyId recovery_key_id;
    uint64_t initial_authority{0};

    friend bool operator==(const InitialAuthorityAssignment&, const InitialAuthorityAssignment&) = default;
};

/**
 * Canonical signed network genesis specification.
 * NetworkID is the exact canonical bytes of network_public_key.
 */
struct NetworkGenesis {
    uint8_t version{CYBOU_NETWORK_GENESIS_VERSION};

    /** Network Public Key (Root authority of this official network). */
    IdentityHybridPublicKey network_public_key{IdentityKeyPurpose::NETWORK_ROOT, {}, {}};

    uint256 genesis_state_root;
    IdentityHybridPublicKey poa_finalizer_public_key{IdentityKeyPurpose::POA_FINALIZER, {}, {}};
    CybouProtocolParameters protocol_parameters;

    std::vector<InitialAuthorityAssignment> initial_authority;

    /** Signature signed by the strictly offline Network Private Key over the canonical specification digest. */
    IdentityHybridSignature signature;

    friend bool operator==(const NetworkGenesis&, const NetworkGenesis&) = default;
};

enum class NetworkGenesisError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    INVALID_NETWORK_KEY,
    NULL_GENESIS_STATE_ROOT,
    INVALID_POA_KEY,
    INVALID_PROTOCOL_PARAMETERS,
    INVALID_INITIAL_AUTHORITY,
    INVALID_SIGNATURE,
};

/** Serializes the unsigned specification payload for signing and hashing. */
std::vector<unsigned char> SerializeNetworkGenesisPayload(const NetworkGenesis& genesis);

/** Computes the canonical specification digest to be signed by the Network Key. */
uint256 ComputeNetworkGenesisDigest(const NetworkGenesis& genesis);

/** Fully serializes the signed NetworkGenesis. */
std::vector<unsigned char> SerializeSignedNetworkGenesis(const NetworkGenesis& genesis);

/** Deserializes a signed NetworkGenesis from canonical bytes. */
std::optional<NetworkGenesis> DeserializeSignedNetworkGenesis(std::span<const unsigned char> bytes);

/** Validates and cryptographically verifies the Signed NetworkGenesis. */
NetworkGenesisError VerifySignedNetworkGenesis(const NetworkGenesis& genesis);

/**
 * High-integrity verified network genesis type that guarantees valid signature and semantics.
 */
class VerifiedNetworkGenesis {
public:
    static std::optional<VerifiedNetworkGenesis> Create(NetworkGenesis genesis);

    const NetworkGenesis& GetGenesis() const noexcept { return m_genesis; }
    const uint256& GetGenesisDigest() const noexcept { return m_genesis_digest; }
    const IdentityHybridPublicKey& GetNetworkPublicKey() const noexcept { return m_genesis.network_public_key; }
    const uint256& GetGenesisStateRoot() const noexcept { return m_genesis.genesis_state_root; }
    const IdentityHybridPublicKey& GetPoaPublicKey() const noexcept { return m_genesis.poa_finalizer_public_key; }
    const CybouProtocolParameters& GetProtocolParameters() const noexcept { return m_genesis.protocol_parameters; }
    std::span<const InitialAuthorityAssignment> GetInitialAuthority() const noexcept { return m_genesis.initial_authority; }

    /** Returns the exact canonical NetworkID bytes representing this network. */
    std::span<const unsigned char> GetNetworkId() const noexcept { return m_network_id_bytes; }

private:
    explicit VerifiedNetworkGenesis(NetworkGenesis genesis, std::vector<unsigned char> network_id_bytes, uint256 genesis_digest);

    NetworkGenesis m_genesis;
    std::vector<unsigned char> m_network_id_bytes;
    uint256 m_genesis_digest{uint256::ZERO};
};

/** Helper to create a validly signed VerifiedNetworkGenesis for testing from a definition. */
VerifiedNetworkGenesis CreateTestVerifiedGenesis(
    const CybouNetworkDefinition& definition,
    const std::vector<InitialAuthorityAssignment>& initial_auth = {});

/** Canonical byte serialization of any IdentityHybridPublicKey. */
std::vector<unsigned char> CanonicalSerializeHybridPublicKey(const IdentityHybridPublicKey& key);
std::optional<IdentityHybridPublicKey> CanonicalDeserializeHybridPublicKey(
    std::span<const unsigned char> bytes,
    std::optional<IdentityKeyPurpose> expected_purpose = std::nullopt);

/** Canonical byte serialization of Network Public Key (exact NetworkID). */
std::vector<unsigned char> CanonicalSerializeNetworkPublicKey(const IdentityHybridPublicKey& key);
std::optional<IdentityHybridPublicKey> CanonicalDeserializeNetworkPublicKey(std::span<const unsigned char> bytes);

/**
 * Immutable, cryptographically verified bundle containing the signed genesis specification,
 * genesis state, and derived network consensus definition.
 */
struct VerifiedNetworkBundle {
    VerifiedNetworkGenesis genesis;
    CybouState genesis_state;
    CybouNetworkDefinition network_definition;
    uint256 genesis_digest;
};

/** Serializes a signed genesis and genesis state into a CYG1 bundle. */
std::optional<std::vector<unsigned char>> SerializeNetworkGenesisBundle(
    const NetworkGenesis& genesis,
    const CybouState& genesis_state);

/** Verifies and deserializes a CYG1 signed genesis bundle. */
std::optional<VerifiedNetworkBundle> VerifyNetworkGenesisBundle(std::span<const unsigned char> bytes);

/** Rejects unconfigured official profiles and bundles with a different NetworkID or GenesisDigest. */
bool MatchesOfficialNetworkProfile(const VerifiedNetworkBundle& bundle, const OfficialNetworkProfile& profile);

/** Loads and verifies a CYG1 signed genesis bundle from disk. */
std::optional<VerifiedNetworkBundle> LoadNetworkGenesisBundle(const std::filesystem::path& path);

} // namespace cybou

#endif // CYBOU_NETWORK_GENESIS_H
