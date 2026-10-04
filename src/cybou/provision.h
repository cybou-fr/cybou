// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROVISION_H
#define CYBOU_PROVISION_H

/// \file
/// \brief Одноразовое офлайн provisioning DEVNET и генерация compiled public constants.

#include <cybou/identity_crypto.h>
#include <cybou/identity_kem.h>
#include <cybou/network_genesis.h>
#include <cybou/recovery_phrase.h>
#include <cybou/state.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {

/// \brief Полный набор результатов DEVNET provisioning: приватный материал, genesis и public constants.
struct DevnetProvisionResult {
    // Network Root (strictly offline)
    /// \brief Entropy офлайн Network Root mnemonic.
    RecoveryEntropy network_entropy{};
    /// \brief Мнемонические слова офлайн Network Root.
    RecoveryWords network_words{};
    /// \brief Public Network Root key, определяющий NetworkID.
    IdentityHybridPublicKey network_public_key;
    /// \brief Exact canonical NetworkID bytes.
    std::vector<unsigned char> network_id_bytes;

    // cybou.cybou Identity
    /// \brief Entropy Identity `cybou.cybou`, включая PoA role derivation.
    RecoveryEntropy cybou_entropy{};
    /// \brief Мнемонические слова Identity `cybou.cybou`.
    RecoveryWords cybou_words{};
    /// \brief Recovery public key `cybou.cybou`.
    IdentityHybridPublicKey cybou_recovery_key;
    /// \brief Recovery Key ID для genesis allocation и recovery flows.
    IdentityKeyId cybou_recovery_key_id{};
    /// \brief Authorization public key `cybou.cybou`.
    IdentityHybridPublicKey cybou_auth_key;
    /// \brief Public XWing KEM key `cybou.cybou`.
    XWingPublicKey cybou_kem_pub{};
    /// \brief PoA finalizer public key, derivable из той же mnemonic роли.
    IdentityHybridPublicKey cybou_poa_pub;
    /// \brief PoA Finalizer Key ID для отчётов и public constants.
    IdentityKeyId cybou_poa_key_id{};

    // bootstrap Identity: ordinary Identity of the bootstrap locator operator
    /// \brief Entropy Identity `bootstrap`.
    RecoveryEntropy bootstrap_entropy{};
    /// \brief Мнемонические слова Identity `bootstrap`.
    RecoveryWords bootstrap_words{};
    /// \brief Recovery Key ID genesis allocation `bootstrap`.
    IdentityKeyId bootstrap_recovery_key_id{};

    // Consensus Genesis & Initial State
    /// \brief Genesis state до высоты 1.
    CybouState genesis_state;
    /// \brief Полностью подписанный immutable NetworkGenesis.
    NetworkGenesis signed_genesis;
    /// \brief BLAKE3 state root genesis_state.
    cybou::Hash256 genesis_state_root;
    /// \brief Canonical bytes signed_genesis.
    std::vector<unsigned char> serialized_signed_genesis;
    /// \brief Canonical bytes genesis_state.
    std::vector<unsigned char> serialized_genesis_state;
};

/// \brief AUTH каждого genesis-выделения: строго выше порога Validation (10,000,000).
inline constexpr uint64_t GENESIS_ALLOCATION_AUTHORITY{10'000'001};
/// \brief Spendable CYBOU genesis-выделения Central Authority.
inline constexpr uint64_t CENTRAL_AUTHORITY_GENESIS_BALANCE{100'000'000};
/// \brief Метка (и зарезервированное имя) genesis-выделения bootstrap Identity.
inline constexpr std::string_view BOOTSTRAP_ALLOCATION_LABEL{"bootstrap"};

/// \brief Уже существующая Identity `cybou.cybou`, переносимая в новую сеть без смены фразы.
struct ExistingCentralAuthority {
    RecoveryEntropy entropy{};
};

/// \brief Генерирует весь DEVNET provisioning полностью в памяти.
/// \return Полный набор секретов и public artifacts либо std::nullopt при криптографической/серилизационной ошибке.
/// \post При успехе результат уже проходит round-trip и self-verification.
/// \param central_authority Если задан, `cybou.cybou` сохраняет эту фразу (новая сеть, тот же PoA-ключ; AccountID создаётся при онбординге).
std::optional<DevnetProvisionResult> GenerateDevnetProvisioning(
    const std::optional<ExistingCentralAuthority>& central_authority = std::nullopt);

/// \brief Выполняет одноразовое provisioning: private secrets и public constants header.
/// \param private_dir Каталог под gitignored private material внутри repository private/.
/// \param constants_header_path Путь к создаваемому public constants header.
/// \return true при полном успешном provisioning без перезаписи существующих материалов.
/// \pre private_dir указывает внутрь repository private/ и либо отсутствует, либо пуст.
/// \post При true secrets и constants созданы эксклюзивно; существующие сети не перезаписываются.
bool ProvisionDevnet(
    const std::filesystem::path& private_dir,
    const std::filesystem::path& constants_header_path,
    const std::optional<ExistingCentralAuthority>& central_authority = std::nullopt);

} // namespace cybou

#endif // CYBOU_PROVISION_H
