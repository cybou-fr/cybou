// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_NETWORK_GENESIS_H
#define CYBOU_NETWORK_GENESIS_H

/// \file
/// \brief Canonical signed NetworkGenesis и операции вокруг immutable official network definition.

#include <cybou/identity_crypto.h>
#include <cybou/protocol_params.h>
#include <cybou/state.h>
#include <cybou/hash256.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/// \brief Проверяет допустимость immutable protocol parameters внутри NetworkGenesis.
bool ValidateProtocolParameters(const CybouProtocolParameters& params);
/// \brief Вычисляет immutable NetworkBinding = SHA-256("CYBOU/NETWORK-ID" || canonical Network Public Key).
cybou::Hash256 ComputeNetworkBinding(const IdentityHybridPublicKey& network_public_key);
/// \brief Создаёт canonical DEV genesis state без локальных модификаций.
CybouState CreateDevGenesisState();

inline constexpr std::string_view CYBOU_GENESIS_SIGNING_DOMAIN{"CYBOU/GENESIS"};

/// \brief Canonical signed network genesis specification; NetworkID — точные canonical bytes network_public_key.
struct NetworkGenesis {

    /// \brief Network Public Key — офлайн root authority конкретной official network.
    IdentityHybridPublicKey network_public_key{IdentityKeyPurpose::NETWORK_ROOT, {}, {}};

    cybou::Hash256 genesis_state_root;
    IdentityHybridPublicKey poa_finalizer_public_key{IdentityKeyPurpose::POA_FINALIZER, {}, {}};
    CybouProtocolParameters protocol_parameters;

    /// \brief Подпись офлайн Network Private Key по canonical digest спецификации.
    IdentityHybridSignature signature;

    friend bool operator==(const NetworkGenesis&, const NetworkGenesis&) = default;
};

/// \brief Причина отказа верификации signed NetworkGenesis.
enum class NetworkGenesisError : uint8_t {
    NONE,
    INVALID_NETWORK_KEY,
    NULL_GENESIS_STATE_ROOT,
    INVALID_POA_KEY,
    INVALID_PROTOCOL_PARAMETERS,
    INVALID_SIGNATURE,
};

/// \brief Сериализует неподписанный canonical payload NetworkGenesis.
std::vector<unsigned char> SerializeNetworkGenesisPayload(const NetworkGenesis& genesis);

/// \brief Вычисляет canonical digest спецификации, подписываемый Network Key.
cybou::Hash256 ComputeNetworkGenesisDigest(const NetworkGenesis& genesis);

/// \brief Полностью сериализует signed NetworkGenesis.
std::vector<unsigned char> SerializeSignedNetworkGenesis(const NetworkGenesis& genesis);

/// \brief Разбирает signed NetworkGenesis из canonical bytes.
std::optional<NetworkGenesis> DeserializeSignedNetworkGenesis(std::span<const unsigned char> bytes);

/// \brief Валидирует и криптографически проверяет signed NetworkGenesis.
NetworkGenesisError VerifySignedNetworkGenesis(const NetworkGenesis& genesis);

/// \brief High-integrity wrapper: внутри только криптографически и семантически verified genesis.
class VerifiedNetworkGenesis {
public:
    /// \brief Создаёт verified-объект только после полной проверки подписи и семантики.
    static std::optional<VerifiedNetworkGenesis> Create(NetworkGenesis genesis);

    const NetworkGenesis& GetGenesis() const noexcept { return m_genesis; }
    const cybou::Hash256& GetGenesisAnchor() const noexcept { return m_genesis_digest; }
    const cybou::Hash256& GetGenesisDigest() const noexcept { return m_genesis_digest; }
    const IdentityHybridPublicKey& GetNetworkPublicKey() const noexcept { return m_genesis.network_public_key; }
    const cybou::Hash256& GetGenesisStateRoot() const noexcept { return m_genesis.genesis_state_root; }
    const IdentityHybridPublicKey& GetPoaPublicKey() const noexcept { return m_genesis.poa_finalizer_public_key; }
    const CybouProtocolParameters& GetProtocolParameters() const noexcept { return m_genesis.protocol_parameters; }

    /// \brief Возвращает точные canonical NetworkID bytes этой сети.
    std::span<const unsigned char> GetNetworkId() const noexcept { return m_network_id_bytes; }

private:
    explicit VerifiedNetworkGenesis(NetworkGenesis genesis, std::vector<unsigned char> network_id_bytes, cybou::Hash256 genesis_digest);

    NetworkGenesis m_genesis;
    std::vector<unsigned char> m_network_id_bytes;
    cybou::Hash256 m_genesis_digest{cybou::Hash256::ZERO};
};

/// \brief Canonical byte serialization любого IdentityHybridPublicKey.
std::vector<unsigned char> CanonicalSerializeHybridPublicKey(const IdentityHybridPublicKey& key);
/// \brief Canonical разбор произвольного IdentityHybridPublicKey с optional проверкой purpose.
std::optional<IdentityHybridPublicKey> CanonicalDeserializeHybridPublicKey(
    std::span<const unsigned char> bytes,
    std::optional<IdentityKeyPurpose> expected_purpose = std::nullopt);

/// \brief Canonical byte serialization Network Public Key, то есть exact NetworkID bytes.
std::vector<unsigned char> CanonicalSerializeNetworkPublicKey(const IdentityHybridPublicKey& key);
/// \brief Canonical разбор exact NetworkID bytes обратно в Network Public Key.
std::optional<IdentityHybridPublicKey> CanonicalDeserializeNetworkPublicKey(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_NETWORK_GENESIS_H
