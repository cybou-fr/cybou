// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Консенсусная экономика хранения: StorageLease и PoA-подписанный StorageSettlement (DEC-279..282).

#ifndef CYBOU_STORAGE_LEASE_H
#define CYBOU_STORAGE_LEASE_H

#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Размер payload StorageLease: OperationID публикации 32 + число периодов 4.
inline constexpr size_t STORAGE_LEASE_PAYLOAD_SIZE{32 + 4};

/// \brief Аренда хранения своей финализированной публикации на `periods` settlement-периодов.
struct StorageLeasePayload {
    cybou::Hash256 publication_id; ///< OperationID финализированной RootPublication автора.
    uint32_t periods{0};           ///< Оплачиваемые периоды; продлевает действующую аренду.

    friend bool operator==(const StorageLeasePayload&, const StorageLeasePayload&) = default;
};

/// \brief Identity-authorized StorageLease.
struct AuthorizedStorageLease {
    IdentityOperationAuthorization authorization; ///< Авторизация владельца публикации.
    StorageLeasePayload lease;                    ///< Payload аренды.

    friend bool operator==(const AuthorizedStorageLease&, const AuthorizedStorageLease&) = default;
};

std::optional<std::array<unsigned char, STORAGE_LEASE_PAYLOAD_SIZE>> SerializeStorageLeasePayload(
    const StorageLeasePayload& lease);
std::optional<StorageLeasePayload> DeserializeStorageLeasePayload(std::span<const unsigned char> bytes);
std::optional<IdentityKeyId> ComputeStorageLeasePayloadCommitment(const StorageLeasePayload& lease);

/// \brief Escrow: `replicas × ceil(units × rate × periods × period_seconds / (2048 × 86400))`.
/// \details DEC-292 isolated transition development: one whole-CYBOU share per replica.
///          Do not deploy before cumulative settlement/state integration is complete.
/// \return std::nullopt при переполнении или нулевых входах.
std::optional<uint64_t> ComputeStorageLeaseEscrow(const CybouProtocolParameters& params, uint32_t units,
    uint8_t replicas, uint32_t periods);
/// \brief Existing combined daily ceiling; pending cumulative-settlement replacement.
std::optional<uint64_t> ComputeStorageLeasePeriodCap(const CybouProtocolParameters& params, uint32_t units,
    uint8_t replicas);

enum class StorageLeaseError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    SENDER_NOT_FOUND,
    PUBLICATION_NOT_FOUND,
    NOT_OWNER,
    INSUFFICIENT_SYSTEM_BALANCE,
    FEE_TRANSFER_FAILED,
    ESCROW_OVERFLOW,
};

/// \brief Переводит \p escrow из System Balance плательщика в аренду публикации, создавая или продлевая её.
/// \pre Достаточный System Balance и отсутствие переполнения уже проверены вызывающим.
void FundStorageLease(CybouState& state, const cybou::Hash256& publication_id, const AccountId& payer,
    uint32_t units, uint8_t replicas, uint32_t periods, uint64_t escrow,
    const CybouProtocolParameters& params, const cybou::Hash256& funding_operation_id);

/// \brief Создаёт или продлевает аренду: комиссия `payment_fee` в Treasury, rent в StorageEscrow (DEC-278).
StorageLeaseError ApplyStorageLease(const AuthorizedStorageLease& op, const cybou::Hash256& network_binding,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Максимум выплат в одном StorageSettlement.
inline constexpr size_t MAX_STORAGE_SETTLEMENT_ENTRIES{1024};
/// \brief Размер одной записи settlement: publication 32 + payout AccountID 32 + amount 8.
inline constexpr size_t STORAGE_SETTLEMENT_ENTRY_SIZE{32 + 32 + 8};

/// \brief Одна выплата provider'у за проверенное хранение по аренде публикации.
struct StorageSettlementEntry {
    cybou::Hash256 publication_id; ///< Ключ аренды.
    AccountId payout_account;      ///< Аккаунт provider'а; никогда не плательщик аренды.
    uint64_t amount{0};            ///< CYBOU из escrow этой аренды.

    friend bool operator==(const StorageSettlementEntry&, const StorageSettlementEntry&) = default;
};

/// \brief PoA-подписанный итог одного settlement-периода (DEC-282).
/// \details PoA — канонический агрегатор off-chain evidence и времени: Full Nodes проверяют подпись,
///          непрерывность периодов, escrow и арифметику, но не сами аудиты.
struct StorageSettlement {
    uint64_t period{0};                          ///< Номер периода, ровно `state.settlement.next_period`.
    uint64_t period_start_utc{0};                ///< UTC-начало периода, непрерывно с предыдущим.
    std::vector<StorageSettlementEntry> entries; ///< Строго по (publication_id, payout_account).
    IdentityHybridSignature poa_signature;       ///< Подпись genesis PoA key над digest.

    friend bool operator==(const StorageSettlement&, const StorageSettlement&) = default;
};

std::optional<std::array<unsigned char, 32>> ComputeStorageSettlementDigest(
    const cybou::Hash256& network_binding, const StorageSettlement& settlement);
std::optional<std::vector<unsigned char>> SerializeStorageSettlement(const StorageSettlement& settlement);
std::optional<StorageSettlement> DeserializeStorageSettlement(std::span<const unsigned char> bytes);

enum class StorageSettlementError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_SIGNATURE,
    WRONG_PERIOD,
    WRONG_PERIOD_START,
    LEASE_NOT_FOUND,
    LEASE_NOT_ACTIVE,
    PAYOUT_ACCOUNT_NOT_FOUND,
    SELF_PAYOUT,
    TOO_MANY_PAYOUTS,
    PAYOUT_EXCEEDS_ESCROW,
    BALANCE_OVERFLOW,
};

/// \brief Выплачивает providers из escrow, продвигает курсор и возвращает escrow закончившихся аренд.
/// \details Onboarding-часть escrow расходуется первой и зачисляется в System Balance provider'а,
///          locked-часть — в его Balance (DEC-281).
StorageSettlementError ApplyStorageSettlement(const StorageSettlement& settlement,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params,
    const IdentityHybridPublicKey& poa_key, CybouState& state);

} // namespace cybou

#endif // CYBOU_STORAGE_LEASE_H
