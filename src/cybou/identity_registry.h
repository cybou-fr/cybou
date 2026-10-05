// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Канонический реестр Identity и проверки операций создания/ротации/авторизации.

#ifndef CYBOU_IDENTITY_REGISTRY_H
#define CYBOU_IDENTITY_REGISTRY_H

#include <cybou/account_creation.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using IdentityKeyId = std::array<unsigned char, 32>;
/// 2565 = 32 AccountID + 8 nonce + 8 key_epoch + 1 kind + 32 payload commitment + 64 Ed25519 + 2420 ML-DSA-44.
inline constexpr size_t IDENTITY_OPERATION_AUTH_SIZE{2565};
/// Верхняя граница одного канонического снимка реестра, чтобы fail-closed ограничивать память и wire.
inline constexpr uint32_t MAX_IDENTITY_REGISTRY_ACCOUNTS{1'000'000};

/// Каноническая запись одной протокольной Identity.
struct IdentityRecord {
    /// Текущий recovery public key.
    IdentityHybridPublicKey recovery_key;
    /// Текущий authorization public key.
    IdentityHybridPublicKey authorization_key;
    /// Коммитмент канонического IdentityKemPackage для `key_epoch`.
    std::array<unsigned char, 32> kem_package_id{};
    /// Единый nonce для всех обычных операций и IdentityRotate.
    uint64_t nonce{0};
    /// Текущий epoch набора Recovery/Authorization/KEM ролей.
    uint64_t key_epoch{0};

    friend bool operator==(const IdentityRecord&, const IdentityRecord&) = default;
};

/// Атомарный запрос ротации всех публичных возможностей Identity.
struct IdentityRotate {
    /// Identity, чьи публичные роли заменяются атомарно.
    AccountId account_id;
    /// Новый recovery public key.
    IdentityHybridPublicKey new_recovery_key;
    /// Новый authorization public key.
    IdentityHybridPublicKey new_authorization_key;
    /// Новый канонический KEM package того же `key_epoch`.
    IdentityKemPackage new_kem_package{};
    /// Текущий общий nonce Identity до применения операции.
    uint64_t nonce{0};
    /// Следующий `key_epoch`; должен быть ровно на 1 больше текущего.
    uint64_t key_epoch{0};
    /// Подпись старым recovery key по digest ротации.
    IdentityHybridSignature old_recovery_signature;
    /// Proof-of-possession новым recovery key по digest ротации.
    IdentityHybridSignature new_recovery_pop;
    /// Proof-of-possession новым authorization key по digest ротации.
    IdentityHybridSignature new_authorization_pop;

    friend bool operator==(const IdentityRotate&, const IdentityRotate&) = default;
};

/// Разрешённые типы операций, подписываемых authorization-ключом Identity.
enum class IdentityOperationKind : uint8_t {
    /// Перевод spendable Balance между аккаунтами.
    PAYMENT = 1,
    /// Необратимый перевод Balance -> System Balance.
    SYSTEM_LOCK = 2,
    /// Commit-фаза регистрации `.cybou` имени.
    NAME_COMMIT = 3,
    /// Reveal-фаза регистрации `.cybou` имени.
    NAME_REVEAL = 4,
    /// Публикация encrypted application root в data plane.
    ROOT_PUBLICATION = 5,
    /// Отзыв собственной финализированной RootPublication.
    REVOKE_PUBLICATION = 6,
    /// Аренда хранения собственной публикации через StorageEscrow (DEC-279).
    STORAGE_LEASE = 7,
};

/// Каноническая authorizaton-обвязка для одной пользовательской операции.
struct IdentityOperationAuthorization {
    /// Авторизующий AccountID.
    AccountId account_id;
    /// Общий nonce текущей Identity.
    uint64_t nonce{0};
    /// Текущий key epoch authorization-ключа.
    uint64_t key_epoch{0};
    /// Вид кандидат-операции.
    IdentityOperationKind kind{IdentityOperationKind::PAYMENT};
    /// Коммитмент полезной нагрузки операции.
    IdentityKeyId payload_commitment{};
    /// Гибридная подпись authorization-ключом.
    IdentityHybridSignature signature;

    friend bool operator==(const IdentityOperationAuthorization&, const IdentityOperationAuthorization&) = default;
};

/// Причина отказа при проверке изменения реестра Identity.
enum class IdentityRegistryError : uint8_t {
    /// Ошибки нет.
    NONE,
    /// AccountCreate не проходит каноническую валидацию.
    INVALID_CREATE,
    /// AccountID уже зарегистрирован.
    ACCOUNT_EXISTS,
    /// RecoveryKeyId уже сопоставлен другой Identity.
    RECOVERY_KEY_EXISTS,
    /// AccountID отсутствует в реестре.
    ACCOUNT_NOT_FOUND,
    /// Ключи/KEM package не проходят базовые проверки/коммитмент.
    INVALID_KEY,
    /// Nonce или key_epoch не совпадают с каноническим состоянием.
    BAD_NONCE,
    /// Дальнейшее увеличение nonce/key_epoch запрещено переполнением.
    NONCE_EXHAUSTED,
    /// Подпись или proof-of-possession невалидны.
    INVALID_SIGNATURE,
    /// Тип/коммитмент payload не проходят базовую валидацию digest.
    INVALID_PAYLOAD,
};

/// Вычисляет digest запроса ротации Identity.
/// \param network_binding NetworkBinding официальной сети.
/// \param request Канонический запрос IdentityRotate.
/// \return 32-байтовый digest или `std::nullopt`, если входные данные недопустимы.
/// \pre `request.key_epoch` уже указывает следующий epoch, а package относится к нему же.
/// \thread_safety Потокобезопасна.
std::optional<IdentityKeyId> ComputeIdentityRotateDigest(
    const cybou::Hash256& network_binding, const IdentityRotate& request);
/// Вычисляет digest обычной операции Identity.
/// \param network_binding NetworkBinding официальной сети.
/// \param request Authorization-обвязка обычной кандидат-операции.
/// \return 32-байтовый digest или `std::nullopt`, если тип/коммитмент/идентификаторы недопустимы.
/// \thread_safety Потокобезопасна.
std::optional<IdentityKeyId> ComputeIdentityOperationDigest(
    const cybou::Hash256& network_binding, const IdentityOperationAuthorization& request);

/// Подсистема канонической проверки и хранения текущих ключей Identity.
class IdentityRegistry
{
public:
    /// Проверяет и регистрирует новую Identity из AccountCreate.
    /// \return Код причины отказа или `IdentityRegistryError::NONE`.
    /// \pre Вызывается в порядке финализированного применения блока.
    /// \post При успехе добавляет запись и индекс RecoveryKeyId; partial state не оставляет.
    IdentityRegistryError Register(const AccountCreateOp& create,
        const cybou::Hash256& network_binding, uint64_t block_height,
        const CybouProtocolParameters& params);
    /// Проверяет и применяет атомарную ротацию Identity.
    /// \return Код причины отказа или `IdentityRegistryError::NONE`.
    /// \post При успехе атомарно меняет Recovery/Authorization/KEM роли и продвигает nonce/key_epoch.
    IdentityRegistryError RotateIdentity(const IdentityRotate& request, const cybou::Hash256& network_binding);
    /// Проверяет и учитывает обычную авторизованную операцию Identity.
    /// \return Код причины отказа или `IdentityRegistryError::NONE`.
    /// \post При успехе только увеличивает nonce; сами доменные изменения выполняются вне реестра.
    IdentityRegistryError AuthorizeOperation(const IdentityOperationAuthorization& request, const cybou::Hash256& network_binding);

    /// Ищет AccountID по текущему RecoveryKeyId.
    /// \return AccountID или `std::nullopt`, если индекс не содержит ключ.
    /// \thread_safety Не потокобезопасен относительно конкурентной записи того же экземпляра.
    std::optional<AccountId> FindByRecoveryKeyId(const IdentityKeyId& id) const;
    /// Возвращает текущую запись Identity по AccountID.
    /// \return Указатель на запись или `nullptr`, если аккаунт не найден.
    /// \warning Указатель инвалидируется любой неконстантной операцией над реестром.
    const IdentityRecord* Find(const AccountId& id) const;
    /// Возвращает все канонические записи реестра.
    const std::map<AccountId, IdentityRecord>& Accounts() const { return m_accounts; }

    friend std::optional<std::vector<unsigned char>> SerializeIdentityRegistry(const IdentityRegistry& registry);
    friend std::optional<IdentityRegistry> DeserializeIdentityRegistry(std::span<const unsigned char> bytes);

private:
    std::map<AccountId, IdentityRecord> m_accounts;
    std::map<IdentityKeyId, AccountId> m_recovery_index;
};

/// Сериализует весь реестр Identity в канонический бинарный формат.
/// \return Бинарный снимок или `std::nullopt`, если реестр содержит недопустимые данные.
std::optional<std::vector<unsigned char>> SerializeIdentityRegistry(const IdentityRegistry& registry);
/// Десериализует и валидирует канонический бинарный реестр Identity.
/// \return Реестр или `std::nullopt`, если входные данные некорректны или превышают лимиты.
std::optional<IdentityRegistry> DeserializeIdentityRegistry(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_IDENTITY_REGISTRY_H
