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
/// \param params Набор immutable protocol parameters из genesis.
/// \return true, если параметры не нарушают инварианты диапазонов и не приводят к переполнению fee-арифметики.
bool ValidateProtocolParameters(const CybouProtocolParameters& params);
/// \brief Вычисляет immutable NetworkBinding = SHA-256("CYBOU/NETWORK-ID" || canonical Network Public Key).
/// \param network_public_key Canonical Network Public Key текущей official network.
/// \return 32-байтовый NetworkBinding, используемый в P2P, блоках и подписях.
cybou::Hash256 ComputeNetworkBinding(const IdentityHybridPublicKey& network_public_key);
/// \brief Создаёт canonical DEV genesis state без локальных модификаций.
/// \return Базовое genesis state до добавления конкретных genesis allocations.
CybouState CreateDevGenesisState();

inline constexpr std::string_view CYBOU_GENESIS_SIGNING_DOMAIN{"CYBOU/GENESIS"};

/// \brief Canonical signed network genesis specification; NetworkID — точные canonical bytes network_public_key.
struct NetworkGenesis {

    /// \brief Network Public Key — офлайн root authority конкретной official network.
    IdentityHybridPublicKey network_public_key{IdentityKeyPurpose::NETWORK_ROOT, {}, {}};

    /// \brief State root начального finalized state этой сети.
    cybou::Hash256 genesis_state_root;
    /// \brief Genesis-authorized PoA public key, единственный допустимый finalizer этой сети.
    IdentityHybridPublicKey poa_finalizer_public_key{IdentityKeyPurpose::POA_FINALIZER, {}, {}};
    /// \brief Immutable protocol parameters, действующие с высоты 0.
    CybouProtocolParameters protocol_parameters;

    /// \brief Подпись офлайн Network Private Key по canonical digest спецификации.
    IdentityHybridSignature signature;

    friend bool operator==(const NetworkGenesis&, const NetworkGenesis&) = default;
};

/// \brief Причина отказа верификации signed NetworkGenesis.
enum class NetworkGenesisError : uint8_t {
    /// \brief Проверка пройдена.
    NONE,
    /// \brief network_public_key имеет неверный purpose, размер или нулевой Ed25519-компонент.
    INVALID_NETWORK_KEY,
    /// \brief genesis_state_root равен нулю и не может быть якорем height-zero state.
    NULL_GENESIS_STATE_ROOT,
    /// \brief poa_finalizer_public_key имеет неверный purpose, размер или нулевой Ed25519-компонент.
    INVALID_POA_KEY,
    /// \brief protocol_parameters нарушают диапазоны или приводят к небезопасной арифметике.
    INVALID_PROTOCOL_PARAMETERS,
    /// \brief Подпись Network Private Key не совпадает с canonical digest спецификации.
    INVALID_SIGNATURE,
};

/// \brief Сериализует неподписанный canonical payload NetworkGenesis.
/// \param genesis Структурированная genesis-спецификация.
/// \return Payload, по которому вычисляется digest и который не содержит signature.
std::vector<unsigned char> SerializeNetworkGenesisPayload(const NetworkGenesis& genesis);

/// \brief Вычисляет canonical digest спецификации, подписываемый Network Key.
/// \param genesis Canonical genesis-спецификация.
/// \return SHA-256 digest домена `CYBOU/GENESIS` и payload.
cybou::Hash256 ComputeNetworkGenesisDigest(const NetworkGenesis& genesis);

/// \brief Полностью сериализует signed NetworkGenesis.
/// \param genesis Полностью заполненная подписанная genesis-спецификация.
/// \return Canonical wire bytes signed genesis.
std::vector<unsigned char> SerializeSignedNetworkGenesis(const NetworkGenesis& genesis);

/// \brief Разбирает signed NetworkGenesis из canonical bytes.
/// \param bytes Exact wire bytes signed genesis.
/// \return Разобранная структура или std::nullopt при нарушении canonical layout.
std::optional<NetworkGenesis> DeserializeSignedNetworkGenesis(std::span<const unsigned char> bytes);

/// \brief Валидирует и криптографически проверяет signed NetworkGenesis.
/// \param genesis Genesis для проверки.
/// \return Причина отказа либо NONE при успехе.
NetworkGenesisError VerifySignedNetworkGenesis(const NetworkGenesis& genesis);

/// \brief High-integrity wrapper: внутри только криптографически и семантически verified genesis.
class VerifiedNetworkGenesis {
public:
    /// \brief Создаёт verified-объект только после полной проверки подписи и семантики.
    /// \param genesis Кандидат на immutable signed genesis.
    /// \return Обёртка или std::nullopt, если VerifySignedNetworkGenesis() не вернул NONE.
    static std::optional<VerifiedNetworkGenesis> Create(NetworkGenesis genesis);

    /// \brief Возвращает исходную verified genesis-структуру.
    const NetworkGenesis& GetGenesis() const noexcept { return m_genesis; }
    /// \brief Возвращает height-zero anchor этой сети; синоним GetGenesisDigest().
    const cybou::Hash256& GetGenesisAnchor() const noexcept { return m_genesis_digest; }
    /// \brief Возвращает digest immutable genesis specification.
    const cybou::Hash256& GetGenesisDigest() const noexcept { return m_genesis_digest; }
    /// \brief Возвращает verified Network Public Key.
    const IdentityHybridPublicKey& GetNetworkPublicKey() const noexcept { return m_genesis.network_public_key; }
    /// \brief Возвращает verified state root начального состояния.
    const cybou::Hash256& GetGenesisStateRoot() const noexcept { return m_genesis.genesis_state_root; }
    /// \brief Возвращает genesis-authorized PoA public key.
    const IdentityHybridPublicKey& GetPoaPublicKey() const noexcept { return m_genesis.poa_finalizer_public_key; }
    /// \brief Возвращает immutable protocol parameters этой сети.
    const CybouProtocolParameters& GetProtocolParameters() const noexcept { return m_genesis.protocol_parameters; }

    /// \brief Возвращает точные canonical NetworkID bytes этой сети.
    /// \return Exact bytes canonical serialized Network Public Key.
    std::span<const unsigned char> GetNetworkId() const noexcept { return m_network_id_bytes; }

private:
    explicit VerifiedNetworkGenesis(NetworkGenesis genesis, std::vector<unsigned char> network_id_bytes, cybou::Hash256 genesis_digest);

    NetworkGenesis m_genesis;
    std::vector<unsigned char> m_network_id_bytes;
    cybou::Hash256 m_genesis_digest{cybou::Hash256::ZERO};
};

/// \brief Canonical byte serialization любого IdentityHybridPublicKey.
/// \param key Публичный ключ Identity любого поддерживаемого purpose.
/// \return Каноническое byte-представление key.
std::vector<unsigned char> CanonicalSerializeHybridPublicKey(const IdentityHybridPublicKey& key);
/// \brief Canonical разбор произвольного IdentityHybridPublicKey с optional проверкой purpose.
/// \param bytes Canonical bytes публичного ключа.
/// \param expected_purpose Optional ожидаемый purpose; при несовпадении возвращается std::nullopt.
/// \return Разобранный ключ или std::nullopt при неверной длине/структуре/purpose.
std::optional<IdentityHybridPublicKey> CanonicalDeserializeHybridPublicKey(
    std::span<const unsigned char> bytes,
    std::optional<IdentityKeyPurpose> expected_purpose = std::nullopt);

/// \brief Canonical byte serialization Network Public Key, то есть exact NetworkID bytes.
/// \param key Network Public Key.
/// \return Exact bytes NetworkID для данной сети.
std::vector<unsigned char> CanonicalSerializeNetworkPublicKey(const IdentityHybridPublicKey& key);
/// \brief Canonical разбор exact NetworkID bytes обратно в Network Public Key.
/// \param bytes Exact NetworkID bytes.
/// \return Network Public Key или std::nullopt, если bytes не кодируют NETWORK_ROOT key.
std::optional<IdentityHybridPublicKey> CanonicalDeserializeNetworkPublicKey(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_NETWORK_GENESIS_H
