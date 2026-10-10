// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Каноническое состояние цепочки CYBOU и детерминированные state-transition helpers.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/economics.h>
#include <cybou/identity_registry.h>
#include <cybou/name_registry.h>
#include <cybou/root_publication.h>
#include <cybou/protocol_params.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cybou {

/// \brief Каноническое состояние аккаунта, коммитящееся в корень состояния.
struct AccountState {
    uint64_t balance{0};          ///< Spendable Balance в CYBOU.
    uint64_t system_balance{0};   ///< Непереводимый System Balance для сетевых комиссий и сервисного бюджета.
    /// \brief Часть `system_balance` onboarding-происхождения (DEC-281): расходуется первой
    ///        и через storage payouts никогда не становится spendable Balance.
    uint64_t onboarding_system_balance{0};
    uint64_t creation_height{0};  ///< Высота блока, на которой аккаунт был финализирован через `AccountCreate`.
    uint64_t creation_epoch{0};   ///< Детерминированный epoch, вычисленный из `creation_height` и protocol params.

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/// \brief Неизменяемое genesis-выделение для recovery-ключа, заявляемое ровно одним AccountCreate.
struct GenesisAllocation {
    uint64_t balance{0};                   ///< Начальный Balance, зарезервированный под один recovery key id.
    std::string label;                     ///< Необязательная genesis-метка/имя, резервируемая без отдельного reveal.
    std::optional<AccountId> claimed_by;   ///< Аккаунт, единожды заявивший allocation; `nullopt` до claim-а.

    friend bool operator==(const GenesisAllocation&, const GenesisAllocation&) = default;
};

inline constexpr size_t MAX_GENESIS_ALLOCATIONS{16};

/// \brief Запись Notarial Register о действующей финализированной RootPublication.
struct PublicationRecord {
    AccountId owner;                   ///< Авторизовавшая публикацию Identity.
    ChunkId chunk_authorization_root{}; ///< Merkle root авторизации chunk-ов для storage admission и purge.
    uint32_t chunk_count{0};           ///< Число authorized chunk-ов = billing units аренды (DEC-279).
    uint64_t height{0};                ///< Высота финализации.

    friend bool operator==(const PublicationRecord&, const PublicationRecord&) = default;
};

/// \brief Финализированная аренда хранения одной публикации (DEC-279).
/// \details Покрывает settlement-периоды `[first_period, end_period)`. Escrow хранится раздельно по
///          происхождению, чтобы onboarding-часть платилась providers только в System Balance (DEC-281).
/// Accepted eligibility is term-scoped; rotation cannot rewrite its historical pair.
struct StorageAcceptedBinding {
    std::array<unsigned char, 32> storage_id{};
    AccountId payout_account;
    uint64_t key_epoch{0};
    uint64_t accepted_height{0};
    friend bool operator==(const StorageAcceptedBinding&, const StorageAcceptedBinding&) = default;
};
struct StorageAssignmentDeclaration {
    Hash256 operation_id;
    uint64_t epoch{0}, height{0};
    std::vector<StorageAcceptedBinding> eligible;
    friend bool operator==(const StorageAssignmentDeclaration&, const StorageAssignmentDeclaration&) = default;
};
struct StorageAssignedUnits {
    uint8_t slot{0};
    std::array<unsigned char, 32> storage_id{};
    AccountId payout_account;
    uint32_t units{0};
    friend bool operator==(const StorageAssignedUnits&, const StorageAssignedUnits&) = default;
};
struct StorageAcceptedAssignment {
    Hash256 operation_id, preparation_id, seed;
    uint64_t epoch{0}, effective_period{0};
    std::vector<StorageAssignedUnits> allocations;
    friend bool operator==(const StorageAcceptedAssignment&, const StorageAcceptedAssignment&) = default;
};

struct StorageServicePayment {
    uint8_t slot{0};
    std::array<unsigned char, 32> storage_id{};
    AccountId payout_account;
    uint64_t verified_unit_seconds{0}, paid{0}, closed_epoch_capacity{0};
    friend bool operator==(const StorageServicePayment&, const StorageServicePayment&) = default;
};

struct StorageFundedTerm {
    cybou::Hash256 funding_operation_id;
    uint64_t first_period{0};
    uint64_t end_period{0};
    uint64_t rate{0};
    uint64_t period_seconds{0};
    uint64_t replica_share{0};
    uint64_t contracted_unit_seconds{0};
    // Original funding provenance, not another live escrow balance.
    uint64_t initial_onboarding{0};
    uint64_t initial_locked{0};
    // Finalized debits of this term only; renewal never rescales or resets them.
    uint64_t paid_onboarding{0};
    uint64_t paid_locked{0};
    uint64_t next_assignment_epoch{1};
    std::vector<StorageAssignmentDeclaration> declarations;
    std::vector<StorageAcceptedAssignment> assignments;
    std::vector<StorageServicePayment> service_payments;
    uint64_t refunded_onboarding{0}, refunded_locked{0};
    friend bool operator==(const StorageFundedTerm&, const StorageFundedTerm&) = default;
};

struct StorageLeaseRecord {
    AccountId payer;                  ///< Владелец публикации, оплативший аренду.
    uint32_t units{0};                ///< Billing units: authorized chunk-и публикации.
    uint8_t replicas{0};              ///< Оплаченное число remote replicas.
    uint64_t first_period{0};         ///< Первый покрытый settlement-период.
    uint64_t end_period{0};           ///< Первый непокрытый период (исключительно).
    uint64_t escrow_onboarding{0};    ///< Escrow onboarding-происхождения.
    uint64_t escrow_locked{0};        ///< Escrow SystemLock-происхождения.
    std::vector<StorageFundedTerm> funded_terms; ///< Immutable funding terms and their finalized debits.

    friend bool operator==(const StorageLeaseRecord&, const StorageLeaseRecord&) = default;
};

inline constexpr uint8_t MAX_STORAGE_LEASE_REPLICAS{16};

/// \brief Курсор PoA-settlement: следующий ожидаемый период и его UTC-начало (DEC-282).
struct StorageSettlementCursor {
    uint64_t next_period{0};           ///< Номер следующего ожидаемого StorageSettlement.
    uint64_t next_period_start_utc{0}; ///< UTC-начало следующего периода; 0 до первого settlement.

    friend bool operator==(const StorageSettlementCursor&, const StorageSettlementCursor&) = default;
};

/// \brief Полный консенсусный снимок CYBOU, из которого вычисляется state root.
struct CybouState {
    std::map<AccountId, AccountState> accounts; ///< Канонические аккаунтные значения, отсортированные по `AccountId`.
    IdentityRegistry identities;   ///< Финализированный Identity registry, согласованный 1:1 с `accounts`.
    NameRegistry names;            ///< Реестр `.cybou` имён и pending commit-ов.
    /** Keyed by recovery key id; only claim and pre-claim Central Authority fees mutate it. */
    std::map<IdentityKeyId, GenesisAllocation> genesis_allocations;
    /// \brief Действующие публикации по OperationID их RootPublication (DEC-271).
    std::map<cybou::Hash256, PublicationRecord> publications;
    /// \brief Курсор ежедневного StorageSettlement.
    StorageSettlementCursor settlement;
    /// \brief Аренды хранения по OperationID публикации; переживают revoke до финального settlement.
    std::map<cybou::Hash256, StorageLeaseRecord> leases;
};

/// \brief Списывает \p amount из System Balance, расходуя onboarding-часть первой (DEC-281).
/// \pre `account.system_balance >= amount`.
/// \return Сколько из списанного было onboarding-происхождения.
uint64_t DebitSystemBalance(AccountState& account, uint64_t amount);
/// \brief Возвращает Balance Central Treasury: незаявленную allocation `cybou` или Balance её claimant'а.
/// \return nullptr при неоднозначном или повреждённом месте учёта.
uint64_t* TreasuryBalance(CybouState& state);

/// \brief Ищет уникальное genesis-выделение Central Authority; дубликаты считаются некорректным состоянием.
/// \param state Состояние для поиска.
/// \return Указатель на единственную allocation с меткой `cybou`, либо `nullptr`, если её нет или если найден дубликат.
/// \note Потокобезопасно для неизменяемого доступа; не изменяет состояние и детерминировано.
GenesisAllocation* FindCentralAuthorityAllocation(CybouState& state);
/// \copydoc FindCentralAuthorityAllocation(CybouState&)
const GenesisAllocation* FindCentralAuthorityAllocation(const CybouState& state);
/// \brief Проверяет, можно ли безопасно зачислить комиссию Central Authority без переполнения.
/// \param state Каноническое состояние.
/// \param fee Комиссия в CYBOU, списываемая из `System Balance`.
/// \return `true`, если место назначения комиссии однозначно определено и прибавление не переполняет `uint64_t`.
/// \post Никаких изменений состояния.
bool CanCreditCentralAuthorityFee(const CybouState& state, uint64_t fee);
/// \brief Атомарно зачисляет комиссию Central Authority в текущее место учёта комиссии.
/// \param state Изменяемое кандидатное состояние.
/// \param fee Комиссия в CYBOU.
/// \return `true` при успешном зачёте; `false`, если destination невалиден или произошло бы переполнение.
/// \pre Перед вызовом обычно проверяют `CanCreditCentralAuthorityFee`.
/// \post При успехе изменяется только баланс текущего получателя комиссии; при неуспехе состояние остаётся без изменений.
bool CreditCentralAuthorityFee(CybouState& state, uint64_t fee);

enum class AccountCreateStateError : uint8_t {
    NONE,                       ///< Переход выполнен детерминированно.
    INVALID_CREATE,             ///< `AccountCreate` не прошёл форматную/криптографическую или registry-проверку.
    ACCOUNT_EXISTS,             ///< `account_id` уже присутствует в каноническом состоянии.
    RECOVERY_KEY_EXISTS,        ///< Recovery binding уже заявлен другим аккаунтом.
    ACCOUNT_LIMIT,              ///< Достигнут лимит числа аккаунтов в состоянии.
    INSUFFICIENT_TREASURY,      ///< Central Treasury не может выплатить onboarding bonus (DEC-277).
    INCONSISTENT_STATE,         ///< Нарушены внутренние инварианты `accounts`/`identities`/fee destination.
};

/// \brief Применяет `AccountCreate` к кандидатному состоянию без коммита в хранилище.
/// \param op Кандидат-операция `AccountCreate`.
/// \param network_binding Привязка текущей сети.
/// \param block_height Высота финализируемого блока-контейнера.
/// \param params Активные protocol parameters.
/// \param state Кандидатное состояние, модифицируемое только при успехе.
/// \return Код причины отказа либо `NONE`.
/// \pre `state` должно быть внутренне согласованным; функция не исправляет уже испорченное состояние.
/// \post При успехе атомарно создаются Identity и AccountState; onboarding bonus переводится из
///       Central Treasury в System Balance (кроме claimant'а самой Treasury) и, при наличии
///       matching genesis allocation, она заявляется ровно один раз.
/// \note Детерминированно на всех Full Node; потокобезопасность не гарантируется для совместного доступа к `state`.
AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameCommit к кандидатному состоянию.
NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameReveal к кандидатному состоянию.
NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Результаты валидации и применения RootPublication.
enum class RootPublicationError : uint8_t {
    NONE,                      ///< Переход выполнен.
    INVALID_PAYLOAD,           ///< Payload неканоничен либо не удалось вычислить/сопоставить комиссию.
    INVALID_AUTHORIZATION,     ///< Identity authorization не совпала с payload или с финализированным Identity registry.
    SENDER_NOT_FOUND,          ///< Авторизующий аккаунт отсутствует в состоянии.
    INSUFFICIENT_SYSTEM_BALANCE, ///< Недостаточно `System Balance` для комиссии публикации.
    FEE_TRANSFER_FAILED,       ///< Комиссия не может быть безопасно зачислена Central Authority fail-closed.
};

/// \brief Применяет RootPublication и маршрутизацию её комиссии.
RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state);

/// \brief Результаты применения RevokePublication.
enum class RevokePublicationError : uint8_t {
    NONE,                        ///< Публикация отозвана; её аренда закрывается после текущего периода.
    INVALID_PAYLOAD,             ///< Payload неканоничен.
    INVALID_AUTHORIZATION,       ///< Identity authorization невалидна.
    SENDER_NOT_FOUND,            ///< Авторизующий аккаунт отсутствует.
    PUBLICATION_NOT_FOUND,       ///< Действующей публикации с таким OperationID нет.
    NOT_OWNER,                   ///< Публикация принадлежит другой Identity.
    INSUFFICIENT_SYSTEM_BALANCE, ///< Недостаточно `System Balance` для комиссии.
    FEE_TRANSFER_FAILED,         ///< Комиссия не может быть безопасно зачислена.
};

/// \brief Отзывает собственную публикацию; комиссия равна `payment_fee`.
RevokePublicationError ApplyRevokePublication(const AuthorizedRevokePublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state);

/// \brief Регистрирует финализируемую RootPublication в Notarial Register.
/// \return false, если запись уже существует или владелец отсутствует (состояние не меняется).
bool RecordPublication(CybouState& state, const cybou::Hash256& publication_id, const AccountId& owner,
    const ChunkId& chunk_authorization_root, uint32_t chunk_count, uint64_t height);

/// \brief Ошибки детерминированной валидации канонического состояния.
enum class StateValidationError : uint8_t {
    NONE,                           ///< Все инварианты канонического состояния соблюдены.
    ACCOUNT_LIMIT_EXCEEDED,         ///< Число аккаунтов превысило консенсусный лимит.
    ACCOUNT_IDENTITY_COUNT_MISMATCH, ///< `accounts` и `identities` потеряли взаимно-однозначность.
    MISSING_IDENTITY,               ///< Для AccountState нет matching Identity record.
    DUPLICATE_RECOVERY_BINDING,     ///< RecoveryKeyId неоднозначен или не индексируется обратно.
    BALANCE_OVERFLOW,               ///< TotalCybou переполнен или onboarding-часть превышает System Balance.
    INVALID_NAME_REGISTRY,          ///< Нарушены правила имён, pending commit-ов или genesis-label binding.
    INVALID_RESOURCE_USAGE,         ///< Регистр публикаций неканоничен.
    INVALID_STORAGE_LEASE,          ///< Аренда хранения неканонична или ссылается на отсутствующий аккаунт.
};

/// \brief Проверяет внутренние инварианты консенсусного состояния.
/// \param state Полный снимок состояния.
/// \return Детализированный код ошибки; `NONE` только для канонически допустимого состояния.
/// \post Состояние не изменяется.
/// \note Потокобезопасно при неизменяемом доступе; детерминировано и fail-closed.
StateValidationError ValidateCybouState(const CybouState& state, uint64_t* out_total_cybou = nullptr);
/// \brief Считает все существующие CYBOU, обнаруживая переполнения (DEC-277).
/// \param state Полный снимок состояния.
/// \return Сумма незаявленных genesis allocation + `Balance` + `System Balance` + StorageEscrow;
///         при переполнении возвращает `std::numeric_limits<uint64_t>::max()`.
/// \post Каждый финализированный блок сохраняет её точно: нет mint и нет burn.
uint64_t TotalCybou(const CybouState& state);

/// \brief Сериализует каноническое состояние в детерминированный бинарный формат.
/// \param state Валидное каноническое состояние.
/// \param validate Если true, предварительно выполняет ValidateCybouState.
/// \return Байты единственного поддерживаемого wire/storage-формата либо `std::nullopt`, если
///         состояние нарушает инварианты или не сериализуется без неоднозначности.
/// \pre `ValidateCybouState(state) == StateValidationError::NONE`.
/// \post При успехе порядок байтов полностью каноничен: все map уже отсортированы по ключу.
std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state, bool validate = true);
/// \brief Десериализует и валидирует каноническое состояние.
/// \param bytes Полный сериализованный state snapshot.
/// \return `CybouState`, если вход точен, все поля каноничны и итоговый снимок проходит `ValidateCybouState`;
///         иначе `std::nullopt`.
/// \note Потокобезопасно; legacy-декодеры отсутствуют по инварианту single-current-baseline.
std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated hash канонического состояния.
/// \param state Валидное состояние.
/// \param validate Если true, сериализация выполняет предварительную ValidateCybouState.
/// \return `state root` либо `std::nullopt`, если состояние не сериализуется канонически.
/// \post При успехе hash зависит только от канонических байтов состояния.
/// \note Потокобезопасно и детерминировано.
std::optional<cybou::Hash256> CybouStateHash(const CybouState& state, bool validate = true);

} // namespace cybou
#endif // CYBOU_STATE_H
